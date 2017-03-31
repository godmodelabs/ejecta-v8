package ag.boersego.bgjs.data;

import android.os.Build;
import android.util.Log;


import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLEncoder;
import java.util.Locale;

import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;


public class AjaxRequest implements Runnable {

    private boolean mDebug;
    private V8UrlCache mCache;
    private boolean mIsCancelled;
    private OkHttpClient mHttpClient;

    public void setHttpClient(OkHttpClient httpClient) {
        mHttpClient = httpClient;
    }

    public interface AjaxListener {
		void success(String data, int code, AjaxRequest request);

		void error(String data, int code, Throwable tr, AjaxRequest request);
	}

    /**
     * This traffic-counting code is also in Java-WebSocket.
     * TODO: Share code.
     */
    public static class AjaxTrafficCounter {

        private static long trafficIn = 0, trafficOut = 0;
        private static int[] trafficInPerMinute = { -1, -1, -1, -1, -1, -1};
        private static int[] trafficOutPerMinute = { -1, -1, -1, -1, -1, -1};
        private static int lastTimeSlot;

        public static long getInTraffic() {
            return trafficIn;
        }

        public static long getOutTraffic() {
            return trafficOut;
        }

        public static int getInTrafficPerMinute() {
            int numFieldsFound = 0, sum = 0;
            for (int i = 0; i < 6; i++) {
                if (trafficInPerMinute[i] > -1) {
                    sum += trafficInPerMinute[i];
                    numFieldsFound++;
                }
            }
            if (numFieldsFound <= 0) {
                return 0;
            }
            return sum / numFieldsFound;
        }

        public static int getOutTrafficPerMinute() {
            int numFieldsFound = 0, sum = 0;
            for (int i = 0; i < 6; i++) {
                if (trafficOutPerMinute[i] > -1) {
                    sum += trafficOutPerMinute[i];
                    numFieldsFound++;
                }
            }
            if (numFieldsFound <= 0) {
                return 0;
            }
            return sum / numFieldsFound;
        }

        public static void addTraffic(final int outTraffic, final int inTraffic) {
            final int nowSlot = (int)(System.currentTimeMillis() / 10000);
            final int nowIndex = nowSlot % 6;
            if (nowSlot != lastTimeSlot) {
                trafficOutPerMinute[nowIndex] = -1;
                trafficInPerMinute[nowIndex] = -1;
                lastTimeSlot = nowSlot;
            }
            if (outTraffic > 0) {
                trafficOut += outTraffic;
                trafficOutPerMinute[nowIndex] += ((trafficOutPerMinute[nowIndex] < 0) ? 1 : 0) + outTraffic;
            }
            if (inTraffic > 0) {
                trafficIn += inTraffic;
                trafficInPerMinute[nowIndex] += ((trafficInPerMinute[nowIndex] < 0) ? 1 : 0) + inTraffic;
            }
        }
    }
	
	protected URI mUrl;
	protected byte[] mData;
	protected AjaxListener mCaller;
	protected Object mDataObject;
	private Object mAdditionalData;
	protected String mMethod = "GET";
	private StringBuilder mResultBuilder;
	protected String mOutputType;
	public String version;
	private String mReferer;
	
	public int connectionTimeout = 12000;
    public int readTimeout = 5000;
	protected String mErrorData;
	protected int mErrorCode;
	protected Exception mErrorThrowable;
	protected String mSuccessData;
	protected int mSuccessCode;
	
	protected boolean mDoRunOnUiThread = true;
	
	private static String mUserAgent = null;
    private static final String TAG = "AjaxRequest";
	
	protected AjaxRequest() {
		
	}
	
	public void setUrl (String url) throws URISyntaxException {
		mUrl = new URI(url);
	}

	public AjaxRequest(String targetURL, String data, AjaxListener caller) throws URISyntaxException {
		if (data != null) {
			mUrl = new URI(targetURL + "?" + data);
		} else {
			mUrl = new URI(targetURL);
		}
		if (data != null) {
            mData = data.getBytes();
        }
		mCaller = caller;
	}

	public AjaxRequest(String url, String data, AjaxListener caller,
			String method) throws URISyntaxException {
        if (data != null) {
            mData = data.getBytes();
        }
		mCaller = caller;
		if (method == null) {
			method = "GET";
		}
		if (method.equals("GET") && data != null) {
			mUrl = new URI(url + "?" + data);
		} else {
			mUrl = new URI(url);
		}
		mMethod  = method;
	}

    public void setCacheInstance (V8UrlCache cache) {
        mCache = cache;
    }

    public void doDebug (boolean debug) {
        mDebug = debug;
    }
	
	@SuppressWarnings("SameParameterValue")
    public void setOutputType(String type) {
		mOutputType = type;
	}
	
	public void addPostData(String key, String value) throws UnsupportedEncodingException
	{
	    if (mResultBuilder == null) {
	    	mResultBuilder = new StringBuilder();
	    } else {
	    	mResultBuilder.append("&");
	    }
	    

	    mResultBuilder.append(URLEncoder.encode(key, "UTF-8"));
	    mResultBuilder.append("=");
	    mResultBuilder.append(URLEncoder.encode(value, "UTF-8"));
	}

	/**
	 * Subclasses can override this to handle the InputStream directly instead of letting this
     * class read it into a string completely
	 * @param connection
     * @return true if the subclass handled reading the Stream
	 */
	protected void onInputStreamReady(final Response connection) throws IOException {

        mSuccessData = connection.body().string();

        storeCacheObject(connection, mSuccessData);

        if (mDebug) {
            Log.d (TAG, "Response: " + mSuccessCode + "/" + mSuccessData);
        }
	}

	@SuppressWarnings("ConstantConditions")
    public void run() {
		URL url;

        Response response = null;
		try {
			// Create connection

            if (AjaxRequest.mUserAgent == null) {
                Locale here = Locale.getDefault();
                AjaxRequest.mUserAgent = "myrmecophaga/" + version + " (Linux; U; Android " + Build.VERSION.RELEASE + "; " + here.getLanguage() + "-" + here.getCountry() + "; " + Build.MANUFACTURER + " " + Build.MODEL + " Build " + Build.DISPLAY + ") AppleWebKit/pi (KHTML, like Gecko) Version/4.0 Mobile Safari/beta";
            }

			url = mUrl.toURL();
            Request.Builder requestBuilder = new Request.Builder()
                    .url(url);

            try {
                requestBuilder.addHeader("User-Agent", mUserAgent);
            } catch (final Exception ignored) { }



            if (mReferer != null) {
                try {
                    requestBuilder.addHeader("Referer", mReferer);
                } catch (final Exception ex) {
                    Log.e(TAG, "Cannot set referer", ex);
                }
            } else {
                try {
                    Log.w(TAG, "no referer set " + mCaller.getClass().getCanonicalName());
                } catch (final Exception ignored) { }
            }

			if (mResultBuilder != null) {
				mData = mResultBuilder.toString().getBytes();
			}
			if (mData != null) {
                if (mOutputType != null) {
                    requestBuilder.post(RequestBody.create(MediaType.parse(mOutputType), mData));
                } else {
                    if (mMethod.equals("POST")) {
                        requestBuilder.post(RequestBody.create(MediaType.parse("application/x-www-form-urlencoded"), mData));
                    }
                }
            } else if (mMethod != null) {
                if (mMethod.equals("DELETE")) {
                    requestBuilder.delete();
                }
            }


			// Get Response
            if (mIsCancelled) {
                return;
            }


            final Request request = requestBuilder.build();

            if (mHttpClient == null) {
                Log.d(TAG, "no http client");
                throw new RuntimeException("no http client");
            }
            response = mHttpClient.newCall(request).execute();
            if (!response.isSuccessful()) throw new IOException("Unexpected code " + response);
            final String responseStr;
            int dataSizeGuess = (int) response.body().contentLength();
            if (dataSizeGuess > 0) {
                AjaxTrafficCounter.addTraffic(0, dataSizeGuess);
            }
            mSuccessCode = response.code();

            onInputStreamReady(response);

		} catch (Exception e) {

			String errDescription;
            if (response != null) {
                try {
                    mErrorData = response.body().string();
                } catch (Exception ignored) {
                }
                mErrorCode = response.code();
            }
            Log.i (TAG, "Cannot load data from api " + mUrl + ": " + mErrorData, e);
            mErrorThrowable = e;

		} finally {
            if (response != null) {
                response.body().close();
            }
		}
	}


    protected void storeCacheObject(final Response connection, final Object cachedObject) {
        if (connection == null) {
            return;
        }
        try {
            if (!connection.cacheControl().noStore() && connection.request().method().equals("GET")) {
                mCache.storeInCache(connection.request().url().toString(), cachedObject, connection.cacheControl().maxAgeSeconds());
            }
        } catch (Exception ex) {
            Log.i (TAG, "Cannot set cache info", ex);
        }
    }

	public void cancel() {
        mIsCancelled = true;
        synchronized (this) {
            this.notifyAll();
        }
    }
	
	public void runCallback() {
		if (mSuccessData != null) {
			mCaller.success(mSuccessData, mSuccessCode, this);
			return;
		}
		mCaller.error(mErrorData, mErrorCode, mErrorThrowable, this);
	}

	public Object getDataObject() {
		return mDataObject;
	}
	
	public void setAdditionalData(Object dataObj) {
		mAdditionalData = dataObj;
	}
	
	public Object getAdditionalData() {
		return mAdditionalData;
	}
	
	public void setReferer(String referer) {
		mReferer = referer;
	}
	
	public String toString() {
		if (mUrl != null) {
			return mUrl.toString();
		}
		
		return "AjaxRequest uninitialized";
	}

	public void doRunOnUiThread(@SuppressWarnings("SameParameterValue") boolean b) {
		mDoRunOnUiThread = b;
	}

}
