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
static std::unique_ptr<ndn::autodiscovery::AutoDiscovery> g_runner;

} // namespace icear


JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_test(JNIEnv* env, jclass, jobject jParams)
{
  std::lock_guard<std::mutex> lk(icear::g_mutex);
  if (icear::g_runner.get() != nullptr) {
    // prevent any double starts
    return;
  }

  auto params = getParams(env, jParams);
  // set/update HOME environment variable
  ::setenv("HOME", params["homePath"].c_str(), true);
  if (params.find("log") != params.end()) {
    ::setenv("NDN_LOG", params["log"].c_str(), true);
  }
  else {
    ::setenv("NDN_LOG", "*=ALL", true);
  }

  NDN_LOG_DEBUG("Will process with app path: " << params.find("homePath")->second.c_str());

  icear::g_thread = std::thread([params] {
      try {
        {
          std::lock_guard<std::mutex> lk(icear::g_mutex);
          if (icear::g_runner.get() != nullptr) {
            // now really prevent double starts
            return;
          }
          icear::g_runner = std::make_unique<ndn::autodiscovery::AutoDiscovery>();
        }

        icear::g_runner->doStart();

        {
          std::lock_guard<std::mutex> lk(icear::g_mutex);
          icear::g_runner.reset();
        }
      } catch (const std::exception& e) {
        NDN_LOG_ERROR(e.what());
      }
    });
}
