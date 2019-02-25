package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.fetch.BGJSModuleFetchRequest
import android.util.Log
import okhttp3.Call
import okhttp3.Callback
import okhttp3.OkHttpClient
import okhttp3.Response
import java.io.IOException
import java.net.URL
import java.util.concurrent.TimeUnit
import java.util.zip.GZIPInputStream

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetch (val okHttpClient: OkHttpClient): JNIV8Module("fetch") {

    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        module?.setV8Field("exports", JNIV8Function.Create(engine) { receiver, arguments ->
          return@Create fetch(engine, arguments)
        })
    }

    fun fetch(engine: V8Engine, arguments: Array<Any>): JNIV8Promise {
        if (arguments == null || arguments.size < 1) {
            throw V8JSException(engine, "TypeError", "fetch needs at least one argument: input")
        }
        val fetchRequest = BGJSModuleFetchRequest(v8Engine = engine, args = arguments)

        val resolver = JNIV8Promise.CreateResolver(engine)

        startRequest(engine, resolver, fetchRequest)

        return resolver.promise
    }

    private fun startRequest(v8Engine: V8Engine, resolver: JNIV8Promise.Resolver, request: BGJSModuleFetchRequest) {
        val httpRequest = request.execute()
        try {
            val call = okHttpClient.newCall(httpRequest)
            val timeout = call.timeout()
            timeout.timeout(request.timeout.toLong(), TimeUnit.MILLISECONDS)
            call.enqueue(object: Callback {

                override fun onFailure(call: Call, e: IOException) {
                    Log.d(TAG, "onFailure", e)
                    // network error or timeout TODO: Shouldn't we reject here?
                    resolver.resolve(BGJSModuleFetchResponse.error(v8Engine))
                }

                override fun onResponse(call: Call, httpResponse: Response) {
                    timeout.clearTimeout()
                    val response = BGJSModuleFetchResponse.createFrom(v8Engine, httpResponse)

                    // HTTP fetch step 5
                    if (isRedirect(response.status)) {
                        // HTTP fetch step 5.2
                        val location = response.headers.get("location")

                        // HTTP fetch step 5.3
                        val locationUrl = location?.let{URL(request.url, it)}

                        // HTTP fetch step 5.5
                        when (request.redirect) {
                            "error" -> {
                                resolver.reject(JNIV8Object.Create(v8Engine, "Error", "redirect mode is set to error ${request.url} , no-redirect"))
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
                                        resolver.reject(JNIV8Object.Create(v8Engine, "Error", "maximum redirect reached at ${request.url} , max-redirect"))
                                        //TODO: Check what to do with finalize() abort() and signal from node fetch
                                        return
                                    }

                                    // HTTP-redirect fetch step 6 (counter increment)
                                    // Create a new Request object.
                                    val redirectRequest = request.clone()
                                    redirectRequest.url = it
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
                        response.body = GZIPInputStream(response.body)
                    }


                    //TODO: deflate
                    // handle the infamous raw deflate response from old servers
                    // a hack for old IIS and Apache servers
                    if (codings == "deflate" || codings == "x-deflate") {

                    }
                    resolver.resolve(response)
                }
            })
        } catch (e: Exception) {
            resolver.reject(JNIV8Object.Create(v8Engine, "Error", "Error parsing data"))
        }
    }


    companion object {
        private val TAG = BGJSModuleFetch::class.java.simpleName
        init {
            JNIV8Object.RegisterV8Class(BGJSModuleFetchHeaders::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchResponse::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchRequest::class.java)
        }

        fun isRedirect(statusCode: Int): Boolean {
            return statusCode == 301 || statusCode == 302 || statusCode == 303 || statusCode == 307 || statusCode == 300
        }
    }

}

