package ag.boersego.bgjs;

/**
 * Created by martin on 26.09.17.
 */

final public class JNIV8GenericObject extends JNIV8Object {
    public static native JNIV8GenericObject Create(V8Engine engine);

    public void dispose() throws RuntimeException {
        super.dispose();
    }

    //------------------------------------------------------------------------
    // internal fields & methods

    protected JNIV8GenericObject(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(engine, jsObjPtr, arguments);
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof JNIV8GenericObject)) {
            return super.equals(obj);
        }

        final JNIV8GenericObject other = (JNIV8GenericObject)obj;
        return other.getV8Fields().equals(other.getV8Fields());
    }
}
