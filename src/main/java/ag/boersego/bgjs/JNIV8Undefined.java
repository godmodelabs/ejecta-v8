package ag.boersego.bgjs;

import android.support.annotation.Keep;
import android.support.annotation.NonNull;

/**
 * Created by martin on 27.09.17.
 */

public class JNIV8Undefined {
    private volatile static JNIV8Undefined instance;
    public synchronized static JNIV8Undefined GetInstance() {
        if(instance == null) {
            instance = new JNIV8Undefined();
        }
        return instance;
    }

    @NonNull
    @Override
    public String toString() {
        return "undefined";
    }

    @Keep
    private JNIV8Undefined() {}
}
