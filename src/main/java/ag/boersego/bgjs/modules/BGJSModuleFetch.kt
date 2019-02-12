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

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetch (val okHttpClient: OkHttpClient): JNIV8Module("fetch") {
    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        module?.setV8Field("exports", JNIV8Function.Create(engine) { receiver, arguments ->
            if (arguments == null || arguments.size < 1) {
                throw RuntimeException("fetch needs at least one argument: input")
            }
            val input = arguments[0]

            val request:BGJSModuleFetchRequest
            if (input is String) {
                val url = URL(input)
                request = BGJSModuleFetchRequest(engine)
                request.url = input
            } else if (input is BGJSModuleFetchRequest) {
                request = input
            } else {
                throw RuntimeException("fetch first argument (input) must be a string or Request object")
            }

            if (arguments.size > 1) {
                val init = arguments[1] as? JNIV8Object?
                if (init != null) {
                    request.initRequest(init, true)
                }
            }

            val resolver = JNIV8Promise.CreateResolver(engine)

            startRequest(engine, resolver, request)

            return@Create resolver.promise
        })
    }

    private fun startRequest(v8Engine: V8Engine, resolver: JNIV8Promise.Resolver, request: BGJSModuleFetchRequest) {
        v8Engine.enqueueOnNextTick {
            val httpRequest = request.execute(okHttpClient)
            try {
                okHttpClient.newCall(httpRequest).enqueue(object: Callback {
                    override fun onFailure(call: Call, e: IOException) {
                        Log.d(TAG, "onFailure", e)
                        // network error or timeout
                        resolver.resolve(BGJSModuleFetchResponse.error(v8Engine))
                    }

                    override fun onResponse(call: Call, response: Response) {
                        resolver.resolve(request.updateFrom(call, response))
                    }
                })
            } catch (e: Exception) {
                resolver.reject("error")
            }
        }
    }



    companion object {
        private val TAG = BGJSModuleFetch::class.java.simpleName
        init {
            JNIV8Object.RegisterV8Class(BGJSModuleFetchHeaders::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchResponse::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchRequest::class.java)
        }
    }

}

