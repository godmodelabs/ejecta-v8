package ag.boersego.bgjs

import ag.boersego.v8annotations.V8Class
import ag.boersego.v8annotations.V8ClassCreationPolicy
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import android.util.Log
import java.util.*
import java.util.concurrent.atomic.AtomicInteger
import kotlin.collections.ArrayList

/**
 * Created by Kevin Read <me@kevin-read.com> on 02.03.18 for myrmecophaga-2.0.
 * Copyright (c) 2018 BörseGo AG. All rights reserved.
 */

@V8Class(creationPolicy = V8ClassCreationPolicy.JAVA_ONLY)
open class BGJSGLView(engine: V8Engine, private val textureView: V8TextureView) : JNIV8Object(engine) {
    private val callbacksResize = ArrayList<JNIV8Function>(2)
    private val callbacksClose = ArrayList<JNIV8Function>(2)
    private val callbacksRedraw = ArrayList<JNIV8Function>(2)
    private val callbacksEvent = ArrayList<JNIV8Function>(3)

    class AnimationFrameRequest (val cb: JNIV8Function, val id: Int)

    private val queuedAnimationRequests = Stack<AnimationFrameRequest>()
    private val nextAnimationRequestId = AtomicInteger(0)

    val devicePixelRatio: Float
        @V8Getter get() = textureView.resources.displayMetrics.density

    val width: Int
        @V8Getter get() = textureView.width

    val height: Int
        @V8Getter get() = textureView.height

    private external fun prepareRedraw()

    private external fun endRedraw()

    external fun setTouchPosition(x: Int, y: Int)

    external fun setViewData(devicePixelRatio: Float, dontClearOnFlip: Boolean, x: Int, y: Int)

    private external fun viewWasResized(x: Int, y: Int)

    @V8Function
    fun on(event: String, cb: JNIV8Function) {
        val list = when (event) {
            "event" -> callbacksEvent
            "resize" -> callbacksResize
            "close" -> callbacksClose
            "redraw" -> callbacksRedraw
            else -> throw IllegalArgumentException("invalid event type '$event'" )
        }
        synchronized(list) {
            list.add(cb)
        }
    }

    @Suppress("unused")
    fun requestAnimationFrame(cb: JNIV8Function): Int {
            val id = nextAnimationRequestId.incrementAndGet()
            queuedAnimationRequests.add(AnimationFrameRequest(cb, id))
            textureView.requestRender()
            return id
    }

    @Suppress("unused")
    fun cancelAnimationFrame(id: Int) {
        for (animationRequest in queuedAnimationRequests) {
            if (animationRequest.id == id) {
                queuedAnimationRequests.remove(animationRequest)
                return
            }
        }
    }

    private fun executeCallbacks(callbacks: ArrayList<JNIV8Function>, vararg args: Any) {
        var lastException: Exception? = null
        var cbs: Array<Any>? = null
        synchronized(callbacks) {
            if (callbacks.isEmpty()) {
                return
            }
            cbs = callbacks.toArray()
        }

        for (cb in cbs!!) {
            try {
                if (cb is JNIV8Function) {
                    cb.callAsV8Function(*args)
                }
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
        v8Engine.runLocked {
            if (DEBUG) {
                Log.d(TAG, "onClose")
            }
            queuedAnimationRequests.clear()
            callbacksResize.clear()
            callbacksEvent.clear()
            executeCallbacks(callbacksClose)
            callbacksClose.clear()
        }
    }

    fun onRedraw():Boolean {
        if (queuedAnimationRequests.isEmpty()) {
            return false
        }

        // Clone and empty both lists of callbacks in a v8::Locker so JS cannot add event listeners
        // while we execute them
        v8Engine.runLocked {
            val callbacksForRedraw = ArrayList<JNIV8Function>(queuedAnimationRequests.size)
            queuedAnimationRequests.forEach { callbacksForRedraw.add(it.cb) }
            queuedAnimationRequests.clear()

                prepareRedraw()
                for (cb in callbacksForRedraw) {
                    cb.callAsV8Function()
                }
                endRedraw()
        }
        return true
    }

    fun onResize() {
        viewWasResized(width, height)
        executeCallbacks(callbacksResize)
    }

    fun onEvent(event: JNIV8Object) {
        executeCallbacks(callbacksEvent, event)
    }

    companion object {
        @Suppress("SimplifyBooleanWithConstants")
        val DEBUG = false && BuildConfig.DEBUG
        val TAG = BGJSGLView::class.java.simpleName!!

        @JvmStatic
        external fun Create(engine: V8Engine): BGJSGLView
    }
}