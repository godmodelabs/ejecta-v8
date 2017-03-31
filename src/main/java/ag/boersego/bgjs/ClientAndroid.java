package ag.boersego.bgjs;

import android.content.res.AssetManager;

/**
 * ClientAndroid.java
 * This is the declaration of all native methods.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 **/

public class ClientAndroid {
	// BGJSContext
	public static native void timeoutCB(long ctxPtr, long jsCb, long thisObj, boolean cleanup, boolean runCallback);
	public static native long initialize(AssetManager am, V8Engine engine, String locale, String lang, String timezone, float density);
    public static native void load (long ctxPtr, String filename);
    public static native void run (long ctxPtr);
    public static native void runCBBoolean (long ctxPtr, long cbPtr, long thisPtr, boolean b);
	
    // AjaxModule
	public static native boolean ajaxDone(long ctxPtr, String data, int responseCode, long jsCbPtr, long thisObj,
                                          long errorCb, long v8CtxPtr, boolean success, boolean processData);
	
	// BGJSGLModule
    public static native long createGL(long ctxPtr, V8TextureView gl2jniView, float pixelRatio, boolean noClearOnFlip, int width, int height);
    public static native int init(long ctxPtr, long objPtr, int width, int height, String callbackName);
    public static native boolean step(long ctxPtr, long jsPtr);
    public static native void setTouchPosition(long ctxPtr, long jsPtr, int x, int y);
    public static native void sendTouchEvent(long ctxPtr, long objPtr, String typeStr,
			float[] xArr, float[] yArr, float scale);
    public static native void redraw (long ctxPtr, long jsPtr);
	public static native void close(long ctxPtr, long jsPtr);

    // BGJSModule
    public static native void cleanupNativeFnPtr (long ctxPtr, long nativePtr);
    public static native void cleanupPersistentFunction (long ctxPtr, long nativePtr);
}
