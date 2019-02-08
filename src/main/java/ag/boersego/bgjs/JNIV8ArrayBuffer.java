package ag.boersego.bgjs;

import androidx.annotation.Keep;

public class JNIV8ArrayBuffer extends JNIV8Object {
    //------------------------------------------------------------------------
    // internal fields & methods
    @Keep
    protected JNIV8ArrayBuffer(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(engine, jsObjPtr, arguments);
    }
}
