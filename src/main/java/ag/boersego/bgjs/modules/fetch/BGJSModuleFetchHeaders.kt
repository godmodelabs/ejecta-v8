package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8Array
import ag.boersego.bgjs.JNIV8Iterator
import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.V8Engine
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Symbols
import okhttp3.Headers
import okhttp3.Request
import java.util.*
import kotlin.collections.HashMap

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
    fun get(name: String): String? {
        return headers.get(normalizeName(name))?.joinToString(",")
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

    @V8Function(symbol = V8Symbols.ITERATOR)
    fun iterator() : JNIV8Iterator {
        val it = headers.entries.iterator()
        return JNIV8Iterator(v8Engine, object: Iterator<Any> {
            override fun hasNext(): Boolean {
                return it.hasNext()
            }

            override fun next(): Any {
                val nextVal = it.next()
                return JNIV8Array.CreateWithElements(v8Engine, nextVal.key, nextVal.value.joinToString(", "))
            }

        })
//
//
//        val next = JNIV8Function.Create(v8Engine) { _, _ ->
//            val result = JNIV8GenericObject.Create(v8Engine)
//            if (it.hasNext()) {
//                val nextVal = it.next()
//                val arr = JNIV8Array.CreateWithElements(v8Engine, nextVal.key, nextVal.value.joinToString(", "))
//                result.setV8Field("value", arr)
//                result.setV8Field("done", false)
//            } else {
//                result.setV8Field("done", true)
//            }
//            return@Create result
//        }
//        val obj = JNIV8GenericObject.Create(v8Engine)
//        obj.setV8Field("next", next)
//        return obj
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
                    throw RuntimeException("init.headers object: values must be Strings (problem with key '${entry.key}'")
                }
                headers.set(entry.key, entry.value as String)
            }

            return headers
        }

        fun createFrom(v8Engine: V8Engine, httpHeaders: Headers): BGJSModuleFetchHeaders {
            val headers = BGJSModuleFetchHeaders(v8Engine)
            for (entry in httpHeaders.toMultimap()) {
                headers.headers.set(entry.key, ArrayList<Any>(entry.value))
            }

            return headers
        }

        val ZERO_BYTE = '\u0000'
        val CONTENT_TYPE = "content-type"
    }
}