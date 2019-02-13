package ag.boersego.bgjs;

import ag.boersego.bgjs.V8Exception;

/**
 * This Exception represents a JavaScript exception (available via getV8Exception)
 *
 * It is used for two purposes:
 *
 * 1) If an exception is encountered in actual JavaScript code that was called from Java,
 * the JavaScript exception is wrapped by an instance of this class which is then "rethrown".
 * In this case, the exception will contain the JavaScript stacktrace!
 *
 * 2) If Java code that is called by JavaScript wants to raise a specific kind of exception,
 * it can construct an instance of this class and throw it;
 * The contained exception is then unwrapped and "rethrown" in JavaScript
 *
 * Example 1)
 * Some JavaScript code called from Java tries to execute an unknown function "foo"
 * This will throw an instance of V8Exception "An exception was encountered in JavaScript", with an instance of
 * V8JSException referenced as `cause`. This cause in turn will contain the actual JavaScript exception:
 * ReferenceError: foo is not defined
 *
 * Example 2)
 * Some Java code called from JavaScript wants to throw an exception because a parameter is out of bounds.
 * To signal that to JavaScript, an instanceof "RangeError" should be thrown.
 * This is done by: new V8JSException(engine, "RangeError", "The parameter was out of bounds")
 * This will construct a new instance of RangeError, and throw that in JavaScript.
 *
 * Created by martin on 23.10.17.
 */

public class V8JSException extends RuntimeException {
    public V8JSException(V8Engine engine, String type, Object... arguments) {
        super("A javascript call encountered an exception of type " + type);
        this.v8Exception = JNIV8Object.Create(engine, type, arguments);
        this.causedByJS = false;
    }

    /**
     * Retrieves the error thrown in JS
     * most likely an instance of JNIV8GenericObject if it was an `Error`
     * Could also be any other wrapped JS value (primitive, function, Java/V8 tuple) or null
     */
    public Object getV8Exception() {
        return v8Exception;
    }

    /**
     * checks if this exception was actually caused by JS, or by native/Java code
     * (see documentation of class for more details)
     */
    public boolean wasCausedByJS() {
        return causedByJS;
    }

    //------------------------------------------------------------------------
    // internal fields & methods
    private Object v8Exception;
    private boolean causedByJS;
    private V8JSException(String message, Object v8Exception, Throwable cause) {
        super(message, cause);
        this.v8Exception = v8Exception;
        this.causedByJS = true;
    }
}
