/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef NFD_TOOLS_NDN_AUTOCONFIG_MULTICAST_DISCOVERY_HPP
#define NFD_TOOLS_NDN_AUTOCONFIG_MULTICAST_DISCOVERY_HPP

#include <ndn-cxx/face.hpp>
#include <ndn-cxx/mgmt/nfd/controller.hpp>
#include <ndn-cxx/mgmt/nfd/face-status.hpp>
#include <ndn-cxx/net/face-uri.hpp>

#include "location-client-tool.hpp"

namespace ndn {
namespace ndncert {

class MobileTerminal
{
public:
  MobileTerminal(KeyChain& keyChain);

  void
  doStart();

  void
  doStop();

private:
  void
  waitUntilFibEntryHasNextHop(size_t nRetriesLeft,
                              const Name& prefix, uint64_t faceId,
                              const std::function<void()>& continueCallback);

  void
  registerPrefixAndEnsureFibEntry(const Name& prefix, uint64_t faceId,
                                  const std::function<void()>& continueCallback);

  void
  registerHubDiscoveryPrefix(const std::vector<nfd::FaceStatus>& dataset);

  void
  afterReg();

  void
  setStrategy();

  void
  onInterest(const InterestFilter& filter, const Interest& interest);

  void
  startListener();

  void
  onRegisterFailed(const Name& prefix, const std::string& reason);

  void
  requestHubData(size_t nRetriesLeft);

  void
  fail(const std::string& msg);

  void
  registerPrefixAndRunNdncert(const Name& caPrefix, uint64_t faceId);

  void
  runNdncert();

  void
  waitUntilFibEntryHasNextHop(const Name& prefix, uint64_t faceId,
                              const std::function<void()>& continueCallback);

public:
  int retval = 0;
  std::string errorInfo = "";

private:
  KeyChain& m_keyChain;
  Face m_face;
  nfd::Controller m_controller;
  Scheduler m_scheduler;
  std::unique_ptr<LocationClientTool> m_ndncertTool;

  int m_nRegs = 0;
  int m_nRegSuccess = 0;
  int m_nRegFailure = 0;
};

} // namespace ndncert
} // namespace ndn

#endif // NFD_TOOLS_NDN_AUTOCONFIG_MULTICAST_DISCOVERY_HPP
