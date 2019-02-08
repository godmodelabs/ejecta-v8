package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.fetch.BGJSModuleFetchRequest
import java.net.URL

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetch : JNIV8Module("fetch") {
    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        val exports = JNIV8GenericObject.Create(engine)

        // exports.setV8Field("fetch", )

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
                    initRequest(request, init)
                }

            }

            return@Create request
        })
    }

    private fun initRequest(request: BGJSModuleFetchRequest, init: JNIV8Object) {
        val fields = init.v8Fields
        request.method = fields["method"] as? String? ?: "GET"

        if (fields.containsKey("headers")) {
            val headerRaw = fields["headers"]
            if (headerRaw is BGJSModuleFetchHeaders) {
                request.headers = headerRaw
            } else if(headerRaw is JNIV8Object) {
                request.headers = BGJSModuleFetchHeaders.createFrom(headerRaw)
            } else {
                throw RuntimeException("TypeError: init.headers is not an object of Headers instance")
            }
        }

        if (fields.containsKey("body")) {
            val bodyRaw = fields["body"]

            if (bodyRaw is String) {
                request.body = bodyRaw
            } else {
                // TODO: implement FormData, Buffer, URLSearchParams
                throw RuntimeException("TypeError: fetch init.body must be of a valid type")
            }
        }
        request.mode = fields["mode"] as? String? ?: "omit"
    }

    companion object {
        init {
            JNIV8Object.RegisterV8Class(BGJSModuleFetchHeaders::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchResponse::class.java)
            JNIV8Object.RegisterV8Class(BGJSModuleFetchRequest::class.java)
        }
    }

}

