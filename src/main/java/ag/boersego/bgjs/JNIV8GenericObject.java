package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

public class JNIV8GenericObject extends JNIV8Object {
    public static JNIV8GenericObject NewInstance(V8Engine engine) {
        return NewInstance(engine.getNativePtr());
    }
    private static native JNIV8GenericObject NewInstance(long nativePtr);

    protected JNIV8GenericObject(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }
}
