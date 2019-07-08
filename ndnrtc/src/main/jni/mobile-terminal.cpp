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
static const uint64_t ROUTE_COST(1);
static const time::milliseconds ROUTE_EXPIRATION = 160_s;
static const time::milliseconds HUB_DISCOVERY_INTEREST_LIFETIME = 2_s;

MobileTerminal::MobileTerminal(KeyChain& keyChain, const std::function<bool()>& filterNetworkChange)
  : m_keyChain(keyChain)
  , m_face(nullptr, m_keyChain)
  , m_controller(m_face, m_keyChain)
  , m_scheduler(m_face.getIoService())
  , m_filterNetworkChange(filterNetworkChange)
{
}

void
MobileTerminal::doStart()
{
  m_networkMonitor = std::make_unique<net::NetworkMonitor>(m_face.getIoService());

  m_networkMonitor->onNetworkStateChanged.connect([this] () {
      m_rerunEvent = m_scheduler.schedule(5_s, [this] {
          // NDN_LOG_DEBUG("Detected network state change");
          if (m_filterNetworkChange()) {
            // NDN_LOG_DEBUG("Do nothing (filtered in the up-call)");
            return;
          }
          else {
            NDN_LOG_INFO("Detected AP change. Re-run NDNCERT");
          }

          m_face.shutdown(); // hopefully, this will stop the process so we ready to re-run...
          runDiscoveryAndNdncert();
        });
    });

  runDiscoveryAndNdncert();

  m_face.processEvents(); // will block until doStop
}

void
MobileTerminal::doStop()
{
  m_scheduler.cancelAllEvents();
  m_networkMonitor.reset();
  m_face.shutdown();
}

void
MobileTerminal::runDiscoveryAndNdncert()
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

}

void MobileTerminal::waitUntilFibEntryHasNextHop(size_t nRetriesLeft,
                                                 const Name& prefix, uint64_t faceId,
                                                 const std::function<void()>& continueCallback)
{
  NDN_LOG_TRACE("Check if FIB entry " << prefix << " exists with nexthop " << faceId << ", retries left: " << nRetriesLeft);
  m_controller.fetch<nfd::FibDataset>(
    [=] (const std::vector<nfd::FibEntry>& result) {
      bool hasDesiredNextHop = false;
      for (const auto& entry : result) {
        if (entry.getPrefix() == prefix) { // right now, there is no better way to query for a specifci FIB entry
          for (const auto& nexthop : entry.getNextHopRecords()) {
            if (nexthop.getFaceId() == faceId) {
              hasDesiredNextHop = true;
              break;
            }
          }
          break;
        }
      }
      if (!hasDesiredNextHop) {
        if (nRetriesLeft > 0) {
          m_scheduler.schedule(1_s, [=] {
              waitUntilFibEntryHasNextHop(nRetriesLeft - 1, prefix, faceId, continueCallback);
            });
        }
      }
      else {
        continueCallback();
      }
    },
    [=] (uint32_t code, const std::string& reason) {
      NDN_LOG_ERROR("ERROR `" << reason << "` when checking for FIB entry for " << prefix << " prefix. Cannot proceed");
      this->retval = -1;
      this->errorInfo = "Error when registering " + prefix.toUri() + " prefix. Cannot proceed";
    });
}


void
MobileTerminal::registerPrefixAndEnsureFibEntry(const Name& prefix, uint64_t faceId,
                                                const std::function<void()>& continueCallback)
{
  // register CA prefix
  ControlParameters parameters;
  parameters.setName(prefix)
    .setFaceId(faceId)
    .setCost(ROUTE_COST)
    .setExpirationPeriod(ROUTE_EXPIRATION);

  m_controller.start<nfd::RibRegisterCommand>(
    parameters,
    [=] (const ControlParameters&) {
      m_scheduler.schedule(500_ms, [=] {
          waitUntilFibEntryHasNextHop(5, prefix, faceId, continueCallback);
        }); // wait up 5 seconds theen declare failure
    },
    [=] (const ControlResponse& resp) {
      NDN_LOG_ERROR("ERROR `" << resp << "` when registering " << prefix << " prefix. Cannot proceed");
      this->retval = -1;
      this->errorInfo = "Error when registering " + prefix.toUri() + " prefix. Cannot proceed";
    });
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
    registerPrefixAndEnsureFibEntry(HUB_DISCOVERY_PREFIX, faceStatus.getFaceId(), [this] {
        ++m_nRegSuccess;
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
    setStrategy();
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
    [this] (const auto&...) {
      requestHubData(3);
    },
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
      if (retriesLeft > 0) {
        NDN_LOG_DEBUG("   Got NACK (" << nack.getReason() << ". Retrying after 1 sec delay...");

        m_scheduler.schedule(1_s, [=] {
            requestHubData(retriesLeft - 1);
          });
      }
      else {
        this->fail("Cannot discover local CA (NACKs)");
      }
    },
    [this, retriesLeft] (const Interest&) {
      if (retriesLeft > 0) {
        NDN_LOG_DEBUG("   Got timeout. Retrying...");
        requestHubData(retriesLeft - 1);
      }
      else {
        this->fail("Cannot discover local CA (timed out)");
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
  NDN_LOG_INFO("Requesting certificate from CA " << caPrefix);

  // // register CA prefix
  registerPrefixAndEnsureFibEntry(caPrefix, faceId, [this, faceId] {
      registerPrefixAndEnsureFibEntry("/localhop/CA", faceId, [this] {
          runNdncert();
        });
    });
}

void
MobileTerminal::runNdncert()
{
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
}

} // namespace ndncert
} // namespace ndn
