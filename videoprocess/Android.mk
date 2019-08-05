# vavpp
#
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

IGNORED_WARNNING = \
	-Wno-sign-compare \
	-Wno-missing-field-initializers \
	-Wno-unused-parameter \

LOCAL_SRC_FILES := \
  ../common/va_display.c \
  ../common/va_display_android.cpp \
  vavpp.cpp

LOCAL_CFLAGS += \
	$(IGNORED_WARNNING) \
    -DANDROID

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	vavpp
LOCAL_PROPRIETARY_MODULE := true

LOCAL_SHARED_LIBRARIES := libva-android libva libdl libdrm  libcutils libutils libgui libm

include $(BUILD_EXECUTABLE)
