package ag.boersego.bgjs;

/**
 * Created by martin on 25.09.17.
 */

public class JNIV8Value {
    public enum EType {
        UNDEFINED, NUMBER, STRING, BOOLEAN, SYMBOL, OBJECT
    };

    private static final EType[] typeValues = EType.values();
    private EType type;
    private Object value;

    public JNIV8Value(double v) {
        type = EType.NUMBER;
        value = v;
    }

    public JNIV8Value(boolean v) {
        type = EType.BOOLEAN;
        value = v;
    }

    public JNIV8Value(int t, Object obj) {
        type = typeValues[t];
        value = obj;
    }

    public EType getType() {
        return type;
    }

    public boolean isFunction() {
        return type == EType.OBJECT && value instanceof JNIV8Function;
    }

    public boolean isArray() {
        return type == EType.OBJECT && value instanceof JNIV8Array;
    }

    public boolean isGenericObject() {
        return type == EType.OBJECT && value instanceof JNIV8GenericObject;
    }

    public boolean isNull() {
        return value == null;
    }

    public long getNumber() {
        return (long)value;
    }

    public boolean getBoolean() {
        return (boolean)value;
    }

    public String getString() {
        return (String)value;
    }

    public JNIV8Object getObject() {
        return (JNIV8Object)value;
    }

    public JNIV8Object getFunction() {
        return (JNIV8Object)value;
    }
}
