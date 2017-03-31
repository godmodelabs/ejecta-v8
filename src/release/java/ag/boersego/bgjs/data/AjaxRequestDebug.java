package ag.boersego.bgjs.data;

import java.io.IOException;
import java.net.HttpURLConnection;
import java.io.InputStream;

public class AjaxRequestDebug {

    public AjaxRequestDebug(final String name) { }
    public void httpExchangeFailed(final IOException ex) { }
    public void postConnect() { }
    public void preConnect(HttpURLConnection connection, Object entity) { }
    public InputStream interpretResponseStream(final InputStream is) {
        return is;
    }
}