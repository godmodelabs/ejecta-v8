package ag.boersego.bgjs;

import ag.boersego.bgjs.V8Exception;

/**
 * Created by martin on 23.10.17.
 */

public class V8JSException extends RuntimeException {
    private Object v8Exception;

    public V8JSException(String message, Object v8Exception, Throwable cause) {
        super(message, cause);
        this.v8Exception = v8Exception;
    }

    /**
     * Retrieves the error thrown in JS
     * most likely an instance of JNIV8GenericObject if it was an `Error`
     * Could also be any other wrapped JS value (primitive, function, Java/V8 tuple) or null
     */
    Object getV8Exception() {
        return v8Exception;
    }

    /**
     * checks if this exception was actually caused by JS, or by native/Java code
     */
    boolean wasCausedByJS() {
        return getCause() == null;
    }
}
