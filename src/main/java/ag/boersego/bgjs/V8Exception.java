package ag.boersego.bgjs;

/**
 * This exception can be thrown by all methods that execute JavaScript Code via a V8Engine
 * It occurs when the executed JavaScript Code encounters an exception.
 *
 * It will always provide an instance of V8JSException as a cause,
 * that contains information about the actual JavaScript exception
 *
 * Example:
 * Some JavaScript code called from Java tries to execute an unknown function "foo"
 * This will throw an instance of V8Exception "An exception was encountered in JavaScript", with an instance of
 * V8JSException referenced as `cause`. This cause in turn will contain the actual JavaScript exception:
 * ReferenceError: foo is not defined
 *
 * Created by martin on 20.10.17.
 */

public class V8Exception extends RuntimeException {
    /**
     * Retrieves the error thrown in JS
     * most likely an instance of JNIV8GenericObject if it was an `Error`
     * Could also be any other wrapped JS value (primitive, function, Java/V8 tuple) or null
     */
    public Object getV8Exception() {
        Throwable cause = this.getCause();
        if(cause instanceof V8JSException) {
            return ((V8Exception)cause).getV8Exception();
        }
        return null;
    }

    //------------------------------------------------------------------------
    // internal fields & methods
    private V8Exception(String message, Throwable cause) {
        super(message, cause);
    }
}
