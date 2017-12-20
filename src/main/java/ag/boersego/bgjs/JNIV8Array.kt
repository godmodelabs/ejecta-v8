package ag.boersego.bgjs

/**
 * Created by martin on 20.12.17.
 */

inline fun <reified T> JNIV8Array.getV8Element(flags: Int, index: Int): T? {
    return getV8Element(flags, T::class.java, index)
}

inline fun <reified T> JNIV8Array.getV8Elements(flags: Int, from: Int, to: Int): Array<T?> {
    return getV8Elements(flags, T::class.java, from, to)
}

inline fun <reified T> JNIV8Array.getV8Elements(flags: Int): Array<T?> {
    return getV8Elements(flags, T::class.java)
}