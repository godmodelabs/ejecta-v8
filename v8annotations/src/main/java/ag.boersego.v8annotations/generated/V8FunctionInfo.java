package ag.boersego.v8annotations.generated;

import ag.boersego.v8annotations.V8Function;

/**
 * Created by martin on 04.10.17.
 */

public class V8FunctionInfo {
    public String method;
    public String property;
    public boolean isStatic;

    public V8FunctionInfo(String property, String method, boolean isStatic) {
        this.property = property;
        this.method = method;
        this.isStatic = isStatic;
    }
};
