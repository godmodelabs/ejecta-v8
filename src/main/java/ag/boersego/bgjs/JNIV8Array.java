package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

final public class JNIV8Array extends JNIV8Object {
    public static JNIV8Array Create(V8Engine engine) {
        return Create(engine.getNativePtr());
    }
    public static JNIV8Array CreateWithLength(V8Engine engine, int length) {
        return Create(engine.getNativePtr(), length);
    }
    public static JNIV8Array CreateWithArray(V8Engine engine, Object[] elements) {
        return Create(engine.getNativePtr(), elements);
    }
    public static JNIV8Array CreateWithElements(V8Engine engine, Object... elements) {
        return Create(engine.getNativePtr(), elements);
    }

    /**
     * returns the length of the array
     */
    public native int getV8Length();

    /**
     * Returns all objects inside of the array
     */
    public native Object[] getV8Elements();

    /**
     * Returns all objects from a specified range inside of the array
     */
    public native Object[] getV8Elements(int from, int to);

    /**
     * Returns the object at the specified index
     * if index is out of bounds, returns JNIV8Undefined
     */
    public native Object getV8Element(int index);

    /**
     * releases the JS array
     * when working with a lot of objects, calling this manually might improve memory usage
     * using this method is completely optional
     *
     * NOTE: object must not be used anymore after calling this method
     */
    public void dispose() throws RuntimeException {
        super.dispose();
    }

    //------------------------------------------------------------------------
    // internal fields & methods
    private static native JNIV8Array Create(long nativePtr);
    private static native JNIV8Array Create(long nativePtr, int length);
    private static native JNIV8Array Create(long nativePtr, Object[] elements);

    protected JNIV8Array(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }
}
