package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Flags

/**
 * Created by martin on 20.12.17.
 */
inline fun <reified T> JNIV8Object.applyV8Method(name: String, flags: Int, arguments: Array<Any?>): T? {
    return applyV8Method(name, flags, T::class.java, arguments)
}

inline fun <reified T> JNIV8Object.callV8Method(name: String, flags: Int, vararg arguments: Any?): T? {
    return callV8Method(name, flags, T::class.java, arguments)
}

inline fun <reified T> JNIV8Object.getV8Field(name: String, flags: Int) : T? {
    return getV8Field(name, flags, T::class.java)
}

inline fun <reified T> JNIV8Object.getV8Fields(flags: Int) : Map<String, T?> {
    return getV8Fields(flags, T::class.java)
}

inline fun <reified T> JNIV8Object.getV8OwnFields(flags: Int) : Map<String, T?> {
    return getV8OwnFields(flags, T::class.java)
}

/* @TODO: are these overloads really a good idea?
inline fun <reified T> JNIV8Object.applyV8MethodNonNull(name: String, flags: Int, arguments: Array<Any?>): T {
    return applyV8Method(name, V8Flags.NonNull or flags, T::class.java, arguments)!!
}

inline fun <reified T> JNIV8Object.callV8MethodNonNull(name: String, flags: Int, vararg arguments: Any?): T {
    return callV8Method(name, V8Flags.NonNull or flags, T::class.java, arguments)!!
}

inline fun <reified T> JNIV8Object.getV8FieldNonNull(name: String, flags: Int) : T {
    return getV8Field(name, V8Flags.NonNull or flags, T::class.java)!!
}
*/