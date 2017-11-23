package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8Function
import ag.boersego.bgjs.JNIV8GenericObject
import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.V8Engine
import ag.boersego.bgjs.data.AjaxRequest
import ag.boersego.bgjs.data.V8UrlCache
import ag.boersego.v8annotations.*
import okhttp3.Headers
import okhttp3.OkHttpClient
import java.net.SocketTimeoutException
import java.util.concurrent.ThreadPoolExecutor

/**
 * Created by Kevin Read <me></me>@kevin-read.com> on 09.11.17 for myrmecophaga-2.0.
 * Copyright (c) 2017 ${ORGANIZATION_NAME}. All rights reserved.
 */

@V8Class(creationPolicy = V8ClassCreationPolicy.JAVA_ONLY)
class HttpResponseDetails : JNIV8Object {
    var _statusCode: Int = 0
    var statusCode: Any?
        @V8Getter
        get() = _statusCode
        @V8Setter
        set(input) {
            return
        }

    constructor(engine: V8Engine) : super(engine)

    private var headers: Headers? = null

    @V8Function
    fun getResponseHeader(args: Array<Any>): Any? {
        if (args.size < 1 || !(args[0] is String)) {
            throw IllegalArgumentException("getResponseHeader wants one String argument")
        }
        return headers?.get(args[0] as String)
    }

    internal fun setReturnData(statusCode: Int, headers: Headers?) {
        this._statusCode = statusCode
        this.headers = headers
    }

}


@V8Class(creationPolicy = V8ClassCreationPolicy.JAVA_ONLY)
class BGJSModuleAjax2Request : JNIV8Object, Runnable {

    private var _done: JNIV8Function? = null
    private var _fail: JNIV8Function? = null
    private var _always: JNIV8Function? = null

    var done: Any?
        @V8Getter
        get() = _done
        @V8Setter
        set(input) {
            if (input is JNIV8Function) {
                _done = input
            } else {
                throw IllegalArgumentException("done must be a function")
            }
        }

    var fail: Any?
        @V8Getter
        get() = _fail
        @V8Setter
        set(input) {
            if (input is JNIV8Function) {
                _fail = input

            } else {
                throw IllegalArgumentException("done must be a function")
            }
        }

    var always: Any?
        @V8Getter
        get() = _always
        @V8Setter
        set(input) {
            if (input is JNIV8Function) {
                _always = input

            } else {
                throw IllegalArgumentException("done must be a function")
            }
        }

    constructor(engine: V8Engine) : super(engine)


    private lateinit var url: String
    private lateinit var method: String
    private val headers: HashMap<String, String> = HashMap()
    private var body: String? = null
    private var aborted: Boolean = false

    @V8Function
    fun abort(args: Array<Any>): Any? {
        // TODO: Why does this need a parameter?
        aborted = true
        _fail?.callAsV8Function(null, "abort")
        _fail?.dispose()
        _done?.dispose()
        _always?.callAsV8Function(null, 0, "abort")
        _always?.dispose()
        return true
    }

    override fun run() {
        val request = object : AjaxRequest(url, body, null, method) {
            override fun run() {
                super.run()

                if (aborted) {
                    return
                }

                if (mSuccessData != null) {
                    val details = HttpResponseDetails(v8Engine)
                    details.setReturnData(mSuccessCode, responseHeaders)

                    _done?.callAsV8Function(mSuccessData, null, details)

                    _always?.callAsV8Function(mSuccessData, mSuccessCode, null)
                } else {
                    var info: String? = null
                    var details: HttpResponseDetails? = null
                    if (mErrorThrowable != null) {
                        info = if (mErrorThrowable is SocketTimeoutException) "timeout" else "error"
                    } else {
                        val details = HttpResponseDetails(v8Engine)
                        details.setReturnData(mErrorCode, responseHeaders)
                    }

                    _fail?.callAsV8Function(mErrorData, info, details)
                    _always?.callAsV8Function(mErrorData, mErrorCode, info)
                }
                _done?.dispose()
                _fail?.dispose()
                _always?.dispose()
            }
        }
        request.setCacheInstance(cache)
        request.setHeaders(headers)
        request.setHttpClient(client)
        executor.execute(request)
    }

    private lateinit var cache: V8UrlCache

    private lateinit var client: OkHttpClient

    private lateinit var executor: ThreadPoolExecutor

    fun setData(url: String, method: String, headerRaw: JNIV8GenericObject?, body: String?,
                cache: V8UrlCache, client: OkHttpClient, executor: ThreadPoolExecutor) {
        this.url = url;
        this.method = method;
        this.cache = cache
        this.client = client
        this.executor = executor

        if (headerRaw != null) {
            val fields = headerRaw.v8Fields
            for ((key, value) in fields) {
                if (value is String) {
                    headers[key] = value
                } else {
                    headers[key] = value.toString()
                }
            }
        }
        this.body = body;

    }

    companion object {
        init {
            JNIV8Object.RegisterV8Class(BGJSModuleAjax2Request::class.java)
            JNIV8Object.RegisterV8Class(HttpResponseDetails::class.java)
        }
    }
}
