LOCAL_PATH:= $(call my-dir)

ifeq ($(RLK_ATRACE_MONITOR_SUPPORT),yes)

include $(CLEAR_VARS)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE := atrace_monitor
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := bin/atrace_monitor
include $(BUILD_PREBUILT)

endif
