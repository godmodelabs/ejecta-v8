package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.BGJSModuleFetchHeaders
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import ag.boersego.v8annotations.V8Symbols
import java.io.InputStream


/**
 * Created by dseifert on 19.September.2019
 */

class BGJSModuleFormData @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    private val entryList: MutableList<Pair<String, String>> = mutableListOf()

    init {
        if (args != null && !args.isEmpty()) {
            val fields = (args[0] as JNIV8Object).v8Fields

            for (entry in fields) {
                if (entry.value !is String) {
                    throw V8JSException(
                        v8Engine,
                        "TypeError",
                        "init.entryList object: values must be Strings (problem with key '${entry.key}'"
                    )
                }
                set(entry.key, entry.value as String)
            }
        }
    }

    @V8Function
    fun append(name: String, value: String) {
        if (value.contains(BGJSModuleFetchHeaders.ZERO_BYTE) || value.contains('\n') || value.contains(
                '\r'
            )
        ) {
            throw V8JSException(v8Engine, "TypeError", "illegal character in value")
        }

        entryList.add(Pair(name, value))
    }

    @V8Function
    fun delete(name: String) {
        entryList.removeAll { it.first == name }
    }

    @V8Function
    fun entries(): JNIV8Array {
        return JNIV8Array.CreateWithArray(v8Engine, entryList.toTypedArray())
    }

    @V8Function
    fun get(name: String?): String? {
        return entryList.firstOrNull { it.first == name }?.second
    }

    @V8Function
    fun getAll(name: String): JNIV8Array {
        if (!entryList.any { it.first == name }) return JNIV8Array.Create(v8Engine)
        return JNIV8Array.CreateWithArray(v8Engine, entryList.filter { it.first == name }.map { it.second }.toTypedArray())
    }


    @V8Function
    fun has(name: String): Boolean {
        return entryList.any { it.first == name }
    }

    @V8Function
    fun keys(): JNIV8Array {
        if (entryList.isEmpty()) return JNIV8Array.Create(v8Engine)
        return JNIV8Array.CreateWithArray(v8Engine, entryList.map { it.first }.toTypedArray())
    }

    @V8Function
    fun values(): JNIV8Array {
        if (entryList.isEmpty()) return JNIV8Array.Create(v8Engine)
        return JNIV8Array.CreateWithArray(v8Engine, entryList.map { it.second }.toTypedArray())
    }

    /**
     * Overwrite an item in the entry list
     */
    @V8Function
    fun set(name: String, value: String) {
        if (value.contains(BGJSModuleFetchHeaders.ZERO_BYTE) || value.contains('\n') || value.contains(
                '\r'
            )
        ) {
            throw V8JSException(v8Engine, "TypeError", "illegal character in value")
        }

        entryList.removeAll { it.first == name }
        entryList.add(Pair(name, value))
    }

    @V8Function(symbol = V8Symbols.ITERATOR)
    fun iterator(): JNIV8Iterator {
        val it = entryList.iterator()
        return JNIV8Iterator(v8Engine, object : Iterator<Any> {
            override fun hasNext(): Boolean {
                return it.hasNext()
            }

            override fun next(): Any {
                val nextPair = it.next()
                return JNIV8Array.CreateWithElements(
                    v8Engine,
                    nextPair.first,
                    nextPair.second
                )
            }

        })
    }

    fun toInputStream(): InputStream {
        var param = ""
        entryList.forEach {
            param += "${it.first}=${it.second}&"
        }
        param = param.dropLast(1)
        return param.byteInputStream()
    }

    val toString = "FormData"
        @V8Getter(symbol = V8Symbols.TO_STRING_TAG) get

}