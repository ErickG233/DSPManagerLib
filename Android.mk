LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

ifneq ($(TARGET_SYSTEM_AUDIO_EFFECTS),true)
LOCAL_VENDOR_MODULE := true
endif

LOCAL_MODULE := libcyanogen-dsp

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE_RELATIVE_PATH := soundfx

LOCAL_PRELINK_MODULE := false

# LOCAL_ARM_MODE := arm

LOCAL_SRC_FILES := \
	cyanogen-dsp.cpp \
	Biquad.cpp \
	Delay.cpp \
	Effect.cpp \
	EffectBassBoost.cpp \
	EffectCompression.cpp \
	EffectEqualizer.cpp \
	EffectVirtualizer.cpp \
	FIR16.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog

include $(BUILD_SHARED_LIBRARY)

ifneq ($(TARGET_USE_DEVICE_AUDIO_EFFECTS_CONF),true)
include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
LOCAL_CPPFLAGS := -D__cpusplus -g -mfloat-abi=softfp -ffunction-sections -fdata-sections -Ofast -ftree-vectorize -mfpu=neon -march=armv7-a -DHAVE_NEON=1 -DNDEBUG
endif

# LOCAL_CPPFLAGS += -DNDEBUG

# -ffunction-sections -fdata-sections -Ofast -ftree-vectorize

LOCAL_MODULE := audio_effects.conf

LOCAL_SRC_FILES := $(LOCAL_MODULE)

LOCAL_MODULE_CLASS := ETC

LOCAL_VENDOR_MODULE := true

include $(BUILD_PREBUILT)
endif
