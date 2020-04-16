package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.fetch.*
import android.util.Log
import okhttp3.Call
import okhttp3.Callback
import okhttp3.OkHttpClient
import okhttp3.Response
import java.io.IOException
import java.net.URL
import java.net.UnknownHostException
import java.util.concurrent.TimeUnit
import java.util.zip.GZIPInputStream

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetch(val okHttpClient: OkHttpClient) : JNIV8Module("fetch") {

    internal lateinit var fetchErrorCreator: JNIV8Function
    internal lateinit var abortErrorCreator: JNIV8Function

    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {

        val fetchFunction = JNIV8Function.Create(engine) { _, arguments ->
            return@Create fetch(engine, arguments)
        }

        fetchFunction.setV8Field("Headers", engine.getConstructor(BGJSModuleFetchHeaders::class.java))
        fetchFunction.setV8Field("Request", engine.getConstructor(BGJSModuleFetchRequest::class.java))
        fetchFunction.setV8Field("Response", engine.getConstructor(BGJSModuleFetchResponse::class.java))
        fetchFunction.setV8Field("AbortController", engine.getConstructor(BGJSModuleAbortController::class.java))
        fetchFunction.setV8Field("AbortSignal", engine.getConstructor(BGJSModuleAbortSignal::class.java))
        fetchFunction.setV8Field("FormData", engine.getConstructor(BGJSModuleFormData::class.java))

        fetchErrorCreator = engine.runScript(FETCHERROR_SCRIPT.trimIndent(), "FetchError") as JNIV8Function
        fetchFunction.setV8Field("FetchError", fetchErrorCreator)

        abortErrorCreator = engine.runScript(ABORTERROR_SCRIPT.trimIndent(), "AbortError") as JNIV8Function
        fetchFunction.setV8Field("AbortError", abortErrorCreator)

        module?.setV8Field("exports", fetchFunction)
    }

    fun fetch(engine: V8Engine, arguments: Array<Any>): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(engine)

        try {
            if (arguments.isEmpty()) {
                throw V8JSException(engine, "TypeError", "fetch needs at least one argument: input")
            }
            startRequest(engine, resolver, BGJSModuleFetchRequest(v8Engine = engine, args = arguments))
        } catch (e: V8JSException) {
            resolver.reject(e.v8Exception)
        }

        return resolver.promise
    }

    private fun startRequest(v8Engine: V8Engine, resolver: JNIV8Promise.Resolver, request: BGJSModuleFetchRequest) {
        val httpRequest = request.execute()
        var fetchResponse: BGJSModuleFetchResponse? = null
        val call = okHttpClient.newCall(httpRequest)
        val timeout = call.timeout()
        val requestTimeOut = request.timeout.toLong() * 1000
        timeout.timeout(requestTimeOut, TimeUnit.MILLISECONDS)

        val signal = request.signal
        var abortAndFinalize :JNIV8Function? = null

        abortAndFinalize = JNIV8Function.Create(v8Engine, object : JNIV8Function.Handler {
            override fun Callback(receiver: Any, arguments: Array<out Any>) {
                fetchResponse?.body?.close()
                fetchResponse?.error = "abort"
                resolver.reject(abortErrorCreator.applyAsV8Constructor(arrayOf("The user aborted a request.")))
                call.cancel()
                signal?.removeEventListener("abort", abortAndFinalize!!)
            }})


        if (signal != null) {
            if (signal.aborted) {
                resolver.reject(abortErrorCreator.applyAsV8Constructor(arrayOf("The user aborted a request.")))
                return
            } else {
                signal.addEventListener("abort", abortAndFinalize!!)
            }
        }

        call.enqueue(object : Callback {

            override fun onFailure(call: Call, e: IOException) {
                Log.d(TAG, "onFailure", e)
                // network error or timeout
                signal?.removeEventListener("abort", abortAndFinalize)
                when {
                    e is UnknownHostException -> resolver.reject(
                        fetchErrorCreator.applyAsV8Constructor(
                            arrayOf(
                                "request to ${request.parsedUrl} failed, reason: ${e.message}",
                                "system",
                                "ENOTFOUND"
                            )
                        )
                    )
                    e.cause?.message?.contains("ECONNREFUSED") == true -> resolver.reject(
                        fetchErrorCreator.applyAsV8Constructor(
                            arrayOf(
                                "request to ${request.parsedUrl} failed, reason: ${e.message}",
                                "system",
                                "ECONNREFUSED"
                            )
                        )
                    )
                    e.message?.contains("unexpected end of stream") == true -> resolver.reject(
                        fetchErrorCreator.applyAsV8Constructor(
                            arrayOf(
                                "request to ${request.parsedUrl} failed, reason: ${e.message}",
                                "system",
                                "ECONNRESET"
                            )
                        )
                    )
                    else -> resolver.reject(
                        fetchErrorCreator.applyAsV8Constructor(
                                arrayOf(
                                    "request to ${request.parsedUrl} failed, reason: ${e.message}",
                                    "system"
                                )
                            )
                    )
                }
            }

            override fun onResponse(call: Call, httpResponse: Response) {
                timeout.clearTimeout()
                if (call.isCanceled()) {
                    signal?.removeEventListener("abort", abortAndFinalize)
                    return
                }
                fetchResponse = request.updateFrom(v8Engine, httpResponse)

                // HTTP fetch step 5
                if (isRedirect(fetchResponse!!.status)) {
                    // HTTP fetch step 5.2
                    val location = fetchResponse!!.headers.get("location")

                    // HTTP fetch step 5.3
                    val locationUrl = location?.let { URL(request.parsedUrl, it) }

                    // HTTP fetch step 5.5
                    when (request.redirect) {
                        "error" -> {
                            resolver.reject(fetchErrorCreator.applyAsV8Constructor(arrayOf("redirect mode is set to error ${request.parsedUrl}", "no-redirect")))
                            signal?.removeEventListener("abort", abortAndFinalize)
                            return
                        }

                        "manual" -> {
                            // node-fetch-specific step: make manual redirect a bit easier to use by setting the Location header value to the resolved URL.
                            locationUrl?.let {
                                fetchResponse!!.headers.set("location", it.toString())
                            }
                        }

                        "follow" -> {
                            // HTTP-redirect fetch step 2
                            locationUrl?.let {

                                // HTTP-redirect fetch step 5
                                if (request.counter >= request.follow) {
                                    signal?.removeEventListener("abort", abortAndFinalize)
                                    resolver.reject(fetchErrorCreator.applyAsV8Constructor(arrayOf("maximum redirect reached at ${request.parsedUrl}", "max-redirect")))
                                    return
                                }

                                // HTTP-redirect fetch step 6 (counter increment)
                                // Create a new Request object.
                                val redirectRequest = request.clone()
                                redirectRequest.parsedUrl = it
                                redirectRequest.counter++

                                // TODO:  HTTP-redirect fetch step 9
                                if (httpResponse.code != 303 && request.body != null) {

                                }

                                // HTTP-redirect fetch step 11
                                if (httpResponse.code == 303 || ((httpResponse.code == 301 || httpResponse.code == 302) && request.method == "POST")) {
                                    redirectRequest.method = "GET"
                                    redirectRequest.body?.close()
                                    redirectRequest.body = null
                                    redirectRequest.headers.delete("content-length")
                                }
                                resolver.resolve(fetch(v8Engine, arrayOf(redirectRequest)))
                                return
                            }
                        }

                    }
                }

                // HTTP-network fetch step 12.1.1.3
                val codings = fetchResponse!!.headers.get("content-encoding")

                // HTTP-network fetch step 12.1.1.4: handle content codings

                // in following scenarios we ignore compression support
                // 1. compression support is disabled
                // 2. HEAD request
                // 3. no Content-Encoding header
                // 4. no content response (204)
                // 5. content not modified response (304)
                if (!request.compress || request.method == "HEAD" || codings == null || httpResponse.code == 204 || httpResponse.code == 304) {
                    resolver.resolve(fetchResponse)
                    return
                }

                //TODO: gzip working?
                if (codings == "gzip" || codings == "x-gzip") {
                    try {
                        fetchResponse!!.body = GZIPInputStream(fetchResponse!!.body)
                    } catch (e: IOException) {
                        signal?.removeEventListener("abort", abortAndFinalize)
                        resolver.reject(fetchErrorCreator.applyAsV8Constructor(arrayOf("Invalid response body while trying to fetch ${fetchResponse!!.url}: ${e.message}", "system", "Z_DATA_ERROR")))
                        return
                    }
                }


                //TODO: deflate
                // handle the infamous raw deflate response from old servers
                // a hack for old IIS and Apache servers
                if (codings == "deflate" || codings == "x-deflate") {

                }
                resolver.resolve(fetchResponse)
            }
        })

    }

    companion object {
        private val TAG = BGJSModuleFetch::class.java.simpleName
        const val FETCHERROR_SCRIPT =    """
        (function() {
            function FetchError(message, type, systemError) {
                this.name = 'FetchError';
                this.message = message || 'An error occurred';
                this.type = type;
                this.code = this.errno = systemError;
            }
            FetchError.prototype = Object.create(Error.prototype);
            FetchError.prototype.constructor = FetchError;
            return FetchError;
        }())
        """
        const val ABORTERROR_SCRIPT = """
        (function() {
            function AbortError(message) {
                this.type = 'aborted';
	            this.message = message;
	            Error.captureStackTrace(this, this.constructor);
            }
            AbortError.prototype = Object.create(Error.prototype);
            AbortError.prototype.constructor = AbortError;
            AbortError.prototype.name = 'AbortError';
            return AbortError;
        }())
        """

        init {
            JNIV8Object.RegisterV8Class(BGJSModuleFetchHeaders::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchResponse::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchRequest::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleAbortController::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleAbortSignal::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFormData::class.java)
        }

        fun isRedirect(statusCode: Int): Boolean {
            return statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308
        }
    }

}

