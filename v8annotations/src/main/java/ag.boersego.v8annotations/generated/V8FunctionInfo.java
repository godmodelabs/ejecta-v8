package ag.boersego.v8annotations.generated;

/**
 * Created by martin on 04.10.17.
 */

public class V8FunctionInfo {
    public String method;
    public String property;
    public boolean isStatic;
    public String returnType;
    public V8FunctionArgumentInfo[] arguments;

    public V8FunctionInfo(String property, String method, String returnType, boolean isStatic, V8FunctionArgumentInfo[] args) {
        this.property = property;
        this.method = method;
        this.isStatic = isStatic;
        this.returnType = returnType;
        this.arguments = args;
    }

    public static class V8FunctionArgumentInfo {
        public String type;
        public boolean isNullable;
        public boolean undefinedIsNull;

        public V8FunctionArgumentInfo(final String type, final boolean isNullable, final boolean undefinedIsNull) {
            this.type = type;
            this.isNullable = isNullable;
            this.undefinedIsNull = undefinedIsNull;
        }
    }
};
