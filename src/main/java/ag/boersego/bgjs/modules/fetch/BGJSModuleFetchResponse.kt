package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.JNIV8Promise
import ag.boersego.bgjs.V8Engine
import ag.boersego.bgjs.getV8Field
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter

class BGJSModuleFetchResponse @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : BGJSModuleFetchBody(v8Engine, jsPtr, args) {

    var status: Int = -1
        internal set
        @V8Getter get

    var statusText: String? = null
        internal set
        @V8Getter get

    var headers: BGJSModuleFetchHeaders? = null
        internal set
        @V8Getter get

    val ok: Boolean
        @V8Getter get() = status in 200..299

    var redirect = false
        internal set
        @V8Getter get

    var type = ""
        internal set
        @V8Getter get

    var url = ""
        internal set
        @V8Getter get

    var useFinalURL = false
        internal set
        @V8Getter get


    init {
        if (args != null) {
            if (args.size > 0) {
                // First argument: body
                val bodyRaw = args[0]
                // TODO: According to spec, this should be a Blob!
                if (bodyRaw is String) {
                    body = bodyRaw
                }
            }

            if (args.size > 1 && args[1] is JNIV8Object) {
                val options = args[1] as JNIV8Object
                if (options.hasV8Field("status")) {
                    status = options.getV8Field<Int>("status")
                }
                if (options.hasV8Field("statusText")) {
                    statusText = options.getV8Field<String>("statusText")
                }
                if (options.hasV8Field("headers")) {
                    val headersRaw = options.getV8Field("headers")
                    headers = headersRaw as? BGJSModuleFetchHeaders
                }
            }
        }
    }

    @V8Function
    fun clone(): BGJSModuleFetchResponse {
        val clone = BGJSModuleFetchResponse(v8Engine)
        clone.headers = headers
        clone.redirect = redirect
        clone.status = status
        clone.statusText = statusText
        clone.type = type
        clone.url = url
        clone.useFinalURL = useFinalURL

        return clone
    }

    @V8Function
    fun error(): BGJSModuleFetchResponse {
        return Companion.error(v8Engine)
    }

    /**
     * The redirect() method of the Response interface returns a Response resulting in a redirect to the specified URL.
     * @param url The URL that the new response is to originate from
     * @param status a status code for the response, needs to be a redirect status
     */
    @V8Function
    fun redirect(url: String, status: Int = -1): BGJSModuleFetchResponse {
        if (status < 301 || status > 304) {
            if (status != 307 && status != 308) {
                throw java.lang.RuntimeException("RangeError: status must be a redirect status")
            }
        }
        val response = BGJSModuleFetchResponse(v8Engine)
        response.url = url

        response.status = status
        response.headers = BGJSModuleFetchHeaders(v8Engine)
        response.headers?.append("Location", url)

        return response
    }

    // Since ejecta-v8 currently cannot register abstract classes we have to override these methods here and just call super

    override var bodyUsed = false
        internal set
        @V8Getter get

    @V8Function
    override fun json(): JNIV8Object {
        return super.json()
    }

    @V8Function
    override fun text(): JNIV8Promise {
        return super.text()
    }

    companion object {
        fun error(v8Engine: V8Engine): BGJSModuleFetchResponse {
            val response = BGJSModuleFetchResponse(v8Engine)
            response.type = "error"
            response.status = 0
            response.statusText = ""
            response.body = null

            return response
        }
    }

}