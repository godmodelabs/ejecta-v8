package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

final public class JNIV8Function extends JNIV8Object {
    public interface Handler {
        Object Callback(Object receiver, Object[] arguments);
    }

    public static JNIV8Function Create(V8Engine engine, JNIV8Function.Handler handler) {
        return Create(engine.getNativePtr(), handler);
    }

    public Object callAsV8Function(Object... arguments) {
        return _callAsV8Function(false, null, arguments);
    }
    public Object callAsV8FunctionWithReceiver(Object receiver, Object... arguments) {
        return _callAsV8Function(false, receiver, arguments);
    }
    public Object callAsV8Constructor(Object... arguments) {
        return _callAsV8Function(true, null, arguments);
    }

    private native Object _callAsV8Function(boolean asConstructor, Object receiver, Object... arguments);

    public void dispose() throws RuntimeException {
        super.dispose();
    }

    //------------------------------------------------------------------------
    // internal fields & methods
    private static native JNIV8Function Create(long nativePtr, JNIV8Function.Handler handler);

    protected JNIV8Function(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }
}
