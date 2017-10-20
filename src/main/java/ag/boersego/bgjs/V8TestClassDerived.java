package ag.boersego.bgjs;

import android.util.Log;

/**
 * Created by martin on 23.08.17.
 */

public class V8TestClassDerived extends V8TestClass2 {
    public V8TestClassDerived(V8Engine engine) {
        super(engine);
    }

    public V8TestClassDerived(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }

    public void test3(long testL, float testF) throws Exception {
        Log.d("V8TestClassDerived", "Overwritte Hello:" + Long.toString(testL) + ":" + Float.toString(testF));
        super.test3(testL, testF);
    }
}
