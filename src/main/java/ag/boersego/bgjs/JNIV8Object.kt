package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Flags

/**
 * Created by martin on 20.12.17.
 */
inline fun <reified T> JNIV8Object.applyV8Method(name: String, arguments: Array<Any?>, flags: Int = V8Flags.Default): T {
    return applyV8MethodTyped(name, flags, T::class.java, arguments) as T
}

inline fun <reified T> JNIV8Object.callV8Method(name: String, vararg arguments: Any?, flags: Int = V8Flags.Default): T {
    return applyV8MethodTyped(name, flags, T::class.java, arguments) as T
}

inline fun <reified T> JNIV8Object.getV8Field(name: String, flags: Int = V8Flags.Default) : T {
    return getV8FieldTyped(name, flags, T::class.java) as T
}

inline fun <reified T> JNIV8Object.getV8Fields(flags: Int = V8Flags.Default) : Map<String, T> {
    return getV8FieldsTyped(flags, T::class.java)
}

inline fun <reified T> JNIV8Object.getV8OwnFields(flags: Int = V8Flags.Default) : Map<String, T> {
    return getV8OwnFieldsTyped(flags, T::class.java)
}

/**
 * Kotlin primitive types have classes that are not really referencable from JNI, maybe because
 * they exist in their own classloader. We work around this by getting the class hashcode in
 * Java and then map these to normal java.lang.* boxed types.
 */
fun registerKotlinAliases() {
    JNIV8Object.RegisterAliasForPrimitive(Boolean::class.java, java.lang.Boolean::class.java)
    JNIV8Object.RegisterAliasForPrimitive(Int::class.java, java.lang.Integer::class.java)
    JNIV8Object.RegisterAliasForPrimitive(Byte::class.java, java.lang.Byte::class.java)
    JNIV8Object.RegisterAliasForPrimitive(Short::class.java, java.lang.Short::class.java)
    JNIV8Object.RegisterAliasForPrimitive(Long::class.java, java.lang.Long::class.java)
    JNIV8Object.RegisterAliasForPrimitive(Float::class.java, java.lang.Float::class.java)
    JNIV8Object.RegisterAliasForPrimitive(Double::class.java, java.lang.Double::class.java)
    JNIV8Object.RegisterAliasForPrimitive(Char::class.java, java.lang.Character::class.java)
}