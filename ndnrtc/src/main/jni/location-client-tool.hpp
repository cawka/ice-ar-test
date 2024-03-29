/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <ndncert/client-module.hpp>
#include <ndncert/challenge-module.hpp>

#include <ndn-cxx/util/signal.hpp>

namespace ndn {
namespace ndncert {

class LocationClientTool
{
public:
  LocationClientTool(Face& face, KeyChain& keyChain, const Name& caPrefix, const Certificate& caCert);

  void
  start(const std::string& userIdentity);

  void
  errorCb(const std::string& errorInfo);

  void
  newCb(const shared_ptr<RequestState>& state);

  void
  selectCb(const shared_ptr<RequestState>& state);

  void
  downloadCb(const shared_ptr<RequestState>& state);

  void
  validateCb(const shared_ptr<RequestState>& state);

  void
  localhopValidateCb(const shared_ptr<RequestState>& state);

private:
  void
  sendLocalhopValidate(const shared_ptr<RequestState>& state,
                       const JsonSection& validateParams,
                       const ClientModule::RequestCallback& requestCallback,
                       const ClientModule::ErrorCallback& errorCallback);

  void
  handleLocalhopValidateResponse(const Interest& request,
                                 const Data& reply,
                                 const shared_ptr<RequestState>& state,
                                 const ClientModule::RequestCallback& requestCallback,
                                 const ClientModule::ErrorCallback& errorCallback);

public:
  util::Signal<LocationClientTool, const Certificate&> onSuccess;
  util::Signal<LocationClientTool, const std::string&> onFailure;

private:
  const std::string LOCATION_CHALLENGE = "LOCATION";
  ClientModule client;
  KeyChain& m_keyChain;
  Face& m_face;
};

} // namespace ndncert
} // namespace ndn
