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
	// BGJSV8Engine
	JNIEXPORT jlong JNICALL Java_ag_boersego_bgjs_V8Engine_createNative(JNIEnv * env, jobject obj);

	// ClientAndroid
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_initialize(
			JNIEnv * env, jobject obj, jobject assetManager, jlong v8Engine, jstring locale, jstring lang, jstring timezone, float density, jstring deviceClass);
	JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_ajaxSuccess(
			JNIEnv * env, jobject obj, jlong ctxPtr, jstring data,
			jint responseCode, jlong cbPtr, jlong thisPtr);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_timeoutCB(
			JNIEnv * env, jobject obj, jlong ctxPtr, jlong jsCbPtr, jlong thisPtr, jboolean cleanup, jboolean runCb);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_runCBBoolean (JNIEnv * env, jobject obj, jlong ctxPtr, jlong cbPtr, jlong thisPtr, jboolean b);

	// BGJSGLModule
    JNIEXPORT jint JNICALL Java_ag_boersego_bgjs_ClientAndroid_cssColorToInt(JNIEnv * env, jobject obj, jstring color);
	JNIEXPORT jlong JNICALL Java_ag_boersego_bgjs_ClientAndroid_createGL(JNIEnv * env,
			jobject obj, jlong ctxPtr, jobject javaGlView, jfloat pixelRatio, jboolean noClearOnFlip, jint width, jint height);
	JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_step(JNIEnv * env,
			jobject obj, jlong ctxPtr, jlong jsPtr);
	JNIEXPORT int JNICALL Java_ag_boersego_bgjs_ClientAndroid_init(JNIEnv * env,
            jobject obj, jlong ctxPtr, jlong objPtr, jint width, jint height, jstring callbackName);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_setTouchPosition(JNIEnv * env,
			jobject obj, jlong ctxPtr, jlong jsPtr, jint x, jint y);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_sendTouchEvent(
			JNIEnv * env, jobject obj, jlong ctxPtr, jlong objPtr, jstring typeStr,
			jfloatArray xArr, jfloatArray yArr, jfloat scale);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_close(JNIEnv * env,
			jobject obj, jlong ctxPtr, jlong jsPtr);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_redraw(JNIEnv * env,
			jobject obj, jlong ctxPtr, jlong jsPtr);
};

#endif