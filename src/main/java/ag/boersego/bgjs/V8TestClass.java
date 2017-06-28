package ag.boersego.bgjs;

import android.util.Log;

/**
 * Created by martin on 13.04.17.
 */

public class V8TestClass extends JNIObject {
    public V8TestClass(long value) {
        Log.d("V8TestClass", "Constructor:"+Long.toString(value));
    }

    public native void test(long testL, float testF, double testD, String str);
    public native String getName();
    public void test3(long testL, float testF) {
        Log.d("V8TestClass", "Hello:" + Long.toString(testL) + ":" + Float.toString(testF));
    }
    public long testLong;

    public static long staticField;
    public static void test4(V8TestClass cls) {
        Log.d("V8TestClass", "Static:" + cls.getName());
    }
}
