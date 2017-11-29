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
    var statusCode: Int = 0
        @V8Getter get

    constructor(engine: V8Engine) : super(engine)

    private var headers: Headers? = null

    @V8Function
    fun getResponseHeader(headerName: String): Any? {
        return headers?.get(headerName)
    }

    internal fun setReturnData(statusCode: Int, headers: Headers?) {
        this.statusCode = statusCode
        this.headers = headers
    }

}


@V8Class(creationPolicy = V8ClassCreationPolicy.JAVA_ONLY)
class BGJSModuleAjax2Request : JNIV8Object, Runnable {

    var done: JNIV8Function? = null
    @V8Getter get
    @V8Setter @V8UndefinedIsNull set

    var fail: JNIV8Function? = null
        @V8Getter get
        @V8Setter @V8UndefinedIsNull set

    var always: JNIV8Function? = null
        @V8Getter get
        @V8Setter @V8UndefinedIsNull set

    constructor(engine: V8Engine) : super(engine)


    private lateinit var url: String
    private lateinit var method: String
    private val headers: HashMap<String, String> = HashMap()
    private var body: String? = null
    private var aborted: Boolean = false

    @V8Function
    fun abort(): Boolean {
        // TODO: Why does this need a parameter?
        aborted = true
        fail?.callAsV8Function(null, "abort")
        fail?.dispose()
        done?.dispose()
        always?.callAsV8Function(null, 0, "abort")
        always?.dispose()
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

                    done?.callAsV8Function(mSuccessData, null, details)

                    always?.callAsV8Function(mSuccessData, mSuccessCode, null)
                } else {
                    var info: String? = null
                    var details: HttpResponseDetails? = null
                    if (mErrorThrowable != null) {
                        info = if (mErrorThrowable is SocketTimeoutException) "timeout" else "error"
                    } else {
                        val details = HttpResponseDetails(v8Engine)
                        details.setReturnData(mErrorCode, responseHeaders)
                    }

                    fail?.callAsV8Function(mErrorData, info, details)
                    always?.callAsV8Function(mErrorData, mErrorCode, info)
                }
                done?.dispose()
                fail?.dispose()
                always?.dispose()
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
