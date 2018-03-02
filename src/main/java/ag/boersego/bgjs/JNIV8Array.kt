package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Flags

/**
 * Created by martin on 20.12.17.
 */

inline fun <reified T> JNIV8Array.getV8Element(index: Int, flags: Int = V8Flags.Default): T {
    return getV8ElementTyped(flags, T::class.java, index) as T
}

inline fun <reified T> JNIV8Array.getV8Elements(from: Int, to: Int, flags: Int = V8Flags.Default): Array<T?> {
    return getV8ElementsTyped(flags, T::class.java, from, to)
}

inline fun <reified T> JNIV8Array.getV8Elements(flags: Int = V8Flags.Default): Array<T?> {
    return getV8ElementsTyped(flags, T::class.java)
}