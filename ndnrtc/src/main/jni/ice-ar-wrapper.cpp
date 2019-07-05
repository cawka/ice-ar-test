/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ice-ar-wrapper.hpp"
#include "mobile-terminal.hpp"

#include <map>
#include <string>
#include <thread>

#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>

#include <ndn-cxx/security/key-chain.hpp>
#include <android/log.h>

#include <boost/log/attributes.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/expressions/keyword.hpp>

NDN_LOG_INIT(NdnRtcWrapper);

std::map<std::string, std::string>
getParams(JNIEnv* env, jobject jParams)
{
  std::map<std::string, std::string> params;

  jclass jcMap = env->GetObjectClass(jParams);
  jclass jcSet = env->FindClass("java/util/Set");
  jclass jcIterator = env->FindClass("java/util/Iterator");
  jclass jcMapEntry = env->FindClass("java/util/Map$Entry");

  jmethodID jcMapEntrySet      = env->GetMethodID(jcMap,      "entrySet", "()Ljava/util/Set;");
  jmethodID jcSetIterator      = env->GetMethodID(jcSet,      "iterator", "()Ljava/util/Iterator;");
  jmethodID jcIteratorHasNext  = env->GetMethodID(jcIterator, "hasNext",  "()Z");
  jmethodID jcIteratorNext     = env->GetMethodID(jcIterator, "next",     "()Ljava/lang/Object;");
  jmethodID jcMapEntryGetKey   = env->GetMethodID(jcMapEntry, "getKey",   "()Ljava/lang/Object;");
  jmethodID jcMapEntryGetValue = env->GetMethodID(jcMapEntry, "getValue", "()Ljava/lang/Object;");

  jobject jParamsEntrySet = env->CallObjectMethod(jParams, jcMapEntrySet);
  jobject jParamsIterator = env->CallObjectMethod(jParamsEntrySet, jcSetIterator);
  jboolean bHasNext = env->CallBooleanMethod(jParamsIterator, jcIteratorHasNext);
  while (bHasNext) {
    jobject entry = env->CallObjectMethod(jParamsIterator, jcIteratorNext);

    jstring jKey = (jstring)env->CallObjectMethod(entry, jcMapEntryGetKey);
    jstring jValue = (jstring)env->CallObjectMethod(entry, jcMapEntryGetValue);

    const char* cKey = env->GetStringUTFChars(jKey, nullptr);
    const char* cValue = env->GetStringUTFChars(jValue, nullptr);

    params.insert(std::make_pair(cKey, cValue));

    env->ReleaseStringUTFChars(jKey, cKey);
    env->ReleaseStringUTFChars(jValue, cValue);

    bHasNext = env->CallBooleanMethod(jParamsIterator, jcIteratorHasNext);
  }

  return params;
}

namespace icear {

static std::thread g_thread;
static std::mutex g_mutex;
static std::unique_ptr<ndn::ndncert::MobileTerminal> g_runner;
static std::unique_ptr<ndn::KeyChain> g_keyChain;

} // namespace icear

struct android_sink_backend : public boost::log::sinks::basic_sink_backend<boost::log::sinks::concurrent_feeding>
{
  void
  consume(const boost::log::record_view& rec)
  {
    auto msg = rec[boost::log::expressions::smessage].get();
  }
};

void
init()
{
  static bool isInit = false;
  if (isInit) {
    return;
  }
  isInit = true;
  boost::log::add_common_attributes();
  typedef boost::log::sinks::synchronous_sink<android_sink_backend> android_sink;
  auto sink = boost::make_shared<android_sink>();

  boost::log::core::get()->add_sink(sink);
}

JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_start(JNIEnv* env, jclass, jobject jParams)
{
  auto params = getParams(env, jParams);
  // set/update HOME environment variable
  ::setenv("HOME", params["homePath"].c_str(), true);

  // set NDN_CLIENT_TRANSPORT to ensure connection TCP socket (unix doesn't work on Android)
  ::setenv("NDN_CLIENT_TRANSPORT", "tcp4://127.0.0.1:6363", true);

  ::setenv("NDN_CLIENT_PIB", "pib-memory", true);
  ::setenv("NDN_CLIENT_TPM", "tpm-memory", true);

  if (params.find("log") != params.end()) {
    ndn::util::Logging::setLevel(params["log"]);
  }
  else {
    ndn::util::Logging::setLevel("*=ALL");
  }

  NDN_LOG_DEBUG("Will process with app path: " << params.find("homePath")->second.c_str());

  std::lock_guard<std::mutex> lk(icear::g_mutex);
  if (icear::g_runner.get() != nullptr) {
    // prevent any double starts
    NDN_LOG_TRACE("Runner already created, do nothing");
    return;
  }

  icear::g_thread = std::thread([params] {
      try {
        {
          std::lock_guard<std::mutex> lk(icear::g_mutex);
          if (icear::g_runner.get() != nullptr) {
            // now really prevent double starts
            NDN_LOG_TRACE("Runner already created, do nothing");
            return;
          }

          if (icear::g_keyChain == nullptr) {
            icear::g_keyChain = std::make_unique<ndn::KeyChain>();
          }

          icear::g_runner = std::make_unique<ndn::ndncert::MobileTerminal>(*icear::g_keyChain);
        }

        NDN_LOG_TRACE("Initiating NDNCERT");
        icear::g_runner->doStart();

        NDN_LOG_TRACE("End of NDNCERT");

        {
          std::lock_guard<std::mutex> lk(icear::g_mutex);
          icear::g_runner.reset();
        }

        NDN_LOG_TRACE("Terminated NDNCERT");
      } catch (const std::exception& e) {
        NDN_LOG_ERROR(e.what());
      }
    });
}

JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_stop(JNIEnv*, jclass)
{
  {
    std::lock_guard<std::mutex> lk(icear::g_mutex);
    if (icear::g_runner.get() == nullptr) {
      return;
    }
    icear::g_runner->doStop();
  }
  NDN_LOG_TRACE("Requested stop, waiting for thread join");
  icear::g_thread.join();
  NDN_LOG_TRACE("Thread joined");
}
