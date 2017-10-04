package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

final public class JNIV8Function extends JNIV8Object {
    abstract public static class Handler {
        abstract Object Callback(Object receiver, Object[] arguments);
    };

    public static JNIV8Function Create(V8Engine engine, JNIV8Function.Handler handler) {
        return Create(engine.getNativePtr(), handler);
    }

    public Object callAsV8Function(Object... arguments) {
        return callAsV8FunctionWithReceiver(null, arguments);
    }
    public native Object callAsV8FunctionWithReceiver(Object receiver, Object... arguments);

    //------------------------------------------------------------------------
    // internal fields & methods
    private static native JNIV8Function Create(long nativePtr, JNIV8Function.Handler handler);

    protected JNIV8Function(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }
}
