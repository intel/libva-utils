LOCAL_PATH:= $(call my-dir)

# mpeg2vldemo
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	mpeg2vldemo.cpp		\
	../common/va_display.c	\
	../common/va_display_android.cpp

LOCAL_CFLAGS += \
    -DANDROID

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	mpeg2vldemo

LOCAL_SHARED_LIBRARIES := libva libva-android libdl libcutils libutils libgui

include $(BUILD_EXECUTABLE)

# loadjpeg
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	loadjpeg.c \
	tinyjpeg.c \
	../common/va_display.c	\
	../common/va_display_android.cpp

LOCAL_CFLAGS += \
    -DANDROID

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	loadjpeg

LOCAL_SHARED_LIBRARIES := libva libva-android libdl libcutils libutils libgui

include $(BUILD_EXECUTABLE)
