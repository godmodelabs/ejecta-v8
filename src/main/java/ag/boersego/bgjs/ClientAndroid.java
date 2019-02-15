package ag.boersego.bgjs;

import android.content.res.AssetManager;

/**
 * ClientAndroid.java
 * This is the declaration of all native methods.
 * <p>
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 **/

public class ClientAndroid {
    // BGJSV8Engine
    public static native void initialize(AssetManager am, V8Engine engine, String locale, String lang, String timezone,
                                         float density, final String deviceClass, final boolean debug, final boolean isStoreBuild, final int maxHeapSizeInMb);

    // BGJSGLModule
    public static native int cssColorToInt(String color);
}
