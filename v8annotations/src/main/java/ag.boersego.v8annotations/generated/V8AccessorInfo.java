package ag.boersego.v8annotations.generated;

/**
 * Created by martin on 04.10.17.
 */

public class V8AccessorInfo {
    public String property;
    public String setter;
    public String getter;
    public String type;
    public boolean isStatic;
    public boolean isNullable;

    public V8AccessorInfo(String property, String type, String getter, String setter, boolean isStatic, boolean isNullable) {
        this.property = property;
        this.getter = getter;
        this.setter = setter;
        this.type = type;
        this.isStatic = isStatic;
        this.isNullable = isNullable;
    };
};
