package ag.boersego.bgjs;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

/**
 * Created by martin on 27.09.17.
 */

public class JNIV8Undefined {
    private static volatile JNIV8Undefined instance;
    public static synchronized JNIV8Undefined GetInstance() {
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
