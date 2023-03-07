package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.*
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import ag.boersego.v8annotations.V8Symbols
import okhttp3.MediaType.Companion.toMediaTypeOrNull
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.InputStream
import java.net.MalformedURLException
import java.net.URL

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetchRequest @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : BGJSModuleFetchBody(v8Engine, jsPtr, args) {

    val toString = "Request"
        @V8Getter(symbol = V8Symbols.TO_STRING_TAG) get

    var cache = "default"
        internal set(value) {
            if (value == "default" || value == "no-store" || value == "reload" || value == "no-cache"
                || value == "force-cache" || value == "only-if-cached") {
                field = value
            } else {
                throw V8JSException(v8Engine, "TypeError", "invalid cache: '$value'")
            }
        }
        @V8Getter get

    lateinit var headers: BGJSModuleFetchHeaders
        internal set
        @V8Getter get

    override var url = ""
        @V8Getter get

    /**
     * Contains the request's method (GET, POST, etc.)
     */
    var method = "GET"
        internal set
        @V8Getter get


    /**
     * Contains the mode for how redirects are handled. It may be one of follow, error, or manual.
     * "follow" = Follow all redirects incurred when fetching a resource.
     * "error" = Return a network error when a request is met with a redirect.
     * "manual" = Retrieves an opaque-redirect filtered response when a request is met with a redirect so that the redirect can be followed manually.
     */
    var redirect = "follow"
        internal set(value) {
            if (value == "follow" || value == "error" || value == "manual") {
                field = value
            } else {
                throw V8JSException(v8Engine, "TypeError", "invalid redirect: '$value'")
            }
        }
        @V8Getter get

    // The following properties are node-fetch extensions

    /**
     * maximum redirect count. 0 to not follow redirect
     */
    var follow = 20
        internal set
        @V8Getter get

    /**
     * current redirect count.
     */
    var counter = 0
        internal set
        @V8Getter get

    /**
     * AbortSignal
     */
    var signal: BGJSModuleAbortSignal? = null
        internal set
        @V8Getter get

    /**
     * req/res timeout in seconds, it resets on redirect. 0 to disable (OS limit applies). Signal is recommended instead.
     */
    var timeout = 0
        internal set
        @V8Getter get

    /**
     * support gzip/deflate content encoding. false to disable
     */
    var compress = true
        internal set
        @V8Getter get

    /**
     * maximum response body size in bytes. 0 to disable
     */
    var size = 0
        internal set
        @V8Getter get

    // agent: null         // http(s).Agent instance, allows custom proxy, certificate, dns lookup etc.

    init {
        if (args != null) {
            // Was constructed in JS, parse parameters
            if (args.isEmpty()) {
                throw V8JSException(v8Engine, "TypeError", "Failed to construct 'Request': 1 argument required, but only 0 present.")
            } else if (args.size > 2) {
                throw V8JSException(v8Engine, "TypeError", "Request needs one or two parameters: Request(input, init)")
            } else {
                setDefaultHeaders()
                val input = args[0]
                if (input is String) {
                    try {
                        parsedUrl = URL(input)
                        if (parsedUrl.userInfo != null) {
                            throw V8JSException(v8Engine, "TypeError", "Request input url cannot have credentials")
                        }
                        url = parsedUrl.toString()
                    } catch (e: MalformedURLException) {
                        throw V8JSException(v8Engine, "TypeError", "Only absolute URLs are supported")
                    }
                } else if (input is BGJSModuleFetchRequest) {
                    this.applyFrom(input)
                }
            }

            if (args.size > 1 && args[1] !is JNIV8Undefined) {
                if (args[1] !is JNIV8GenericObject) {
                    throw V8JSException(v8Engine, "TypeError", "Request init must be an object")
                }

                val init = args[1] as JNIV8GenericObject
                initRequest(init, false)

                if (body != null && (method == "GET" || method == "HEAD")) {
                    throw V8JSException(v8Engine, "TypeError", "Cannot have body with GET or HEAD")
                }
            }
        }
    }

    private fun initRequest(init: JNIV8Object, isFromFetch: Boolean) {
        val fields = init.v8Fields
        // When initializing a request from init, set some defaults first
        //TODO: do we need this?
        if (isFromFetch) {
            method = if (fields[KEY_METHOD] == "navigate") {
                "same-origin"
            } else {
                fields[KEY_METHOD] as? String? ?: ""
            }
        }

        method = fields[KEY_METHOD] as? String? ?: "GET"

        if (fields.containsKey(KEY_HEADERS)) {
            val headerRaw = fields[KEY_HEADERS]
            headers = when (headerRaw) {
                is BGJSModuleFetchHeaders -> headerRaw.clone()
                is JNIV8Object            -> BGJSModuleFetchHeaders.createFrom(headerRaw)
                else                      -> throw V8JSException(v8Engine, "TypeError", "init.headers is not an object or Header instance")
            }
        }

        if (fields.containsKey(KEY_SIGNAL) && fields[KEY_SIGNAL] is BGJSModuleAbortSignal) {
            signal = fields[KEY_SIGNAL] as BGJSModuleAbortSignal
        }

        if (fields.containsKey(KEY_BODY)) {
            val bodyRaw = fields[KEY_BODY]
            body = createBodyFromRaw(bodyRaw)
            if (!headers.has("content-type")) {
                extractContentType(bodyRaw)?.let {
                    headers.set("content-type", it)
                }
            }
        }

        redirect = fields[KEY_REDIRECT] as? String? ?: redirect
        follow = (fields[KEY_FOLLOW] as? Double)?.toInt() ?: follow
        timeout = (fields[KEY_TIMEOUT] as? Double)?.toInt() ?: timeout
        compress = fields[KEY_COMPRESS] as? Boolean? ?: compress
        size = (fields[KEY_SIZE] as? Double)?.toInt() ?: size
    }

    private fun setDefaultHeaders() {
        // node-fetch sets these default headers, so we will do the same
        headers = BGJSModuleFetchHeaders(v8Engine)
        headers.set("accept", "*/*")
        if (compress) {
            headers.append("accept-encoding", "gzip,deflate")
        }
        headers.set("connection", "close")
        if (body != null) {
            headers.set("transfer-encoding", "chunked")
        }
    }
    /**
     * The clone() method of the Request interface creates a copy of the current Request object.
     *
     * clone() throws a TypeError if the response Body has already been used. In fact, the main reason clone()
     * exists is to allow multiple uses of Body objects (when they are one-use only.)
     * @return cloned request object
     */
    @V8Function
    fun clone(): BGJSModuleFetchRequest {
        if (bodyUsed) {
            throw V8JSException(v8Engine, "TypeError", "cannot clone because body already use")
        }
        val clone = BGJSModuleFetchRequest(v8Engine)
        clone.applyFrom(this)
        return clone
    }

    private fun applyFrom(other: BGJSModuleFetchRequest) {
        parsedUrl = other.parsedUrl
        url = other.parsedUrl.toString()
        headers = other.headers.clone()
        method = other.method
        redirect = other.redirect
        follow = other.follow
        signal = other.signal
        timeout = other.timeout
        compress = other.compress
        size = other.size
        counter = other.counter
        if(other.body != null) body = other.cloneBody()
    }

    private fun cloneBody(): InputStream {
        val byteOutputStream = ByteArrayOutputStream()
        body?.use { input ->
            byteOutputStream.use { output ->
                input.copyTo(output)
                input.reset()
            }
        }
        return ByteArrayInputStream(byteOutputStream.toByteArray())
    }

    fun execute(): Request {
        val builder = Request.Builder()
        try {
            builder.url(parsedUrl)
        } catch (e: IllegalArgumentException) {
            throw V8JSException(v8Engine, "TypeError", "Only HTTP(S) protocols are supported")
        }

        // Origin header set for Android App (differs from fetch spec)

        // Idea: see what node-fetch does here: https://github.com/bitinn/node-fetch/blob/master/src/request.js

        // Chapter 4: Fetch
        // 3. If request’s header list does not contain `Accept`, then
        if (!headers.has("accept")) {
            headers.append("accept", "*/*")
        }

        if (parsedUrl.protocol.isEmpty() || parsedUrl.host.isEmpty()) {

        }
        headers.applyToRequest(builder)

        body?.let {
            // Only set a body if we know it's content-type. This is also derived from how the body is set
            val contentType = headers.get("content-type") ?: throw V8JSException(v8Engine, "TypeError", "cannot have body without knowing content-type")
            val mediaType = contentType.toMediaTypeOrNull() ?: throw V8JSException(v8Engine, "cannot encode body with unknown content-type '$contentType'")
            builder.method(method, it.readBytes().toRequestBody(mediaType))
            it.reset()
        }?: if (method == "POST") builder.method(method, ByteArray(0).toRequestBody()) else builder.method(method, null)

        return builder.build()
    }

    private fun doHttpNetworkOrCacheFetch(builder: Request.Builder) {
        // See whatwg spec: chapter 4.5

        // 12. If httpRequest’s cache mode is "default" and httpRequest’s header list contains `If-Modified-Since`, `If-None-Match`, `If-Unmodified-Since`, `If-Match`, or `If-Range`, then set httpRequest’s cache mode to "no-store"
        if (cache == "default" && (headers.has("if-modified-since") || headers.has("if-none-match") || headers.has("If-Unmodified-Since") || headers.has("if-match") || headers.has("if-range"))) {
            cache = "no-store"
        }

        // 13. If httpRequest’s cache mode is "no-cache" and httpRequest’s header list does not contain `Cache-Control`, then append `Cache-Control`/`max-age=0` to httpRequest’s header list.
        if (cache == "no-cache" && !headers.has("cache-control")) {
            headers.set("cache-control", "max-age=0")
        }

        // 14. If httpRequest’s cache mode is "no-store" or "reload", then:
        if (cache == "no-store" || cache == "reload") {
            // If httpRequest’s header list does not contain `Pragma`, then append `Pragma`/`no-cache` to httpRequest’s header list.
            if (!headers.has("pragma")) {
                headers.set("pragma", "no-cache")
            }

            // If httpRequest’s header list does not contain `Cache-Control`, then append `Cache-Control`/`no-cache` to httpRequest’s header list.
            if (!headers.has("cache-control")) {
                headers.set("cache-control", "no-cache")
            }
        }

        // 15. If httpRequest’s header list contains `Range`, then append `Accept-Encoding`/`identity` to httpRequest’s header list.
        if (headers.has("range")) {
            headers.set("accept-encoding", "identity")
        }

        // TODO: cookies
        // test okhttp cookie jar

        // TODO: 19. If httpRequest’s cache mode is neither "no-store" nor "reload", then:
        // Set storedResponse to the result of selecting a response from the HTTP cache, possibly needing validation, as per the "Constructing Responses from Caches" chapter of HTTP Caching [HTTP-CACHING], if any.
        // If storedResponse is non-null, then:
        // If response is null: If httpRequest’s cache mode is "only-if-cached", then return a network error.
        // If httpRequest’s method is unsafe and forwardResponse’s status is in the range 200 to 399, inclusive, invalidate appropriate stored responses in the HTTP cache, as per the "Invalidation" chapter of HTTP Caching, and set storedResponse to null. [HTTP-CACHING]

        headers.applyToRequest(builder)

        val body = body
        if (body == null) {
            builder.method(method, null)
        } else {
            // Only set a body if we know it's content-type. This is also derived from how the body is set
            val contentType = headers.get("content-type") ?: throw V8JSException(v8Engine, "TypeError", "cannot have body without knowing content-type")

            val mediaType = contentType.toMediaTypeOrNull() ?: throw V8JSException(v8Engine, "TypeError", "cannot encode body with unknown content-type '$contentType'")
            builder.method(method, body.readBytes().toRequestBody(mediaType))
        }
    }

    /**
     * The create a fetch response from completed okhttp response
     */
    fun updateFrom(v8Engine: V8Engine, httpResponse: Response): BGJSModuleFetchResponse {
        val response = BGJSModuleFetchResponse(v8Engine)
        response.headers = BGJSModuleFetchHeaders.createFrom(v8Engine, httpResponse.headers)
        response.url = parsedUrl.toString()
        response.status = httpResponse.code
        response.statusText = httpResponse.message
        response.body = httpResponse.body?.byteStream()
        response.redirect = httpResponse.isRedirect

        return response
    }

    // Since ejecta-v8 currently cannot register abstract classes we have to override these methods here and just call super
    override var bodyUsed = false
        @V8Getter get

    @V8Function
    override fun arrayBuffer(): JNIV8Promise {
        return super.arrayBuffer()
    }

    @V8Function
    override fun json(): JNIV8Promise {
        return super.json()
    }

    @V8Function
    override fun text(): JNIV8Promise {
        return super.text()
    }

    companion object {
        const val KEY_METHOD = "method"
        const val KEY_HEADERS = "headers"
        const val KEY_BODY = "body"
        const val KEY_CACHE = "cache"
        const val KEY_REDIRECT = "redirect"
        const val KEY_FOLLOW = "follow"
        const val KEY_SIGNAL = "signal"
        const val KEY_TIMEOUT = "timeout"
        const val KEY_COMPRESS = "compress"
        const val KEY_SIZE = "size"
    }
}