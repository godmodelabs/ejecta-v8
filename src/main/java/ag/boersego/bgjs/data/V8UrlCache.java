package ag.boersego.bgjs.data;

/**
 * Copyright BörseGo AG 2014
 * Created by kread on 09.04.14.
 */
public interface V8UrlCache {
    void storeInCache(String url, Object response, int maxAge, final long size);
}
