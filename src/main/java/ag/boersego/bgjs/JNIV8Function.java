package ag.boersego.bgjs;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import ag.boersego.v8annotations.V8Flags;

/**
 * Created by martin on 26.09.17.
 */

@SuppressWarnings("unused")
public final class JNIV8Function extends JNIV8Object {
    public interface Handler {
        Object Callback(@NonNull Object receiver, @NonNull Object[] arguments);
    }

    public static native JNIV8Function Create(V8Engine engine, JNIV8Function.Handler handler);

    public @Nullable
    Object callAsV8Function(@Nullable Object... arguments) {
        String callContext = "callAsV8Function";

        /*
        // enable for debugging locks (disabled by default for performance reasons):
        StackTraceElement element = Thread.currentThread().getStackTrace()[3];
        callContext += "/" + element.getClassName() + "::" + element.getMethodName();
        */

        return _callAsV8Function(false, 0, 0, Object.class, null, callContext, arguments);
    }

    public @Nullable
    Object applyAsV8Function(@NonNull Object[] arguments) {
        return _callAsV8Function(false, 0, 0, Object.class, null, "applyAsV8Function", arguments);
    }

    public @Nullable
    Object callAsV8FunctionWithReceiver(@NonNull Object receiver, @Nullable Object... arguments) {
        return _callAsV8Function(false, 0, 0, Object.class, receiver, "callAsV8FunctionWithReceiver", arguments);
    }

    public @Nullable
    Object applyAsV8FunctionWithReceiver(@NonNull Object receiver, @NonNull Object[] arguments) {
        return _callAsV8Function(false, 0, 0, Object.class, receiver, "applyAsV8FunctionWithReceiver", arguments);
    }

    public @NonNull
    Object callAsV8Constructor(@Nullable Object... arguments) {
        return _callAsV8Function(true, V8Flags.NonNull, 0, Object.class, null, "callAsV8Constructor", arguments);
    }

    public @NonNull
    Object applyAsV8Constructor(@Nullable Object[] arguments) {
        return _callAsV8Function(true, V8Flags.NonNull, 0, Object.class, null, "applyAsV8Constructor", arguments);
    }


    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T callAsV8FunctionTyped(int flags, @NonNull Class<T> returnType, @Nullable Object... arguments) {
        return (T) _callAsV8Function(false, flags, returnType.hashCode(), returnType, null, "callAsV8FunctionTyped", arguments);
    }

    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T callAsV8FunctionTyped(@NonNull Class<T> returnType, @Nullable Object... arguments) {
        return (T) _callAsV8Function(false, V8Flags.Default, returnType.hashCode(), returnType, null, "callAsV8FunctionTyped", arguments);
    }

    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T callAsV8FunctionWithReceiverTyped(int flags, @NonNull Class<T> returnType, @NonNull Object receiver, @Nullable Object... arguments) {
        return (T) _callAsV8Function(false, flags, returnType.hashCode(), returnType, receiver, "callAsV8FunctionWithReceiverTyped", arguments);
    }

    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T callAsV8FunctionWithReceiverTyped(@NonNull Class<T> returnType, @NonNull Object receiver, @Nullable Object... arguments) {
        return (T) _callAsV8Function(false, V8Flags.Default, returnType.hashCode(), returnType, receiver, "callAsV8FunctionWithReceiverTyped", arguments);
    }

    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T applyAsV8FunctionTyped(int flags, @NonNull Class<T> returnType, @NonNull Object[] arguments) {
        return (T) _callAsV8Function(false, flags, returnType.hashCode(), returnType, null, "applyAsV8FunctionTyped", arguments);
    }

    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T applyAsV8FunctionTyped(@NonNull Class<T> returnType, @NonNull Object[] arguments) {
        return (T) _callAsV8Function(false, V8Flags.Default, returnType.hashCode(), returnType, null, "applyAsV8FunctionTyped", arguments);
    }

    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T applyAsV8FunctionWithReceiverTyped(int flags, @NonNull Class<T> returnType, @NonNull Object receiver, @NonNull Object[] arguments) {
        return (T) _callAsV8Function(false, flags, returnType.hashCode(), returnType, receiver, "applyAsV8FunctionWithReceiverTyped", arguments);
    }

    @SuppressWarnings("unchecked")
    public @Nullable
    <T> T applyAsV8FunctionWithReceiverTyped(@NonNull Class<T> returnType, @NonNull Object receiver, @NonNull Object[] arguments) {
        return (T) _callAsV8Function(false, V8Flags.Default, returnType.hashCode(), returnType, receiver, "applyAsV8FunctionWithReceiverTyped", arguments);
    }

    @Override
    public void dispose() throws RuntimeException {
        super.dispose();
    }

    //------------------------------------------------------------------------
    // internal fields & methods
    private native Object _callAsV8Function(boolean asConstructor, int flags, int type, Class returnType, Object receiver, String callContext, Object... arguments);

    @Keep
    protected JNIV8Function(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(engine, jsObjPtr, arguments);
    }
}
