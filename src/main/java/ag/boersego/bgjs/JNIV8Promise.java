package ag.boersego.bgjs;

import ag.boersego.v8annotations.V8Flags;
import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class JNIV8Promise extends JNIV8Object {
    public class Resolver extends JNIV8Object {
        //------------------------------------------------------------------------
        // internal fields & methods
        @Keep
        protected Resolver(V8Engine engine, long jsObjPtr, Object[] arguments) {
            super(engine, jsObjPtr, arguments);
        }

        public native @NonNull JNIV8Promise getPromise();
        public native boolean resolve(@Nullable Object value);
        public native boolean reject(@Nullable Object value);
    }

    public static native Resolver CreateResolver(@NonNull V8Engine engine);

    public @NonNull JNIV8Promise then(@NonNull JNIV8Function resolvedHandler) {
        JNIV8Promise promise = callV8MethodTyped("then", V8Flags.NonNull, JNIV8Promise.class, resolvedHandler);
        if(promise == null) throw new NullPointerException();
        return promise;
    }
    public @NonNull JNIV8Promise then(@NonNull JNIV8Function resolvedHandler, @Nullable JNIV8Function rejectedHandler) {
        JNIV8Promise promise = callV8MethodTyped("then", V8Flags.NonNull, JNIV8Promise.class, resolvedHandler, rejectedHandler);
        if(promise == null) throw new NullPointerException();
        return promise;
    }
    public @NonNull JNIV8Promise onCatch(@NonNull JNIV8Function.Handler handler) {
        JNIV8Promise promise = callV8MethodTyped("catch", V8Flags.NonNull, JNIV8Promise.class, handler);
        if(promise == null) throw new NullPointerException();
        return promise;
    }
    public @NonNull JNIV8Promise onFinally(@NonNull JNIV8Function.Handler handler) {
        JNIV8Promise promise = callV8MethodTyped("finally", V8Flags.NonNull, JNIV8Promise.class, handler);
        if(promise == null) throw new NullPointerException();
        return promise;
    }

    //------------------------------------------------------------------------
    // internal fields & methods
    @Keep
    protected JNIV8Promise(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(engine, jsObjPtr, arguments);
    }
}
