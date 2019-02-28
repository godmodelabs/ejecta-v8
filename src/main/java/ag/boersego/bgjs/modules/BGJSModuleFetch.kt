package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.fetch.BGJSModuleFetchRequest
import android.util.Log
import okhttp3.Call
import okhttp3.Callback
import okhttp3.OkHttpClient
import okhttp3.Response
import java.io.IOException
import java.io.InputStream
import java.net.URL
import java.net.UnknownHostException
import java.util.concurrent.TimeUnit
import java.util.zip.GZIPInputStream

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetch(val okHttpClient: OkHttpClient) : JNIV8Module("fetch") {

    internal lateinit var errorCreator: JNIV8Function

    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {

        val fetchFunction = JNIV8Function.Create(engine) { receiver, arguments ->
            return@Create fetch(engine, arguments)
        }

        fetchFunction.setV8Field("Headers", engine.getConstructor(BGJSModuleFetchHeaders::class.java))
        fetchFunction.setV8Field("Request", engine.getConstructor(BGJSModuleFetchRequest::class.java))
        fetchFunction.setV8Field("Response", engine.getConstructor(BGJSModuleFetchResponse::class.java))

        errorCreator = engine.runScript(FETCHERROR_SCRIPT.trimIndent(), "FetchError") as JNIV8Function

        fetchFunction.setV8Field("FetchError", errorCreator)

        module?.setV8Field("exports", fetchFunction)
    }

    fun fetch(engine: V8Engine, arguments: Array<Any>): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(engine)

        try {
            if (arguments == null || arguments.size < 1) {
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
        val call = okHttpClient.newCall(httpRequest)
        val timeout = call.timeout()
        timeout.timeout(request.timeout.toLong(), TimeUnit.MILLISECONDS)
        call.enqueue(object : Callback {

            override fun onFailure(call: Call, e: IOException) {
                Log.d(TAG, "onFailure", e)
                // network error or timeout
                when {
                    e is UnknownHostException -> resolver.reject(
                        errorCreator.applyAsV8Constructor(
                            arrayOf(
                                "request to ${request.parsedUrl} failed, reason: ${e.message}",
                                "system",
                                "ENOTFOUND"
                            )
                        )
                    )
                    e.cause?.message?.contains("ECONNREFUSED") == true -> resolver.reject(
                        errorCreator.applyAsV8Constructor(
                            arrayOf(
                                "request to ${request.parsedUrl} failed, reason: ${e.message}",
                                "system",
                                "ECONNREFUSED"
                            )
                        )
                    )
                    e.message?.contains("unexpected end of stream") == true -> resolver.reject(
                        errorCreator.applyAsV8Constructor(
                            arrayOf(
                                "request to ${request.parsedUrl} failed, reason: ${e.message}",
                                "system",
                                "ECONNRESET"
                            )
                        )
                    )
                    else -> errorCreator.applyAsV8Constructor(
                        arrayOf(
                            "request to ${request.parsedUrl} failed, reason: ${e.message}",
                            "system"
                        )
                    )
                }
            }

            override fun onResponse(call: Call, httpResponse: Response) {
                timeout.clearTimeout()
                val response = request.updateFrom(v8Engine, httpResponse)

                // HTTP fetch step 5
                if (isRedirect(response.status)) {
                    // HTTP fetch step 5.2
                    val location = response.headers.get("location")

                    // HTTP fetch step 5.3
                    val locationUrl = location?.let { URL(request.parsedUrl, it) }

                    // HTTP fetch step 5.5
                    when (request.redirect) {
                        "error" -> {
                            resolver.reject(errorCreator.applyAsV8Constructor(arrayOf("redirect mode is set to error ${request.parsedUrl}", "no-redirect")))
                            //TODO: Check what to do with finalize() abort() and signal from node fetch
                            return
                        }

                        "manual" -> {
                            // node-fetch-specific step: make manual redirect a bit easier to use by setting the Location header value to the resolved URL.
                            locationUrl?.let {
                                response.headers.set("location", it.toString())
                            }
                        }

                        "follow" -> {
                            // HTTP-redirect fetch step 2
                            locationUrl?.let {

                                // HTTP-redirect fetch step 5
                                if (request.counter >= request.follow) {
                                    resolver.reject(errorCreator.applyAsV8Constructor(arrayOf("maximum redirect reached at ${request.parsedUrl}", "max-redirect")))
                                    //TODO: Check what to do with finalize() abort() and signal from node fetch
                                    return
                                }

                                // HTTP-redirect fetch step 6 (counter increment)
                                // Create a new Request object.
                                val redirectRequest = request.clone()
                                redirectRequest.parsedUrl = it
                                redirectRequest.counter++

                                // TODO:  HTTP-redirect fetch step 9
                                if (httpResponse.code() != 303 && request.body != null) {

                                }

                                // HTTP-redirect fetch step 11
                                if (httpResponse.code() == 303 || ((httpResponse.code() == 301 || httpResponse.code() == 302) && request.method == "POST")) {
                                    redirectRequest.method = "GET"
                                    redirectRequest.body?.close()
                                    redirectRequest.body = null
                                    redirectRequest.headers.delete("content-length")
                                }
                                resolver.resolve(fetch(v8Engine, arrayOf(redirectRequest)))

                                //TODO: Check what to do with finalize() abort() and signal from node fetch

                                return
                            }
                        }

                    }
                }

                // HTTP-network fetch step 12.1.1.3
                val codings = response.headers.get("content-encoding")

                // HTTP-network fetch step 12.1.1.4: handle content codings

                // in following scenarios we ignore compression support
                // 1. compression support is disabled
                // 2. HEAD request
                // 3. no Content-Encoding header
                // 4. no content response (204)
                // 5. content not modified response (304)
                if (!request.compress || request.method == "HEAD" || codings == null || httpResponse.code() == 204 || httpResponse.code() == 304) {
                    resolver.resolve(response)
                    return
                }

                //TODO: gzip
                if (codings == "gzip" || codings == "x-gzip") {
                    try {
                        response.body = GZIPInputStream(response.body)
                    } catch (e: IOException) {
                        resolver.reject(errorCreator.applyAsV8Constructor(arrayOf("Invalid response body while trying to fetch ${response.url}: ${e.message}", "system", "Z_DATA_ERROR")))
                        return
                    }
                }


                //TODO: deflate
                // handle the infamous raw deflate response from old servers
                // a hack for old IIS and Apache servers
                if (codings == "deflate" || codings == "x-deflate") {

                }
                resolver.resolve(response)
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

                const _ = Error.prepareStackTrace;
                Error.prepareStackTrace = (_, stack) => stack;
                Error.captureStackTrace(this);
                Error.prepareStackTrace = _;
            }
            FetchError.prototype = Object.create(Error.prototype);
            FetchError.prototype.constructor = FetchError;
            return FetchError;
        }())
        """

        init {
            JNIV8Object.RegisterV8Class(BGJSModuleFetchHeaders::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchResponse::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchRequest::class.java)
        }

        fun isRedirect(statusCode: Int): Boolean {
            return statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 308
        }
    }

}

