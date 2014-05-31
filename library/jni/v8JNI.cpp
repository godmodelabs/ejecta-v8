/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>


#include <stdio.h>
#include <stdlib.h>

#include "v8Engine.h"
#include "node_script.h"

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

extern "C" {
    JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_initialize(JNIEnv * env, jobject obj, jobject assetManager);
    JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_load(JNIEnv * env, jobject obj, jstring path);
    JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_tick(JNIEnv * env, jobject obj, jint ms);
    JNIEXPORT bool JNICALL Java_de_boersego_charting_V8JNI_ajaxSuccess(JNIEnv * env, jobject obj, jstring data, jint responseCode, jint cbPtr, jint thisPtr);
    JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_run(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_initialize(JNIEnv * env, jobject obj, jobject assetManager)
{
	v8Engine& ct= v8Engine::getInstance();
	ct.registerAssetManager(env, obj, assetManager);
}

JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_load(JNIEnv * env, jobject obj, jstring path)
{
	v8Engine& ct= v8Engine::getInstance();
	ct.load(env, obj, path);
}

JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_run(JNIEnv * env, jobject obj) {
	v8Engine& ct= v8Engine::getInstance();
	ct.run(env, obj);
}

JNIEXPORT bool JNICALL Java_de_boersego_charting_V8JNI_ajaxSuccess(JNIEnv * env, jobject obj, jstring data, jint responseCode, jint cbPtr, jint thisPtr)
{
	v8Engine& ct= v8Engine::getInstance();
	return ct.ajaxSuccess(env, obj, data, responseCode, cbPtr, thisPtr);
}


JNIEXPORT void JNICALL Java_de_boersego_charting_V8JNI_tick(JNIEnv * env, jobject obj, jint ms)
{
	v8Engine& ct= v8Engine::getInstance();
	ct.tick(ms);
}
