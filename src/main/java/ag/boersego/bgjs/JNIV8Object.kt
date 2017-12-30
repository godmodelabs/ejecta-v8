package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Flags

/**
 * Created by martin on 20.12.17.
 */
inline fun <reified T> JNIV8Object.applyV8Method(name: String, arguments: Array<Any?>, flags: Int = V8Flags.Default): T? {
    return applyV8MethodTyped(name, flags, T::class.java, arguments)
}

inline fun <reified T> JNIV8Object.callV8Method(name: String, vararg arguments: Any?, flags: Int = V8Flags.Default): T? {
    return callV8MethodTyped(name, flags, T::class.java, arguments)
}

inline fun <reified T> JNIV8Object.getV8Field(name: String, flags: Int = V8Flags.Default) : T? {
    return getV8FieldTyped(name, flags, T::class.java)
}

inline fun <reified T> JNIV8Object.getV8Fields(flags: Int = V8Flags.Default) : Map<String, T?> {
    return getV8FieldsTyped(flags, T::class.java)
}

inline fun <reified T> JNIV8Object.getV8OwnFields(flags: Int = V8Flags.Default) : Map<String, T?> {
    return getV8OwnFieldsTyped(flags, T::class.java)
}
