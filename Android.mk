LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= atrace_monitor.c atrace_impl.c bugreport_impl.c common.c conn.c

LOCAL_MODULE:= atrace_monitor

LOCAL_MODULE_TAGS:= optional

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    libutils \
    libz \

#LOCAL_INIT_RC := atrace.rc

include $(BUILD_EXECUTABLE)

######
#include $(CLEAR_VARS)

#LOCAL_SRC_FILES := conn.c

#LOCAL_MODULE := connj

#LOCAL_MODULE_TAGS := optional

#LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    libutils \
    libz \

#include $(BUILD_EXECUTABLE)
