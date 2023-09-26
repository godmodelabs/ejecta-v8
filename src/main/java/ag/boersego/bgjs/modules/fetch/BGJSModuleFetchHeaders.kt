package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.*
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import ag.boersego.v8annotations.V8Symbols
import okhttp3.Headers
import okhttp3.Request
import java.text.DecimalFormat
import java.text.DecimalFormatSymbols
import java.util.*

class BGJSModuleFetchHeaders @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    private val headers = LinkedHashMap<String, ArrayList<Any>>()

    init {
        if (!args.isNullOrEmpty()) {
            val fields = (args[0] as JNIV8Object).v8Fields

            for (entry in fields) {
                if (entry.value !is String) {
                    throw V8JSException(
                        v8Engine,
                        "TypeError",
                        "init.headers object: values must be Strings (problem with key '${entry.key}'"
                    )
                }
                set(entry.key, entry.value as String)
            }
        }
    }

    private fun normalizePotentialValue(ptValue: Any?): String {
        return when (ptValue) {
            is JNIV8Array -> ptValue.joinToString(",")
            is Double -> {
                val format = DecimalFormat("#.#")
                format.decimalFormatSymbols = DecimalFormatSymbols(Locale.ENGLISH)
                return format.format(ptValue)
            }
            null -> "null"
            else -> {
                if (ptValue.toString().contains(ZERO_BYTE) || ptValue.toString().contains('\n') || ptValue.toString().contains('\r')) {
                    throw V8JSException(v8Engine, "TypeError", "illegal character in value")
                }
                ptValue.toString().trim()
            }
        }
    }

    private fun normalizeName(name: Any?): String {
        if (name is JNIV8Array || name is JNIV8GenericObject || (name as? String)?.isEmpty() == true) {
            throw V8JSException(v8Engine, "TypeError", "illegal name")
        }
        return name?.toString()?.trim()?.lowercase(Locale.ROOT) ?:"null"
    }

    val toString = "Headers"
        @V8Getter(symbol = V8Symbols.TO_STRING_TAG) get

    @V8Function
    fun append(rawName: Any?, rawValue: Any?) {
        val name = normalizeName(rawName)
        val value = normalizePotentialValue(rawValue)
        var currentList = headers[name]

        if (currentList == null) {
            currentList = ArrayList()
        } else {
            headers.remove(name)
        }
        currentList.add(value)
        headers[name] = currentList
    }

    /**
     * Overwrite a header in the header list
     */
    @V8Function
    fun set(rawName: Any?, rawValue: Any?) {
        val name = normalizeName(rawName)
        val value = normalizePotentialValue(rawValue)

        var currentList = headers[name]

        if (currentList == null) {
            currentList = ArrayList()
            headers[name] = currentList
        } else {
            currentList.clear()
        }
        currentList.add(value)
    }

    @V8Function
    fun delete(name: Any?) {
        headers.remove(normalizeName(name))
    }

    @V8Function
    fun get(name: Any?): String? {
        return headers[normalizeName(name)]?.joinToString(", ")
    }

    @V8Function
    fun has(name: Any?): Boolean {
        return headers.containsKey(normalizeName(name))
    }

    @V8Function
    fun entries(): JNIV8Array {
        val result = arrayOfNulls<JNIV8Array>(headers.size)
        var i = 0
        for (header in headers) {
            val innerList = JNIV8Array.CreateWithElements(v8Engine, header.key, header.value.joinToString(", "))
            result[i++] = innerList
        }

        return JNIV8Array.CreateWithArray(v8Engine, result)
    }

    @V8Function(symbol = V8Symbols.ITERATOR)
    fun iterator() : JNIV8Iterator {
        val it = headers.entries.reversed().iterator()
        return JNIV8Iterator(v8Engine, object: Iterator<Any> {
            override fun hasNext(): Boolean {
                return it.hasNext()
            }

            override fun next(): Any {
                val nextVal = it.next()
                return JNIV8Array.CreateWithElements(v8Engine, nextVal.key, nextVal.value.joinToString(", "))
            }

        })
    }

    @V8Function
    fun keys(): JNIV8Array {
        return JNIV8Array.CreateWithArray(v8Engine, headers.keys.toTypedArray())
    }

    @V8Function
    fun values(): JNIV8Array {
        val valueList = ArrayList<Any>()
        for (header in headers.values) {
            valueList.add(header.joinToString(","))
        }

        return JNIV8Array.CreateWithArray(v8Engine, valueList.toTypedArray())
    }

    fun clone(): BGJSModuleFetchHeaders {
        val clone = BGJSModuleFetchHeaders(v8Engine)
        clone.headers.putAll(headers)

        return clone
    }

    fun applyToRequest(builder: Request.Builder) {
        for (header in headers.entries) {
            builder.addHeader(header.key, header.value.joinToString(","))
        }
    }

    companion object {
        fun createFrom(headerRaw: JNIV8Object): BGJSModuleFetchHeaders {
            val fields = headerRaw.v8Fields

            val headers = BGJSModuleFetchHeaders(headerRaw.v8Engine)
            for (entry in fields) {
                if (entry.value !is String) {
                    throw V8JSException(headerRaw.v8Engine, "TypeError", "init.headers object: values must be Strings (problem with key '${entry.key}'")
                }
                headers.set(entry.key, entry.value as String)
            }

            return headers
        }

        fun createFrom(v8Engine: V8Engine, httpHeaders: Headers): BGJSModuleFetchHeaders {
            val headers = BGJSModuleFetchHeaders(v8Engine)
            for (entry in httpHeaders.toMultimap()) {
                headers.headers[entry.key] = ArrayList<Any>(entry.value)
            }

            return headers
        }

        const val ZERO_BYTE = '\u0000'
    }
}