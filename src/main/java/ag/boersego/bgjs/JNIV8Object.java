package ag.boersego.bgjs;
import java.lang.reflect.Modifier;
import java.util.HashMap;
import java.util.Map;

/**
 * Created by martin on 18.08.17.
 */

abstract public class JNIV8Object extends JNIObject {
    class ReturnType {
        static final byte
                Object = 0,
                Boolean = 1,
                Byte = 2,
                Character = 3,
                Short = 4,
                Integer = 5,
                Long = 6,
                Float = 7,
                Double = 8,
                String = 9,
                Void = 10;
    };

    static protected HashMap<Class, Byte> typeMap;
    static {
        typeMap = new HashMap<>();
        typeMap.put(Object.class, ReturnType.Object);
        typeMap.put(Boolean.class, ReturnType.Boolean);
        typeMap.put(Byte.class, ReturnType.Byte);
        typeMap.put(Character.class, ReturnType.Character);
        typeMap.put(Short.class, ReturnType.Short);
        typeMap.put(Integer.class, ReturnType.Integer);
        typeMap.put(Long.class, ReturnType.Long);
        typeMap.put(Float.class, ReturnType.Float);
        typeMap.put(Double.class, ReturnType.Double);
        typeMap.put(String.class, ReturnType.String);
        typeMap.put(Void.class, ReturnType.Void);
    }

    static public void RegisterV8Class(Class<? extends JNIV8Object> derivedClass) {
        if(Modifier.isAbstract(derivedClass.getModifiers())) {
            throw new RuntimeException("Abstract classes can not be registered");
        }

        // there might be one or more abstract levels between this class and the next registered superclass
        // => we can just skip them!
        Class superClass = derivedClass.getSuperclass();
        while(superClass != JNIV8Object.class && Modifier.isAbstract(superClass.getModifiers())) {
            superClass = superClass.getSuperclass();
        }

        RegisterV8Class(derivedClass.getCanonicalName(), superClass.getCanonicalName());
    }

    static private native void RegisterV8Class(String derivedClass, String baseClass);

    public native double toNumber();
    public native String toString();
    public native String toJSON();

    public V8Engine getV8Engine() {
        return _engine;
    }

    public native Object applyV8Method(String name, Object[] arguments);
    @SuppressWarnings({"unchecked"})
    public <T> T applyV8Method(String name, Class<T> returnType, Object[] arguments) {
        byte type = 0;
        if(typeMap.containsKey(returnType)) {
            type = typeMap.get(returnType);
        }
        return (T) applyV8Method(name, type, arguments);
    }
    private native Object applyV8Method(String name, byte returnType, Object[] arguments);

    public native Object callV8Method(String name, Object... arguments);
    @SuppressWarnings({"unchecked"})
    public <T> T callV8Method(String name, Class<T> returnType, Object... arguments) {
        byte type = 0;
        if(typeMap.containsKey(returnType)) {
            type = typeMap.get(returnType);
        }
        return (T) applyV8Method(name, type, arguments);
    }

    public native Object getV8Field(String name);
    @SuppressWarnings({"unchecked"})
    public <T> T getV8Field(String name, Class<T> returnType) {
        byte type = 0;
        if(typeMap.containsKey(returnType)) {
            type = typeMap.get(returnType);
        }
        return (T) getV8Field(name, type);
    }
    private native Object getV8Field(String name, byte returnType);


    public boolean hasV8Field(String name) {
        return hasV8Field(name, false);
    }
    public boolean hasV8OwnField(String name) {
        return hasV8Field(name, true);
    }

    public String[] getV8Keys() {
        return getV8Keys(false);
    }
    public Map<String,Object> getV8Fields()  {
        return getV8Fields(false);
    }

    public String[] getV8OwnKeys() {
        return getV8Keys(true);
    }
    public Map<String,Object> getV8OwnFields() {
        return getV8Fields(true);
    }

    public native void setV8Field(String name, Object value);
    public native void setV8Fields(Map<String, Object> fields);

    protected native void adjustJSExternalMemory(long change);

    //------------------------------------------------------------------------
    // internal fields & methods
    private V8Engine _engine;

    public JNIV8Object(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(true);
        _engine = engine;
        initNativeJNIV8Object(getClass().getCanonicalName(), engine, jsObjPtr);
        initAutomaticDisposure();
    }

    public JNIV8Object(V8Engine engine) {
        super(true);
        _engine = engine;
        initNativeJNIV8Object(getClass().getCanonicalName(), engine, 0);
        initAutomaticDisposure();
    }

    private native boolean hasV8Field(String name, boolean ownOnly);
    private native String[] getV8Keys(boolean ownOnly);
    private native Map<String,Object> getV8Fields(boolean ownOnly);
    private native void initNativeJNIV8Object(String canonicalName, V8Engine engine, long jsObjPtr);

    /**
     * convert a wrapped object to a number using the javascript coercion rules
     * @param obj
     * @return double
     */
    public static double asNumber(Object obj) {
        if(obj == null) {
            return 0;
        } else if(obj instanceof JNIV8Object) {
            return ((JNIV8Object) obj).toNumber();
        } else if(obj instanceof Number) {
            // handles Byte, Short, Integer, Long, Float, Double
            return ((Number)obj).doubleValue();
        } else if(obj instanceof String) {
            return Double.valueOf((String)obj);
        } else if(obj instanceof Boolean) {
            return ((Boolean) obj ? 1 : 0);
        } else if(obj instanceof JNIV8Undefined) {
            return Double.NaN;
        } else if(obj instanceof Character) {
            return Character.getNumericValue((Character) obj);
        }
        // other java-only types are "false"
        throw new ClassCastException("Cannot convert to number: " + obj);
    }

    /**
     * Check if a wrapped object is truthy (see https://developer.mozilla.org/en-US/docs/Glossary/Falsy)
     * @param obj
     * @return true if truthy
     */
    public static boolean asBoolean(Object obj) {
        if (obj == null) {
            return false;
        } else if(obj instanceof JNIV8Object) {
            return true;
        } else if (obj instanceof Number) {
            return ((Number)obj).intValue() > 0;
        } else if (obj instanceof String) {
            return !((String)obj).isEmpty();
        } else if (obj instanceof Boolean) {
            return ((Boolean)obj);
        } else if (obj instanceof JNIV8Undefined) {
            return false;
        } else if(obj instanceof Character) {
            return (Character) obj > 0;
        }
        // other java-only types are "false"
        throw new ClassCastException("Cannot convert to boolean: " + obj);
    }

    /**
     * convert a wrapped object to a String using the javascript "toString" method
     * @param obj
     * @return String
     */
    public static String asString(Object obj) {
        if(obj == null) {
            return "";
        } else if(obj instanceof JNIV8Object) {
            return obj.toString();
        } else if(obj instanceof Number) {
            // handles Byte, Short, Integer, Long, Float, Double
            return String.valueOf(obj);
        } else if(obj instanceof String) {
            return String.valueOf(obj);
        } else if(obj instanceof Boolean) {
            return String.valueOf(obj);
        } else if(obj instanceof JNIV8Undefined) {
            return "undefined";
        } else if(obj instanceof Character) {
            return String.valueOf(obj);
        }
        // other java-only types are "false"
        throw new ClassCastException("Cannot convert to String: " + obj);
    }
}
