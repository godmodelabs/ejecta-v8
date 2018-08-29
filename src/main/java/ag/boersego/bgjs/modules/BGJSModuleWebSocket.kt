package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.v8annotations.*
import android.annotation.SuppressLint
import android.util.Log
import okhttp3.*

/**
 * Created by Kevin Read <me@kevin-read.com> on 23.11.17 for myrmecophaga-2.0.
 * Copyright (c) 2017 BörseGo AG. All rights reserved.
 */

@SuppressLint("LogNotTimber")
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
        socket = httpClient.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                super.onOpen(webSocket, response)
                if (_readyState == ReadyState.CLOSING || _readyState == ReadyState.CLOSED) {
                    return
                }
                v8Engine.runLocked {
                    _readyState = ReadyState.OPEN
                    onopen?.callAsV8Function()
                }
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                super.onFailure(webSocket, t, response)

                v8Engine.runLocked {
                    if (DEBUG) {
                        Log.w(TAG, "onFailure:", t)
                    }
                    _readyState = ReadyState.CLOSED

                    onerror?.callAsV8Function()
                    // It is safe to call onClose, since onFailure will not call any other callback
                    val closeEvent = JNIV8GenericObject.Create(v8Engine)
                    closeEvent.setV8Field("code", 1000)
                    closeEvent.setV8Field("reason", "timeout")
                    onclose?.callAsV8Function(closeEvent)
                    closeEvent.dispose()
                }
                onClose()
                socket = null
            }

            override fun onMessage(webSocket: WebSocket, text: String) {
                super.onMessage(webSocket, text)
                v8Engine.runLocked {
                    val data = JNIV8GenericObject.Create(v8Engine)
                    data.setV8Field("data", text)

                    onmessage?.callAsV8Function(data)
                }
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                super.onClosed(webSocket, code, reason)
                v8Engine.runLocked {

                    if (onclose != null) {
                        val closeEvent = JNIV8GenericObject.Create(v8Engine)
                        closeEvent.setV8Field("code", code)
                        closeEvent.setV8Field("reason", reason)
                        onclose?.callAsV8Function(closeEvent)
                        closeEvent.dispose()
                    }
                    _readyState = ReadyState.CLOSED
                    onClose()
                    socket = null
                }
            }

            override fun onClosing(webSocket: WebSocket, code: Int, reason: String) {
                super.onClosing(webSocket, code, reason)

                _readyState = ReadyState.CLOSING
            }
        })
    }

    @V8Function
    @JvmOverloads
    fun close(code: Int = 1000, @V8UndefinedIsNull reason: String? = null) {
        socket?.close(code, reason)
    }

    @V8Function
    fun send(msg: String): Any? {
        var success = false
        v8Engine.runLocked {
            if (_readyState != ReadyState.OPEN && _readyState != ReadyState.CLOSING) {
                throw RuntimeException("INVALID_STATE_ERR")
            }
            if (socket != null) {
                socket?.send(msg)
                success = true
            }
        }
        return success
    }

    constructor(engine: V8Engine) : super(engine)

    var onclose: JNIV8Function? = null
        @V8Getter get
        @V8Setter set
    var onerror: JNIV8Function? = null
        @V8Getter get
        @V8Setter set
    var onmessage: JNIV8Function? = null
        @V8Getter get
        @V8Setter set
    var onopen: JNIV8Function? = null
        @V8Getter get
        @V8Setter set
    private var _readyState: ReadyState = ReadyState.CONNECTING

    @Suppress("unused")
    var readyState: Any? = null
        @V8Getter
        get() = _readyState.ordinal

    var url: Any? = null
        @V8Getter
        get() = _url

    @Suppress("unused")
    var bufferedAmount: Any? = null
        @V8Getter
        get() = if (socket != null) socket?.queueSize()?.toInt() else 0


    private lateinit var httpClient: OkHttpClient

    private lateinit var _url: String

    private lateinit var onClose: () -> Unit

    fun setData(okHttpClient: OkHttpClient, url: String, onClose: () -> Unit) {
        this.httpClient = okHttpClient
        this._url = url
        this.onClose = onClose
    }


    fun closeForSuspend() {
        if (_readyState != ReadyState.CLOSED) {
            _readyState = ReadyState.CLOSING
            v8Engine.runLocked {

                if (onclose != null) {
                    val closeEvent = JNIV8GenericObject.Create(v8Engine)
                    closeEvent.setV8Field("code", 1013)
                    closeEvent.setV8Field("reason", "suspend")
                    onclose?.callAsV8Function(closeEvent)
                    closeEvent.dispose()
                }
                _readyState = ReadyState.CLOSED

                socket?.close(1000, "")
            }
        }
    }

    companion object {
        val TAG = BGJSWebSocket::class.java.simpleName!!
        @Suppress("SimplifyBooleanWithConstants")
        val DEBUG = false && BuildConfig.DEBUG
    }
}

class BGJSModuleWebSocket(private var okHttpClient: OkHttpClient) : JNIV8Module("websocket") {

    private val sockets = ArrayList<BGJSWebSocket>()

    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        val exports = JNIV8GenericObject.Create(engine)

        exports.setV8Field("WebSocket", JNIV8Function.Create(engine) { _, arguments ->
            if (arguments.isEmpty() || arguments[0] !is String) {
                throw IllegalArgumentException("create needs one string argument (url)")
            }
            val websocket = BGJSWebSocket(engine)

            websocket.setData(okHttpClient, arguments[0] as String, {
                sockets.remove(websocket)
                Unit
            })

            // Add to list of all sockets so we can shut them down when the V8Engine pauses
            sockets.add(websocket)

            engine.enqueueOnNextTick(websocket)

            websocket
        })

        module?.setV8Field("exports", exports)
    }

    companion object {
        init {
            JNIV8Object.RegisterV8Class(BGJSWebSocket::class.java)
        }

        private val TAG = BGJSModuleWebSocket::class.java.simpleName
    }

    fun onSuspend() {
        for (socket in sockets) {
            socket.closeForSuspend()
        }
        sockets.clear()
    }

}