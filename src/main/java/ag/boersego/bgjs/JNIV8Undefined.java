package ag.boersego.bgjs;

/**
 * Created by martin on 27.09.17.
 */

public class JNIV8Undefined {
    private static JNIV8Undefined instance;
    public static JNIV8Undefined GetInstance() {
        if(instance == null) {
            instance = new JNIV8Undefined();
        }
        return instance;
    }

    protected JNIV8Undefined() {}
}
