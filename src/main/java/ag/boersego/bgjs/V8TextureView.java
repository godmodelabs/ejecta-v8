package ag.boersego.bgjs;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.opengl.EGL14;
import android.opengl.GLES10;
import android.opengl.GLSurfaceView;
import android.opengl.GLUtils;
import android.os.Build;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.util.Log;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.TextureView;
import android.view.ViewParent;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;

// import org.jetbrains.annotations.NotNull;

// TextureView is ICS+ only

/**
 * V8TextureView wraps an Android TextureView in a way that OpenGL calls from V8 can draw on it
 *
 * @author kread
 */
@TargetApi(Build.VERSION_CODES.ICE_CREAM_SANDWICH)
@SuppressLint("LogNotTimber")
abstract public class V8TextureView extends TextureView implements TextureView.SurfaceTextureListener {

    private final V8Engine mEngine;
    protected RenderThread mRenderThread;
    private final MotionEvent.PointerCoords[] mTouches = new MotionEvent.PointerCoords[MAX_NUM_TOUCHES];
    private final boolean[] mTouchesThere = new boolean[MAX_NUM_TOUCHES];
    private int mNumTouches = 0;
    private double mTouchDistance;
    protected static float mScaling;
    private boolean mInteractive;
    private PointerCoords mTouchStart;
    private final float mTouchSlop;
    protected IV8GLViewOnRender mCallback;
    private Rect mViewRect;
    private boolean mFinished = false;


    protected boolean DEBUG;
    @SuppressWarnings("PointlessBooleanExpression")
    private static final boolean LOG_FPS = false && BuildConfig.DEBUG;
    private static final int MAX_NUM_TOUCHES = 10;
    private static final int TOUCH_SLOP = 5;
    private static final String TAG = "V8TextureView";
    private int[] mEglVersion;
    private float mClearRed, mClearGreen, mClearBlue, mClearAlpha;
    private boolean mClearColorSet;
    protected boolean mDontClearOnFlip;
    private BGJSGLView mBGJSGLView;
    private int mSurfaceWidth;
    private int mSurfaceHeight;
    /// True if this view is in the process of being removed and we should no longer process events
    private boolean mIsShuttingDown;

    /**
     * Create a new V8TextureView instance
     *
     * @param context Context instance
     * @param engine  the V8Engine to use
     */
    public V8TextureView(final Context context, @NonNull final V8Engine engine) {
        super(context);
        mEngine = engine;
        final Resources r = getResources();
        if (r != null) {
            mScaling = r.getDisplayMetrics().density;
        }
        mTouchSlop = TOUCH_SLOP * mScaling;
        setSurfaceTextureListener(this);
    }

    public V8Engine getEngine() {
        return mEngine;
    }

    public void doDebug(final boolean debug) {
        DEBUG = debug;
    }

    abstract public void onGLCreated(BGJSGLView jsViewObject);

    public void onGLRecreated(final BGJSGLView jsViewObject) {
        if (mBGJSGLView != null) {
            mBGJSGLView.onResize();
        }
    }

    abstract public void onGLCreateError(Exception ex);

    protected void onFrameRendered(final BGJSGLView jsViewObject) {

    }

    @Override
    public void onSurfaceTextureAvailable(final SurfaceTexture surface, final int width, final int height) {
        // It is possible that the containing view wants to kill this before we have even been able to start the render thread
        if (mFinished || mIsShuttingDown) {
            return;
        }
        mRenderThread = new RenderThread(surface);
        if (DEBUG) {
            Log.d(TAG, "Starting render thread");
        }

        mSurfaceWidth = width;
        mSurfaceHeight = height;

        mRenderThread.start();
    }

    /**
     * Interactive views absorb touches
     */
    public void setInteractive(final boolean interactive) {
        mInteractive = interactive;
    }

    /**
     * Request a redraw
     */
    public void requestRender() {
        final RenderThread renderthread = mRenderThread;
        if (renderthread != null && !mIsShuttingDown) {
            renderthread.requestRender();
        }
    }

    @Override
    public boolean onTouchEvent(final MotionEvent ev) {
        if (mIsShuttingDown) {
            return false;
        }
        // Non-interactive surfaces don't handle touch but pass it on
        if (!mInteractive) {
            return super.onTouchEvent(ev);
        }
        final int action = ev.getActionMasked();
        final int index = ev.getActionIndex();
        final int count = ev.getPointerCount();

        if (DEBUG) {
            Log.d(TAG,
                    "Action " + action + ", index " + index + ", x " + ev.getX(index) + ", id "
                            + ev.getPointerId(index));
        }
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                // Initialize the size of this view on touch start so we can use it to detect when the fingers exit the surface
                mViewRect = new Rect(getLeft(), getTop(), getRight(), getBottom());

                final int id = ev.getPointerId(index);
                // As long as we are in a gesture we don't want any view parents to handle touch events
                final ViewParent vp = getParent();
                if (vp != null) {
                    vp.requestDisallowInterceptTouchEvent(true);
                }

                // We have a limit on the number of touch points that we pass to JS
                if (id < MAX_NUM_TOUCHES) {
                    if (mTouches[id] == null) {
                        mTouches[id] = new MotionEvent.PointerCoords();
                    }
                    ev.getPointerCoords(index, mTouches[id]);
                    // Scale the touch pointers to logical pixels. JS doesn't know about dpi
                    mTouches[id].x = (int) (mTouches[id].x / mScaling);
                    mTouches[id].y = (int) (mTouches[id].y / mScaling);

                    // If this is the first pointer of a potential multi touch gesture, record the start position
                    // of the gesture
                    if (action == MotionEvent.ACTION_DOWN) {
                        mTouchStart = new PointerCoords(mTouches[id]);
                    }
                    // Mark the pointer index as valid
                    mTouchesThere[id] = true;

                    // For pinch-zoom, calculate the distance
                    if (count == 2 && mTouches[0] != null && mTouches[1] != null) {
                        mTouchDistance = Math.sqrt((mTouches[0].x - mTouches[1].x) * (mTouches[0].x - mTouches[1].x)
                                + (mTouches[0].y - mTouches[1].y) * (mTouches[0].y - mTouches[1].y));
                    }
                    mNumTouches = count;
                    if (mEngine == null || mRenderThread == null) {
                        return false;
                    }
                    // And pass the touch event to JS
                    if (mBGJSGLView != null) {
                        mBGJSGLView.setTouchPosition((int) mTouches[id].x, (int) mTouches[id].y);
                    }

                }

                sendTouchEvent("touchstart");
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                boolean touchDirty = false;
                for (int i = 0; i < count; i++) {
                    final int id = ev.getPointerId(i);
                    if (id < MAX_NUM_TOUCHES) {
                        final int oldX = (int) mTouches[id].x;
                        final int oldY = (int) mTouches[id].y;
                        ev.getPointerCoords(i, mTouches[id]);
                        mTouches[id].x = (int) (mTouches[id].x / mScaling);
                        mTouches[id].y = (int) (mTouches[id].y / mScaling);

                        // If the touch hasn't moved yet, check if we are over the
                        // slop before we report moves
                        if (mTouchStart != null) {
                            final float touchDiffX = mTouches[id].x - mTouchStart.x;
                            final float touchDiffY = mTouches[id].y - mTouchStart.y;
                            if (touchDiffX > mTouchSlop || touchDiffY > mTouchSlop || touchDiffX < -mTouchSlop
                                    || touchDiffY < -mTouchSlop) {
                                touchDirty = true;
                                mTouchStart = null;
                            }
                        } else {
                            if (mTouches[id].x != oldX || mTouches[id].y != oldY) {
                                touchDirty = true;
                            }
                        }

                        if (touchDirty && mBGJSGLView != null) {
                            mBGJSGLView.setTouchPosition((int) mTouches[id].x, (int) mTouches[id].y);
                        }
                    }
                }

                String touchEvent = "touchmove";
                boolean endThisTouch = false;
                // If the pointer leaves the surface when moving, end the touch
                if (!mViewRect.contains(getLeft() + (int) ev.getX(), getTop() + (int) ev.getY())) {
                    touchEvent = "touchend";
                    endThisTouch = true;
                }

                // We only send touch events to JS if there is a signifcant change in pointer coords
                if (touchDirty) {
                    sendTouchEvent(touchEvent);
                }
                if (endThisTouch) {
                    return false;
                }
                break;
            }

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
            case MotionEvent.ACTION_CANCEL: {
                final int id = ev.getPointerId(index);
                if (id < MAX_NUM_TOUCHES) {
                    mTouchesThere[id] = false;
                }
                sendTouchEvent("touchend");
                break;
            }
        }
        return true;
    }

    /**
     * Send a normalized touch event to JS
     *
     * @param type the type of event: touchstart, touchmove or touchend
     */
    private void sendTouchEvent(final String type) {
        if (mRenderThread == null || mIsShuttingDown) {
            return;
        }

        // This is actually the pointer to the BGJSView and could have been cleared because
        // it was closed.
        final BGJSGLView jsGLView = mBGJSGLView;

        if (jsGLView == null) {
            return;
        }
        int count = 0;
        for (int i = 0; i < MAX_NUM_TOUCHES; i++) {
            if (mTouchesThere[i]) {
                count++;
            }
        }
        mNumTouches = count;

        // We need to calculate the scale of a pinch-zoom gesture from touches 1 & 2
        double x1 = 0;
        double y1 = 0;
        double x2 = 0;
        double y2 = 0;
        final double scale;
        // Also create a WhatWG compatible touch event object per touch
        final JNIV8GenericObject[] touchObjs = new JNIV8GenericObject[mNumTouches];
        int j = 0;
        for (int i = 0; i < MAX_NUM_TOUCHES; i++) {
            if (mTouchesThere[i]) {
                final float x = mTouches[i].x;
                final float y = mTouches[i].y;
                if (j == 0) {
                    x1 = x;
                    y1 = y;
                } else if (j == 1) {
                    x2 = x;
                    y2 = y;
                }
                final JNIV8GenericObject touchObj = JNIV8GenericObject.Create(mEngine);
                touchObj.setV8Field("clientX", x);
                touchObj.setV8Field("clientY", y);
                touchObjs[j] = touchObj;
                j++;
            }
        }
        if (mNumTouches == 2) {
            scale = Math.sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2)) / mTouchDistance;
        } else {
            scale = 1f;
        }
        if (DEBUG) {
            Log.d(TAG, "touch event: type " + type + ", p1 " + x1 + ", " + y1 + ", p2 " + x2 + ", " + y2 + ", scale "
                    + scale);
        }

        final JNIV8Array touches = JNIV8Array.CreateWithElements(mEngine, (Object[]) touchObjs);

        final JNIV8GenericObject touchEventObj = JNIV8GenericObject.Create(mEngine);
        touchEventObj.setV8Field("type", type);
        touchEventObj.setV8Field("scale", scale);
        touchEventObj.setV8Field("touches", touches);

        // Just double check that it hasn't been removed since
        if (mBGJSGLView != null) {
            jsGLView.onEvent(touchEventObj);
        }

        for (final JNIV8GenericObject touch : touchObjs) {
            touch.dispose();
        }
        touches.dispose();
        touchEventObj.dispose();
    }

    public void setClearColor(final int color) {
        try {
            // Convert to long to make unsigned
            final long c = color & 0x00000000ffffffffL;
            mClearAlpha = (float) ((c & 0xff000000L) >> 24) / 256f;
            mClearRed = (float) ((c & 0x00ff0000L) >> 16) / 256f;
            mClearGreen = (float) ((c & 0x0000ff00L) >> 8) / 256f;
            mClearBlue = (float) ((c & 0x000000ffL)) / 256f;
            mClearColorSet = true;
        } catch (final Exception e) {
            if (DEBUG) {
                Log.i(TAG, "Cannot set clear color from background color", e);
            }
        }
    }

    public static boolean isEmulator() {
        return Build.MODEL.contains("google_sdk")
                || Build.MODEL.contains("Emulator")
                || Build.MODEL.contains("Android SDK built for x86")
                || Build.MANUFACTURER.contains("Genymotion")
                // || (Build.BRAND.startsWith("generic") && Build.DEVICE.startsWith("generic"))
                || "google_sdk".equals(Build.PRODUCT);
    }

    public void removeClearColor() {
        mClearColorSet = false;
    }

    /**
     * Reset all touch information
     */
    void resetTouches() {
        mNumTouches = 0;
        for (int i = 0; i < MAX_NUM_TOUCHES; i++) {
            mTouchesThere[i] = false;
        }
        mTouchDistance = 0;
    }

    public void dontClearOnFlip(final boolean dontClear) {
        mDontClearOnFlip = dontClear;
    }

    public void shutdown() {
        mIsShuttingDown = true;
    }

    private static class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

        private static final String TAG = "V8ConfigChooser";
        private static final boolean DEBUG = false;
        private final int[] mEglVersion;
        private final boolean mDontTouchSwap;

        public ConfigChooser(final int r, final int g, final int b, final int a, final int depth, final int stencil, final int[] version) {
            mRedSize = r;
            mGreenSize = g;
            mBlueSize = b;
            mAlphaSize = a;
            mDepthSize = depth;
            mStencilSize = stencil;
            mEglVersion = version;
            mDontTouchSwap = isEmulator();
            if (DEBUG) {
                Log.d(TAG, "EGL version " + version[0] + "." + version[1]);
            }
        }

        /*
         * This EGL config specification is used to specify 1.1 rendering. We
         * use a minimum size of 4 bits for red/green/blue, but will perform
         * actual matching in chooseConfig() below.
         */
        // private static int EGL_OPENGL_ES2_BIT = 4;
        private static final int[] s_configAttribs2 = {EGL10.EGL_RED_SIZE, 4, EGL10.EGL_GREEN_SIZE, 4, EGL10.EGL_BLUE_SIZE,
                4, EGL10.EGL_NONE};

        @Override
        public EGLConfig chooseConfig(final EGL10 egl, final EGLDisplay display) {

            /*
             * Get the number of minimally matching EGL configurations
             */
            final int[] num_config = new int[1];
            egl.eglChooseConfig(display, s_configAttribs2, null, 0, num_config);

            final int numConfigs = num_config[0];

            if (numConfigs <= 0) {
                throw new IllegalArgumentException("No configs match configSpec");
            }

            /*
             * Allocate then read the array of minimally matching EGL configs
             */
            final EGLConfig[] configs = new EGLConfig[numConfigs];
            egl.eglChooseConfig(display, s_configAttribs2, configs, numConfigs, num_config);

            if (DEBUG) {
                printConfigs(egl, display, configs);
            }
            /*
             * Now return the "best" one
             */
            return chooseConfig(egl, display, configs);
        }

        EGLConfig chooseConfig(final EGL10 egl, final EGLDisplay display, final EGLConfig[] configs) {
            EGLConfig bestConfig = null;
            int bestDepth = 128;
            for (final EGLConfig config : configs) {
                boolean hasSwap = false;
                if (!mDontTouchSwap && mEglVersion[0] > 1 || mEglVersion[1] >= 4) {
                    final int surfaceType = findConfigAttrib(egl, display, config, EGL14.EGL_SURFACE_TYPE, 0);
                    if ((surfaceType & EGL14.EGL_SWAP_BEHAVIOR_PRESERVED_BIT) != 0) {
                        hasSwap = true;
                    }
                    if (DEBUG) {
                        Log.d(TAG, "surfaceType " + surfaceType + " has preserved bit " + hasSwap);
                    }
                }

                // Get depth and stencil sizes
                final int d = findConfigAttrib(egl, display, config, EGL10.EGL_DEPTH_SIZE, 0);
                final int s = findConfigAttrib(egl, display, config, EGL10.EGL_STENCIL_SIZE, 0);

                // We need at least mDepthSize and mStencilSize bits
                if (s < mStencilSize) {
                    if (DEBUG) {
                        Log.d(TAG, "Not good enough: stencil " + s);
                    }
                    continue;
                }

                // We also want at least a minimum size for red/green/blue/alpha
                final int r = findConfigAttrib(egl, display, config, EGL10.EGL_RED_SIZE, 0);
                final int g = findConfigAttrib(egl, display, config, EGL10.EGL_GREEN_SIZE, 0);
                final int b = findConfigAttrib(egl, display, config, EGL10.EGL_BLUE_SIZE, 0);
                final int a = findConfigAttrib(egl, display, config, EGL10.EGL_ALPHA_SIZE, 0);

                if (r <= mRedSize && g <= mGreenSize && b <= mBlueSize && a >= mAlphaSize) {
                    // But we'll try to figure out which configuration matches best
                    final int distance = (a - mAlphaSize) + mStencilSize - s + mDepthSize - d + (mRedSize - r) + (mGreenSize - g) + (mBlueSize - b) + (hasSwap ? 4 : 0);
                    if (DEBUG) {
                        Log.d(TAG, "Good enough: " + r + ", " + g + ", " + b + ", depth " + d + ", stencil " + s
                                + ", alpha " + a + ", distance " + distance);
                    }
                    // If this configuration is a better match than the best to date, replace it
                    if (distance < bestDepth) {
                        bestConfig = config;
                        bestDepth = distance;
                    }
                } else {
                    if (DEBUG) {
                        Log.d(TAG, "Not good enough: " + r + ", " + g + ", " + b + ", depth " + d + ", stencil " + s
                                + ", alpha " + a);
                    }
                }
            }
            // Lets hope we found at least one acceptable config
            if (bestConfig != null) {
                if (DEBUG) {
                    Log.d(TAG, "Best config");
                    printConfig(egl, display, bestConfig);
                }
                return bestConfig;
            }

            return null;
        }

        private int findConfigAttrib(final EGL10 egl, final EGLDisplay display, final EGLConfig config, final int attribute, final int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        /**
         * Pretty-print OpenGL configs
         *
         * @param egl     egl instance
         * @param display egl display
         * @param configs the list of configs to print
         */
        private void printConfigs(final EGL10 egl, final EGLDisplay display, final EGLConfig[] configs) {
            final int numConfigs = configs.length;
            if (DEBUG) {
                Log.w(TAG, String.format("%d configurations", numConfigs));
            }
            for (int i = 0; i < numConfigs; i++) {
                if (DEBUG) {
                    Log.w(TAG, String.format("Configuration %d:\n", i));
                }
                printConfig(egl, display, configs[i]);
            }
        }

        /**
         * Pretty-print one OpenGL config
         *
         * @param egl     egl instance
         * @param display egl display
         * @param config  the config to print
         */
        private void printConfig(final EGL10 egl, final EGLDisplay display, final EGLConfig config) {
            final int[] attributes = {EGL10.EGL_BUFFER_SIZE, EGL10.EGL_ALPHA_SIZE, EGL10.EGL_BLUE_SIZE,
                    EGL10.EGL_GREEN_SIZE, EGL10.EGL_RED_SIZE, EGL10.EGL_DEPTH_SIZE, EGL10.EGL_STENCIL_SIZE,
                    EGL10.EGL_CONFIG_CAVEAT, EGL10.EGL_CONFIG_ID, EGL10.EGL_LEVEL, EGL10.EGL_MAX_PBUFFER_HEIGHT,
                    EGL10.EGL_MAX_PBUFFER_PIXELS, EGL10.EGL_MAX_PBUFFER_WIDTH,
                    EGL10.EGL_NATIVE_RENDERABLE,
                    EGL10.EGL_NATIVE_VISUAL_ID,
                    EGL10.EGL_NATIVE_VISUAL_TYPE,
                    0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
                    EGL10.EGL_SAMPLES, EGL10.EGL_SAMPLE_BUFFERS, EGL10.EGL_SURFACE_TYPE, EGL10.EGL_TRANSPARENT_TYPE,
                    EGL10.EGL_TRANSPARENT_RED_VALUE, EGL10.EGL_TRANSPARENT_GREEN_VALUE,
                    EGL10.EGL_TRANSPARENT_BLUE_VALUE,
                    0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
                    0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
                    0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
                    0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
                    EGL10.EGL_LUMINANCE_SIZE, EGL10.EGL_ALPHA_MASK_SIZE, EGL10.EGL_COLOR_BUFFER_TYPE,
                    EGL10.EGL_RENDERABLE_TYPE, 0x3042 // EGL10.EGL_CONFORMANT
            };
            final String[] names = {"EGL_BUFFER_SIZE", "EGL_ALPHA_SIZE", "EGL_BLUE_SIZE", "EGL_GREEN_SIZE", "EGL_RED_SIZE",
                    "EGL_DEPTH_SIZE", "EGL_STENCIL_SIZE", "EGL_CONFIG_CAVEAT", "EGL_CONFIG_ID", "EGL_LEVEL",
                    "EGL_MAX_PBUFFER_HEIGHT", "EGL_MAX_PBUFFER_PIXELS", "EGL_MAX_PBUFFER_WIDTH",
                    "EGL_NATIVE_RENDERABLE", "EGL_NATIVE_VISUAL_ID", "EGL_NATIVE_VISUAL_TYPE",
                    "EGL_PRESERVED_RESOURCES", "EGL_SAMPLES", "EGL_SAMPLE_BUFFERS", "EGL_SURFACE_TYPE",
                    "EGL_TRANSPARENT_TYPE", "EGL_TRANSPARENT_RED_VALUE", "EGL_TRANSPARENT_GREEN_VALUE",
                    "EGL_TRANSPARENT_BLUE_VALUE", "EGL_BIND_TO_TEXTURE_RGB", "EGL_BIND_TO_TEXTURE_RGBA",
                    "EGL_MIN_SWAP_INTERVAL", "EGL_MAX_SWAP_INTERVAL", "EGL_LUMINANCE_SIZE", "EGL_ALPHA_MASK_SIZE",
                    "EGL_COLOR_BUFFER_TYPE", "EGL_RENDERABLE_TYPE", "EGL_CONFORMANT"};
            final int[] value = new int[1];
            for (int i = 0; i < attributes.length; i++) {
                final int attribute = attributes[i];
                final String name = names[i];
                if (egl.eglGetConfigAttrib(display, config, attribute, value)) {
                    if (DEBUG) {
                        Log.w(TAG, String.format("  %s: %d\n", name, value[0]));
                    }
                } else {
                    while (egl.eglGetError() != EGL10.EGL_SUCCESS) {

                    }
                }
            }
        }

        // Subclasses can adjust these values:
        final int mRedSize;
        final int mGreenSize;
        final int mBlueSize;
        final int mAlphaSize;
        final int mDepthSize;
        final int mStencilSize;
        private final int[] mValue = new int[1];
    }

    public void setRenderCallback(final IV8GLViewOnRender listener) {
        mCallback = listener;
    }

    /**
     * Pause rendering. Will tell render thread to sleep.
     */
    public void pause() {
        final RenderThread renderthread = mRenderThread;
        if (renderthread != null) {
            renderthread.pause();
        }
    }

    /**
     * Resume rendering. Will wake render thread up.
     */
    public void unpause() {
        final RenderThread renderthread = mRenderThread;
        if (renderthread != null) {
            renderthread.unpause();
        }
    }

    /**
     * Create the JNI side of OpenGL init. Can be overriden by subclasses to instantiate other BGJSGLView subclasses
     *
     * @return pointer to JNI object
     */
    protected BGJSGLView createGL() {
        final BGJSGLView glView = new BGJSGLView(mEngine, this);
        glView.setViewData(mScaling, mDontClearOnFlip, getMeasuredWidth(), getMeasuredHeight());

        return glView;
    }

    /**
     * The thread were the actual rendering is done. In Android OpenGL operations are always done on a background thread.
     *
     * @author kread
     */
    private class RenderThread extends Thread {
        private static final String TAG = "V8RenderThread";

        static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

        private volatile boolean mFinished = false;    // If the thread is finished and quit
        private final SurfaceTexture mSurface;

        private EGL10 mEgl;
        private EGLDisplay mEglDisplay;
        private EGLConfig mEglConfig;
        private EGLContext mEglContext;
        private EGLSurface mEglSurface;


        private long mLastRenderSec;    // Used to calculate FPS
        private int mRenderCnt;

        private boolean mRenderPending;
        private boolean mReinitPending;


        private boolean mPaused;
        private int[] mEglVersion;
        // True if this surface can preserve color buffer contents on swap
        private boolean mHasSwap;


        RenderThread(final SurfaceTexture surface) {
            mSurface = surface;
        }

        /**
         * Unset paused state and wake up thread
         */
        public void unpause() {
            synchronized (this) {
                mPaused = false;
                notifyAll();
                if (DEBUG) {
                    Log.d(TAG, "Requested unpause");
                }
            }
        }

        /**
         * Set paused state and wake up thread
         */
        void pause() {
            synchronized (this) {
                mPaused = true;
                notifyAll();
                if (DEBUG) {
                    Log.d(TAG, "Requested pause");
                }
            }
        }

        void requestRender() {
            if (mIsShuttingDown) {
                return;
            }
            synchronized (this) {
                mRenderPending = true;
                notifyAll();
                if (DEBUG) {
                    Log.d(TAG, "Requested render mJSID " + mBGJSGLView);
                }
            }
        }

        void reinitGl() {
            synchronized (this) {
                mReinitPending = true;
                notifyAll();
                if (DEBUG) {
                    Log.d(TAG, "Requested reinit");
                }
            }
        }

        @Override
        public void run() {
            if (mFinished) {
                return;
            }
            // This thread might ultimately house Handlers, so initialize a looper
            Looper.prepare();
            if (DEBUG) {
                Log.d(TAG, "Thread running");
            }

            // We try to initialize the OpenGL stack with a valid config
            try {
                if (mFinished) {
                    return;
                }
                initGL();
            } catch (final Exception ex) {
                // If we cannot initialize OpenGL, log it and then quit
                onGLCreateError(ex);
                try {
                    finishGL();
                } catch (final Exception ignored) {
                }
                return;
            }

            if (DEBUG) {
                Log.d(TAG, "GL init done");
            }

            if (mFinished) {
                return;
            }

            if (mClearColorSet) {
                GLES10.glClearColor(mClearRed, mClearGreen, mClearBlue, mClearAlpha);
                GLES10.glClear(GLES10.GL_COLOR_BUFFER_BIT);
            }

            // Create a C instance of GLView and record the native ID
            mBGJSGLView = createGL();

            onGLCreated(mBGJSGLView);

            if (mCallback != null) {
                // Tell clients that rendering has started
                mCallback.renderStarted(V8TextureView.this);
            }

            while (!mFinished) {

                synchronized (this) {
                    while (mPaused && !mFinished) {
                        if (!mRenderPending) {
                            if (DEBUG) {
                                Log.d(TAG, "Paused");
                            }
                            try {
                                wait();
                            } catch (final InterruptedException e) {
                                // the surface was destroyed
                                break;
                            }
                            if (DEBUG) {
                                Log.d(TAG, "Pause done");
                            }
                        } else {
                            if (DEBUG) {
                                Log.d(TAG, "Paused, but resuming becquse rp " + (mRenderPending ? "true" : "false"));
                            }
                            break;
                        }
                    }
                }
                if (mFinished) {
                    break;
                }
                checkCurrent();

                // Draw here
                final long startRender = System.currentTimeMillis();
                final long now = startRender / 1000;
                if (now != mLastRenderSec) {
                    if (DEBUG) {
                        if (LOG_FPS) {
                            Log.d(TAG, "FPS: " + mRenderCnt);
                        }
                    }
                    mRenderCnt = 0;
                    mLastRenderSec = now;
                }
                if (DEBUG) {
                    Log.d(TAG, "Will now draw frame");
                }


                if (mBGJSGLView != null) {
                    mBGJSGLView.onRedraw();
                }

                mRenderCnt++;

                if (mFinished) {
                    break;
                }

                // We don't swap buffers here. Because we don't want to clear buffers on buffer swap, we need to do it in native code.
                // This is important because JS Canvas also doesn't clear except via fillRect
                /* if (!mHasSwap && didDraw) {
                    mEgl.eglSwapBuffers(mEglDisplay, mEglSurface);
                    checkEglError("eglSwapBuffers");
                } */

                // Since we're double buffering, also clear the back buffer
                if (mClearColorSet) {
                    GLES10.glClearColor(mClearRed, mClearGreen, mClearBlue, mClearAlpha);
                    GLES10.glClear(GLES10.GL_COLOR_BUFFER_BIT);
                }

                synchronized (this) {
                    if (mFinished) {
                        break;
                    }
                    // If no rendering or other changes are pending, sleep till the next request
                    if (!mRenderPending && !mFinished) {
                        if (DEBUG) {
                            Log.d(TAG, "No work pending, sleeping");
                        }
                        try {

                            wait(100000);
                        } catch (final InterruptedException e) {
                            // End the loop
                            break;
                        }
                    }

                    if (mFinished) {
                        break;
                    }

                    final long renderDone = System.currentTimeMillis();

                    // To achieve a max of 60 fps we will sleep a tad more if we didn't sleep just now
                    if (renderDone - startRender < 16) {
                        if (DEBUG) {
                            Log.d(TAG, "Last frame was less than 16ms ago, sleeping for "
                                    + (16 - (renderDone - startRender)));
                        }
                        try {
                            Thread.sleep(16 - (renderDone - startRender));
                        } catch (final InterruptedException ignored) {
                        }
                    }
                }
                if (mFinished) {
                    break;
                }

                mRenderPending = false;
                if (mReinitPending && !mFinished) {
                    if (DEBUG) {
                        Log.d(TAG, "Reinit pending executing");
                    }
                    try {
                        reinitGL();
                    } catch (final Exception e) {
                        if (mFinished) {
                            // The surface went away while we were reinitializing, ignore
                            break;
                        }
                        // For now, we assume that this is not fatal but log it
                    }
                    onGLRecreated(mBGJSGLView);
                    mReinitPending = false;
                }

                // The party that created V8TextureView might want to be notified once we have rendered a frame
                onFrameRendered(mBGJSGLView);
            }

            if (mCallback != null) {
                mCallback.renderThreadClosed(V8TextureView.this);
            }

            if (mBGJSGLView != null) {
                mBGJSGLView.onClose();
            }

            mBGJSGLView = null;

            finishGL();
            // Exit the Looper we started
            final Looper looper = Looper.myLooper();
            if (looper != null) {
                if (Build.VERSION.SDK_INT >= 18) {
                    looper.quitSafely();
                } else {
                    looper.quit();
                }
            } else {
                if (DEBUG) {
                    Log.w(TAG, "Renderthread has no looper!");
                }
            }
            mRenderThread = null;
        }

        @SuppressWarnings("unused")
        private void checkEglError(final String prompt) {
            int error;
            while ((error = mEgl.eglGetError()) != EGL10.EGL_SUCCESS) {
                if (DEBUG) {
                    Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
                }
            }
        }

        private void finishGL() {
            mEgl.eglMakeCurrent(mEglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);

            mEgl.eglDestroyContext(mEglDisplay, mEglContext);
            mEgl.eglDestroySurface(mEglDisplay, mEglSurface);
            mEglSurface = null;
            mEglContext = null;
        }

        private void checkCurrent() {
            if (!mEglContext.equals(mEgl.eglGetCurrentContext())
                    || !mEglSurface.equals(mEgl.eglGetCurrentSurface(EGL10.EGL_DRAW))) {
                if (!mEgl.eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
                    final int error = mEgl.eglGetError();
                    if (error == EGL10.EGL_SUCCESS) {
                        return;
                    }
                    throw new RuntimeException("eglMakeCurrent failed " + GLUtils.getEGLErrorString(mEgl.eglGetError()));
                }
            }
        }

        private void reinitGL() {
            mEgl.eglDestroySurface(mEglDisplay, mEglSurface);

            mEgl.eglMakeCurrent(mEglDisplay, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_SURFACE, EGL10.EGL_NO_CONTEXT);

            mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig, mSurface, null);

            if (mEglSurface == null || mEglSurface == EGL10.EGL_NO_SURFACE) {
                final int error = mEgl.eglGetError();
                if (error == EGL10.EGL_BAD_NATIVE_WINDOW) {
                    if (DEBUG) {
                        Log.e(TAG, "createWindowSurface returned EGL_BAD_NATIVE_WINDOW.");
                    }
                    return;
                }
                throw new RuntimeException("createWindowSurface failed " + GLUtils.getEGLErrorString(error));
            }

            if (!mEgl.eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
                throw new RuntimeException("eglMakeCurrent failed " + GLUtils.getEGLErrorString(mEgl.eglGetError()));
            }
        }

        private void initGL() {
            mEgl = (EGL10) EGLContext.getEGL();

            mEglDisplay = mEgl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
            if (mEglDisplay == EGL10.EGL_NO_DISPLAY) {
                throw new RuntimeException("eglGetDisplay failed " + GLUtils.getEGLErrorString(mEgl.eglGetError()));
            }

            final int[] version = new int[2];
            if (!mEgl.eglInitialize(mEglDisplay, version)) {
                throw new RuntimeException("eglInitialize failed " + GLUtils.getEGLErrorString(mEgl.eglGetError()));
            }
            mEglVersion = version;

            final ConfigChooser chooser = new ConfigChooser(8, 8, 8, 0, 0, 8, version);
            mEglConfig = chooser.chooseConfig(mEgl, mEglDisplay);
            if (mEglConfig == null) {
                throw new RuntimeException("eglConfig not initialized");
            }

            mEglContext = createContext(mEgl, mEglDisplay, mEglConfig);

            mEglSurface = mEgl.eglCreateWindowSurface(mEglDisplay, mEglConfig, mSurface, null);

            final int[] surfaceSwapValues = new int[2];
            if (mEgl.eglGetConfigAttrib(mEglDisplay, mEglConfig, EGL14.EGL_SURFACE_TYPE, surfaceSwapValues)) {
                if ((surfaceSwapValues[0] & EGL14.EGL_SWAP_BEHAVIOR_PRESERVED_BIT) != 0) {
                    mHasSwap = true;
                }
            }

            if (mEglSurface == null || mEglSurface == EGL10.EGL_NO_SURFACE) {
                final int error = mEgl.eglGetError();
                if (error == EGL10.EGL_BAD_NATIVE_WINDOW) {
                    if (DEBUG) {
                        Log.e(TAG, "createWindowSurface returned EGL_BAD_NATIVE_WINDOW.");
                    }
                    return;
                }
                throw new RuntimeException("createWindowSurface failed " + GLUtils.getEGLErrorString(error));
            }

            if (!mEgl.eglMakeCurrent(mEglDisplay, mEglSurface, mEglSurface, mEglContext)) {
                if (mEgl.eglGetError() != EGL10.EGL_SUCCESS) {
                    throw new RuntimeException("eglMakeCurrent failed " + GLUtils.getEGLErrorString(mEgl.eglGetError()));
                }
            }

            GLES10.glClearColor(1, 1, 1, 0);
            GLES10.glClear(GLES10.GL_COLOR_BUFFER_BIT);
        }

        EGLContext createContext(final EGL10 egl, final EGLDisplay eglDisplay, final EGLConfig eglConfig) {
            final int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 1, EGL10.EGL_NONE};
            return egl.eglCreateContext(eglDisplay, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
        }

        void finish() {
            mFinished = true;
            synchronized (this) {
                interrupt();
            }
        }

        SurfaceTexture getSurface() {
            return mSurface;
        }
    }

    @Override
    public boolean onSurfaceTextureDestroyed(final SurfaceTexture surface) {
        if (DEBUG) {
            Log.d(TAG, "Finishing thread");
        }
        final RenderThread renderthread = mRenderThread;
        if (renderthread != null) {
            renderthread.finish();
        }

        // If there is no rendering happening we can just release the surface immediately
        return true;
    }

    public void finish() {
        // Mark this finished in any case, even when there is no render thread running yet
        mFinished = true;
        final RenderThread renderthread = mRenderThread;
        if (renderthread != null) {
            renderthread.finish();
            try {
                renderthread.join();
            } catch (InterruptedException e) {
                if (DEBUG) {
                    Log.d(TAG, "Cannot join render thread", e);
                }
            }
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(final SurfaceTexture surface, final int width, final int height) {

        if (mSurfaceWidth == width && mSurfaceHeight == height) {
            return;
        }
        mSurfaceWidth = width;
        mSurfaceHeight = height;

        final RenderThread renderthread = mRenderThread;
        if (renderthread != null) {
            if (DEBUG) {
                Log.d(TAG, "Surface changed " + surface + ", old " + renderthread.getSurface() + ", new width " + width + ", new height " + height + ", old width " + mSurfaceWidth + " height " + mSurfaceHeight);
            }
            renderthread.reinitGl();
            resetTouches();
        }

    }

    @Override
    public void onSurfaceTextureUpdated(final SurfaceTexture surface) {
    }

}
