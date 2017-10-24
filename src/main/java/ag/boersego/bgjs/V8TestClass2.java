package ag.boersego.bgjs;

import android.util.Log;

import java.util.Map;

/**
 * Created by martin on 07.09.17.
 */

public class V8TestClass2 extends V8TestClass {
    public V8TestClass2(V8Engine engine) {
        super(engine);
        shadowField = 456;
    }

    public V8TestClass2(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
        shadowField = 456;
    }

    public void test3(long testL, float testF) {
//        throw new Exception("boing");

//        getV8Engine().parseJSON("{\"foo\":5xzcx}");


        Log.d("V8TestClass2", "Overwritten Hello:" + Long.toString(testL) + ":" + Float.toString(testF));

            JNIV8Function func = JNIV8Function.Create(getV8Engine(), (receiver, arguments) -> "Hello from Java");

        JNIV8Function func2 = JNIV8Function.Create(getV8Engine(), (receiver, arguments) -> "Hello from Java");

        Object objT = getV8Engine().parseJSON("{\"foo\":5}");

        //String test = objT.toJSON();
        //String test2 = objT.toString();

        Object objT2 = getV8Engine().runScript("(function foo() { return 7; })", "test");

        JNIV8GenericObject obj = JNIV8GenericObject.Create(getV8Engine());
        obj.getV8Field("test");
        obj.setV8Field("test", 13);
        obj.setV8Field(null, 19);

        Object test = obj.getV8Field(null);

        JNIV8Array array = JNIV8Array.CreateWithElements(getV8Engine(), 1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
        ((JNIV8Function)getV8Field("v8Func")).callAsV8Function(func, func2, 123, "abc", obj, array.callV8Method("slice", 0, 2));

        Object[] values = array.getV8Elements();
        Object value = array.getV8Element(0);
        Object[] values2 = array.getV8Elements(2, 4);

        int i = 1;
/*


        Object v3 = callV8Method("v8Func", 123, "abc", obj, this);

        JNIV8Array arr = JNIV8Array.NewInstance(getV8Engine());

        JNIV8Function f = JNIV8Function.NewInstance(getV8Engine());

        Object v = getV8Field("v8Prop");
        Object v2 = getV8Field("v8Prop2");

        setV8Field("v8Prop", 123);
        v = getV8Field("v8Prop");
        */
    }

    public long shadowField;
    public static long staticShadowField = 456;

    public int testLong;
    public static long staticField;

    public native void overwriteMe();
}
