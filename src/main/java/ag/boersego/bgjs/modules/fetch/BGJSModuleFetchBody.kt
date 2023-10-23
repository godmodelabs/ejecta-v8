package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.BGJSModuleFetch
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
            bodyReader?.use {
                resolver.resolve(Create(v8Engine, "ArrayBuffer", 0))
            } ?: resolver.reject(Create(v8Engine, "TypeError", "no body"))
        }

        return resolver.promise
    }

    @V8Function
    open fun json(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        if (consumeBody(resolver)) {
            bodyReader?.use { reader ->
                try {
                    resolver.resolve(v8Engine.parseJSON(reader.readText()))
                } catch (e: Exception) {
                    resolver.reject(
                        (v8Engine.runScript(BGJSModuleFetch.FETCHERROR_SCRIPT.trimIndent(), "FetchError") as JNIV8Function).applyAsV8Constructor(
                            arrayOf(
                                "invalid json response body at ${this.url} reason: ${e.message}",
                                "invalid-json"
                            )
                        )
                    )
                }
            } ?: resolver.reject(Create(v8Engine, "TypeError", "no body"))
        }

        return resolver.promise
    }

    @V8Function
    open fun text(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        if (consumeBody(resolver)) {
            bodyReader?.use { reader ->
                resolver.resolve(reader.readText())
            } ?: resolver.reject(Create(v8Engine, "TypeError", "no body"))
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

        return true
    }

    companion object {
        fun createBodyFromRaw(bodyRaw: Any?): InputStream? {
            return when (bodyRaw) {
                is InputStream -> bodyRaw
                is JNIV8GenericObject -> (bodyRaw as? JNIV8Object)?.toString()?.byteInputStream()
                is BGJSModuleFormData -> bodyRaw.toInputStream()
                is JNIV8ArrayBuffer -> bodyRaw.toString().byteInputStream()
                else -> (bodyRaw as? String)?.byteInputStream()
            }
        }

        fun extractContentType (bodyRaw: Any?): String? {
            return when (bodyRaw) {
                is JNIV8GenericObject -> "application/x-www-form-urlencoded"
                is BGJSModuleFormData -> "application/x-www-form-urlencoded "
                is JNIV8ArrayBuffer -> null
                is InputStream -> null
                else -> "text/plain;charset=UTF-8"
            }
        }
    }
}