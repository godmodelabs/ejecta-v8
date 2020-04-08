package ag.boersego.bgjs;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.support.annotation.NonNull;
import android.util.Log;

import java.io.File;
import java.util.*;

/**
 * v8Engine
 * This class handles all lifetime and thread-related data around a v8 instance.
 * <p>
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 **/

@SuppressWarnings("unused")
@SuppressLint("LogNotTimber")
public class V8Engine extends JNIObject {
    static {
        System.loadLibrary("bgjs");
        JNIV8Object.RegisterAliasForPrimitive(Number.class, Double.class);
        // Register kotlin primitives (which from the viewpoint of JNI are classes that don't have a classloader!)
        JNIV8ObjectKt.registerKotlinAliases();
        JNIV8Object.RegisterV8Class(JNIV8Iterator.class);
    }

    protected static V8Engine mInstance;
    private String mStoragePath;
    protected Handler mHandler;
    private boolean mReady;
    private ArrayList<V8EngineHandler> mHandlers = null;

    private static final String TAG = V8Engine.class.getSimpleName();

    private ArrayList<JNIV8Module> mModules = new ArrayList<>();

    public native void pause();

    public native void unpause();

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
     * Enqueue a wrapped v8 function to be executed on the next tick
     *
     * @param function the function to execute once the currently executing JS block has relinquished control
     */
    public native void enqueueOnNextTick(JNIV8Function function);

    public void enqueueOnNextTick(Runnable runnable) {
        this.enqueueOnNextTick(JNIV8Function.Create(this, (Object receiver, Object[] arguments) -> {
            runnable.run();
            return JNIV8Undefined.GetInstance();
        }));
    }

    public interface V8EngineHandler {
        void onReady();
    }

    public void start(final @NonNull Context application) {
        _initialize(application, "node_modules/");
    }

    public void start(final @NonNull Context application, String commonJSPath) {
        _initialize(application, commonJSPath);
    }

    private void _initialize(final @NonNull Context application, String commonJSPath) {
        // Lets check of external storage is available, otherwise let's use internal storage
        File cacheDir = application.getExternalCacheDir();
        if (cacheDir == null) {
            cacheDir = application.getCacheDir();
        }
        mStoragePath = cacheDir.toString();

        // this will create an eventloop thread on the native side
        // intitialization of the v8 context & the `onReady` callback will run inside of that thread
        final int maxHeapSizeForV8 = (int) (Runtime.getRuntime().maxMemory() / 1024 / 1024 / 3);
        initialize(application.getAssets(), commonJSPath, maxHeapSizeForV8);
    }

    public boolean isReady() {
        return mReady;
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

    /**
     * this method is responsible for handling exceptions that are raised by
     * asynchronously triggered javascript code (promises, timeouts, intervals)
     * these exceptions are always thrown on to the main thread directly to guarantee identical stack traces
     * and to make it impossible for any other code to catch them
     * @param error
     */
    private void onThrow(RuntimeException error) {
        Log.e(TAG, "Exception encountered in javascript code; application will crash", error);
        Handler mainHandler = new Handler(Looper.getMainLooper());
        mainHandler.post(() -> {
            throw error;
        });
    }

    /**
     * Override to be notified once the engine is ready
     */
    protected void onReady() {
        mReady =  true;
        if (mHandlers != null) {
            for (final V8EngineHandler h : mHandlers) {
                h.onReady();
            }
            mHandlers.clear();
        }
    }

    /**
     * Override to be notified once the engine is suspended
     */
    protected void onSuspend() {
        for (final JNIV8Module module : mModules) {
            if (module instanceof JNIV8Module.IJNIV8Suspendable) {
                ((JNIV8Module.IJNIV8Suspendable) module).onSuspend();
            }
        }
    }

    /**
     * Override to be notified once the engine is resumed
     */
    protected void onResume() {
        for (final JNIV8Module module : mModules) {
            if (module instanceof JNIV8Module.IJNIV8Suspendable) {
                ((JNIV8Module.IJNIV8Suspendable) module).onResume();
            }
        }
    }

    public native void shutdown();

    private native void initialize(AssetManager am, String commonJSPath, final int maxHeapSizeInMb);
}
