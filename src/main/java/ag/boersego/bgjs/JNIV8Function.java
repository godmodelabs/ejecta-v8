package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

public class JNIV8Function extends JNIV8Object {
    public static JNIV8Function NewInstance(V8Engine engine) {
        return NewInstance(engine.getNativePtr());
    }
    private static native JNIV8Function NewInstance(long nativePtr);

    protected JNIV8Function(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }

    public native Object callAsV8Function(Object... arguments);

    // @TODO: call with this pointer? "callAsV8FunctionFromObject"?
}
