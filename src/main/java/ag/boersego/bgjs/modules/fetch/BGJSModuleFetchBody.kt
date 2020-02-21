package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.fetch.BGJSModuleFormData
import ag.boersego.v8annotations.V8Function
import java.io.BufferedReader
import java.io.InputStream
import java.io.InputStreamReader
import java.net.URL


abstract class BGJSModuleFetchBody @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    var error: String? = null
    internal var body: InputStream? = null
    private var bodyReader: BufferedReader? = null
    lateinit var parsedUrl: URL
    open var url = ""
        internal set
    open var bodyUsed = false
        internal set


    @V8Function
    open fun arrayBuffer(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        if (consumeBody(resolver)) {
            if (bodyReader != null) {
                //TODO: Use correct JNIV8BufferArray here when it is implemented
                resolver.resolve(Create(v8Engine, "ArrayBuffer", 0))
                bodyReader!!.close()
            } else {
                resolver.reject(Create(v8Engine, "TypeError", "no body"))
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
                    resolver.reject((v8Engine.runScript(BGJSModuleFetch.FETCHERROR_SCRIPT.trimIndent(), "FetchError") as JNIV8Function).applyAsV8Constructor(
                        arrayOf(
                            "invalid json response body at ${this.url} reason: ${e.message}",
                            "invalid-json"
                        )))}
                bodyReader!!.close()
            } else {
                resolver.reject(Create(v8Engine, "TypeError", "no body"))
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
                resolver.reject(Create(v8Engine, "TypeError", "no body"))
            }
        }

        return resolver.promise
    }

    private fun consumeBody(resolver: JNIV8Promise.Resolver): Boolean {
        if (bodyUsed) {
            resolver.reject(Create(v8Engine, "TypeError", "body used already"))
            return false
        }
        bodyUsed = true
        bodyReader = BufferedReader(InputStreamReader(body))

        if (error == "abort") {
            resolver.reject(
                (v8Engine.runScript(
                    BGJSModuleFetch.ABORTERROR_SCRIPT.trimIndent(),
                    "AbortError"
                ) as JNIV8Function).applyAsV8Constructor(arrayOf("The user aborted a request.")))
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
                is BGJSModuleFormData -> bodyRaw.toInputStream()
                //TODO: Check how JNIv8Arraybuffer will be handled
                is JNIV8ArrayBuffer -> bodyRaw.toString().byteInputStream()
                else -> (bodyRaw as? String)?.byteInputStream()
            }
        }

        fun extractContentType (bodyRaw: Any?): String? {
            return when (bodyRaw) {
                //TODO: check isURLSearchParam? see https://github.com/bitinn/node-fetch/issues/296#issuecomment-307598143
                is JNIV8GenericObject -> "application/x-www-form-urlencoded"
                is BGJSModuleFormData -> "application/x-www-form-urlencoded "
                is JNIV8ArrayBuffer -> null //TODO
                is InputStream -> null
                else -> "text/plain;charset=UTF-8"
            }
        }
    }
}