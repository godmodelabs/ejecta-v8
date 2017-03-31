package ag.boersego.bgjs;

import android.app.Application;
import android.content.res.AssetManager;
import android.content.res.Resources;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.Log;
import android.util.SparseArray;

import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.TimeZone;
import java.util.concurrent.ThreadPoolExecutor;

import ag.boersego.bgjs.data.AjaxRequest;
import ag.boersego.bgjs.data.V8UrlCache;
import okhttp3.OkHttpClient;

/**
 * v8Engine
 * This class handles all lifetime and thread-related data around a v8 instance.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 **/

public class V8Engine extends Thread implements Handler.Callback {

	protected static V8Engine mInstance;
	protected Handler mHandler;
	private String scriptPath;
	private AssetManager assetManager;
	private boolean mReady;
	private ArrayList<V8EngineHandler> mHandlers = null;
	private final SparseArray<V8Timeout> mTimeouts = new SparseArray<V8Timeout>(50);
	protected long mNativePtr;
	private int mLastTimeoutId = 1;
	private final HashSet<V8Timeout> mTimeoutsToGC = new HashSet<V8Timeout>();
	private V8Timeout mRunningTO;
	protected final String mLocale;
	protected final String mLang;
	protected final String mTimeZone;
	private final HashMap<String, ArrayList<V8EventCB> > mEvents = new HashMap<String, ArrayList<V8EventCB> >();
	protected float mDensity;

	private static final String TAG = "V8Engine";
	private static boolean DEBUG = false && BuildConfig.DEBUG;
    private V8UrlCache mCache;
	private ThreadPoolExecutor mTPExecutor;
    private OkHttpClient mHttpClient;

    public static void doDebug (boolean debug) {
        DEBUG = debug;
    }
	
	public interface V8EngineHandler {
		abstract public void onReady();
	}
	
	public class V8EventCB {
		public final long cbPtr;
		public final long thisPtr;
		public final String event;
		
		public V8EventCB (long cbPtr, long thisPtr, String event) {
			this.cbPtr = cbPtr;
			this.thisPtr = thisPtr;
			this.event = event;
		}
	}
	
	public class V8Timeout implements Runnable {
		final long jsCbPtr;
		final long thisObjPtr;
		final long timeout;
		final boolean recurring;
		private final int id;
		private boolean dead;
		
		public V8Timeout (long jsCbPtr, long thisObjPtr, long timeout, boolean recurring, int id) {
			this.jsCbPtr = jsCbPtr;
			this.thisObjPtr = thisObjPtr;
			this.timeout = timeout;
			this.recurring = recurring;
			this.id = id;
		}
		
		public void setAsDead() {
			this.dead = true;
		}

		@Override
		public void run() {
			mRunningTO = this;
			if (this.dead) {
				mTimeoutsToGC.add(this);
				if (!mHandler.hasMessages(MSG_CLEANUP)) {
					mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
				}
				return;
			}
			if (DEBUG) { Log.d (TAG, "timeout ready (id " + id + ") to " + timeout + ", now calling cb " + jsCbPtr); }
			// synchronized(BGJSPushHelper.getInstance(null)) {
				ClientAndroid.timeoutCB(mNativePtr, jsCbPtr, thisObjPtr, false, true);
			// }
			synchronized (mTimeouts) {
				if (this.dead) {
					mTimeoutsToGC.add(this);
					if (!mHandler.hasMessages(MSG_CLEANUP)) {
						mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
					}
					return;
				}
				if (!recurring) {
					mTimeouts.remove(id);
					if (DEBUG) {
						Log.d (TAG, "timeout deleted cb id " + id + ", " + jsCbPtr);
					}
					mTimeoutsToGC.add(this);
				} else {
					mHandler.postDelayed(this, timeout);
					if (DEBUG) {
						Log.d (TAG, "Re-posting recurring timer id " + id + ", " + jsCbPtr);
					}
				}
			}
		}
	}

	protected V8Engine(Application application, String path) {
        if (path != null) {
            scriptPath = path;
        }
        if (application != null) {
            assetManager = application.getAssets();
            final Resources r = application.getResources();
            if (r != null) {
                mDensity = r.getDisplayMetrics().density;
            }
        }
        Locale locale = Locale.getDefault();
		String country = locale.getCountry();
		if (country.isEmpty()) {
			mLocale = locale.getLanguage();
		} else {
			mLocale = locale.getLanguage() + "_" + country;
		}
		mLang = locale.getLanguage();
		mTimeZone = TimeZone.getDefault().getID();
		
	}

    public void setUrlCache (V8UrlCache cache) {
        mCache = cache;
    }

	public void setTPExecutor(final ThreadPoolExecutor executor) {
		mTPExecutor = executor;
	}

    public long getNativePtr() {
		return mNativePtr;
	}
	
	public static boolean isReady () {
		return getInstance().mReady;
	}

	public synchronized static V8Engine getInstance(Application app, String path) {
		if (mInstance == null) {
            if(app == null || path == null) {
                throw new RuntimeException("V8Engine hasn't been initialized");
            }
			mInstance = new V8Engine(app, path);

			mInstance.start();
		}
		return mInstance;
	}

	public static V8Engine getInstance() {
		return getInstance(null, null);
	}

    public static V8Engine getCachedInstance() {
        return mInstance;
    }

    public void initializeV8(AssetManager assetManager) {
		try {
			Thread.sleep(100);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		mNativePtr = ClientAndroid.initialize(assetManager, this, mLocale, mLang, mTimeZone, mDensity);
    }

	@Override
	public void run() {
		this.setName("V8Engine");
		
		System.loadLibrary("bgjs");

		Looper.prepare();
		mHandler = new Handler (this);
        initializeV8(assetManager);

		assetManager = null;
		ClientAndroid.load(mNativePtr, scriptPath);
		ClientAndroid.run(mNativePtr);

		mHandler.sendMessageAtFrontOfQueue(mHandler.obtainMessage(MSG_READY));
		mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
		Looper.loop();
	}

    protected void onFromJsInstance (String event, long cbPtr, long thisPtr) {
        ArrayList<V8EventCB> eventList = mInstance.mEvents.get(event);
        if (eventList == null) {
            eventList = new ArrayList<V8Engine.V8EventCB>();
            mInstance.mEvents.put(event, eventList);
        }

        if (DEBUG) {
            Log.d (TAG, "Adding on " + event + ", " + cbPtr + ", " + thisPtr);
        }
        V8EventCB cb = mInstance.new V8EventCB(cbPtr, thisPtr, event);
        eventList.add (cb);
    }
	
	public static void onFromJS (String event, long cbPtr, long thisPtr) {
		synchronized (mInstance.mEvents) {
            mInstance.onFromJsInstance(event, cbPtr, thisPtr);
		}
	}
	
	protected void callOnCBBool(String event, boolean b) {
		synchronized (mEvents) {
			ArrayList<V8EventCB> eventList = mEvents.get(event);
			if (eventList == null) {
				if (DEBUG) {
					Log.i (TAG, "No listeners for event " + event);
				}
				return;
			}
			
			for (V8EventCB cb : eventList) {
				if (DEBUG) {
					Log.d (TAG, "Calling on " + event + ", " + cb.cbPtr);
				}
				ClientAndroid.runCBBoolean(mNativePtr, cb.cbPtr, cb.thisPtr, b);
			}
		}
	}

	
	public synchronized void addStatusHandler (V8EngineHandler h) {
		if (mReady) {
			h.onReady();
			return;
		}
		if (mHandlers == null) {
			mHandlers = new ArrayList<V8EngineHandler>(1);
		}
		mHandlers.add(h);
	}

    public synchronized void removeStatusHandler(final V8EngineHandler handler) {
        if (mHandlers == null) {
            return;
        }
        mHandlers.remove(handler);
    }

    public boolean handleMessage (Message msg) {
        switch (msg.what) {
            case MSG_CLEANUP:
                cleanup();
                mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
                return true;
            case MSG_QUIT:
                Looper.myLooper().quit();
                return true;
            case MSG_LOAD:
                return true;
            case MSG_AJAX:
                V8AjaxRequest req = (V8AjaxRequest) msg.obj;
                if (req != null) {
                    req.doCallBack();
                }
                return true;
            case MSG_READY:
                mReady = true;
                if (mHandlers != null) {
                    for (V8EngineHandler h : mHandlers) {
                        h.onReady();
                    }
					mHandlers.clear();
                }
                return true;
        }
        return false;
    }

	
	public void cleanup () {
		V8Timeout[] timeOutCopy;
		synchronized (mTimeouts) {
			timeOutCopy = new V8Timeout[mTimeoutsToGC.size()];
			timeOutCopy = (V8Timeout[]) mTimeoutsToGC.toArray(timeOutCopy);
			mTimeoutsToGC.clear();
		}
		try {
			final int count = mTimeoutsToGC.size();
			for (V8Timeout to : timeOutCopy) {
				ClientAndroid.timeoutCB(mNativePtr, to.jsCbPtr, to.thisObjPtr, true, false);
			}
			if (DEBUG) {
				Log.d (TAG, "Cleaned up " + count + " timeouts");
			}
		} catch (Exception ex) {
			Log.i (TAG, "Couldn't clear timeoutsGC", ex);
		}
	}
	
	public static int setTimeout (long jsCbPtr, long thisObjPtr, long timeout, boolean recurring) {
		if (mInstance != null) {
			return mInstance.setTimeoutInst (jsCbPtr, thisObjPtr, timeout, recurring);
		} else {
			Log.e (TAG, "Cannot do setTimeout when no instance of V8Engine there");
		}
		return -1;
	}
	
	private int setTimeoutInst(long jsCbPtr, long thisObjPtr, long timeout, boolean recurring) {
		synchronized(mTimeouts) {
			int id = mLastTimeoutId++;
			V8Timeout to = new V8Timeout(jsCbPtr, thisObjPtr, timeout, recurring, id);
			mTimeouts.append(id, to);
			mHandler.postDelayed(to, timeout);
			if (DEBUG) { Log.d (TAG, "setTimeout added instance " + to + ", to " + timeout + ", id " + id + ", recurring " + recurring); }
			return id;
		}
	}

	public static void removeTimeout(int id) {
		if (mInstance != null) {
			mInstance.removeTimeoutInst(id);
		} else {
			Log.e (TAG, "Cannot do removeTimeout when no instance of V8Engine there");
		}
	}
	
	void removeTimeoutInst(int id) {
		synchronized (mTimeouts) {
			V8Timeout to = mTimeouts.get(id);
			if (to != null) {
				to.setAsDead();
				mHandler.removeCallbacks(to, null);
				if (DEBUG) {
					Log.d (TAG, "Removed timeout (clearTimeout) " + id);
				}
				mTimeouts.remove(id);
				
				if (mRunningTO != to) {
					mTimeoutsToGC.add(to);
				}
				if (!mHandler.hasMessages(MSG_CLEANUP)) {
					mHandler.sendMessageDelayed(mHandler.obtainMessage(MSG_CLEANUP), DELAY_CLEANUP);
				}
			} else {
				Log.i (TAG, "Couldn't remove timeout (clearTimeout) " + id);
			}
		}
	}

	public void setHttpClient(final OkHttpClient client) {
		mHttpClient = client;
	}

	public class V8AjaxRequest implements AjaxRequest.AjaxListener {
        private final long mV8CtxPtr;
        private AjaxRequest mReq;
		private long mCbPtr;
		private long mThisObj;
		private boolean mSuccess;
		private String mData;
		private int mCode;
		private long mErrorCb;
		private boolean mProcessData;

		public V8AjaxRequest(String url, long jsCallbackPtr, long thisObj, long errorCb, long v8ContextPtr,
				String data, String method, boolean processData) {
			mCbPtr = jsCallbackPtr;
			mThisObj = thisObj;
			mErrorCb = errorCb;
			mV8CtxPtr = v8ContextPtr;
			mSuccess = false;
			mProcessData = processData;
			try {
				mReq = new AjaxRequest(url, data, this, method);
			} catch (URISyntaxException e) {
				Log.e(TAG, "Cannot create URL", e);
				ClientAndroid.ajaxDone(mNativePtr, null, 500, mCbPtr, mThisObj, mErrorCb, mV8CtxPtr, false, mProcessData);
                return;
			}
			mReq.setCacheInstance(mCache);
            mReq.setHttpClient(mHttpClient);
			mReq.doRunOnUiThread(false);
			mReq.setOutputType("application/json");

			final Runnable runnable = new Runnable() {
				@Override
				public void run() {
					if (DEBUG) {
						Log.d(TAG, "Executing V8Ajax request");
					}
					mReq.run();
                    mReq.runCallback();
					V8Engine engine = V8Engine.getInstance();
					engine.mHandler.sendMessage(engine.mHandler
							.obtainMessage(MSG_AJAX, V8AjaxRequest.this));
				}
			};
			if (mTPExecutor != null) {
				mTPExecutor.execute(runnable);
			} else {
				final Thread thr = new Thread(runnable);
				thr.start();
			}
		}

		public void doCallBack() {
			if (DEBUG) {
				Log.d(TAG, "Calling V8 success cb " + mCbPtr + ", thisObj "
						+ mThisObj + " for code " + mCode + ", thread "
						+ Thread.currentThread().getId());
			}
			ClientAndroid.ajaxDone(mNativePtr, mData, mCode, mCbPtr, mThisObj, mErrorCb, mV8CtxPtr, mSuccess, mProcessData);
		}

		public void success(String data, int code, AjaxRequest r) {
			mSuccess = true;
			mData = data;
			mCode = code;
		}

		public void error(String data, int code, Throwable tr, AjaxRequest r) {
			mSuccess = false;
			mData = data;
			mCode = code;
		}
	}

	public static void doAjaxRequest(String url, long jsCb, long thisObj, long errorCb, long v8ContextPtr,
			String data, String method, boolean processData) {
		V8AjaxRequest req = mInstance.new V8AjaxRequest(url, jsCb, thisObj, errorCb, v8ContextPtr,
				data, method, processData);
		if (DEBUG) {
			Log.d(TAG, "Preparing to do ajax request on thread "
					+ Thread.currentThread().getId());
		}
	}

	public void shutdown() {
		mHandler.sendEmptyMessage(MSG_QUIT);
	}

	// public void loadURL(String URL)
	private static final int MSG_CLEANUP = 1;
	private static final int MSG_QUIT = 2;
	private static final int MSG_LOAD = 3;
	private static final int MSG_AJAX = 4;
	private static final int MSG_READY = 5;


	public static final int TICK_SLEEP = 250;
	private static final int DELAY_CLEANUP = 10 * 1000;


}
