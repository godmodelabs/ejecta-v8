package ag.boersego.bgjs;

/**
 * Created by martin on 29.09.17.
 */

public class V8PureJavaClass extends JNIV8Object {
    public V8PureJavaClass(V8Engine engine) {
        super(engine);
    }

    public V8PureJavaClass(V8Engine engine, long jsObjPtr) {
        super(engine, jsObjPtr);
    }
}
