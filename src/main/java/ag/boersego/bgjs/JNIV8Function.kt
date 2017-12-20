package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Flags

/**
 * Created by martin on 20.12.17.
 */

inline fun <reified T> JNIV8Function.callAsV8Function(flags: Int, vararg arguments: Any?): T? {
    return applyAsV8Function(flags, T::class.java, arguments)
}

inline fun <reified T> JNIV8Function.callAsV8FunctionWithReceiver(flags: Int, receiver: Any, vararg arguments: Any?): T? {
    return applyAsV8FunctionWithReceiver(flags, T::class.java, receiver, arguments)
}

inline fun <reified T> JNIV8Function.applyAsV8Function(flags: Int, arguments: Array<Any?>): T? {
    return applyAsV8Function(flags, T::class.java, arguments)
}

inline fun <reified T> JNIV8Function.applyAsV8FunctionWithReceiver(flags: Int, receiver: Any, arguments: Array<Any?>): T? {
    return applyAsV8FunctionWithReceiver(flags, T::class.java, receiver, arguments)
}

/* @TODO: are these overloads really a good idea?
inline fun <reified T> JNIV8Function.callAsV8FunctionNonNull(flags: Int, vararg arguments: Any?): T {
    return applyAsV8Function(V8Flags.NonNull or flags, T::class.java, arguments)!!
}

inline fun <reified T> JNIV8Function.callAsV8FunctionWithReceiverNonNull(flags: Int, receiver: Any, vararg arguments: Any?): T {
    return applyAsV8FunctionWithReceiver(V8Flags.NonNull or flags,  T::class.java, receiver, arguments)!!
}

inline fun <reified T> JNIV8Function.applyAsV8FunctionNonNull(flags: Int, arguments: Array<Any?>): T {
    return applyAsV8Function(V8Flags.NonNull or flags, T::class.java, arguments)!!
}

inline fun <reified T> JNIV8Function.applyAsV8FunctionWithReceiverNonNull(flags: Int, receiver: Any, arguments: Array<Any?>): T? {
    return applyAsV8FunctionWithReceiver(V8Flags.NonNull or flags,  T::class.java, receiver, arguments)!!
}
*/