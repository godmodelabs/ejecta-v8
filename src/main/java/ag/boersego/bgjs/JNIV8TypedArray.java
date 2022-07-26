package ag.boersego.bgjs;

import androidx.annotation.Keep;

final public class JNIV8TypedArray extends JNIV8Object {
    //------------------------------------------------------------------------
    // internal fields & methods
    @Keep
    JNIV8TypedArray(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(engine, jsObjPtr, arguments);
    }

    /**
     * returns the length of the array
     */
    public native int getV8Length();
}
