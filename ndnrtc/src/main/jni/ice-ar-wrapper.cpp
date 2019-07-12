/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ice-ar-wrapper.hpp"
#include "mobile-terminal.hpp"

#include <map>
#include <string>
#include <thread>

#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/util/exception.hpp>
#include <ndn-cxx/util/logger.hpp>
#include <ndn-cxx/util/logging.hpp>

#include <boost/log/attributes.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/utility/setup.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/expressions/keyword.hpp>

NDN_LOG_INIT(ndncert.Runner);

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
static std::string g_ssid = "";

} // namespace icear

void init(JNIEnv* env);

JavaVM* g_vm;

class ScopedEnv
{
public:
  ScopedEnv()
  {
    int getEnvStat = g_vm->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (getEnvStat == JNI_EDETACHED) {
      g_vm->AttachCurrentThread(&env, nullptr);
      wasAttached = true;
    } else if (getEnvStat == JNI_OK) {
      //
    } else if (getEnvStat == JNI_EVERSION) {
      // NDN_THROW(std::runtime_error("GetEnv: version not supported"));
    }
  }

  ~ScopedEnv()
  {
    if (wasAttached) {
      g_vm->DetachCurrentThread();
    }
  }

  JNIEnv*
  get()
  {
    return env;
  }

private:
  JNIEnv* env;
  bool wasAttached = false;
};

template<class T>
class GlobalRef
{
public:
  GlobalRef(JNIEnv* env, T t)
  {
    m_globalRef = reinterpret_cast<T>(env->NewGlobalRef(t));
  }

  ~GlobalRef()
  {
    ScopedEnv env;
    env.get()->DeleteGlobalRef(m_globalRef);
  }

  T&
  get() {
    return m_globalRef;
  }

private:
  T m_globalRef;
};

template<class T>
class LocalRef
{
public:
  LocalRef(JNIEnv* env, T t)
    : m_env(env)
    , m_localRef(t)
  {
  }

  ~LocalRef()
  {
    // must be called on the same thread !!!
    m_env->DeleteLocalRef(m_localRef);
  }

  T&
  get() {
    return m_localRef;
  }

private:
  JNIEnv* m_env;
  T m_localRef;
};

JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_start(JNIEnv* env, jclass, jobject jParams, jobject notify)
{
  init(env); // logging facilities, though not sure it gonna work

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

  NDN_LOG_TRACE("Will process with app path: " << params.find("homePath")->second.c_str());

  std::lock_guard<std::mutex> lk(icear::g_mutex);
  if (icear::g_runner.get() != nullptr) {
    // prevent any double starts
    NDN_LOG_TRACE("Runner already created, do nothing");
    return;
  }

  auto notifyGlobal = std::make_shared<GlobalRef<jobject>>(env, notify);

  auto jcNotify = env->GetObjectClass(notifyGlobal->get());
  auto jcNotifyOnStart = env->GetMethodID(jcNotify, "onStarted", "()V");
  auto jcNotifyOnStop = env->GetMethodID(jcNotify, "onStopped", "()V");
  if (jcNotifyOnStart == nullptr || jcNotifyOnStop == nullptr) {
    NDN_THROW(std::logic_error("Notification methods not found, abort"));
  }

  auto jcNotifyGetWifi = env->GetMethodID(jcNotify, "getWifi", "()Ljava/lang/String;");
  if (jcNotifyGetWifi == nullptr) {
    NDN_THROW(std::logic_error("jcNotifyGetWifi method not found, abort"));
  }

  icear::g_thread = std::thread([params, notifyGlobal, jcNotifyOnStart, jcNotifyOnStop, jcNotifyGetWifi] {
      try {
        ScopedEnv env;
        auto getSsidFromAndroid = [&env, jcNotifyGetWifi, notifyGlobal] {
          // jcNotifyGetWifi
          jstring jssid = (jstring)env.get()->CallObjectMethod(notifyGlobal->get(), jcNotifyGetWifi);
          const char* ssid = env.get()->GetStringUTFChars(jssid, nullptr);
          std::string retval = ssid;
          env.get()->ReleaseStringUTFChars(jssid, ssid);
          return retval;
        };

        {
          std::lock_guard<std::mutex> lk(icear::g_mutex);
          if (icear::g_runner.get() != nullptr) {
            // now really prevent double starts
            NDN_LOG_TRACE("Runner already created, do nothing");
            return;
          }

          icear::g_ssid = getSsidFromAndroid();
          if (icear::g_ssid == "02:00:00:00:00:00") {
            NDN_LOG_ERROR("Unknown SSID (location permission denied)");
          }
          else {
            NDN_LOG_INFO("Connected SSID WiFi: " << icear::g_ssid);
          }

          if (icear::g_keyChain == nullptr) {
            icear::g_keyChain = std::make_unique<ndn::KeyChain>();
          }

          icear::g_runner = std::make_unique<ndn::ndncert::MobileTerminal>(*icear::g_keyChain, [&getSsidFromAndroid] {
              auto ssid = getSsidFromAndroid();
              if (ssid == "02:00:00:00:00:00") {
                NDN_LOG_ERROR("Unknown SSID (location permission denied)");
                NDN_LOG_DEBUG("Assume re-connected to a new WiFi");
                return false;
              }
              if (ssid != icear::g_ssid) {
                icear::g_ssid = ssid;
                NDN_LOG_DEBUG("Re-connected to a new SSID WiFi: " << icear::g_ssid);
                return false;
              }
              else {
                return true;
              }
            });
        }

        env.get()->CallVoidMethod(notifyGlobal->get(), jcNotifyOnStart);

        NDN_LOG_INFO("NDNCERT + AP monitoring started");
        icear::g_runner->doStart();

        {
          std::lock_guard<std::mutex> lk(icear::g_mutex);
          icear::g_runner.reset();
        }

        NDN_LOG_INFO("NDNCERT + AP monitoring terminated");
        env.get()->CallVoidMethod(notifyGlobal->get(), jcNotifyOnStop);
      } catch (const std::exception& e) {
        NDN_LOG_ERROR(e.what());
      }
    });
  icear::g_thread.detach();
}

JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_stop(JNIEnv*, jclass)
{
  {
    std::lock_guard<std::mutex> lk(icear::g_mutex);
    if (icear::g_runner.get() != nullptr) {
      icear::g_runner->doStop();
    }
  }
}

std::list<std::function<void(JNIEnv* env, const std::string& module, const std::string& severity,
                             const std::string& message)>> g_callbacks;

JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_attach(JNIEnv* env, jclass, jobject logcat)
{
  init(env);

  auto logcatGlobal = std::make_shared<GlobalRef<jobject>>(env, logcat);

  auto jcLogcatFragment = env->GetObjectClass(logcatGlobal->get());
  auto jcLogcatFragmentAddMessageFromNative = env->GetMethodID(jcLogcatFragment,
                                                               "addMessageFromNative", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");

  g_callbacks.push_back([logcatGlobal, jcLogcatFragmentAddMessageFromNative]
                        (JNIEnv* genv, const std::string& module, const std::string& severity, const std::string& message) mutable {

      genv->CallVoidMethod(logcatGlobal->get(), jcLogcatFragmentAddMessageFromNative,
                           LocalRef<jstring>(genv, genv->NewStringUTF(module.c_str())).get(),
                           LocalRef<jstring>(genv, genv->NewStringUTF(severity.c_str())).get(),
                           LocalRef<jstring>(genv, genv->NewStringUTF(message.c_str())).get());
    });
}

JNIEXPORT void JNICALL
Java_net_named_1data_ice_1ar_NdnRtcWrapper_detach(JNIEnv* env, jclass, jobject logcat)
{
  g_callbacks.clear();
}

struct android_sink_backend : public boost::log::sinks::basic_sink_backend<boost::log::sinks::concurrent_feeding>
{
  void
  consume(const boost::log::record_view& rec)
  {
    ScopedEnv genv;

    auto msg = rec[boost::log::expressions::smessage].get();
    auto module = rec[ndn::util::log::module].get();
    auto level = rec[ndn::util::log::severity].get();
    for (auto& callback : g_callbacks) {
      callback(genv.get(), module, boost::lexical_cast<std::string>(level), msg);
    }
  }
};

void
init(JNIEnv* env)
{
  static bool isInit = false;
  if (isInit) {
    return;
  }
  isInit = true;

  env->GetJavaVM(&g_vm);

  typedef boost::log::sinks::synchronous_sink<android_sink_backend> android_sink;
  auto sink = boost::make_shared<android_sink>();

  boost::log::core::get()->add_sink(sink);

}
