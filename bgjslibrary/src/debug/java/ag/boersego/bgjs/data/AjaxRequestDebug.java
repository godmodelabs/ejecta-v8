package ag.boersego.bgjs.data;

import com.facebook.stetho.urlconnection.StethoURLConnectionManager;

/**
 * Created by kread on 18/02/16.
 */
public class AjaxRequestDebug extends StethoURLConnectionManager {
    public AjaxRequestDebug(String friendlyName) {
        super(friendlyName);
    }
}
