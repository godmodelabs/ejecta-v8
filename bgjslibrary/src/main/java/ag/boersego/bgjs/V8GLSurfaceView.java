/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package ag.boersego.bgjs;
/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.ViewParent;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL10;

// import org.jetbrains.annotations.NotNull;


/**
 * A simple GLSurfaceView sub-class that demonstrate how to perform
 * OpenGL ES 2.0 rendering into a GL Surface. Note the following important
 * details:
 *
 * - The class must use a custom context factory to enable 2.0 rendering.
 *   See ContextFactory class definition below.
 *
 * - The class must use a custom EGLConfigChooser to be able to select
 *   an EGLConfig that supports 2.0. This is done by providing a config
 *   specification to eglChooseConfig() that has the attribute
 *   EGL10.ELG_RENDERABLE_TYPE containing the EGL_OPENGL_ES2_BIT flag
 *   set. See ConfigChooser class definition below.
 *
 * - The class must select the surface's format, then choose an EGLConfig
 *   that matches it exactly (with regards to red/green/blue/alpha channels
 *   bit depths). Failure to do so would result in an EGL_BAD_MATCH error.
 */
abstract public class V8GLSurfaceView extends GLSurfaceView implements IV8GL {
	
	// TODO: Create drawing cache for scrolling
	// See http://dev.widemeadows.de/2011/11/02/android-bitmap-von-surfaceview-erzeugen/
	
	private final MotionEvent.PointerCoords[] mTouches = new MotionEvent.PointerCoords[MAX_NUM_TOUCHES];
	private final boolean[] mTouchesThere = new boolean[MAX_NUM_TOUCHES];
	private int mNumTouches = 0;
	private Renderer mRenderer;
	private double mTouchDistance;
	protected float mScaling;
	private boolean mInteractive;
    private long mUiNativePtr;
	private PointerCoords mTouchStart;
	private float mTouchSlop;
	private IV8GLViewOnRender mCallback;
	private long mConfigId;
	
    private static final String TAG = "GL2JNIView";
    private boolean DEBUG = false;
    private static final int TOUCH_SLOP = 5;
    private static final int MAX_NUM_TOUCHES = 10;
    protected String mCbName;
    private String mJSParam;

    /**
     * Create a new V8GLSurfaceView instance
     * @param context Context instance
     * @param jsCbName The name of the JS function to call once the view is created
     * @param param The parameter to pass to the JS function
     */
    public V8GLSurfaceView(Context context, String jsCbName, String param) {
        super(context);
        mTouchSlop = TOUCH_SLOP * mScaling;
        init(false, 0, 8, param, jsCbName);
    }

    public V8GLSurfaceView(Context context, boolean translucent, int depth, int stencil, String param, String jsCbName) {
        super(context);
        init(translucent, depth, stencil, param, jsCbName);
    }
    
    public void setInteractive (boolean interactive) {
    	ViewParent p = getParent();
    	if (p != null) {
    		p.requestDisallowInterceptTouchEvent(interactive);
    		if (DEBUG) { Log.d (TAG, "setInteractive: Disallowing parent touch intercept " + interactive); }
    	}
    	mInteractive = interactive;
    }
    
    public void onAttachedToWindow() {
    	super.onAttachedToWindow();
    	ViewParent p = getParent();
    	if (p != null) {
    		if (DEBUG) { Log.d (TAG, "setInteractive: Disallowing parent touch intercept " + mInteractive); }
    		p.requestDisallowInterceptTouchEvent(mInteractive);
    	}
    }

    /**
     * Create the JNI side of OpenGL init. Can be overriden by subclasses to instantiate other BGJSGLView subclasses
     * @return pointer to JNI object
     */
    protected int createGL () {
        return ClientAndroid.createGL(V8Engine.getInstance().getNativePtr(), this, mScaling, false);
    }

    public void doDebug(boolean debug) {
        DEBUG = debug;
    }

    abstract public void onGLCreated (int jsId);

    abstract public void onGLRecreated (int jsId);

    abstract public void onGLCreateError (Exception ex, String moreInfo);

    abstract public void onRenderAttentionNeeded (int jsId);

    public void doNeedAttention(boolean b) {
        mRenderer.setNeedAttention(b);
    }

    private void init(boolean translucent, int depth, int stencil, String jsParam, String jsCbName) {
    	setZOrderOnTop(true);
        final Resources r = getResources();
        mCbName = jsCbName;
        mJSParam = jsParam;

        if (r != null) {
            mScaling = r.getDisplayMetrics().density;
        }
    	/* final int thisDepth = getDisplay().getPixelFormat();
    	boolean notEightBit = thisDepth == PixelFormat.RGB_565 || thisDepth == PixelFormat.RGBA_5551; */

        /* By default, GLSurfaceView() creates a RGB_565 opaque surface.
         * If we want a translucent one, we should change the surface's
         * format here, using PixelFormat.TRANSLUCENT for GL Surfaces
         * is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
         */
        /* if (translucent) {
            this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        } */

        /* Setup the context factory for 2.0 rendering.
         * See ContextFactory class definition below
         */
        setEGLContextFactory(new ContextFactory(this));

        /* We need to choose an EGLConfig that matches the format of
         * our surface exactly. This is going to be done in our
         * custom config chooser. See ConfigChooser class definition
         * below.
         */
        setEGLConfigChooser( translucent ?
                             new ConfigChooser(8, 8, 8, 0, depth, stencil, DEBUG) :
                             new ConfigChooser(5, 6, 5, 0, depth, stencil, DEBUG) );

        /* Set the renderer responsible for frame rendering */
        mRenderer = new Renderer(jsCbName);
        setRenderer(mRenderer);
        setRenderMode(RENDERMODE_WHEN_DIRTY);
        // this.requestRender();
    }

    private static class ContextFactory implements GLSurfaceView.EGLContextFactory {

        private final V8GLSurfaceView mSurfaceInst;

        public ContextFactory(V8GLSurfaceView inst) {
            mSurfaceInst = inst;
        }
        private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;
        public EGLContext createContext(EGL10 egl, EGLDisplay display, EGLConfig eglConfig) {
            Log.w(TAG, "creating OpenGL ES 2.0 context");
            checkEglError("Before eglCreateContext", egl);
            int[] attrib_list = {EGL_CONTEXT_CLIENT_VERSION, 1, EGL10.EGL_NONE };
            EGLContext context = null;
            try {
            	context = egl.eglCreateContext(display, eglConfig, EGL10.EGL_NO_CONTEXT, attrib_list);
            } catch (IllegalArgumentException ex) {
                String moreInfo = null;

                try {
                    StringBuilder configDebug = new StringBuilder();
                    ConfigChooser.printConfig(egl, display, eglConfig, configDebug);
                    moreInfo = configDebug.toString();
                } catch (Exception innerEx) { }
                mSurfaceInst.onGLCreateError(ex, moreInfo);
            }
            checkEglError("After eglCreateContext", egl);
            return context;
        }

        public void destroyContext(EGL10 egl, EGLDisplay display, EGLContext context) {
            egl.eglDestroyContext(display, context);
        }
    }

    private static void checkEglError(String prompt, EGL10 egl) {
        int error;
        while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
            Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    private static class ConfigChooser implements GLSurfaceView.EGLConfigChooser {

        private boolean DEBUG = false;

        public ConfigChooser(int r, int g, int b, int a, int depth, int stencil, boolean debug) {
            mRedSize = r;
            mGreenSize = g;
            mBlueSize = b;
            mAlphaSize = a;
            mDepthSize = depth;
            mStencilSize = stencil;
            DEBUG = debug;
        }

        /* This EGL config specification is used to specify 2.0 rendering.
         * We use a minimum size of 4 bits for red/green/blue, but will
         * perform actual matching in chooseConfig() below.
         */
        private static int EGL_OPENGL_ES2_BIT = 4;
        private static final int[] s_configAttribs2 =
        {
            EGL10.EGL_RED_SIZE, 4,
            EGL10.EGL_GREEN_SIZE, 4,
            EGL10.EGL_BLUE_SIZE, 4,
            EGL10.EGL_NONE
        };

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {

            /* Get the number of minimally matching EGL configurations
             */
            int[] num_config = new int[1];
            egl.eglChooseConfig(display, s_configAttribs2, null, 0, num_config);

            int numConfigs = num_config[0];

            if (numConfigs <= 0) {
                throw new IllegalArgumentException("No configs match configSpec");
            }

            /* Allocate then read the array of minimally matching EGL configs
             */
            EGLConfig[] configs = new EGLConfig[numConfigs];
            egl.eglChooseConfig(display, s_configAttribs2, configs, numConfigs, num_config);

            if (DEBUG) {
                 printConfigs(egl, display, configs);
            }
            /* Now return the "best" one
             */
            return chooseConfig(egl, display, configs);
        }

        public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display,
                EGLConfig[] configs) {
        	EGLConfig bestConfig = null;
        	int bestDepth = 128;
            for(EGLConfig config : configs) {
                int d = findConfigAttrib(egl, display, config,
                        EGL10.EGL_DEPTH_SIZE, 0);
                int s = findConfigAttrib(egl, display, config,
                        EGL10.EGL_STENCIL_SIZE, 0);

                // We need at least mDepthSize and mStencilSize bits
                if (s < mStencilSize) {
                	if (DEBUG) {
                		Log.d (TAG, "Not good enough: stencil " + s);
                	}
                    continue;
                }

                // We want an *exact* match for red/green/blue/alpha
                int r = findConfigAttrib(egl, display, config,
                        EGL10.EGL_RED_SIZE, 0);
                int g = findConfigAttrib(egl, display, config,
                            EGL10.EGL_GREEN_SIZE, 0);
                int b = findConfigAttrib(egl, display, config,
                            EGL10.EGL_BLUE_SIZE, 0);
                int a = findConfigAttrib(egl, display, config,
                        EGL10.EGL_ALPHA_SIZE, 0);

                if (r <= mRedSize && g <= mGreenSize && b <= mBlueSize && a <= mAlphaSize) {
                	int distance = a + mStencilSize - s + mDepthSize - d;
                	if (DEBUG) {
                		Log.d (TAG, "Good enough: " + r + ", " + g + ", " + b + ", depth " + d + ", stencil " + s + ", alpha " + a + ", distance " + distance);
                	}
                    if (distance < bestDepth) {
                    	bestConfig = config;
                    	bestDepth = distance;
                    }
                }
            
                if (DEBUG) {
                	Log.d (TAG, "Not good enough: " + r + ", " + g + ", " + b + ", depth " + d + ", stencil " + s + ", alpha " + a);
                }
            }
            if (bestConfig != null) {
            	if (DEBUG) {
            		Log.d (TAG, "Best config");
            		printConfig(egl, display, bestConfig, null);
            	}
            	return bestConfig;
            }
            
            return null;
        }

        private int findConfigAttrib(EGL10 egl, EGLDisplay display,
                EGLConfig config, int attribute, int defaultValue) {

            if (egl.eglGetConfigAttrib(display, config, attribute, mValue)) {
                return mValue[0];
            }
            return defaultValue;
        }

        private void printConfigs(EGL10 egl, EGLDisplay display,
            EGLConfig[] configs) {
            int numConfigs = configs.length;
            Log.w(TAG, String.format("%d configurations", numConfigs));
            for (int i = 0; i < numConfigs; i++) {
                Log.w(TAG, String.format("Configuration %d:\n", i));
                printConfig(egl, display, configs[i], null);
            }
        }

        public static void printConfig(EGL10 egl, EGLDisplay display,
                EGLConfig config, StringBuilder sb) {
            int[] attributes = {
                    EGL10.EGL_BUFFER_SIZE,
                    EGL10.EGL_ALPHA_SIZE,
                    EGL10.EGL_BLUE_SIZE,
                    EGL10.EGL_GREEN_SIZE,
                    EGL10.EGL_RED_SIZE,
                    EGL10.EGL_DEPTH_SIZE,
                    EGL10.EGL_STENCIL_SIZE,
                    EGL10.EGL_CONFIG_CAVEAT,
                    EGL10.EGL_CONFIG_ID,
                    EGL10.EGL_LEVEL,
                    EGL10.EGL_MAX_PBUFFER_HEIGHT,
                    EGL10.EGL_MAX_PBUFFER_PIXELS,
                    EGL10.EGL_MAX_PBUFFER_WIDTH,
                    EGL10.EGL_NATIVE_RENDERABLE,
                    EGL10.EGL_NATIVE_VISUAL_ID,
                    EGL10.EGL_NATIVE_VISUAL_TYPE,
                    0x3030, // EGL10.EGL_PRESERVED_RESOURCES,
                    EGL10.EGL_SAMPLES,
                    EGL10.EGL_SAMPLE_BUFFERS,
                    EGL10.EGL_SURFACE_TYPE,
                    EGL10.EGL_TRANSPARENT_TYPE,
                    EGL10.EGL_TRANSPARENT_RED_VALUE,
                    EGL10.EGL_TRANSPARENT_GREEN_VALUE,
                    EGL10.EGL_TRANSPARENT_BLUE_VALUE,
                    0x3039, // EGL10.EGL_BIND_TO_TEXTURE_RGB,
                    0x303A, // EGL10.EGL_BIND_TO_TEXTURE_RGBA,
                    0x303B, // EGL10.EGL_MIN_SWAP_INTERVAL,
                    0x303C, // EGL10.EGL_MAX_SWAP_INTERVAL,
                    EGL10.EGL_LUMINANCE_SIZE,
                    EGL10.EGL_ALPHA_MASK_SIZE,
                    EGL10.EGL_COLOR_BUFFER_TYPE,
                    EGL10.EGL_RENDERABLE_TYPE,
                    0x3042 // EGL10.EGL_CONFORMANT
            };
            String[] names = {
                    "EGL_BUFFER_SIZE",
                    "EGL_ALPHA_SIZE",
                    "EGL_BLUE_SIZE",
                    "EGL_GREEN_SIZE",
                    "EGL_RED_SIZE",
                    "EGL_DEPTH_SIZE",
                    "EGL_STENCIL_SIZE",
                    "EGL_CONFIG_CAVEAT",
                    "EGL_CONFIG_ID",
                    "EGL_LEVEL",
                    "EGL_MAX_PBUFFER_HEIGHT",
                    "EGL_MAX_PBUFFER_PIXELS",
                    "EGL_MAX_PBUFFER_WIDTH",
                    "EGL_NATIVE_RENDERABLE",
                    "EGL_NATIVE_VISUAL_ID",
                    "EGL_NATIVE_VISUAL_TYPE",
                    "EGL_PRESERVED_RESOURCES",
                    "EGL_SAMPLES",
                    "EGL_SAMPLE_BUFFERS",
                    "EGL_SURFACE_TYPE",
                    "EGL_TRANSPARENT_TYPE",
                    "EGL_TRANSPARENT_RED_VALUE",
                    "EGL_TRANSPARENT_GREEN_VALUE",
                    "EGL_TRANSPARENT_BLUE_VALUE",
                    "EGL_BIND_TO_TEXTURE_RGB",
                    "EGL_BIND_TO_TEXTURE_RGBA",
                    "EGL_MIN_SWAP_INTERVAL",
                    "EGL_MAX_SWAP_INTERVAL",
                    "EGL_LUMINANCE_SIZE",
                    "EGL_ALPHA_MASK_SIZE",
                    "EGL_COLOR_BUFFER_TYPE",
                    "EGL_RENDERABLE_TYPE",
                    "EGL_CONFORMANT"
            };
            int[] value = new int[1];
            try {
	            for (int i = 0; i < attributes.length; i++) {
	                int attribute = attributes[i];
	                String name = names[i];
	                if ( egl.eglGetConfigAttrib(display, config, attribute, value)) {
	                	if (sb != null) {
	                		sb.append("  ").append(name).append(": ").append(value[0]);
	                	} else {
	                		Log.w(TAG, String.format("  %s: %d\n", name, value[0]));
	                	}
	                } else {
	                    // Log.w(TAG, String.format("  %s: failed\n", name));
                        //noinspection StatementWithEmptyBody
                        while (egl.eglGetError() != EGL10.EGL_SUCCESS);
	                }
	            }
            } catch (Exception ex) {
            	Log.d (TAG, "printConfig", ex);
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
    
    /* @Override
    public boolean dispatchTouchEvent (MotionEvent event) {
    	if (mInteractive) {
    		return true;
    	}
    	return false;
    } */
    
    @Override
    public boolean onTouchEvent(/* @NotNull */ MotionEvent ev) {
    	
    	if (!mInteractive) {
    		return super.onTouchEvent(ev);
    	}
        final int action = ev.getActionMasked();
        final int index = ev.getActionIndex();
        final int count = ev.getPointerCount();
        
        // ev.getPointerCoords(index, outPointerCoords)
        if (DEBUG) {
        	Log.d (TAG, "Action " + action + ", index " + index + ", x " + ev.getX(index) + ", id " + ev.getPointerId(index));
        }
        switch (action) {
        case MotionEvent.ACTION_DOWN:
        case MotionEvent.ACTION_POINTER_DOWN: {
            final int id = ev.getPointerId(index);
            final ViewParent vp = getParent();
            if (vp != null) {
                vp.requestDisallowInterceptTouchEvent(true);
            }

        	if (id < MAX_NUM_TOUCHES) {
				if (mTouches[id] == null) {
					mTouches[id] = new MotionEvent.PointerCoords();
				}
				ev.getPointerCoords(index, mTouches[id]);
				mTouches[id].x = (int) (mTouches[id].x / mScaling);
				mTouches[id].y = (int) (mTouches[id].y / mScaling);

				if (action == MotionEvent.ACTION_DOWN) {
					mTouchStart = new PointerCoords();
					mTouchStart.x = mTouches[id].x;
					mTouchStart.y = mTouches[id].y;
				}
				if (id < MAX_NUM_TOUCHES) {
					mTouchesThere[id] = true;
				}
				if (count == 2 && mTouches[0] != null && mTouches[1] != null) {
					mTouchDistance = Math.sqrt((mTouches[0].x - mTouches[1].x) * (mTouches[0].x - mTouches[1].x)
							+ (mTouches[0].y - mTouches[1].y) * (mTouches[0].y - mTouches[1].y));
				}
				mNumTouches = count;
				ClientAndroid.setTouchPosition(V8Engine.getInstance().getNativePtr(), mRenderer.mJSId, (int)mTouches[id].x, (int)mTouches[id].y);
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
					if (touchDirty) {
						ClientAndroid.setTouchPosition(V8Engine.getInstance().getNativePtr(), mRenderer.mJSId, (int)mTouches[id].x, (int)mTouches[id].y);
					}
				}
			}
			
			String touchEvent = "touchmove";
			boolean endThisTouch = false;
			/* if(!mViewRect.contains(getLeft() + (int) ev.getX(), getTop() + (int) ev.getY())) {
				touchEvent = "touchend";
				endThisTouch = true;
			} */

			if (touchDirty) {				
				sendTouchEvent(touchEvent);
			}
            //noinspection ConstantConditions
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
    
    private void sendTouchEvent(String type) {
    	int count = 0;
    	for (int i = 0; i < MAX_NUM_TOUCHES; i++) {
    		if (mTouchesThere[i]) {
    			count++;
    		}
    	}
    	mNumTouches = count;
    	
    	final float[] x = new float[mNumTouches], y = new float[mNumTouches];
    	double x1 = 0, y1 = 0, x2 = 0, y2 = 0, scale = 0;
    	int j = 0;
    	for (int i = 0; i < MAX_NUM_TOUCHES; i++) {
    		if (mTouchesThere[i]) {
    			x[j] = mTouches[i].x;
    			y[j] = mTouches[i].y;
    			if (j == 0) {
    				x1 = x[j];
    				y1 = y[j];
    			} else if (j == 1) {
    				x2 = x[j];
    				y2 = y[j];
    			}
    			j++;
    		}
    	}
    	if (mNumTouches == 2) {
    		scale = Math.sqrt((x1 - x2)*(x1 - x2) + (y1 - y2)*(y1 - y2)) / mTouchDistance;
    	}
    	if (DEBUG) {
    		Log.d (TAG, "touch event: p1 " + x1 + ", " + y1 + ", p2 " + x2 + ", " + y2 + ", scale " + scale);
    	}
    	ClientAndroid.sendTouchEvent(V8Engine.getInstance().getNativePtr(), mRenderer.mJSId, type, x, y, (float)scale);
    }
    
	void resetTouches() {
		mNumTouches = 0;
		for (int i = 0; i < MAX_NUM_TOUCHES; i++) {
			mTouchesThere[i] = false;
		}
		mTouchDistance = 0;
	}
	
	protected void requestRenderQueue() {
		if (mRenderer.mRenderMe) {
			Log.d (TAG, "Not requesting rerender");
			return;
		}
		Log.d (TAG, "Requesting rerender");
		mRenderer.mRenderMe = true;
		super.requestRender();
	}
	
	@Override
	protected void onDetachedFromWindow() {
		super.onDetachedFromWindow();
		mRenderer.onDetachedFromWindow();
	}
	
	/* @Override
	public void requestRender() {
		Log.d (TAG, "Would have requested render");
	} */

    private class Renderer implements GLSurfaceView.Renderer {
        public int mJSId = 0;
        private int mRenderCnt = 0;
        private long mLastRenderSec = 0;
		private float mScaling;
		private boolean mLastRenderEmpty = false;
		private boolean mRenderMe = true;
		private final String mCbName;
		private boolean mRenderPending;
        private boolean mNeedsAttention;

        public Renderer (String jsCbName) {
			mCbName = jsCbName;
		}
		
        public void requestRender() {
			synchronized(this) {
				mRenderPending = true;
				requestRenderQueue();
				this.notifyAll();
				if (DEBUG) {
					Log.d (TAG, "Requested render");
				}
			}
		}

		public void onDrawFrame(GL10 gl) {
			final long now = System.currentTimeMillis() / 1000;
			if (now != mLastRenderSec) {
				Log.d (TAG, "FPS: " + mRenderCnt);
				mRenderCnt = 0;
				mLastRenderSec = now;
			}
			if (DEBUG) {
				Log.d (TAG, "Will now draw frame");
			}
			// printBuffers();
			final boolean didDraw = ClientAndroid.step(V8Engine.getInstance().getNativePtr(), mJSId);
            
            // If no rendering was done, swap buffers again. Stupid bug, but onDrawFrame is called at least twice regardless of how often we requested rendering
            if (!didDraw && !mLastRenderEmpty) {
            	mLastRenderEmpty = true;
            	Log.d (TAG, "Didn't draw, swapping back");
            	// GL2JNIView.this.requestRender();
            	// swapBuffers();
            }
            
            /* if (!mRenderPending && !mSetInstrumentPending) {
                try {
                	synchronized(this) {
                		wait(10000);
                	}
                } catch (InterruptedException e) {
                    // Ignore
                }
            } */
            if (mNeedsAttention) {
                if (DEBUG) {
                    Log.d(TAG, "Attending V8TextureView super");
                }
                V8GLSurfaceView.this.onRenderAttentionNeeded(mJSId);
                mNeedsAttention = false;
            }
            
            mRenderMe = false;
            mRenderCnt++;
        }

        public void setNeedAttention (boolean b) {
            synchronized (this) {
                if (mNeedsAttention != b) {
                    mNeedsAttention = b;
                    if (b) {
                        this.notifyAll();
                    }
                    if (DEBUG) {
                        Log.d(TAG, "Requested attention " + b);
                    }
                }
            }
        }

		@SuppressLint("MissingSuperCall")
		public void onDetachedFromWindow() {
			if (mCallback != null) {
				mCallback.renderThreadClosed(mJSId);
			}
			if (mJSId != 0) {
				ClientAndroid.close(V8Engine.getInstance().getNativePtr(), mJSId);
			}
		}

		public void onSurfaceChanged(GL10 gl, int width, int height) {
            V8GLSurfaceView.this.onGLCreated(mJSId);
            if (mCallback != null) {
            	mCallback.renderStarted(mJSId);
            }
            V8GLSurfaceView.this.resetTouches();
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            if (mJSId == 0) {
                final Resources r = getResources();
                if (r != null) {
            	    mScaling = r.getDisplayMetrics().density;
                }
            	mJSId = createGL();
            }
            
            try {
                String vendor = gl.glGetString(GL10.GL_VENDOR);
                String glVersion = gl.glGetString(GL10.GL_VERSION);
	            Context c = getContext();
	            /* if (c instanceof Activity) {
	            	MyrmApplication ma = (MyrmApplication)((Activity)c).getApplication();
	            	ma.setOpenGlInfo(vendor, glVersion);
	            } */
            } catch (Exception ex) { }
        }
        
       public void swapBuffers()
        {
            EGL10 curEgl = (EGL10)EGLContext.getEGL();

            EGLDisplay curDisplay = curEgl.eglGetCurrentDisplay();
            if (curDisplay == EGL10.EGL_NO_DISPLAY) { Log.e("myApp","No default display"); return; }    

            EGLSurface curSurface = curEgl.eglGetCurrentSurface(EGL10.EGL_DRAW);
            if (curSurface == EGL10.EGL_NO_SURFACE) { Log.e("myApp","No current surface"); return; }

            curEgl.eglSwapBuffers(curDisplay, curSurface);
        }
       
       public void printBuffers() {
           EGL10 curEgl = (EGL10)EGLContext.getEGL();

           EGLDisplay curDisplay = curEgl.eglGetCurrentDisplay();
           if (curDisplay == EGL10.EGL_NO_DISPLAY) { Log.e("myApp","No default display"); return; }    

           EGLSurface curSurface = curEgl.eglGetCurrentSurface(EGL10.EGL_DRAW);
           if (curSurface == EGL10.EGL_NO_SURFACE) { Log.e("myApp","No current surface"); return; }
           Log.d (TAG, "front " + curDisplay + ", back " + curSurface);
       }
    }
    
    @Override
    public void setRenderCallback (IV8GLViewOnRender listener) {
    	mCallback = listener;
    }


	@Override
	public void unpause() {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void pause() {
		// TODO Auto-generated method stub
		
	}
}
