# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

NDK_TOOLCHAIN_VERSION=clang

LOCAL_PATH:= $(call my-dir)

# v8_base
include $(CLEAR_VARS)
LOCAL_MODULE := v8-base-prebuilt
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libv8_base.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_STATIC_LIBRARY)

# v8_snapshot
include $(CLEAR_VARS)
LOCAL_MODULE := v8-snapshot-prebuilt
LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libv8_snapshot.a
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include
include $(PREBUILT_STATIC_LIBRARY)


# freetype
#include $(CLEAR_VARS)
#LOCAL_MODULE := freetype
#LOCAL_SRC_FILES := libs/$(TARGET_ARCH_ABI)/libfreetype.a
#LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)/include/ft2
#include $(PREBUILT_STATIC_LIBRARY)

# Our JNI stuff
include $(CLEAR_VARS)

LOCAL_MODULE	:= libbgjs
#LOCAL_CFLAGS    :=  -g -fno-omit-frame-pointer -fno-strict-aliasing -I ~/bin/android-ndk-latest/sources/cxx-stl/stlport/stlport/ # -Werror #-Ijni/pixman/pixman -Ijni/cairo/src -Ijni/cairo-extra -Ijni/pixman-extra -Wno-missing-field-initializers -Wno-attributes
#LOCAL_CFLAGS	:= -fcxx-exceptions 
LOCAL_C_INCLUDES	:= $(LOCAL_PATH)/ejecta/EJCanvas $(LOCAL_PATH)/utils

LOCAL_SRC_FILES		:= bgjs/BGJSContext.cpp bgjs/ClientAndroid.cpp bgjs/BGJSModule.cpp bgjs/BGJSClass.cpp \
	utils/mallocdebug.cpp \
	bgjs/BGJSJavaWrapper.cpp \
	bgjs/modules/AjaxModule.cpp bgjs/modules/BGJSGLModule.cpp \
	bgjs/BGJSCanvasContext.cpp bgjs/BGJSView.cpp bgjs/BGJSGLView.cpp \
	ejecta/EJCanvas/EJCanvasContext.cpp ejecta/EJConvert.cpp \
	ejecta/EJCanvas/EJPath.cpp ejecta/EJCanvas/EJTexture.cpp ejecta/EJCanvas/NdkMisc.cpp ejecta/EJCanvas/EJImageData.cpp \
	ejecta/EJCanvas/EJFont.cpp ejecta/EJCanvas/CGCompat.cpp ejecta/EJCanvas/EJCanvasContextScreen.cpp \
	lodepng/lodepng.cpp $(subst jni, ., $(wildcard $(LOCAL_PATH)/submodules/*.cpp))

# $(warning $(subst jni, ., $(wildcard $(LOCAL_PATH)/submodules/*.cpp)))

LOCAL_LDLIBS    := -llog -lGLESv1_CM  -lm -landroid -ljnigraphics -lEGL #-L../libs/armeabi -lv8_base -lv8_snapshot -ljnigraphics
LOCAL_STATIC_LIBRARIES	:= v8-base-prebuilt v8-snapshot-prebuilt freetype  # libcairo libpixman cpufeatures 


#TARGET_thumb_release_CFLAGS := $(filter-out -ffunction-sections,$(TARGET_thumb_release_CFLAGS))
#TARGET_thumb_release_CFLAGS := $(filter-out -fomit-frame-pointer,$(TARGET_thumb_release_CFLAGS))
#TARGET_thumb_release_CFLAGS := $(filter-out -Os,$(TARGET_thumb_release_CFLAGS))
#TARGET_thumb_release_CFLAGS := $(filter-out -mthumb,$(TARGET_thumb_release_CFLAGS))
#TARGET_CFLAGS := $(filter-out -ffunction-sections,$(TARGET_CFLAGS))

include $(BUILD_SHARED_LIBRARY)

#$(call import-module,android/cpufeatures)

