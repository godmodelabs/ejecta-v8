package ag.boersego.bgjs;

import ag.boersego.bgjs.data.V8UrlCache;
import ag.boersego.bgjs.modules.BGJSModuleAjax;
import ag.boersego.bgjs.modules.BGJSModuleFetch;
import ag.boersego.bgjs.modules.BGJSModuleLocalStorage;
import ag.boersego.bgjs.modules.BGJSModuleWebSocket;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.util.SparseArray;
import androidx.annotation.NonNull;
import okhttp3.OkHttpClient;

import java.io.File;
import java.util.*;
import java.util.concurrent.ThreadPoolExecutor;

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
        JNIV8Object.RegisterV8Class(JNIV8Iterator.class);
    }

    protected static V8Engine mInstance;
    private final boolean mIsTablet;
    private final String mStoragePath;
    protected Handler mHandler;
    private boolean mReady;
    private ArrayList<V8EngineHandler> mHandlers = null;
    private final String mLocale;
    private final String mLang;
    private final String mTimeZone;
    private float mDensity;
    private boolean mPaused;

    private static final String TAG = V8Engine.class.getSimpleName();

    @SuppressWarnings("PointlessBooleanExpression")
    private boolean mDebug = false && BuildConfig.DEBUG;

    private ArrayList<JNIV8Module> mModules = new ArrayList<>();

    public void doDebug(final boolean debug) {
        mDebug = debug;
    }

    public void unpause() {
        if (!mPaused) {
            return;
        }
        mPaused = false;

        for (final JNIV8Module module : mModules) {
            if (module instanceof JNIV8Module.IJNIV8Suspendable) {
                ((JNIV8Module.IJNIV8Suspendable) module).onResume();
            }
        }
    }


    public void pause() {
        mPaused = true;

        for (final JNIV8Module module : mModules) {
            if (module instanceof JNIV8Module.IJNIV8Suspendable) {
                ((JNIV8Module.IJNIV8Suspendable) module).onSuspend();
            }
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

    public V8Engine(final @NonNull Context application, final boolean isStoreBuild) {
        final AssetManager assetManager = application.getAssets();
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

        jsThread = new Thread(new V8EngineRunnable(assetManager, isStoreBuild));
        jsThread.setName("EjectaV8JavaScriptContext");
        jsThread.start();
    }

    public void setUrlCache(final V8UrlCache cache) {
        BGJSModuleAjax.getInstance().setUrlCache(cache);
    }

    public void setTPExecutor(final ThreadPoolExecutor executor) {
        BGJSModuleAjax.getInstance().setExecutor(executor);
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

    private Thread jsThread = null;

    /**
     * This Runnable is the main loop of a separate Thread where v8 code is executed.
     */
    class V8EngineRunnable implements Runnable {
        private final boolean isStoreBuild;
        private AssetManager assetManager;

        V8EngineRunnable(AssetManager assetManager, boolean isStoreBuild) {
            this.assetManager = assetManager;
            this.isStoreBuild = isStoreBuild;
        }

        @Override
        public void run() {

            try {
                Thread.sleep(10);
            } catch (final InterruptedException e) {
                e.printStackTrace();
            }
            Log.d(TAG, "Initializing V8Engine");
            final int maxHeapSizeForV8 = (int) (Runtime.getRuntime().maxMemory() / 1024 / 1024 / 3);
            if (mDebug) {
                Log.d(TAG, "Max heap size for v8 is " + maxHeapSizeForV8 + " MB");
            }

            initialize(assetManager, mLocale, mLang, mTimeZone, mDensity, mIsTablet ? "tablet" : "phone", mDebug, isStoreBuild, maxHeapSizeForV8);
            assetManager = null;

            Looper.prepare();
            mHandler = new Handler(V8Engine.this);

            mHandler.sendMessageAtFrontOfQueue(mHandler.obtainMessage(MSG_READY));
            Looper.loop();
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
                onReady();
                return true;
        }
        return false;
    }

    /**
     * Override to be notified once the engine is ready
     */
    protected void onReady() {
        if (mHandlers != null) {
            for (final V8EngineHandler h : mHandlers) {
                h.onReady();
            }
            mHandlers.clear();
        }
    }

    public void setHttpClient(final OkHttpClient client) {
        BGJSModuleAjax.getInstance().setHttpClient(client);
        registerModule(new BGJSModuleWebSocket(client));
        registerModule(new BGJSModuleFetch(client));
    }


    public native void shutdown();

    private native void initialize(AssetManager am, String locale, String lang, String timezone,
                                   float density, final String deviceClass, final boolean debug, final boolean isStoreBuild, final int maxHeapSizeInMb);

    // public void loadURL(String URL)
    private static final int MSG_QUIT = 2;
    private static final int MSG_LOAD = 3;
    private static final int MSG_READY = 5;
}
