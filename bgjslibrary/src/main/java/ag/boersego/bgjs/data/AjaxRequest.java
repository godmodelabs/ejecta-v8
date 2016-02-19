package ag.boersego.bgjs.data;

import android.os.Build;
import android.util.Log;


import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.net.HttpURLConnection;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.net.URLEncoder;
import java.util.Locale;


public class AjaxRequest extends AjaxRequestDebug implements Runnable {

    private boolean mDebug;
    private V8UrlCache mCache;
    private boolean mIsCancelled;

    public interface AjaxListener {
		public void success(String data, int code, AjaxRequest request);

		public void error(String data, int code, Throwable tr, AjaxRequest request);
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
	protected String mData;
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
        super("");
		
	}
	
	public void setUrl (String url) throws URISyntaxException {
		mUrl = new URI(url);
	}

	public AjaxRequest(String targetURL, String data, AjaxListener caller) throws URISyntaxException {
        super(targetURL);
		if (data != null) {
			mUrl = new URI(targetURL + "?" + data);
		} else {
			mUrl = new URI(targetURL);
		}
		mData = data;
		mCaller = caller;
	}

	public AjaxRequest(String url, String data, AjaxListener caller,
			String method) throws URISyntaxException {
        super(url);
		mData = data;
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
	 * @param is the InputStream from the URL connection
	 * @return true if the subclass handled reading the Stream
	 */
	protected void onInputStreamReady(final InputStream is, final HttpURLConnection connection) throws IOException {
        BufferedReader rd = new BufferedReader(new InputStreamReader(is));
        String line;
        StringBuilder response = new StringBuilder(4096);

        // TODO: Optimize me
        while ((line = rd.readLine()) != null) {
            if (mIsCancelled) {
                return;
            }
            response.append(line);
            response.append('\r');
        }
        rd.close();
        mSuccessData = response.toString();

        storeCacheObject(connection, mSuccessData);

        if (mDebug) {
            Log.d (TAG, "Response: " + mSuccessCode + "/" + mSuccessData);
        }
	}

	@SuppressWarnings("ConstantConditions")
    public void run() {
		URL url;
		HttpURLConnection connection = null;
        InputStream is = null;
		try {
			// Create connection
			url = mUrl.toURL();
			connection = (HttpURLConnection) url.openConnection();
			connection.setInstanceFollowRedirects(true);
			connection.setConnectTimeout(connectionTimeout);
			connection.setReadTimeout(readTimeout);
			Locale here = Locale.getDefault();
			
			if (AjaxRequest.mUserAgent == null) {
				AjaxRequest.mUserAgent = "myrmecophaga/" + version + " (Linux; U; Android " + Build.VERSION.RELEASE + "; " + here.getLanguage() + "-" + here.getCountry() + "; " + Build.MANUFACTURER + " " + Build.MODEL + " Build " + Build.DISPLAY + ") AppleWebKit/pi (KHTML, like Gecko) Version/4.0 Mobile Safari/beta";
			}
			connection.setRequestProperty("User-Agent", AjaxRequest.mUserAgent);
			if (mReferer != null) {
				connection.setRequestProperty("Referer", mReferer);
			}

            //noinspection ConstantConditions
            if (mDebug) {
				Log.d (TAG, "Opening URL " + mUrl);
				/* connection.setDefaultUseCaches(false);
				connection.setUseCaches(false); */
			} else {
				connection.setDefaultUseCaches(true);
				connection.setUseCaches(true);
			}
			connection.setDoInput(true);
			connection.setRequestMethod(mMethod);
			if (mResultBuilder != null) {
				mData = mResultBuilder.toString();
			}
			if (mData != null) {
				if (mOutputType != null) {
					connection.setRequestProperty("Content-Type",
						mOutputType);
				} else {
					if (mMethod.equals("POST")) {
						connection.setRequestProperty("Content-Type", "application/x-www-form-urlencoded ");
					}
				}
				byte[] outData = mData.getBytes();

				connection.setRequestProperty("Content-Length",
                        Integer.toString(outData.length));
				connection.setRequestProperty("Content-Language", "en-US");
				connection.setDoOutput(true);

                // TODO: Add post
                super.preConnect(connection, null);

				// Send request
				OutputStream wr = connection.getOutputStream();
				wr.write(outData);
				if (mDebug) {
					Log.d (TAG, mMethod + " DATA " + mData);
				}
				wr.close();
			} else {
                // TODO: Add post
                super.preConnect(connection, null);
            }


			// Get Response
            if (mIsCancelled) {
                return;
            }
			is = super.interpretResponseStream(connection.getInputStream());
            final String responseStr;
            int dataSizeGuess = connection.getContentLength();
            if (dataSizeGuess > 0) {
                AjaxTrafficCounter.addTraffic(0, dataSizeGuess);
            }
            mSuccessCode = connection.getResponseCode();
            super.postConnect();

            onInputStreamReady(is, connection);

		} catch (Exception e) {

			String errDescription;
            if (e instanceof IOException) {
                super.httpExchangeFailed((IOException)e);
            }
			try {
                if (connection != null) {
                    if (is != null) {
                        try {
                            is.close();
                        } catch (IOException ignored) { }
                    }
                    is = connection.getErrorStream();
                    if (is != null) {
                        BufferedReader rd = new BufferedReader(new InputStreamReader(is));
                        String line;
                        StringBuilder response = new StringBuilder(16384);

                        // TODO: Optimize me
                        while ((line = rd.readLine()) != null) {
                            response.append(line);
                            response.append('\r');
                        }
                        rd.close();
                        errDescription = response.toString();
                    } else {
                        if (connection != null) {
                            errDescription = connection.getResponseMessage();
                        } else {
                            errDescription = "connection is null";
                        }
                    }
                } else {
                    errDescription = "Connection is null";
                }
				Log.e (TAG, "Cannot load data from api: " + errDescription, e);
				mErrorData = errDescription;
                if (connection != null) {
				    mErrorCode = connection.getResponseCode();
                }
				mErrorThrowable = e;
			} catch (IOException ex) {
				Log.e (TAG, "Cannot get error info", ex);
				mErrorData = null;
				mErrorCode = 0;
				mErrorThrowable = ex;
			}

		} finally {
            if (is != null) {
                try {
                    is.close();
                } catch (IOException ignored) {}
            }
			if (connection != null) {
				connection.disconnect();
			}
		}
	}


    protected void storeCacheObject(final HttpURLConnection connection, final Object cachedObject) {
        if (connection == null) {
            return;
        }
        try {
            String cc = connection.getHeaderField("Cache-Control");
            if (cc != null && mMethod.equals("GET")) {
                if (!cc.contains("no-store")) {
                    // Response is cacheable
                    if (cc.contains("max-age")) {
                        int maxAge;
                        int token = cc.indexOf("max-age=");
                        int start = cc.indexOf("=", token) + 1;
                        int end = cc.indexOf(",", start + 1);
                        if (end != -1) {
                            maxAge = Integer.parseInt(cc.substring(start, end));
                        } else {
                            maxAge = Integer.parseInt(cc.substring(start));
                        }
                        if (maxAge > 0 && mCache != null) {
                            mCache.storeInCache(mUrl.toString(), cachedObject, maxAge);
                            if (mDebug) {
                                Log.d (TAG, "Stored in cache for " + maxAge + " secs");
                            }
                        }
                    }
                }
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
