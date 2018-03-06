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
	// BGJSV8Engine
	public static native void timeoutCB(V8Engine engine, long jsCb, long thisObj, boolean cleanup, boolean runCallback);
	public static native void initialize(AssetManager am, V8Engine engine, String locale, String lang, String timezone, float density, final String deviceClass, final boolean debug);
    public static native void runCBBoolean (V8Engine engine, long cbPtr, long thisPtr, boolean b);

	// BGJSGLModule
    public static native int cssColorToInt(String color);
}
