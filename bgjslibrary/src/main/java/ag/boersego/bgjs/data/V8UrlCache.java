package ag.boersego.bgjs.data;

/**
 * Copyright BörseGo AG 2014
 * Created by kread on 09.04.14.
 */
public interface V8UrlCache {
    abstract public void storeInCache (String url, String response, int maxAge);
    abstract public void storeInCache (String url, Object response, int maxAge);
}
