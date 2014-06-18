package ag.boersego.bgjs.sample;

import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import android.widget.FrameLayout;

public class DemoWebviewFragment extends Fragment {
    private static final String ARG_PARAM = "param";

    private String mUrl;


    /**
     * Use this factory method to create a new instance of
     * this fragment using the provided parameters.
     *
     * @param url URL to html file
     * @return A new instance of fragment DemoWebviewFragment.
     */
    // TODO: Rename and change types and number of parameters
    public static DemoWebviewFragment newInstance(String url) {
        DemoWebviewFragment fragment = new DemoWebviewFragment();
        Bundle args = new Bundle();
        args.putString(ARG_PARAM, url);
        fragment.setArguments(args);
        return fragment;
    }
    public DemoWebviewFragment() {
        // Required empty public constructor
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            mUrl = getArguments().getString(ARG_PARAM);
        }
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState) {
        // Inflate the layout for this fragment
        FrameLayout view = (FrameLayout) inflater.inflate(R.layout.fragment_demo_webview, container, false);

        WebView web = (WebView) view.findViewById(R.id.demo_webview);
        web.getSettings().setJavaScriptEnabled(true);
        web.loadUrl("file:///android_asset/" + mUrl);

        return view;

    }
}
