LOCAL_PATH := $(call my-dir)
LOCAL_PATH_SAVED := $(LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_MODULE := ndnrtc-wrapper
LOCAL_SRC_FILES := ndnrtc-wrapper.cpp
LOCAL_SHARED_LIBRARIES := ndnrtc_shared boost_system_shared boost_thread_shared boost_log_shared boost_stacktrace_basic_shared
LOCAL_LDLIBS := -llog -latomic
LOCAL_CFLAGS := -DBOOST_LOG_DYN_LINK -DBOOST_STACKTRACE_DYN_LINK
include $(BUILD_SHARED_LIBRARY)

# Explicitly define versions of precompiled modules
$(call import-module,../packages/ndnrtc/0.0.2)
