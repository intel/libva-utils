# For vainfo
# =====================================================

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	vainfo.c		\
	../common/va_display.c	\

LOCAL_CFLAGS += \
  -DANDROID

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := vainfo

LOCAL_SHARED_LIBRARIES := libva-android libva libdl libdrm libcutils libutils libgui

include $(BUILD_EXECUTABLE)

