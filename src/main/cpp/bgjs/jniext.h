#ifndef __BGJS_JNIEXT_H
#define __BGJS_JNIEXT_H  1

/**
 * jniext
 * Header declaring all JNI exported methods
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 */

extern "C" {
	// ClientAndroid
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_initialize(
			JNIEnv * env, jobject obj, jobject assetManager, jobject v8Engine, jstring locale, jstring lang,
            jstring timezone, float density, jstring deviceClass, jboolean debug, jboolean isStoreBuild, jint maxHeapSizeInMb);
	JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_ajaxDone(
		JNIEnv * env, jobject obj, jobject engine, jstring dataStr, jint responseCode,
		jlong jsCbPtr, jlong thisPtr, jlong errorCb, jboolean success, jboolean processData);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_timeoutCB(
			JNIEnv * env, jobject obj, jobject engine, jlong jsCbPtr, jlong thisPtr, jboolean cleanup, jboolean runCb);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_runCBBoolean (JNIEnv * env, jobject obj, jobject engine, jlong cbPtr, jlong thisPtr, jboolean b);

	// BGJSGLModule
    JNIEXPORT jint JNICALL Java_ag_boersego_bgjs_ClientAndroid_cssColorToInt(JNIEnv * env, jobject obj, jstring color);
	JNIEXPORT jlong JNICALL Java_ag_boersego_bgjs_ClientAndroid_createGL(JNIEnv * env,
			jobject obj, jobject engine, jobject javaGlView, jfloat pixelRatio, jboolean noClearOnFlip, jint width, jint height);
	JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_step(JNIEnv * env,
			jobject obj, jobject engine, jlong jsPtr);
	JNIEXPORT int JNICALL Java_ag_boersego_bgjs_ClientAndroid_init(JNIEnv * env,
            jobject obj, jobject engine, jlong objPtr, jint width, jint height, jstring callbackName);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_setTouchPosition(JNIEnv * env,
			jobject obj, jobject engine, jlong jsPtr, jint x, jint y);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_sendTouchEvent(
			JNIEnv * env, jobject obj, jobject engine, jlong objPtr, jstring typeStr,
			jfloatArray xArr, jfloatArray yArr, jfloat scale);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_close(JNIEnv * env,
			jobject obj, jobject engine, jlong jsPtr);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_redraw(JNIEnv * env,
			jobject obj, jobject engine, jlong jsPtr);
};

#endif