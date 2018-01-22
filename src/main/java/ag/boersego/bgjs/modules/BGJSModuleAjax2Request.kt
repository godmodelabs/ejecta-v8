package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8Function
import ag.boersego.bgjs.JNIV8GenericObject
import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.V8Engine
import ag.boersego.bgjs.data.AjaxRequest
import ag.boersego.bgjs.data.V8UrlCache
import ag.boersego.v8annotations.V8Class
import ag.boersego.v8annotations.V8ClassCreationPolicy
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import android.annotation.SuppressLint
import android.util.Log
import okhttp3.FormBody
import okhttp3.Headers
import okhttp3.OkHttpClient
import java.net.SocketTimeoutException
import java.net.URLEncoder
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

    internal fun setReturnData(statusCode: Int, headers: Headers?): HttpResponseDetails {
        this.statusCode = statusCode
        this.headers = headers
        return this
    }

}


@V8Class(creationPolicy = V8ClassCreationPolicy.JAVA_ONLY)
class BGJSModuleAjax2Request : JNIV8Object, Runnable {

    @V8Function
    fun done(cb: JNIV8Function?): BGJSModuleAjax2Request {
        if (cb != null) {
            callbacks.add(Pair(CallbackType.DONE, cb))
        }
        return this
    }

    @V8Function
    fun fail(cb: JNIV8Function?): BGJSModuleAjax2Request {
        if (cb != null) {
        callbacks.add(Pair(CallbackType.FAIL, cb))
    }
        return this
    }

    @V8Function
    fun always(cb: JNIV8Function?): BGJSModuleAjax2Request {
        if (cb != null) {
            callbacks.add(Pair(CallbackType.ALWAYS, cb))
        }
        return this
    }

    private var callbacks = ArrayList<Pair<CallbackType, JNIV8Function>>()

    constructor(engine: V8Engine) : super(engine)

    enum class CallbackType {
        DONE,
        FAIL,
        ALWAYS
    }


    private lateinit var url: String
    private lateinit var method: String
    private val headers: HashMap<String, String> = HashMap(Companion.httpAdditionalHeaders)
    private var body: String? = null
    private var aborted: Boolean = false
    private var outputType: String? = null

    @V8Function
    fun abort(): Boolean {
        // TODO: Why does this need a parameter?
        aborted = true
        callbacks.forEach { cb ->
            if (cb.first != CallbackType.DONE) {
                cb.second.callAsV8Function(null, "abort")
            }
            cb.second.dispose()
        }
        return true
    }

    override fun run() {
        val request = object : AjaxRequest(url, body, null, method) {
            @SuppressLint("LogNotTimber")
            override fun run() {
                super.run()

                if (aborted) {
                    return
                }

                var contentType = responseHeaders?.get("Content-Type");
                _responseIsJson = contentType?.startsWith("application/json") ?: false

                // TODO: Cookie handling

                if (mSuccessData != null) {
                    // Retrieve any cookies

                    val details = HttpResponseDetails(v8Engine)
                    details.setReturnData(mSuccessCode, responseHeaders)

                    if (_responseIsJson) {
                        try {
                            var parsedResponse = v8Engine.parseJSON(mSuccessData)
                            callCallbacks(CallbackType.DONE, parsedResponse, null, details, mSuccessCode)
                        } catch (e: Exception) {
                            // Call fail callback with parse errors
                            var failDetails = HttpResponseDetails(v8Engine).setReturnData(mSuccessCode, responseHeaders)
                            callCallbacks(CallbackType.FAIL, mSuccessData, "parseerror", failDetails, mErrorCode)
                        }

                    } else {
                        callCallbacks(CallbackType.DONE, mSuccessData, null, details, mSuccessCode)
                    }

                } else {
                    var info: String? = null
                    var failDetails: HttpResponseDetails? = null
                    if (mErrorThrowable != null) {
                        info = if (mErrorThrowable is SocketTimeoutException) "timeout" else "error"
                    } else {
                        failDetails = HttpResponseDetails(v8Engine)
                        failDetails.setReturnData(mErrorCode, responseHeaders)
                    }
                    Log.d(TAG, "Error code " + mErrorCode + ", info " + info + ", body " + mErrorData)
                    val errorObj = JNIV8GenericObject.Create(v8Engine)
                    if (_responseIsJson && mErrorData != null) {
                        try {
                            var parsedResponse = v8Engine.parseJSON(mErrorData)
                            errorObj.setV8Field("responseJSON", parsedResponse)
                            callCallbacks(CallbackType.FAIL, errorObj, null, failDetails, mErrorCode)
                        } catch (e: Exception) {
                            // Call fail callback with parse errors
                            errorObj.setV8Field("responseJSON", mErrorData)
                            callCallbacks(CallbackType.FAIL, errorObj, "parseerror", failDetails, mErrorCode)
                        }
                    } else {
                        val returnObject = mErrorData ?: JNIV8GenericObject.Create(v8Engine)
                        callCallbacks(CallbackType.FAIL, returnObject, info, failDetails, mErrorCode)
                    }
                }
            }
        }
        request.connectionTimeout = 30000
        request.setCacheInstance(cache)
        request.setHeaders(headers)
        request.setHttpClient(client)
        if (formBody != null) {
            request.setFormBody(formBody!!)
        }
        if (outputType != null) {
            request.setOutputType(outputType)
        }
        executor.execute(request)
    }

    private fun callCallbacks(type: CallbackType, returnObject: Any?, info: String?, details: HttpResponseDetails?, errorCode: Int) {
        for (cb in callbacks) {
            try {
                when (cb.first) {
                    CallbackType.ALWAYS ->
                            cb.second.callAsV8Function(returnObject, errorCode, info)
                    type ->
                            cb.second.callAsV8Function(returnObject, info, details)
                }
            } catch (e: Exception) {
                Log.e(TAG, "Exception thrown when calling ajax " + cb.first + " callback", e)
            }
            cb.second.dispose()
        }
    }

    private lateinit var cache: V8UrlCache

    private lateinit var client: OkHttpClient

    private lateinit var executor: ThreadPoolExecutor

    private var _responseIsJson: Boolean = false

    private var formBody: FormBody? = null

    fun setData(url: String, method: String, headerRaw: JNIV8GenericObject?, body: Any?,
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

                if (key == "Content-Type") {
                    outputType = headers[key]
                }
            }
        }

        if (outputType == null && body != null) {
            outputType = "application/x-www-form-urlencoded; charset=UTF-8"
        }

        if (method == "POST") {
            val requestIsJson = outputType?.startsWith("application/json")
            if (requestIsJson == true) {
                if (body is String) {
                    this.body = body
                } else {
                    this.body = v8Engine.getGlobalObject().getV8FieldTyped("JSON", JNIV8Object::class.java)!!.callV8Method("stringify", body) as String
                }
            } else {
                // We're sending a form object
                if (body is JNIV8Object && requestIsJson != true) {
                    val formBody = FormBody.Builder()
                    val bodyMap = body.v8Fields

                    for (entry in bodyMap.entries) {
                        formBody.add(entry.key, entry.value as String)
                    }
                    this.formBody = formBody.build()
                } else if (body is String) {
                    this.body = body;
                } else {
                    Log.w(TAG, "Cannot set body type " + body)
                }
            }
        } else {
            if (body is JNIV8Object) {
                val bodyMap = body.v8Fields
                if (!bodyMap.isEmpty()) {
                    val urlBuilder = StringBuilder(url)
                    urlBuilder.append("?")
                    var isFirstItem = true
                    val entries = bodyMap.entries
                    for (entry in entries) {
                        if (!isFirstItem) {
                            urlBuilder.append("&")
                        }
                        isFirstItem = false

                        urlBuilder.append(URLEncoder.encode(entry.key, "UTF-8"))
                        urlBuilder.append("=")
                        urlBuilder.append(URLEncoder.encode(entry.value as String, "UTF-8"))
                    }
                    this.url = urlBuilder.toString()
                }

            }
        }

    }

    companion object {
        private var httpAdditionalHeaders: HashMap<String, String>
        val TAG = BGJSModuleAjax2Request::class.java.simpleName

        init {
            JNIV8Object.RegisterV8Class(BGJSModuleAjax2Request::class.java)
            JNIV8Object.RegisterV8Class(HttpResponseDetails::class.java)
            httpAdditionalHeaders = hashMapOf("Accept-Charset" to "utf-8",
                    "Accept-Language" to "en-US",
                    "Cache-Control" to "no-cache, no-store",
                    "Connection" to "keep-alive")
        }
    }
}
