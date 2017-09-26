package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

public class JNIV8Array extends JNIV8Object {
    public static JNIV8Array NewInstance(V8Engine engine) {
        return NewInstance(engine.getNativePtr());
    }
    private static native JNIV8Array NewInstance(long nativePtr);

    protected JNIV8Array(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }
}
