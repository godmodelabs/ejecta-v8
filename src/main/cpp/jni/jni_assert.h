//
// Created by Martin Kleinhans on 22.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNI_ASSERT_H
#define ANDROID_TRADINGLIB_SAMPLE_JNI_ASSERT_H

#include <android/log.h>

#ifdef ENABLE_JNI_ASSERT
#define JNI_ASSERT_AT_LINE(a) #a
#define JNI_ASSERT_AT(a,b) a ":" JNI_ASSERT_AT_LINE(b)

#define JNI_ASSERT(test, msg) if(!(test)) __android_log_assert(#test,"JNIWrapper","[Assertion Failure] \n" JNI_ASSERT_AT(__FILE__,__LINE__) "\n" msg);

#define JNI_ASSERTF(test, msg, ...) if(!(test)) __android_log_assert(#test,"JNIWrapper","[Assertion Failure] \n" JNI_ASSERT_AT(__FILE__,__LINE__) "\n" msg, __VA_ARGS__);
#else
#define JNI_ASSERT(test, ...)
#define JNI_ASSERTF(test, ...)
#endif

#endif //ANDROID_TRADINGLIB_SAMPLE_JNI_ASSERT_H
