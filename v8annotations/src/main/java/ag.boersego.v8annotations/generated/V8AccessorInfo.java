package ag.boersego.v8annotations.generated;

/**
 * Created by martin on 04.10.17.
 */

public class V8AccessorInfo {
    public String property;
    public String setter;
    public String getter;
    public boolean isStatic;

    public V8AccessorInfo(String property, String getter, String setter, boolean isStatic) {
        this.property = property;
        this.getter = getter;
        this.setter = setter;
        this.isStatic = isStatic;
    };
};
