package ag.boersego.bgjs;

import android.annotation.SuppressLint;
import android.app.Application;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.util.Log;
import android.util.SparseArray;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.TimeZone;
import java.util.concurrent.ThreadPoolExecutor;

import ag.boersego.bgjs.data.V8UrlCache;
import ag.boersego.bgjs.modules.BGJSModuleAjax;
import ag.boersego.bgjs.modules.BGJSModuleLocalStorage;
import ag.boersego.bgjs.modules.BGJSModuleWebSocket;
import okhttp3.OkHttpClient;

/**
 * v8Engine
 * This class handles all lifetime and thread-related data around a v8 instance.
 * <p>
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 **/

@SuppressWarnings("unused")
@SuppressLint("LogNotTimber")
public class V8Engine extends JNIObject implements Handler.Callback {
    static {
        System.loadLibrary("bgjs");
        JNIV8Object.RegisterAliasForPrimitive(Number.class, Double.class);
        // Register kotlin primitives (which from the viewpoint of JNI are classes that don't have a classloader!)
        JNIV8ObjectKt.registerKotlinAliases();
    }

    protected static V8Engine mInstance;
    private final boolean mIsTablet;
    private final String mStoragePath;
    protected Handler mHandler;
    private boolean mReady;
    private ArrayList<V8EngineHandler> mHandlers = null;
    private final SparseArray<V8Timeout> mTimeouts = new SparseArray<>(50);
    private int mLastTimeoutId = 1;
    private final HashSet<V8Timeout> mTimeoutsToGC = new HashSet<>();
    private V8Timeout mRunningTO;
    private final String mLocale;
    private final String mLang;
    private final String mTimeZone;
    private final HashMap<String, ArrayList<V8EventCB>> mEvents = new HashMap<>();
    private float mDensity;
    private boolean mPaused;

    private static final String TAG = V8Engine.class.getSimpleName();

    @SuppressWarnings("PointlessBooleanExpression")
    private boolean mDebug = false && BuildConfig.DEBUG;
    private final ArrayList<Runnable> mNextTickQueue = new ArrayList<>();
    private boolean mJobQueueActive = false;
    private final Runnable mQueueWaitRunnable = () -> {
        while (true) {
            synchronized (mNextTickQueue) {
                mJobQueueActive = true;
                if (mNextTickQueue.isEmpty() || mPaused) {
                    if (mDebug && mPaused) {
                        Log.d(TAG, "enqueued jobs quit early because of suspend");
                    }
                    mJobQueueActive = false;
                    return;
                }
            }
            final long v8Locker = lock();
            try {
                final ArrayList<Runnable> jobsToRun;

                synchronized (mNextTickQueue) {
                    jobsToRun = new ArrayList<>(mNextTickQueue);
                    mNextTickQueue.clear();
                }

                for (final Runnable r : jobsToRun) {
                    r.run();
                }
            } finally {
                unlock(v8Locker);
            }
        }
    };

    private ArrayList<V8Timeout> mTimeoutsToAddAfterPause = new ArrayList<>();
    private ArrayList<JNIV8Module> mModules = new ArrayList<>();

    public void doDebug(final boolean debug) {
        mDebug = debug;
    }

    public void unpause() {
        if (!mPaused) {
            return;
        }
        mPaused = false;

        if (mHandler != null) {
            mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
        }

        for (final JNIV8Module module : mModules) {
            if (module instanceof JNIV8Module.IJNIV8Suspendable) {
                ((JNIV8Module.IJNIV8Suspendable) module).onResume();
            }
        }

        synchronized (mTimeouts) {
            // Check if there timeouts that were requested after we went into pause state
            for (final V8Timeout to : mTimeoutsToAddAfterPause) {
                mTimeouts.append(to.id, to);
                mHandler.postDelayed(to, to.timeout);
                if (mDebug) {
                    Log.d(TAG, "setTimeout unpaused added instance " + to + ", to " + to.timeout + ", id " + to.id + ", recurring " + to.recurring);
                }
            }
            mTimeoutsToAddAfterPause.clear();
        }

        // Check if we need to enqueue functions on a next tick, since these were not enqueued if the
        // engine was already paused.
        enqueueAndStartProcessing(null);
    }

    private boolean enqueueAndStartProcessing(@Nullable final Runnable jobToEnqueue) {
        final boolean startOnEmptyQueue = jobToEnqueue != null;
        final boolean startBusyWaiting;
        synchronized (mNextTickQueue) {
            startBusyWaiting = mNextTickQueue.isEmpty() == startOnEmptyQueue && !mJobQueueActive;
            if (jobToEnqueue != null) {
                mNextTickQueue.add(jobToEnqueue);
            }
        }

        if (mDebug) {
            Log.d(TAG, "unpause: starting enqueued jobs");
        }

        if (startBusyWaiting) {
            if (Thread.currentThread() == jsThread) {
                // We cannot really enqueue on our own thread!!
                new Thread(mQueueWaitRunnable).start();
            } else {
                mHandler.postAtFrontOfQueue(mQueueWaitRunnable);
            }
        }
        return startBusyWaiting;
    }

    public void pause() {
        mPaused = true;
        if (mHandler != null) {
            mHandler.removeMessages(MSG_CLEANUP);
        }

        for (final JNIV8Module module : mModules) {
            if (module instanceof JNIV8Module.IJNIV8Suspendable) {
                ((JNIV8Module.IJNIV8Suspendable) module).onSuspend();
            }
        }

        if (mQueueWaitRunnable != null && mHandler != null) {
            mHandler.removeCallbacks(mQueueWaitRunnable);
        }
    }

    /**
     * Execute a Runnable within a v8 level lock on this v8 engine and hence this v8 Isolate.
     *
     * @param runInLocker the Runnable to execute with the lock held
     * @return the result of the block
     */
    public void runLocked(final Runnable runInLocker) {
        final long lockerInst = lock();
        try {
            runInLocker.run();
        } finally {
            unlock(lockerInst);
        }
    }

    /**
     * Enqueue any Runnable to be executed on the next tick
     *
     * @param runnable the function to execute once the currently executing JS block has relinquished control
     * @return true if it had to be scheduled, false if other functions were queued already
     */
    public boolean enqueueOnNextTick(final Runnable runnable) {
        final boolean startBusyWaiting = enqueueAndStartProcessing(runnable);

        return startBusyWaiting;
    }

    /**
     * Enqueue a wrapped v8 function to be executed on the next tick
     *
     * @param function the function to execute once the currently executing JS block has relinquished control
     * @return true if it had to be scheduled, false if other functions were queued already
     */
    public boolean enqueueOnNextTick(final JNIV8Function function) {
        return enqueueOnNextTick(function::callAsV8Function);
    }

    public interface V8EngineHandler {
        void onReady();
    }

    public class V8EventCB {
        public final long cbPtr;
        final long thisPtr;
        public final String event;

        V8EventCB(final long cbPtr, final long thisPtr, final String event) {
            this.cbPtr = cbPtr;
            this.thisPtr = thisPtr;
            this.event = event;
        }
    }

    private class V8Timeout implements Runnable {
        final long jsCbPtr;
        final long thisObjPtr;
        final long timeout;
        final boolean recurring;
        private final int id;
        private boolean dead;

        V8Timeout(final long jsCbPtr, final long thisObjPtr, final long timeout, final boolean recurring, final int id) {
            this.jsCbPtr = jsCbPtr;
            this.thisObjPtr = thisObjPtr;
            this.timeout = timeout;
            this.recurring = recurring;
            this.id = id;
        }

        void setAsDead() {
            dead = true;
        }

        @Override
        public void run() {
            mRunningTO = this;
            if (dead) {
                mTimeoutsToGC.add(this);
                if (!mHandler.hasMessages(MSG_CLEANUP)) {
                    mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
                }
                return;
            }
            if (mDebug) {
                Log.d(TAG, "timeout ready (id " + id + ") to " + timeout + ", now calling cb " + jsCbPtr);
            }
            // synchronized(BGJSPushHelper.getInstance(null)) {
            ClientAndroid.timeoutCB(V8Engine.this, jsCbPtr, thisObjPtr, false, true);
            // }
            synchronized (mTimeouts) {
                if (dead) {
                    mTimeoutsToGC.add(this);
                    if (!mHandler.hasMessages(MSG_CLEANUP)) {
                        mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
                    }
                    return;
                }
                if (!recurring) {
                    mTimeouts.remove(id);
                    if (mDebug) {
                        Log.d(TAG, "timeout deleted cb id " + id + ", " + jsCbPtr);
                    }
                    mTimeoutsToGC.add(this);
                } else {
                    mHandler.postDelayed(this, timeout);
                    if (mDebug) {
                        Log.d(TAG, "Re-posting recurring timer id " + id + ", " + jsCbPtr);
                    }
                }
            }
        }
    }

    protected V8Engine(final Application application, final boolean isStoreBuild) {
        final AssetManager assetManager;
        if (application != null) {
            assetManager = application.getAssets();
            final Resources r = application.getResources();
            if (r != null) {
                mDensity = r.getDisplayMetrics().density;
                mIsTablet = r.getBoolean(R.bool.isTablet);
            } else {
                throw new RuntimeException("No resources available");
            }

            // Lets check of external storage is available, otherwise let's use internal storage
            File cacheDir = application.getExternalCacheDir();
            if (cacheDir == null) {
                cacheDir = application.getCacheDir();
            }
            mStoragePath = cacheDir.toString();
        } else {
            throw new RuntimeException("Application is null");
        }
        final Locale locale = Locale.getDefault();
        final String country = locale.getCountry();
        if (country.isEmpty()) {
            mLocale = locale.getLanguage();
        } else {
            mLocale = locale.getLanguage() + "_" + country;
        }
        mLang = locale.getLanguage();
        mTimeZone = TimeZone.getDefault().getID();

        // Register bundled Java-bridged JS modules
        registerModule(BGJSModuleAjax.getInstance());
        registerModule(new BGJSModuleLocalStorage(application.getApplicationContext()));

        // start thread
        initializeV8(assetManager, isStoreBuild);

        jsThread = new Thread(new V8EngineRunnable());
        jsThread.setName("EjectaV8JavaScriptContext");
        jsThread.start();
    }

    public void setUrlCache(final V8UrlCache cache) {
        BGJSModuleAjax.getInstance().setUrlCache(cache);
    }

    public void setTPExecutor(final ThreadPoolExecutor executor) {
        BGJSModuleAjax.getInstance().setExecutor(executor);
    }

    public static boolean isReady() {
        return getInstance().mReady;
    }

    public synchronized static V8Engine getInstance(final Application app, final boolean isStoreBuild) {
        if (mInstance == null) {
            if (app == null) {
                throw new RuntimeException("V8Engine hasn't been initialized");
            }
            mInstance = new V8Engine(app, isStoreBuild);
        }
        return mInstance;
    }

    @NonNull
    public static V8Engine getInstance() {
        return getInstance(null, true);
    }

    @Nullable
    public static V8Engine getCachedInstance() {
        return mInstance;
    }

    public void initializeV8(final AssetManager assetManager, final boolean isStoreBuild) {
        try {
            Thread.sleep(100);
        } catch (final InterruptedException e) {
            e.printStackTrace();
        }
        Log.d(TAG, "Initializing V8Engine");
        final int maxHeapSizeForV8 = (int) (Runtime.getRuntime().maxMemory() / 1024 / 1024 / 3);
        if (mDebug) {
            Log.d(TAG, "Max heap size for v8 is " + maxHeapSizeForV8 + " MB");
        }

        ClientAndroid.initialize(assetManager, this, mLocale, mLang, mTimeZone, mDensity, mIsTablet ? "tablet" : "phone", mDebug, isStoreBuild, maxHeapSizeForV8);
    }

    private native void registerModuleNative(JNIV8Module module);

    public void registerModule(final JNIV8Module module) {
        registerModuleNative(module);
        mModules.add(module);
    }

    public JNIV8Function getConstructor(final Class<? extends JNIV8Object> jniv8class) {
        return getConstructor(jniv8class.getCanonicalName());
    }

    private native JNIV8Function getConstructor(String canonicalName);

    public native Object parseJSON(String json);

    public native Object runScript(String script, String name);

    public native Object require(String file);

    /**
     * Dumps v8 heap to filen
     *
     * @return the path to the file
     */
    private native String dumpHeap(String path);

    public String dumpV8Heap() {
        synchronized (this) {
            return dumpHeap(mStoragePath);
        }
    }

    public native JNIV8GenericObject getGlobalObject();

    /**
     * Create a v8::Locker and return the pointer to the instance
     *
     * @return pointer to v8::Locker
     */
    private native long lock();

    /**
     * Destroy / Leave a v8::Locker
     *
     * @param lockerPtr the pointer to the Locker instance
     */
    private native void unlock(long lockerPtr);

    private Thread jsThread = null;

    /**
     * This Runnable is the main loop of a separate Thread where v8 code is executed.
     */
    class V8EngineRunnable implements Runnable {
        V8EngineRunnable() {
        }

        @Override
        public void run() {
            Looper.prepare();
            mHandler = new Handler(V8Engine.this);

            mHandler.sendMessageAtFrontOfQueue(mHandler.obtainMessage(MSG_READY));
            mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
            Looper.loop();
        }
    }

    private void onFromJsInstance(final String event, final long cbPtr, final long thisPtr) {
        ArrayList<V8EventCB> eventList = mInstance.mEvents.get(event);
        if (eventList == null) {
            eventList = new ArrayList<>();
            mInstance.mEvents.put(event, eventList);
        }

        if (mDebug) {
            Log.d(TAG, "Adding on " + event + ", " + cbPtr + ", " + thisPtr);
        }
        final V8EventCB cb = mInstance.new V8EventCB(cbPtr, thisPtr, event);
        eventList.add(cb);
    }

    public static void onFromJS(final String event, final long cbPtr, final long thisPtr) {
        synchronized (mInstance.mEvents) {
            mInstance.onFromJsInstance(event, cbPtr, thisPtr);
        }
    }

    public synchronized void addStatusHandler(final V8EngineHandler h) {
        if (mReady) {
            h.onReady();
            return;
        }
        if (mHandlers == null) {
            mHandlers = new ArrayList<>(1);
        }
        mHandlers.add(h);
    }

    public synchronized void removeStatusHandler(final V8EngineHandler handler) {
        if (mHandlers == null) {
            return;
        }
        mHandlers.remove(handler);
    }

    @Override
    public boolean handleMessage(final Message msg) {
        switch (msg.what) {
            case MSG_CLEANUP:
                cleanup();
                mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
                return true;
            case MSG_QUIT:
                final Looper looper = Looper.myLooper();
                if (looper != null) {
                    looper.quit();
                }
                return true;
            case MSG_LOAD:
                return true;
            case MSG_READY:
                mReady = true;
                if (mHandlers != null) {
                    for (final V8EngineHandler h : mHandlers) {
                        h.onReady();
                    }
                    mHandlers.clear();
                }
                return true;
        }
        return false;
    }


    public void cleanup() {
        V8Timeout[] timeOutCopy;
        synchronized (mTimeouts) {
            timeOutCopy = new V8Timeout[mTimeoutsToGC.size()];
            timeOutCopy = mTimeoutsToGC.toArray(timeOutCopy);
            mTimeoutsToGC.clear();
        }
        try {
            final int count = mTimeoutsToGC.size();
            for (final V8Timeout to : timeOutCopy) {
                ClientAndroid.timeoutCB(this, to.jsCbPtr, to.thisObjPtr, true, false);
            }
            if (mDebug) {
                Log.d(TAG, "Cleaned up " + count + " timeouts");
            }
        } catch (final Exception ex) {
            Log.i(TAG, "Couldn't clear timeoutsGC", ex);
        }
    }

    public static int setTimeout(final long jsCbPtr, final long thisObjPtr, final long timeout, final boolean recurring) {
        if (mInstance != null) {
            return mInstance.setTimeoutInst(jsCbPtr, thisObjPtr, timeout, recurring);
        } else {
            Log.e(TAG, "Cannot do setTimeout when no instance of V8Engine there");
        }
        return -1;
    }

    private int setTimeoutInst(final long jsCbPtr, final long thisObjPtr, final long timeout, final boolean recurring) {
        synchronized (mTimeouts) {
            final int id = mLastTimeoutId++;
            final V8Timeout to = new V8Timeout(jsCbPtr, thisObjPtr, timeout, recurring, id);
            if (mPaused) {
                mTimeoutsToAddAfterPause.add(to);
            } else {
                mTimeouts.append(id, to);
                mHandler.postDelayed(to, timeout);
                if (mDebug) {
                    Log.d(TAG, "setTimeout added instance " + to + ", to " + timeout + ", id " + id + ", recurring " + recurring);
                }
            }
            return id;
        }
    }

    public static void removeTimeout(final int id) {
        if (mInstance != null) {
            mInstance.removeTimeoutInst(id);
        } else {
            Log.e(TAG, "Cannot do removeTimeout when no instance of V8Engine there");
        }
    }

    private void removeTimeoutInst(final int id) {
        synchronized (mTimeouts) {
            final V8Timeout to = mTimeouts.get(id);
            if (to != null) {
                to.setAsDead();
                mHandler.removeCallbacks(to, null);
                if (mDebug) {
                    Log.d(TAG, "Removed timeout (clearTimeout) " + id);
                }
                mTimeoutsToAddAfterPause.remove(to);
                mTimeouts.remove(id);

                if (mRunningTO != to) {
                    mTimeoutsToGC.add(to);
                }
                if (!mHandler.hasMessages(MSG_CLEANUP)) {
                    mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
                }
            } else {
                Log.i(TAG, "Couldn't remove timeout (clearTimeout) " + id);
            }
        }
    }

    public void setHttpClient(final OkHttpClient client) {
        BGJSModuleAjax.getInstance().setHttpClient(client);
        registerModule(new BGJSModuleWebSocket(client));
    }


    public void shutdown() {
        mHandler.sendEmptyMessage(MSG_QUIT);
    }

    // public void loadURL(String URL)
    private static final int MSG_CLEANUP = 1;
    private static final int MSG_QUIT = 2;
    private static final int MSG_LOAD = 3;
    private static final int MSG_READY = 5;


    public static final int TICK_SLEEP = 250;
    private static final int DELAY_CLEANUP = 10 * 1000;


}
