package ag.boersego.bgjs;
import java.util.Map;

/**
 * Created by martin on 18.08.17.
 */


abstract public class JNIV8Object extends JNIObject {
    public V8Engine getV8Engine() {
        return _engine;
    }

    public native Object callV8Method(String name, Object... arguments);
    /*
    Question: Do we need these? (coercion on JS side or in Java?)
    public native Object callV8MethodAsNumber(String name, Object... arguments);
    public native Object callV8MethodAsString(String name, Object... arguments);
    public native Object callV8MethodAsBoolean(String name, Object... arguments);
    */
    public native Object getV8Field(String name);
    /*
    Question: Do we need these? (coercion on JS side or in Java?)
    public native double getV8FieldAsNumber(String name);
    public native String getV8FieldAsString(String name);
    public native boolean getV8FieldAsBoolean(String name);
    */

    public boolean hasV8Field(String name) {
        return hasV8Field(name, false);
    }
    public boolean hasV8OwnField(String name) {
        return hasV8Field(name, true);
    }

    public String[] getV8Keys() {
        return getV8Keys(false);
    }
    public Map<String,Object> getV8Fields()  {
        return getV8Fields(false);
    }

    public String[] getV8OwnKeys() {
        return getV8Keys(true);
    }
    public Map<String,Object> getV8OwnFields() {
        return getV8Fields(true);
    }

    public native String toV8String();

    public native void setV8Field(String name, Object value);

    protected native void adjustJSExternalMemory(long change);

    //------------------------------------------------------------------------
    // internal fields & methods
    private V8Engine _engine;

    public JNIV8Object(V8Engine engine, long jsObjPtr) {
        _engine = engine;
        initNativeJNIV8Object(engine.getNativePtr(), jsObjPtr);
    }

    public JNIV8Object(V8Engine engine) {
        _engine = engine;
        initNativeJNIV8Object(engine.getNativePtr(), 0);
    }

    private native boolean hasV8Field(String name, boolean ownOnly);
    private native String[] getV8Keys(boolean ownOnly);
    private native Map<String,Object> getV8Fields(boolean ownOnly);
    private native void initNativeJNIV8Object(long enginePtr, long jsObjPtr);
}
