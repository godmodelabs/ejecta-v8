package ag.boersego.bgjs;

/**
 * Created by martin on 18.08.17.
 */


public class JNIV8Object extends JNIObject {

    private native void initNativeJNIV8Object(long enginePtr, long jsObjPtr);

    private static native void initBinding();
    static {
        initBinding();
    }

    public JNIV8Object(V8Engine engine, long jsObjPtr) {
        initNativeJNIV8Object(engine.getNativePtr(), jsObjPtr);
    }

    public JNIV8Object(V8Engine engine) {
        initNativeJNIV8Object(engine.getNativePtr(), 0);
    }

    protected native void adjustJSExternalMemory(long change);
}
