package ag.boersego.bgjs.sample;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;

/**
 * Copyright BörseGo AG 2014
 * Created by kread on 18.06.14.
 */
public class FragmentFactory {

    public static Fragment createFragment(String argument, Context ctx) {
        Fragment fragment = null;
        Bundle arguments = new Bundle();

        if (argument.startsWith("webview:")) {
            fragment = new DemoWebviewFragment();
            arguments.putString(DemoEjectaFragment.ARG_ITEM_ID, argument.replace("webview:", ""));
        } else if (argument.startsWith("url:")) {
            Intent i = new Intent(Intent.ACTION_VIEW);
            i.setData(Uri.parse(argument.replace("url:", "")));
            ctx.startActivity(i);
        } else {
            fragment = new DemoEjectaFragment();
            arguments.putString(DemoEjectaFragment.ARG_ITEM_ID, argument);
        }

        if (fragment != null) {
            fragment.setArguments(arguments);
        }

        return fragment;
    }
}
