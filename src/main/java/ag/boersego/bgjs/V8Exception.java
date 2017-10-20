package ag.boersego.bgjs;

/**
 * Created by martin on 20.10.17.
 */

public class V8Exception extends Exception {
    private Object v8Exception;

    public V8Exception(String message, Object v8Exception, Throwable cause) {
        super(message, cause);
        this.v8Exception = v8Exception;
    }

    Object getV8Exception() {
        return v8Exception;
    }
    // @TODO: release native handle
}
