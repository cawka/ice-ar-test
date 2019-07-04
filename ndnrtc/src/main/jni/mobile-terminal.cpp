/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "mobile-terminal.hpp"

#include <ndn-cxx/encoding/tlv-nfd.hpp>
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/security/verification-helpers.hpp>
#include <ndn-cxx/util/random.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include <ndn-cxx/util/logger.hpp>

namespace ndn {
namespace ndncert {

NDN_LOG_INIT(ndncert.MobileTerminal);

using nfd::ControlParameters;
using nfd::ControlResponse;

static const Name HUB_DISCOVERY_PREFIX("/localhop/ndn-autoconf/CA");
static const uint64_t HUB_DISCOVERY_ROUTE_COST(1);
static const time::milliseconds HUB_DISCOVERY_ROUTE_EXPIRATION = 160_s;
static const time::milliseconds HUB_DISCOVERY_INTEREST_LIFETIME = 2_s;

MobileTerminal::MobileTerminal()
  : m_face(nullptr, m_keyChain)
  , m_controller(m_face, m_keyChain)
  , m_scheduler(m_face.getIoService())
{
}

void
MobileTerminal::doStart()
{
  m_controller.start<nfd::FaceUpdateCommand>(
    nfd::ControlParameters()
      .setFlagBit(nfd::FaceFlagBit::BIT_LOCAL_FIELDS_ENABLED, true),
    [this] (const auto&...) {
      nfd::FaceQueryFilter filter;
      filter.setLinkType(nfd::LINK_TYPE_MULTI_ACCESS);

      m_controller.fetch<nfd::FaceQueryDataset>(
        filter,
        bind(&MobileTerminal::registerHubDiscoveryPrefix, this, _1),
        [this] (uint32_t code, const std::string& reason) {
          this->fail("Error " + to_string(code) + " when querying multi-access faces: " + reason);
        });
    },
    [this] (const auto&...) {
      this->fail("Cannot set FaceFlags bit");
    });

  m_face.processEvents();
}

void
MobileTerminal::registerHubDiscoveryPrefix(const std::vector<nfd::FaceStatus>& dataset)
{
  if (dataset.empty()) {
    this->fail("No multi-access faces available");
    return;
  }

  m_nRegs = dataset.size();
  m_nRegSuccess = 0;
  m_nRegFailure = 0;

  for (const auto& faceStatus : dataset) {
    ControlParameters parameters;
    parameters.setName(HUB_DISCOVERY_PREFIX)
              .setFaceId(faceStatus.getFaceId())
              .setCost(HUB_DISCOVERY_ROUTE_COST)
              .setExpirationPeriod(HUB_DISCOVERY_ROUTE_EXPIRATION);

    m_controller.start<nfd::RibRegisterCommand>(
      parameters,
      [this] (const ControlParameters&) {
        ++m_nRegSuccess;
        afterReg();
      },
      [this, faceStatus] (const ControlResponse& resp) {
        NDN_LOG_ERROR("Error " << resp.getCode() << " when registering hub discovery prefix "
                      << "for face " << faceStatus.getFaceId() << " (" << faceStatus.getRemoteUri()
                      << "): " << resp.getText());
        ++m_nRegFailure;
        afterReg();
      });
  }
}

void
MobileTerminal::afterReg()
{
  if (m_nRegSuccess + m_nRegFailure < m_nRegs) {
    return; // continue waiting
  }
  if (m_nRegSuccess > 0) {
    NDN_LOG_TRACE("Registered to " << m_nRegSuccess << " faces");
    this->setStrategy();
  }
  else {
    this->fail("Cannot register hub discovery prefix for any face");
  }
}

void
MobileTerminal::setStrategy()
{
  ControlParameters parameters;
  parameters.setName(HUB_DISCOVERY_PREFIX)
            .setStrategy("/localhost/nfd/strategy/multicast"),

  m_controller.start<nfd::StrategyChoiceSetCommand>(
    parameters,
    [this] (const auto&...) { requestHubData(3); },
    [this] (const ControlResponse& resp) {
      this->fail("Error " + to_string(resp.getCode()) + " when setting multicast strategy: " +
                 resp.getText());
    });
}

void
MobileTerminal::requestHubData(size_t retriesLeft)
{
  Interest interest(HUB_DISCOVERY_PREFIX);
  interest.setInterestLifetime(HUB_DISCOVERY_INTEREST_LIFETIME);
  interest.setMustBeFresh(true);
  interest.setCanBePrefix(true);

  NDN_LOG_INFO("Discover localhop CA via " << interest);

  m_face.expressInterest(interest,
    [this] (const Interest&, const Data& data) {

      const Block& content = data.getContent();
      content.parse();
      ndn::security::v2::Certificate cert(data.getContent().blockFromValue());

      NDN_LOG_INFO("Discovered CA:\n" << cert);

      // if (ndn::security::verifySignature(data, cert)){
      //   std::cout << "Device certificate verified by trust anchor!!!\n";
      // }

      uint64_t faceId = 0;
      auto tag = data.getTag<lp::IncomingFaceIdTag>();
      if (tag != nullptr) {
        faceId = tag->get();
      }
      else {
        NDN_LOG_ERROR("Incoming data missing IncomingFaceIdTag");
      }

      // Get CA namespace
      Name caName = cert.getName().getPrefix(-4);

      m_ndncertTool = std::make_unique<ndncert::LocationClientTool>(m_face, m_keyChain, caName, cert);

      // Get certificate to be used for signing data
      registerPrefixAndRunNdncert(caName, faceId);
    },
    [this, retriesLeft] (const Interest&, const lp::Nack& nack) {
      NDN_LOG_DEBUG("Got NACK: " << nack.getReason());
      if (retriesLeft > 0) {
        NDN_LOG_DEBUG("   Retrying in 1 second...");

        m_scheduler.schedule(1_s, [=] {
            requestHubData(retriesLeft - 1);
          });
      }
    },
    [this, retriesLeft] (const Interest&) {
      NDN_LOG_DEBUG("Request timed out");
      if (retriesLeft > 0) {
        NDN_LOG_DEBUG("   Retrying...");
        requestHubData(retriesLeft - 1);
      }
    });
}

void
MobileTerminal::fail(const std::string& msg)
{
  NDN_LOG_ERROR("Discovery failed: " << msg);
}

void
MobileTerminal::registerPrefixAndRunNdncert(const Name& caPrefix, uint64_t faceId)
{
  // register CA prefix
  ControlParameters parameters;
  parameters.setName(caPrefix)
    .setFaceId(faceId)
    .setCost(HUB_DISCOVERY_ROUTE_COST)
    .setExpirationPeriod(HUB_DISCOVERY_ROUTE_EXPIRATION);

  m_controller.start<nfd::RibRegisterCommand>(
    parameters,
    [this, caPrefix] (const ControlParameters&) {

      NDN_LOG_INFO("Requesting certificate from CA " << caPrefix);

      try {
        const std::string letters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890";
        std::string randomUserIdentity;
        std::generate_n(std::back_inserter(randomUserIdentity), 10,
                        [&letters] () -> char {
                          return letters[random::generateSecureWord32() % letters.size()];
                        });

        BOOST_ASSERT(m_ndncertTool != nullptr);
        m_ndncertTool->start(randomUserIdentity);
      }
      catch (const std::exception& error) {
        NDN_LOG_ERROR(boost::diagnostic_information(error));
        this->retval = -1;
        this->errorInfo = error.what();
      }
    },
    [this] (const ControlResponse& resp) {
      NDN_LOG_ERROR("ERROR " << resp.getCode() << " when registering CA prefix. Cannot proceed");
      this->retval = -1;
      this->errorInfo = "Error when registering CA prefix. Cannot proceed";
    });
}

} // namespace ndncert
} // namespace ndn
