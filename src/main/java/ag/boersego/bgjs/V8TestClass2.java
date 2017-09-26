package ag.boersego.bgjs;

import android.util.Log;

/**
 * Created by martin on 07.09.17.
 */

public class V8TestClass2 extends V8TestClass {
    public V8TestClass2(V8Engine engine) {
        super(engine);
        Log.d("V8TestClass2", "Constructor");
        shadowField = 456;
    }

    public V8TestClass2(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
        Log.d("V8TestClass2", "Constructor:");
        shadowField = 456;
    }

    public void test3(long testL, float testF) {
        Log.d("V8TestClass2", "Overwritten Hello:" + Long.toString(testL) + ":" + Float.toString(testF));

        JNIV8GenericObject obj = JNIV8GenericObject.NewInstance(getV8Engine());
        obj.getV8Field("test");

        JNIV8Array arr = JNIV8Array.NewInstance(getV8Engine());

        JNIV8Function f = JNIV8Function.NewInstance(getV8Engine());

        JNIV8Value v = getV8Field("v8Prop");
        JNIV8Value v2 = getV8Field("v8Prop2");
        JNIV8Value v3 = callV8Method("v8Func");
    }
    public long shadowField;
    public static long staticShadowField = 456;

    public int testLong;
    public static long staticField;

    public native void overwriteMe();
}
