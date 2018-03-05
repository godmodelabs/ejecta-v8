package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Class
import ag.boersego.v8annotations.V8ClassCreationPolicy
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import android.util.Log
import java.util.*
import java.util.concurrent.atomic.AtomicInteger

/**
 * Created by Kevin Read <me@kevin-read.com> on 02.03.18 for myrmecophaga-2.0.
 * Copyright (c) 2018 BörseGo AG. All rights reserved.
 */

@V8Class(creationPolicy = V8ClassCreationPolicy.JAVA_ONLY)
open class BGJSGLView(engine: V8Engine, val textureView: V8TextureView) : JNIV8Object(engine) {
    private val callbacksResize = ArrayList<JNIV8Function>(2)
    private val callbacksClose = ArrayList<JNIV8Function>(2)
    private val callbacksRedraw = ArrayList<JNIV8Function>(2)
    private val callbacksEvent = ArrayList<JNIV8Function>(3)

    class AnimationFrameRequest (val cb: JNIV8Function, val id: Int)

    private val queuedAnimationRequests = Stack<AnimationFrameRequest>()
    private val nextAnimationRequestId = AtomicInteger(0)

    // lateinit var textureView: V8TextureView

    val devicePixelRatio: Float
        @V8Getter get() = textureView.resources.displayMetrics.density

    val width: Int
        @V8Getter get() = textureView.width

    val height: Int
        @V8Getter get() = textureView.height

    external fun prepareRedraw()

    external fun endRedraw()

    external fun setTouchPosition(x: Int, y: Int)

    external fun setViewData(devicePixelRatio: Float, dontClearOnFlip: Boolean, x: Int, y: Int)

    @V8Function
    fun on(event: String, cb: JNIV8Function) {
        // TODO: Synchronize these
        when (event) {
            "event" -> callbacksEvent.add(cb)
            "resize" -> callbacksResize.add(cb)
            "close" -> callbacksClose.add(cb)
            "redraw" -> callbacksRedraw.add(cb)
            else -> throw IllegalArgumentException("invalid event type '$event'" )
        }
    }

    @Suppress("unused")
    fun requestAnimationFrame(cb: JNIV8Function): Int {
        // synchronized(this) {
            val id = nextAnimationRequestId.incrementAndGet()
            queuedAnimationRequests.add(AnimationFrameRequest(cb, id))
            textureView.requestRender()
            return id
        // }
    }

    @Suppress("unused")
    fun cancelAnimationFrame(id: Int) {
        // synchronized(this) {
            for (animationRequest in queuedAnimationRequests) {
                if (animationRequest.id == id) {
                    // TODO: dispose
                    queuedAnimationRequests.remove(animationRequest)
                    break
                }
            }
//            if (queuedAnimationRequests.isEmpty()) {
//                textureView.
//            }
        // }
    }

    private fun executeCallbacks(callbacks: ArrayList<JNIV8Function>, vararg args: Any) {
        var lastException: Exception? = null
        for (cb in callbacks) {
            try {
                cb.callAsV8Function(*args)
            } catch (e: Exception) {
                lastException = e
                Log.e(TAG, "Exception when running close callback", e)
            }
        }
        if (lastException != null) {
            throw lastException
        }
    }

    fun onClose() {
        executeCallbacks(callbacksClose)
    }

    fun onRedraw():Boolean {
        var didDraw = false
        // synchronized(this) {
        // Clone and empty both lists of callbacks
        val locker = v8Engine.lock()
        val callbacksForThisRedraw = ArrayList<JNIV8Function>(30)
        callbacksForThisRedraw.addAll(callbacksRedraw)
        queuedAnimationRequests.forEach { callbacksForThisRedraw.add(it.cb) }
        // callbacksRedraw.clear()
        queuedAnimationRequests.clear()
        v8Engine.unlock(locker)

        didDraw = !callbacksForThisRedraw.isEmpty()

        if (didDraw) {
            prepareRedraw()
            for (cb in callbacksForThisRedraw) {
                cb.callAsV8Function()
            }
            endRedraw()
        }
        return didDraw
    }

    fun onResize() {
        executeCallbacks(callbacksResize)
    }

    fun onEvent(event: JNIV8Object) {
        executeCallbacks(callbacksEvent, event)
    }

    fun cleanup() {
        // TODO: remove all event listeners and dispose
        // TODO: romve all animation frame requests and dispose them

    }

    companion object {
        val TAG = BGJSGLView::class.java.simpleName

        @JvmStatic
        external fun Create(engine: V8Engine): BGJSGLView
    }
}