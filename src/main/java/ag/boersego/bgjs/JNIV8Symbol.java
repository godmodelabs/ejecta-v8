package ag.boersego.bgjs;

import androidx.annotation.Keep;

import ag.boersego.v8annotations.V8Symbols;

public class JNIV8Symbol extends JNIV8Object {
    /**
     * searches for existing symbols in a runtime-wide symbol registry
     * if a symbol for the given key already exists it is returned;
     * otherwise a new symbol gets created and stored in the registry
     */
    public static native JNIV8Symbol For(V8Engine engine, String name);
    /**
     * provides access to well-known / predefined JavaScript symbols
     */
    public static JNIV8Symbol For(V8Engine engine, V8Symbols symbol) {
        return ForEnumString(engine, symbol.toString());
    }
    /**
     * creates a new symbol
     * the returned symbol is only accessible via the returned reference
     */
    public static native JNIV8Symbol Create(V8Engine engine, String description);

    //------------------------------------------------------------------------
    // internal fields & methods

    public static native JNIV8Symbol ForEnumString(V8Engine engine, String symbol);
    @Keep
    protected JNIV8Symbol(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(engine, jsObjPtr, arguments);
    }
}
