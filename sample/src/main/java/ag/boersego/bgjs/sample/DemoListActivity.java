package ag.boersego.bgjs.sample;

import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentActivity;

import ag.boersego.bgjs.V8Engine;


/**
 * An activity representing a list of Demos. This activity
 * has different presentations for handset and tablet-size devices. On
 * handsets, the activity presents a list of items, which when touched,
 * lead to a {@link DemoDetailActivity} representing
 * item details. On tablets, the activity presents the list of items and
 * item details side-by-side using two vertical panes.
 * <p>
 * The activity makes heavy use of fragments. The list of items is a
 * {@link DemoListFragment} and the item details
 * (if present) is a {@link DemoEjectaFragment}.
 * <p>
 * This activity also implements the required
 * {@link DemoListFragment.Callbacks} interface
 * to listen for item selections.
 */
public class DemoListActivity extends FragmentActivity
        implements DemoListFragment.Callbacks {

    /**
     * Whether or not the activity is in two-pane mode, i.e. running on a tablet
     * device.
     */
    private boolean mTwoPane;
    private V8Engine mV8Instance;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mV8Instance = V8Engine.getInstance(getApplication(), "js/plasma.js");

        setContentView(R.layout.activity_demo_list);

        if (findViewById(R.id.demo_detail_container) != null) {
            // The detail container view will be present only in the
            // large-screen layouts (res/values-large and
            // res/values-sw600dp). If this view is present, then the
            // activity should be in two-pane mode.
            mTwoPane = true;

            // In two-pane mode, list items should be given the
            // 'activated' state when touched.
            ((DemoListFragment) getSupportFragmentManager()
                    .findFragmentById(R.id.demo_list))
                    .setActivateOnItemClick(true);
        }

        // TODO: If exposing deep links into your app, handle intents here.
    }

    /**
     * Callback method from {@link DemoListFragment.Callbacks}
     * indicating that the item with the given ID was selected.
     */
    @Override
    public void onItemSelected(String id) {
        if (mTwoPane) {
            // In two-pane mode, show the detail view in this activity by
            // adding or replacing the detail fragment using a
            // fragment transaction.
            Fragment fragment = FragmentFactory.createFragment(id, this);

            if (fragment != null) {
                getSupportFragmentManager().beginTransaction()
                        .replace(R.id.demo_detail_container, fragment)
                        .commit();
            }

        } else {
            // In single-pane mode, simply start the detail activity
            // for the selected item ID.
            Intent detailIntent = new Intent(this, DemoDetailActivity.class);
            detailIntent.putExtra(DemoEjectaFragment.ARG_ITEM_ID, id);
            startActivity(detailIntent);
        }
    }
}
