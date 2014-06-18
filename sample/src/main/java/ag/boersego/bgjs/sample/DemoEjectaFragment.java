package ag.boersego.bgjs.sample;

import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import ag.boersego.bgjs.ClientAndroid;
import ag.boersego.bgjs.IV8GL;
import ag.boersego.bgjs.V8Engine;
import ag.boersego.bgjs.V8GLSurfaceView;
import ag.boersego.bgjs.V8TextureView;
import ag.boersego.bgjs.sample.dummy.DummyContent;

/**
 * A fragment representing a single Demo detail screen.
 * This fragment is either contained in a {@link DemoListActivity}
 * in two-pane mode (on tablets) or a {@link DemoDetailActivity}
 * on handsets.
 */
public class DemoEjectaFragment extends Fragment implements V8Engine.V8EngineHandler {
    /**
     * The fragment argument representing the item ID that this fragment
     * represents.
     */
    public static final String ARG_ITEM_ID = "param";

    /**
     * The dummy content this fragment is presenting.
     */
    private DummyContent.DummyItem mItem;
    private IV8GL mView;
    private FrameLayout mRootView;
    private boolean mEngineReady;
    private V8Engine mV8Engine;
    private int mJSId;

    private static final String TAG = "DemoDetailFragment";
    private String mScriptCb = "startPlasma";

    /**
     * Mandatory empty constructor for the fragment manager to instantiate the
     * fragment (e.g. upon screen orientation changes).
     */
    public DemoEjectaFragment() {
    }

    public void onAttach(Activity a) {
        super.onAttach(a);

        mV8Engine = V8Engine.getInstance();

        mV8Engine.addStatusHandler(this);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (getArguments().containsKey(ARG_ITEM_ID)) {
            // Load the dummy content specified by the fragment
            // arguments. In a real-world scenario, use a Loader
            // to load content from a content provider.
            mScriptCb = getArguments().getString(ARG_ITEM_ID);
        }
    }

    private void initializeV8 (int jsId) {
        mJSId = jsId;

        final float density = getResources().getDisplayMetrics().density;
        final View jsView = (View)mView;
        final int width = (int)(jsView.getWidth() / density);
        final int height = (int)(jsView.getHeight() / density);
        ClientAndroid.init(mV8Engine.getNativePtr(), jsId, width, height, mScriptCb);
    }

    protected void createGLView() {
        // We need to wait until both our parent view and the v8Engine are ready
        if (mRootView == null || !mEngineReady) {
            return;
        }

        Log.d(TAG, "Creating GL view and calling callback " + mScriptCb);

        if (Build.VERSION.SDK_INT > 10) {
            // HC and up have TextureVew
            final V8TextureView tv = new V8TextureView(getActivity(), mScriptCb, "") {

                @Override
                public void onGLCreated(int jsId) {
                    initializeV8(jsId);
                }

                @Override
                public void onGLRecreated(int jsId) {
                    onGLCreated(jsId);
                }

                @Override
                public void onGLCreateError(Exception ex) {
                    Log.d (TAG, "OpenGL error", ex);
                }

                @Override
                public void onRenderAttentionNeeded(int jsId) {

                }
            };
            // tv.doDebug(true);
            mView = tv;
        } else {
            mView = new V8GLSurfaceView(getActivity(), "viewCreated", "") {
                @Override
                public void onGLCreated(int jsId) {
                    initializeV8(jsId);
                }

                @Override
                public void onGLRecreated(int jsId) {
                    onGLCreated(jsId);
                }

                @Override
                public void onGLCreateError(Exception ex, String moreInfo) {
                    Log.d (TAG, "OpenGL error", ex);
                }

                @Override
                public void onRenderAttentionNeeded(int jsId) {

                }
            };
        }
        View v = (View) mView;
        mRootView.addView(v, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {

        mRootView = (FrameLayout) inflater.inflate(R.layout.fragment_demo_detail, container, false);

        createGLView();

        return mRootView;
    }

    @Override
    public void onResume() {
        super.onResume();

        if (mView != null) {
            // Resume the JS stack for this view
            mView.unpause();
        }
    }

    @Override
    public void onPause() {
        super.onPause();

        if (mView != null) {
            // Pause the JS stack for this view
            mView.pause();
        }
    }

    @Override
    public void onReady() {
        mEngineReady = true;
        createGLView();
    }
}
