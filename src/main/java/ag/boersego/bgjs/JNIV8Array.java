package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

public class JNIV8Array extends JNIV8Object {
    public static JNIV8Array Create(V8Engine engine) {
        return Create(engine.getNativePtr());
    }

    /*
    @TODO: implement methods

    which one?

    public static JNIV8Array NewInstance(V8Engine engine, Object[] elements) {
        return NewInstance(engine.getNativePtr(), elements);
    }

    public static JNIV8Array NewInstance(V8Engine engine, Object... elements) {
        return NewInstance(engine.getNativePtr(), elements);
    }

    public native int getLength();
    public native Object getV8ElementAtIndex(int index);
    public native void pushV8Element(Object element);

    slice? splice? shift? pop?

     */
    //------------------------------------------------------------------------
    // internal fields & methods
    private static native JNIV8Array Create(long nativePtr);

    protected JNIV8Array(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }
}
