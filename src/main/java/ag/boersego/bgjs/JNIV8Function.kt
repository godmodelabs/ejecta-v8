@file:Suppress("unused")

package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Flags

/**
 * Created by martin on 20.12.17.
 */

inline fun <reified T> JNIV8Function.callAsV8Function(vararg arguments: Any?, flags: Int = V8Flags.Default): T {
    return applyAsV8FunctionTyped(flags, T::class.java, arguments) as T
}

inline fun <reified T> JNIV8Function.callAsV8FunctionWithReceiver(receiver: Any, vararg arguments: Any?, flags: Int = V8Flags.Default): T {
    return applyAsV8FunctionWithReceiverTyped(flags, T::class.java, receiver, arguments) as T
}

inline fun <reified T> JNIV8Function.applyAsV8Function(arguments: Array<Any?>, flags: Int = V8Flags.Default): T {
    return applyAsV8FunctionTyped(flags, T::class.java, arguments) as T
}

inline fun <reified T> JNIV8Function.applyAsV8FunctionWithReceiver(receiver: Any, arguments: Array<Any?>, flags: Int = V8Flags.Default): T {
    return applyAsV8FunctionWithReceiverTyped(flags, T::class.java, receiver, arguments) as T
}
