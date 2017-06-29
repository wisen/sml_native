LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ATM_VERSION_POSTFIX=$(shell date +%Y%m%d)
$(shell echo "#define ATM_VERSION_POSTFIX \"${ATM_VERSION_POSTFIX}\"" > ${LOCAL_PATH}/version.d)

LOCAL_SRC_FILES:= atrace_monitor.c atrace_impl.c bugreport_impl.c common.c conn.c logcat_impl.c vibrator_impl.cpp

LOCAL_MODULE:= smls

LOCAL_MODULE_TAGS:= optional

LOCAL_SHARED_LIBRARIES := \
    libbinder \
    libcutils \
    libutils \
    libz \

#LOCAL_INIT_RC := atrace.rc

include $(BUILD_EXECUTABLE)

#$(shell cp -av $(OUT)/system/bin/smls ./bin/smls)

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
