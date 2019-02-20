package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.*
import ag.boersego.bgjs.modules.BGJSModuleFetchBody
import ag.boersego.bgjs.modules.BGJSModuleFetchHeaders
import ag.boersego.bgjs.modules.BGJSModuleFetchResponse
import ag.boersego.v8annotations.V8Flags
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import okhttp3.*
import java.net.MalformedURLException
import java.net.URL

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetchRequest @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : BGJSModuleFetchBody(v8Engine, jsPtr, args) {

    private lateinit var parsedUrl: URL

    private var fallbackMode: String? = null

    private var fallbackCredentials = "omit"

    private var hasKeepAlive = false

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

    var context = ""
        internal set
        @V8Getter get

    /**
     * Contains the subresource integrity value of the request (e.g., sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=).
     */
    var integrity = ""
        internal set
        @V8Getter get

    var credentials = "omit"
        internal set(value) {
            if (value == "omit" || value == "same-origin" || value == "include") {
                field = value
            } else {
                throw RuntimeException("invalid credentials: '$value'")
            }
        }
        @V8Getter get

    var destination = ""
        internal set
        @V8Getter get

    var headers: BGJSModuleFetchHeaders? = null
        internal set
        @V8Getter get

    /**
     * Contains the request's method (GET, POST, etc.)
     */
    var method = "GET"
        internal set
        @V8Getter get
    /**
     * Contains the mode of the request (e.g., cors, no-cors, same-origin, navigate.)
     */
    var mode = "no-cors"
        internal set(value) {
            if (value == "same-origin" || value == "cors" || value == "no-cors" || value == "navigate" || value == "websocket") {
                field = value
            } else {
                throw RuntimeException("invalid mode: '$value'")
            }
        }
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
                throw RuntimeException("invalid redirect: '$value'")
            }
        }
        @V8Getter get

    /**
     * Contains the referrer of the request (e.g., client).
     */
    var referrer = "client"
        internal set
        @V8Getter get

    var referrerPolicy = ""
        internal set
        @V8Getter get

    var url = ""
        internal set
        @V8Getter get

    // The following properties are node-fetch extensions
    /**
     * maximum redirect count. 0 to not follow redirect
     */
    var follow = 20
        internal set
        @V8Getter get

    /**
     * req/res timeout in ms, it resets on redirect. 0 to disable (OS limit applies). Signal is recommended instead.
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
            if (args.size > 2) {
                throw RuntimeException("TypeError: Request needs no, one or two parameters: Request(input, init)")
            }
            if (args.size > 0) {
                val input = args[0]
                if (input is String) {
                    try {
                        parsedUrl = URL(input)
                        if (parsedUrl.userInfo != null) {
                            throw RuntimeException("TypeError: Request input url cannot have credentials")
                        }
                        this.fallbackMode = "cors"
                        this.fallbackCredentials = "same-origin"
                    } catch (e: MalformedURLException) {
                        throw RuntimeException("TypeError: Request input is invalid url '$input'", e)
                    }
                } else if (input is BGJSModuleFetchRequest) {
                    this.applyFrom(input)
                }
            }

            if (args.size > 1 && args[1] !is JNIV8Undefined) {
                if (args[1] !is JNIV8GenericObject) {
                    throw RuntimeException("TypeError: Request init must be an object")
                }

                val init = args[1] as JNIV8GenericObject
                initRequest(init, false)

                if (body != null && (method == "GET" || method == "HEAD")) {
                    throw RuntimeException("TypeError: Cannot have body with GET or HEAD")
                }
            }
        }
    }

    internal fun initRequest(init: JNIV8Object, isFromFetch: Boolean) {
        val fields = init.v8Fields
        // When initializing a request from init, set some defaults first
        if (isFromFetch) {
            referrer = "client"
            referrerPolicy = ""
            if (fields[KEY_METHOD] == "navigate") {
                method = "same-origin"
            } else {
                method = fields[KEY_METHOD] as? String? ?: ""
            }
        }

        method = fields[KEY_METHOD] as? String? ?: "GET"

        if (fields.containsKey(KEY_HEADERS)) {
            val headerRaw = fields[KEY_HEADERS]
            if (headerRaw is BGJSModuleFetchHeaders) {
                headers = headerRaw.clone()
            } else if(headerRaw is JNIV8Object) {
                headers = BGJSModuleFetchHeaders.createFrom(headerRaw)
            } else {
                throw V8JSException(v8Engine, "TypeError", "init.headers is not an object or Header instance")
            }
        }

        // node-fetch sets these default headers, so we will do the same
        if (headers == null) {
            headers = BGJSModuleFetchHeaders(v8Engine)
            headers?.set("accept", "*/*")
            if (compress) {
                headers?.append("accept-encoding", "gzip,deflate")
            }
            headers?.set("connection", "close")
            if (body != null) {
                headers?.set("transfer-encoding", "chunked")
            }
        }

        if (headers?.has("user-agent") == false) {
            headers?.set("user-agent", "ejecta-v8")
        }

        if (fields.containsKey(KEY_BODY)) {
            val bodyRaw = fields[KEY_BODY]
            body = BGJSModuleFetchBody.createBodyFromRaw(bodyRaw)
            if (headers?.has("content-type") == false) {
                BGJSModuleFetchBody.extractContentType(bodyRaw)?.let {
                    headers?.set("content-type", it)
                }
            }
        }

        // This is not according to spec, but we don't lay restrictions on referrer
        referrer = fields[KEY_REFERRER] as? String? ?: referrer

        referrerPolicy = fields[KEY_REFERRER_POLICY] as? String? ?: referrerPolicy

        val newMode = fields[KEY_MODE] as? String? ?: fallbackMode

        if (newMode == "navigate") {
            throw RuntimeException("TypeError: mode navigate not allowed")
        }

        if (newMode != null) {
            mode = newMode
        }

        credentials = fields[KEY_CREDENTIALS] as? String? ?: fallbackCredentials

        cache = fields[KEY_CACHE] as? String? ?: cache

        redirect = fields[KEY_REDIRECT] as? String? ?: redirect

        integrity = fields[KEY_INTEGRITY] as? String? ?: integrity

        val setKeepAlive = init.getV8FieldTyped<Boolean>(KEY_KEEPALIVE, V8Flags.UndefinedIsNull, Boolean::class.java)
        if (setKeepAlive != null) {
            hasKeepAlive = setKeepAlive
        }

        // TODO: this is probably complete, check spec
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
            throw RuntimeException("TypeError: cannot clone because body already used")
        }
        val clone = BGJSModuleFetchRequest(v8Engine)
        clone.cache = cache
        clone.context = context
        clone.credentials = credentials
        clone.destination = destination
        clone.headers = headers?.clone()
        clone.method = method
        clone.mode = mode
        clone.redirect = redirect
        clone.referrer = referrer
        clone.referrerPolicy = referrerPolicy
        clone.url = url

        return clone
    }

    private fun applyFrom(other: BGJSModuleFetchRequest) {
        cache = other.cache
        context = other.context
        credentials = other.credentials
        destination = other.destination
        headers = other.headers?.clone()
        method = other.method
        mode = other.mode
        redirect = other.redirect
        referrer = other.referrer
        referrerPolicy = other.referrerPolicy
        url = other.url
    }

    fun execute(okHttpClient: OkHttpClient): Request {
        val builder = Request.Builder().url(url)

        // Idea: see what node-fetch does here: https://github.com/bitinn/node-fetch/blob/master/src/request.js

        // Chapter 4: Fetch
        // 3. If request’s header list does not contain `Accept`, then
        if (headers?.has("accept") == false) {
            headers?.append("accept", "*/*")
        }

        // 4.1. Main fetch

        // 5. If response is null, then set response to the result of running the steps corresponding to the first matching statement:
        // TODO: request’s mode is "navigate" or "websocket"
        // TODO: request’s mode is "no-cors"
        // TODO: decide which scheme fetch to do


        return builder.build()
    }

    private fun doHttpNetworkOrCacheFetch(builder: Request.Builder) {
        // See whatwg spec: chapter 4.5

        // 12. If httpRequest’s cache mode is "default" and httpRequest’s header list contains `If-Modified-Since`, `If-None-Match`, `If-Unmodified-Since`, `If-Match`, or `If-Range`, then set httpRequest’s cache mode to "no-store"
        if (cache == "default" &&
            (headers?.has("if-modified-since") == true || headers?.has("if-none-match") == true
                    || headers?.has("If-Unmodified-Since") == true || headers?.has("if-match") == true || headers?.has("if-range") == true)) {
            cache = "no-store"
        }

        // 13. If httpRequest’s cache mode is "no-cache" and httpRequest’s header list does not contain `Cache-Control`, then append `Cache-Control`/`max-age=0` to httpRequest’s header list.
        if (cache == "no-cache" && headers?.has("cache-control") == false) {
            headers?.set("cache-control", "max-age=0")
        }

        // 14. If httpRequest’s cache mode is "no-store" or "reload", then:
        if (cache == "no-store" || cache == "reload") {
            // If httpRequest’s header list does not contain `Pragma`, then append `Pragma`/`no-cache` to httpRequest’s header list.
            if (headers?.has("pragma") == false) {
                headers?.set("pragma", "no-cache")
            }

            // If httpRequest’s header list does not contain `Cache-Control`, then append `Cache-Control`/`no-cache` to httpRequest’s header list.
            if (headers?.has("cache-control") == false) {
                headers?.set("cache-control", "no-cache")
            }
        }

        // 15. If httpRequest’s header list contains `Range`, then append `Accept-Encoding`/`identity` to httpRequest’s header list.
        if (headers?.has("range") == true) {
            headers?.set("accept-encoding", "identity")
        }

        // TODO: cookies
        // test okhttp cookie jar

        // TODO: 19. If httpRequest’s cache mode is neither "no-store" nor "reload", then:
        // Set storedResponse to the result of selecting a response from the HTTP cache, possibly needing validation, as per the "Constructing Responses from Caches" chapter of HTTP Caching [HTTP-CACHING], if any.
        // If storedResponse is non-null, then:
        // If response is null: If httpRequest’s cache mode is "only-if-cached", then return a network error.
        // If httpRequest’s method is unsafe and forwardResponse’s status is in the range 200 to 399, inclusive, invalidate appropriate stored responses in the HTTP cache, as per the "Invalidation" chapter of HTTP Caching, and set storedResponse to null. [HTTP-CACHING]

        headers?.applyToRequest(builder)

        if (referrer.isNotBlank()) {
            builder.addHeader("referer", referrer)
        }

        val body = body
        if (body == null) {
            builder.method(method, null)
        } else {
            // Only set a body if we know it's content-type. This is also derived from how the body is set
            val contentType = headers?.get("content-type") as? String?
            if (contentType == null) {
                throw RuntimeException("TypeError: cannot have body without knowing content-type")
            }

            val mediaType = MediaType.parse(contentType)
            if (mediaType == null) {
                throw RuntimeException("TypeError: cannot encode body with unknown content-type '$contentType'")
            }
            builder.method(method, RequestBody.create(mediaType, body.readBytes()))
        }



    }

    /**
     * The okhttp request has completed, create a fetch response
     */
    fun updateFrom(call: Call, httpResponse: Response): BGJSModuleFetchResponse {
        val status = httpResponse.code()
        // if (status == 206 && )
        val response = BGJSModuleFetchResponse(v8Engine)
        response.headers = BGJSModuleFetchHeaders.createFrom(v8Engine, httpResponse.headers())
        response.status = response.status
        //TODO: Body as Reader, InputStream, BufferedSource?
        response.body = httpResponse.body()?.byteStream()

        // TODO: fill in all the other fields of response
        // TODO: handle redirection types

        return response
    }

    // Since ejecta-v8 currently cannot register abstract classes we have to override these methods here and just call super

    override var bodyUsed = false
        internal set
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
        val KEY_MODE = "mode"
        val KEY_METHOD = "method"
        val KEY_HEADERS = "headers"
        val KEY_BODY = "body"
        val KEY_REFERRER = "referrer"
        val KEY_REFERRER_POLICY = "referrerPolicy"
        val KEY_CREDENTIALS = "credentials"
        val KEY_CACHE = "cache"
        val KEY_REDIRECT = "redirect"
        val KEY_INTEGRITY = "integrity"
        val KEY_KEEPALIVE = "keepalive"
    }
}