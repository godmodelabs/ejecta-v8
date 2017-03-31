#ifndef __OS_ANDROID_H

#define __OS_ANDROID_H	1

#include <android/log.h>
#include <jni.h>

/**
 * os-android.h
 * OS independance wrapper for Android.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 */

#define LOG_DEBUG	ANDROID_LOG_DEBUG
#define LOG_INFO	ANDROID_LOG_INFO
#define LOG_ERROR	ANDROID_LOG_ERROR

#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define LOG(LOG_LEVEL, ...)  __android_log_print(LOG_LEVEL, LOG_TAG, __VA_ARGS__)

#endif
