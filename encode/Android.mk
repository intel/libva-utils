# h264encode
#
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
  ../common/va_display.c \
  ../common/va_display_android.cpp \
  h264encode.c

LOCAL_CFLAGS += \
    -DANDROID

LOCAL_C_INCLUDES += \
  $(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	h264encode

LOCAL_SHARED_LIBRARIES := libva-android libva libdl  libcutils libutils libgui libm

include $(BUILD_EXECUTABLE)

# avcenc
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	../common/va_display.c			\
	../common/va_display_android.cpp	\
	avcenc.c

LOCAL_CFLAGS += \
	-DANDROID

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	avcenc

LOCAL_SHARED_LIBRARIES := libva-android libva libdl libcutils libutils libgui

include $(BUILD_EXECUTABLE)

# vp9enc
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	../common/va_display.c			\
	../common/va_display_android.cpp	\
	vp9enc.c

LOCAL_CFLAGS += \
	-DANDROID

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	vp9enc

LOCAL_SHARED_LIBRARIES := libva-android libva libdl libcutils libutils libgui

include $(BUILD_EXECUTABLE)

# jpegenc
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	../common/va_display.c			\
	../common/va_display_android.cpp	\
	jpegenc.c

LOCAL_CFLAGS += \
	-DANDROID

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	jpegenc

LOCAL_SHARED_LIBRARIES := libva-android libva libdl libcutils libutils libgui

include $(BUILD_EXECUTABLE)

# mpeg2vaenc
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	../common/va_display.c			\
	../common/va_display_android.cpp	\
	mpeg2vaenc.c

LOCAL_CFLAGS += \
	-DANDROID

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	mpeg2vaenc

LOCAL_SHARED_LIBRARIES := libva-android libva libdl libcutils libutils libgui

include $(BUILD_EXECUTABLE)

# svctenc
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	../common/va_display.c			\
	../common/va_display_android.cpp	\
	svctenc.c

LOCAL_CFLAGS += \
	-DANDROID

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../common

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE :=	svctenc

LOCAL_SHARED_LIBRARIES := libva-android libva libdl libcutils libutils libgui

include $(BUILD_EXECUTABLE)
