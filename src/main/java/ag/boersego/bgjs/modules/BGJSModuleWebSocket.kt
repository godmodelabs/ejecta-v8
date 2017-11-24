package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.v8annotations.*
import okhttp3.*

/**
 * Created by Kevin Read <me@kevin-read.com> on 23.11.17 for myrmecophaga-2.0.
 * Copyright (c) 2017 BörseGo AG. All rights reserved.
 */

@V8Class(creationPolicy = V8ClassCreationPolicy.JAVA_ONLY)
class BGJSWebSocket : JNIV8Object, Runnable {

    private enum class ReadyState {
        CONNECTING,
        OPEN,
        CLOSING,
        CLOSED
    }


    private var socket: WebSocket? = null

    override fun run() {
        val request = Request.Builder().url(_url).build()
        socket = httpClient.newWebSocket(request, object: WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response?) {
                super.onOpen(webSocket, response)
                _readyState = ReadyState.OPEN
                _onopen?.callAsV8Function()
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable?, response: Response?) {
                super.onFailure(webSocket, t, response)
                _readyState = ReadyState.CLOSED

                _onerror?.callAsV8Function()
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                super.onMessage(webSocket, text)

                _onmessage?.callAsV8Function(text)
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String?) {
                super.onClosed(webSocket, code, reason)
                _readyState = ReadyState.CLOSED

                if (_onclose != null) {
                    val closeEvent = JNIV8GenericObject.Create(v8Engine)
                    closeEvent.setV8Field("code", code)
                    closeEvent.setV8Field("reason", reason)
                    _onclose?.callAsV8Function(closeEvent)
                    closeEvent.dispose()
                }
                socket = null
            }

            override fun onClosing(webSocket: WebSocket, code: Int, reason: String?) {
                super.onClosing(webSocket, code, reason)

                _readyState = ReadyState.CLOSING
            }
        });
    }

    @V8Function
    fun close(args: Array<Any>): Any? {
        if (socket != null) {
            val code = if (args.isNotEmpty() && args[0] is Number) (args[0] as Number).toInt() else 1000
            val reason = if (args.size > 1 && args[1] is String) (args[1] as String) else null
            socket?.close(code, reason)
        }
        return JNIV8Undefined.GetInstance()
    }

    @V8Function
    fun send(args: Array<Any>): Any? {
        if (_readyState != ReadyState.OPEN) {
            throw RuntimeException("INVALID_STATE_ERR")
        }
        if (socket != null) {
            if (args.size == 0 || !(args[0] is String)) {
                throw IllegalArgumentException("send needs one argument of type string")
            }
            socket?.send(args[0] as String)
            return true
        }
        return false
    }

    constructor(engine: V8Engine) : super(engine)

    private var _onclose: JNIV8Function? = null
    private var _onerror: JNIV8Function? = null
    private var _onmessage: JNIV8Function? = null
    private var _onopen: JNIV8Function? = null
    private var _readyState: ReadyState = ReadyState.CONNECTING

    var readyState: Any? = null
        @V8Getter
        get() = _readyState.ordinal

    var url: Any? = null
        @V8Getter
        get() = _url

    var bufferedAmount: Any? = null
        @V8Getter
        get() = if (socket != null) socket?.queueSize()?.toInt() else 0

    var onclose: Any?
        @V8Getter
        get() = _onclose
        @V8Setter
        set(input) {
            if (input is JNIV8Function) {
                _onclose = input
            } else {
                throw IllegalArgumentException("onclose must be a function")
            }
        }

    var onerror: Any?
        @V8Getter
        get() = _onerror
        @V8Setter
        set(input) {
            if (input is JNIV8Function) {
                _onerror = input
            } else {
                throw IllegalArgumentException("onerror must be a function")
            }
        }

    var onmessage: Any?
        @V8Getter
        get() = _onmessage
        @V8Setter
        set(input) {
            if (input is JNIV8Function) {
                _onmessage = input
            } else {
                throw IllegalArgumentException("onmessage must be a function")
            }
        }
    var onopen: Any?
        @V8Getter
        get() = _onopen
        @V8Setter
        set(input) {
            if (input is JNIV8Function) {
                _onopen = input
            } else {
                throw IllegalArgumentException("onopen must be a function")
            }
        }

    private lateinit var httpClient: OkHttpClient

    private lateinit var _url: String

    fun setData(okHttpClient: OkHttpClient, url: String) {
        this.httpClient = okHttpClient
        this._url = url
    }
}

class BGJSModuleWebSocket (okHttpClient: OkHttpClient) : JNIV8Module("websocket") {

    private var okHttpClient: OkHttpClient = okHttpClient

    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        val exports = JNIV8GenericObject.Create(engine)

        exports.setV8Field("create", JNIV8Function.Create(engine, { receiver, arguments ->
            if (arguments.size < 1 || !(arguments[0] is String)) {
                throw IllegalArgumentException("create needs one string argument (url)")
            }
            val websocket = BGJSWebSocket(engine)
            websocket.setData(okHttpClient, arguments[0] as String)
            engine.enqueueOnNextTick(websocket)

            websocket
        }))

        module?.setV8Field("exports", exports)
    }
    companion object {
        init {
            JNIV8Object.RegisterV8Class(BGJSWebSocket::class.java)
        }
    }

}