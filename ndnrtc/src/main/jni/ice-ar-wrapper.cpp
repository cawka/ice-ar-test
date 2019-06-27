#include "ice-ar-wrapper.hpp"

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

namespace foobar {

static std::thread g_thread;

} // namespace foobar


JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_test(JNIEnv* env, jclass, jobject jParams)
{
  auto params = getParams(env, jParams);
  // set/update HOME environment variable
  ::setenv("HOME", params["homePath"].c_str(), true);
  ::setenv("NDN_LOG", "*=ALL", true);
  NDN_LOG_INFO("Will process with app path: " << params.find("homePath")->second.c_str());

  foobar::g_thread = std::thread([params] {
      try {
        ndn::security::v2::KeyChain keyChain;
        NDN_LOG_INFO("Created keychain object with: " << keyChain.getPib().getPibLocator() << ", " << keyChain.getTpm().getTpmLocator());

        auto id = keyChain.createIdentity("/foo/bar");
        NDN_LOG_INFO("Identity created " << id.getName());
      } catch (const std::exception& e) {
        NDN_LOG_ERROR(e.what());
      }
    });
}