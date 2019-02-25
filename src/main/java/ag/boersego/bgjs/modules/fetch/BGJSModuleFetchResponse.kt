package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.JNIV8Promise
import ag.boersego.bgjs.V8Engine
import ag.boersego.bgjs.getV8Field
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import okhttp3.Response

class BGJSModuleFetchResponse @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : BGJSModuleFetchBody(v8Engine, jsPtr, args) {

    var status: Int = -1
        internal set
        @V8Getter get

    var statusText: String? = null
        internal set
        @V8Getter get

    lateinit var headers: BGJSModuleFetchHeaders
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
                body = BGJSModuleFetchBody.createBodyFromRaw(bodyRaw)
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
                    headers = headersRaw as BGJSModuleFetchHeaders
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
        fun error(v8Engine: V8Engine): BGJSModuleFetchResponse {
            val response = BGJSModuleFetchResponse(v8Engine)
            response.type = "error"
            response.status = 0
            response.statusText = ""
            response.body = null

            return response
        }

        /**
         * The create a fetch response from completed okhttp response
         */
        fun createFrom(v8Engine: V8Engine, httpResponse: Response): BGJSModuleFetchResponse {
            val response = BGJSModuleFetchResponse(v8Engine)
            response.headers = BGJSModuleFetchHeaders.createFrom(v8Engine, httpResponse.headers())
            response.status = httpResponse.code()
            //TODO: statusText from httpResonse
            //response.statusText = httpResponse.
            //TODO: Body as Reader, InputStream, BufferedSource?
            response.body = httpResponse.body()?.byteStream()
            response.redirect = httpResponse.isRedirect

            return response
        }
    }

}