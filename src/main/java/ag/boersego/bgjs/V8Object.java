package ag.boersego.bgjs;

/**
 * Created by martin on 18.08.17.
 */


public class V8Object extends JNIObject {
    public static class Arguments {

    };
    /*
    public static class Context {
        private V8Engine engine;
        private long jsObjPtr;
        public Context(V8Engine engine) {
            this.engine = engine;
            this.jsObjPtr = 0;
        }
        private Context(V8Engine engine, long jsObjPtr) {
            this.engine = engine;
            this.jsObjPtr = jsObjPtr;
        }
    }
    */

    private native void initNativeV8Object(long enginePtr, long jsObjPtr);

    private static native void initBinding();
    static {
        initBinding();
    }

    public V8Object(V8Engine engine, long jsObjPtr) {
        initNativeV8Object(engine.getNativePtr(), jsObjPtr);
    }

    public V8Object(V8Engine engine) {
        initNativeV8Object(engine.getNativePtr(), 0);
    }

    public long getJSObjectPtr() {
        return 0;
    }
}
