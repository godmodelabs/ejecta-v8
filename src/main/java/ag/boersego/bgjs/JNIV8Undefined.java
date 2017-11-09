package ag.boersego.bgjs;

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

    private JNIV8Undefined() {}
}
