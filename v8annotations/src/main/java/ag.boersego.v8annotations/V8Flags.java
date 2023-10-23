package ag.boersego.v8annotations;

/**
 * Created by martin on 20.12.17.
 */

public class V8Flags {

    /*
     * Default behaviour
     */
    public static final int Default = 0;

    /*
     * throw exception instead of returning null
     * has no effect if CoerceNull is set or requested type can not handle null (e.g. double)
     * if combined with UndefinedIsNull, will also make undefined throw
     */
    public static final int NonNull = 1;

    /*
     * return undefined as null
     * has no effect on coercion or if requested type can not handle null (e.g. double)
     */
    public static final int UndefinedIsNull = 1<<1;

    /*
     * throw exception when encountering values of different type
     * instead of coercing them
     * Note: null is allowed if requested type can handle it (e.g. Double, but not double)!
     */
    public static final int Strict = 1<<2;

    /*
     * coerce null even if requested type can handle it (e.g. Double)
     */
    public static final int CoerceNull = 1<<3;
}