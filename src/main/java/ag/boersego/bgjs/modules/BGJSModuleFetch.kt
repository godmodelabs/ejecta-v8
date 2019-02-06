package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import java.util.*
import kotlin.collections.ArrayList

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetch : JNIV8Module("fetch") {
    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        TODO("not implemented") //To change body of created functions use File | Settings | File Templates.
    }

}

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
        val ZERO_BYTE = '\u0000'
    }
}

abstract open class BGJSModuleFetchBody @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {
    var body: String? = null
        internal set
        @V8Getter get

    var bodyUsed = false
        internal set
        @V8Getter get

    @V8Function
    fun json(): JNIV8Object {
        // TODO: parse
        return JNIV8GenericObject.Create(v8Engine)
    }
}

class BGJSModuleFetchResponse @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : BGJSModuleFetchBody(v8Engine, jsPtr, args) {

    var status: Int = -1
        internal set
        @V8Getter get

    var statusText: String? = null
        internal set
        @V8Getter get

    var headers: BGJSModuleFetchHeaders? = null
        internal set
        @V8Getter get

    val ok: Boolean
        @V8Getter get() = status in 200..299

    var redirect = false
        internal set
        @V8Getter get

    var type = ""
        internal set
        @V8Getter get

    var url = ""
        internal set
        @V8Getter get

    var useFinalURL = false
        internal set
        @V8Getter get


    init {
        if (args != null) {
            if (args.size > 0) {
                // First argument: body
                val bodyRaw = args[0]
                // TODO: According to spec, this should be a Blob!
                if (bodyRaw is String) {
                    body = bodyRaw
                }
            }

            if (args.size > 1 && args[1] is JNIV8Object) {
                val options = args[1] as JNIV8Object
                if (options.hasV8Field("status")) {
                    status = options.getV8Field<Int>("status")
                }
                if (options.hasV8Field("statusText")) {
                    statusText = options.getV8Field<String>("statusText")
                }
                if (options.hasV8Field("headers")) {
                    val headersRaw = options.getV8Field("headers")
                    headers = headersRaw as? BGJSModuleFetchHeaders
                }
            }
        }
    }


}