LOCAL_PATH := $(call my-dir)

#Prebuilt libraries
include $(CLEAR_VARS)
LOCAL_MODULE := aospPdfium

ARCH_PATH = $(TARGET_ARCH_ABI)
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    ARCH_PATH = armeabi
endif

ifeq ($(TARGET_ARCH_ABI), arm64-v8a)
    ARCH_PATH = arm64
endif

LOCAL_SRC_FILES := $(LOCAL_PATH)/lib/$(ARCH_PATH)/libpdfium.so

include $(PREBUILT_SHARED_LIBRARY)


#Main JNI library
include $(CLEAR_VARS)
LOCAL_MODULE := jniPdfium

LOCAL_CFLAGS += -DHAVE_PTHREADS
LOCAL_C_INCLUDES += $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES += aospPdfium
LOCAL_LDLIBS += -llog -landroid

LOCAL_SRC_FILES :=  $(LOCAL_PATH)/src/mainJNILib.cpp

include $(BUILD_SHARED_LIBRARY)
