package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.V8Engine
import ag.boersego.v8annotations.V8Function
import java.util.*

class BGJSModuleFetchHeaders @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    private val headers = HashMap<String, ArrayList<Any>>()

    private fun normalizePotentialValue(ptValue: String): String {
        return ptValue.trim()
    }

    private fun normalizeName(name: String): String {
        return name.trim().toLowerCase(Locale.ROOT)
    }

    @V8Function
    fun append(rawName: String, rawValue: String) {
        val value = normalizePotentialValue(rawValue)
        if (value.contains(ZERO_BYTE) || value.contains('\n') || value.contains('\r')) {
            throw RuntimeException("TypeError: illegal character in value")
        }
        val name = normalizeName(rawName)

        var currentList = headers[name]

        if (currentList == null) {
            currentList = ArrayList()
            headers.put(name, currentList)
        }
        currentList.add(value)
    }

    /**
     * Overwrite a header in the header list
     */
    @V8Function
    fun set(rawName: String, rawValue: String) {
        val value = normalizePotentialValue(rawValue)
        if (value.contains(ZERO_BYTE) || value.contains('\n') || value.contains('\r')) {
            throw RuntimeException("TypeError: illegal character in value")
        }
        val name = normalizeName(rawName)

        var currentList = headers[name]

        if (currentList == null) {
            currentList = ArrayList()
            headers.put(name, currentList)
        } else {
            currentList.clear()
        }
        currentList.add(value)
    }

    @V8Function
    fun delete(name: String) {
        headers.remove(normalizeName(name))
    }

    @V8Function
    fun get(name: String): Any? {
        return headers.get(normalizeName(name))
    }

    @V8Function
    fun has(name: String): Boolean {
        return headers.containsKey(normalizeName(name))
    }

    @V8Function
    fun entries(): List<List<String>> {
        val valueList = ArrayList<List<String>>()
        for (header in headers) {
            val innerList = ArrayList<String>()
            innerList.add(header.key)
            innerList.add(header.value.joinToString(", "))
        }

        return valueList
    }

    @V8Function
    fun keys(): List<String> {
        val keyList = ArrayList<String>(headers.keys)

        return keyList
    }

    @V8Function
    fun values(): List<Any> {
        val valueList = ArrayList<Any>()
        for (header in headers.values) {
            valueList.add(header.joinToString(","))
        }

        return valueList
    }

    companion object {
        fun createFrom(headerRaw: JNIV8Object): BGJSModuleFetchHeaders {
            val fields = headerRaw.v8Fields

            val headers = BGJSModuleFetchHeaders(headerRaw.v8Engine)
            for (entry in fields) {
                if (entry.value !is String) {
                    throw RuntimeException("init.headers object: values must be Strings (problem with key '${entry.key}'")
                }
                headers.set(entry.key, entry.value as String)
            }

            return headers
        }

        val ZERO_BYTE = '\u0000'
    }
}