package ag.boersego.bgjs;

/**
 * Created by martin on 18.08.17.
 */


public class JNIV8Object extends JNIObject {

    private V8Engine _engine;

    public JNIV8Object(V8Engine engine, long jsObjPtr) {
        _engine = engine;
        initNativeJNIV8Object(engine.getNativePtr(), jsObjPtr);
    }

    public JNIV8Object(V8Engine engine) {
        _engine = engine;
        initNativeJNIV8Object(engine.getNativePtr(), 0);
    }

    public V8Engine getV8Engine() {
        return _engine;
    }

    public native JNIV8Value callV8Method(String name, Object... arguments);
    public native JNIV8Value getV8Field(String name);
    public native void setV8Field(String name, Object value);

    protected native void adjustJSExternalMemory(long change);

    private native void initNativeJNIV8Object(long enginePtr, long jsObjPtr);
}
