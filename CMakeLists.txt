cmake_minimum_required(VERSION 3.4.1)

set (CMAKE_CXX_STANDARD 11)

#--------------------------------------------------
# Includes
#--------------------------------------------------
include_directories(
            include
            src/main/cpp/ejecta/EJCanvas
            src/main/cpp/utils
            src/main/cpp/lodepng
)

#--------------------------------------------------
# Source
#--------------------------------------------------
add_library( # Sets the name of the library.
             bgjs

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/cpp/jni/JNIClassInfo.cpp
             src/main/cpp/jni/JNIClass.cpp
             src/main/cpp/jni/JNIObject.cpp
             src/main/cpp/jni/JNIBase.cpp
             src/main/cpp/jni/JNIWrapper.cpp
             src/main/cpp/bgjs/BGJSV8Engine.cpp
             src/main/cpp/bgjs/ClientAndroid.cpp
             src/main/cpp/bgjs/BGJSModule.cpp
             src/main/cpp/bgjs/BGJSClass.cpp
             src/main/cpp/utils/mallocdebug.cpp
             src/main/cpp/bgjs/BGJSJavaWrapper.cpp
             src/main/cpp/bgjs/modules/AjaxModule.cpp
             src/main/cpp/bgjs/modules/BGJSGLModule.cpp
             src/main/cpp/bgjs/BGJSCanvasContext.cpp
             src/main/cpp/bgjs/BGJSView.cpp
             src/main/cpp/bgjs/BGJSGLView.cpp
             src/main/cpp/ejecta/EJCanvas/EJCanvasContext.cpp
             src/main/cpp/ejecta/EJConvert.cpp
             src/main/cpp/ejecta/EJConvertColorRGBA.cpp
             src/main/cpp/ejecta/EJCanvas/EJPath.cpp
             src/main/cpp/ejecta/EJCanvas/EJTexture.cpp
             src/main/cpp/ejecta/EJCanvas/NdkMisc.cpp
             src/main/cpp/ejecta/EJCanvas/EJImageData.cpp
             src/main/cpp/ejecta/EJCanvas/EJFont.cpp
             src/main/cpp/ejecta/EJCanvas/CGCompat.cpp
             src/main/cpp/ejecta/EJCanvas/EJCanvasContextScreen.cpp
             src/main/cpp/lodepng/lodepng.cpp
             src/main/cpp/v8/JNIV8Marshalling.cpp
             src/main/cpp/v8/V8ClassInfo.cpp
             src/main/cpp/v8/JNIV8Wrapper.cpp
             src/main/cpp/v8/JNIV8Object.cpp
             src/main/cpp/v8/JNIV8GenericObject.cpp
             src/main/cpp/v8/JNIV8Array.cpp
             src/main/cpp/v8/JNIV8Function.cpp
             )

#--------------------------------------------------
# definitions
#--------------------------------------------------
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
add_definitions(-DENABLE_JNI_ASSERT=1)
endif()

#--------------------------------------------------
# select right v8 library for current abi
#--------------------------------------------------

if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i686")
  set(v8-abi x86)
elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
  set(v8-abi x86_64)
elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7-a")
  set(v8-abi armeabi-v7a)
elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
  set(v8-abi arm64-v8a)
endif()

MESSAGE( STATUS "CMAKE_SYSTEM_PROCESSOR:         " ${CMAKE_SYSTEM_PROCESSOR} )

# Other possible abis that are currently not possible because we don't have the precompiled v8 libraries:
# armv5te - for armeabi ABI
# armv6 - for armeabi-v6 with VFP ABI
# mips - for mips ABI
# mips64 - for mips64 ABI

set(v8-lib ${CMAKE_SOURCE_DIR}/libs/v8/${v8-abi}/libv8.a)

MESSAGE( STATUS "v8-lib:         " ${v8-lib} )

#--------------------------------------------------
# other dependencies
#--------------------------------------------------
find_library( log-lib log )

#--------------------------------------------------
# link
#--------------------------------------------------
target_link_libraries( # Specifies the target library.
                       bgjs

                       # Links the target library to the log library
                       # included in the NDK.
                       ${v8-lib}
                       GLESv1_CM
                       EGL
                       android
                       ${log-lib} )