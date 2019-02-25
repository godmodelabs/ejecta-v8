package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import ag.boersego.v8annotations.V8Symbols
import android.util.Log
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStream
import java.io.InputStreamReader
import java.nio.Buffer


abstract open class BGJSModuleFetchBody @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    internal var body: InputStream? = null
    private var bodyReader: BufferedReader? = null
    internal var error: String? = null

    open var bodyUsed = false
        internal set


    @V8Function
    open fun arrayBuffer(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        if (consumeBody(resolver)) {
            if (bodyReader != null) {
                //TODO: Use correct JNIV8BufferArray here when it is implemented
                resolver.resolve(JNIV8Object.Create(v8Engine, "ArrayBuffer", 0))
                bodyReader!!.close()
            } else {
                resolver.reject(JNIV8Object.Create(v8Engine, "TypeError", "no body"))
            }
        }

        return resolver.promise
    }

    @V8Function
    open fun json(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        if (consumeBody(resolver)) {
            if (bodyReader != null) {
                try {
                    resolver.resolve(v8Engine.parseJSON(bodyReader!!.readText()))
                } catch (e: Exception) {
                    resolver.reject(JNIV8Object.Create(v8Engine, "TypeError", "invalid json response body, reason: ${e.message}"))
                }
                bodyReader!!.close()
            } else {
                resolver.reject(JNIV8Object.Create(v8Engine, "TypeError", "no body"))
            }
        }

        return resolver.promise
    }

    @V8Function
    open fun text(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        if (consumeBody(resolver)) {
            if (bodyReader != null) {
                resolver.resolve(bodyReader!!.readText())
                bodyReader!!.close()
            } else {
                resolver.reject(JNIV8Object.Create(v8Engine, "TypeError", "no body"))
            }
        }

        return resolver.promise
    }

    private fun consumeBody(resolver: JNIV8Promise.Resolver): Boolean {
        if (bodyUsed) {
            resolver.reject(JNIV8Object.Create(v8Engine, "TypeError", "body used already"))
            return false
        }
        bodyUsed = true
        bodyReader = BufferedReader(InputStreamReader(body))

        //TODO: in node fetch there are several listeners on the body stream, that can throw errors
        if (error != null) {
            resolver.reject(JNIV8Object.Create(v8Engine, "Error", error))
            return false
        }

        //TODO: Timeout
//        // allow timeout on slow response body
//        if (this.timeout) {
//            resTimeout = setTimeout(() => {
//                abort = true;
//                reject(new FetchError(`Response timeout while trying to fetch ${this.url} (over ${this.timeout}ms)`, 'body-timeout'));
//            }, this.timeout);
//        }

        return true
    }

    companion object {
        fun createBodyFromRaw(bodyRaw: Any?): InputStream? {
            return when (bodyRaw) {
                is InputStream -> bodyRaw
                //TODO: URLSearchParams are parsed from javascript as JNIV8GenericObject, how do we check isURLSearchParam? see https://github.com/bitinn/node-fetch/issues/296#issuecomment-307598143
                is JNIV8GenericObject -> (bodyRaw as? JNIV8Object)?.toString()?.byteInputStream()
                //TODO: implement FormData
                //TODO: Check how JNIv8Arraybuffer will be handled
                is JNIV8ArrayBuffer -> bodyRaw.toString().byteInputStream()
                else -> (bodyRaw as? String)?.byteInputStream()
            }
        }

        fun extractContentType (bodyRaw: Any?): String? {
            return when (bodyRaw) {
                //TODO: check isURLSearchParam? see https://github.com/bitinn/node-fetch/issues/296#issuecomment-307598143
                is JNIV8GenericObject -> "application/x-www-form-urlencoded;charset=UTF-8"
                //TODO: implement FormData
                is JNIV8ArrayBuffer -> null //TODO
                is InputStream -> null
                else -> "text/plain;charset=UTF-8"
            }
        }
    }
}