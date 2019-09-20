package ag.boersego.bgjs;
import ag.boersego.v8annotations.V8Flags;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.lang.reflect.Modifier;
import java.util.Map;

/**
 * Created by martin on 18.08.17.
 */

@SuppressWarnings("unused")
abstract public class JNIV8Object extends JNIObject {
    static public void RegisterAliasForPrimitive(Class alias, Class primitive) {
        RegisterAliasForPrimitive(alias.hashCode(), primitive.hashCode());
    }
    static private native void RegisterAliasForPrimitive(int aliasType, int primitiveType);

    static public void RegisterV8Class(Class<? extends JNIV8Object> derivedClass) {
        if (Modifier.isAbstract(derivedClass.getModifiers())) {
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

    static public native JNIV8Object Create(V8Engine engine, String name, Object... arguments);

    public native double toNumber();
    public native String toString();
    public native String toJSON();
    public native boolean isInstanceOf(JNIV8Function constructor);
    public native boolean isInstanceOf(String name);

    public V8Engine getV8Engine() {
        return _engine;
    }

    public @Nullable Object applyV8Method(String name, Object[] arguments) {
        return _applyV8Method(name, 0, 0, Object.class, arguments);
    }
    @SuppressWarnings({"unchecked"})
    public @Nullable <T> T applyV8MethodTyped(@NonNull String name, int flags, @NonNull Class<T> returnType, @NonNull Object[] arguments) {
        return (T) _applyV8Method(name, flags, returnType.hashCode(), returnType, arguments);
    }
    @SuppressWarnings({"unchecked"})
    public @Nullable <T> T applyV8MethodTyped(@NonNull String name, @NonNull Class<T> returnType, @NonNull Object[] arguments) {
        return (T) _applyV8Method(name, V8Flags.Default, returnType.hashCode(), returnType, arguments);
    }

    private native Object _applyV8Method(@NonNull String name, int flags, int type, Class returnType, Object[] arguments);

    public @Nullable Object callV8Method(@NonNull String name, Object... arguments) {
        return _applyV8Method(name, 0, 0, Object.class, arguments);
    }
    @SuppressWarnings({"unchecked"})
    public @Nullable <T> T callV8MethodTyped(@NonNull String name,  int flags, @NonNull Class<T> returnType, @Nullable Object... arguments) {
        return (T) _applyV8Method(name, flags, returnType.hashCode(), returnType, arguments);
    }
    @SuppressWarnings({"unchecked"})
    public @Nullable <T> T callV8MethodTyped(@NonNull String name, @NonNull Class<T> returnType, @Nullable Object... arguments) {
        return (T) _applyV8Method(name, V8Flags.Default, returnType.hashCode(), returnType, arguments);
    }

    public @Nullable Object getV8Field(@NonNull String name) {
        return _getV8Field(name, 0, 0, Object.class);
    }
    @SuppressWarnings({"unchecked"})
    public @Nullable <T> T getV8FieldTyped(@NonNull String name, int flags, @NonNull Class<T> returnType) {
        return (T) _getV8Field(name, flags, returnType.hashCode(), returnType);
    }
    @SuppressWarnings({"unchecked"})
    public @Nullable <T> T getV8FieldTyped(@NonNull String name, @NonNull Class<T> returnType) {
        return (T) _getV8Field(name, V8Flags.Default, returnType.hashCode(), returnType);
    }
    private native Object _getV8Field(String name, int flags, int type, Class returnType);

    public boolean hasV8Field(@NonNull String name) {
        return hasV8Field(name, false);
    }
    public boolean hasV8OwnField(@NonNull String name) {
        return hasV8Field(name, true);
    }

    public @NonNull String[] getV8Keys() {
        return getV8Keys(false);
    }
    public @NonNull Map<String,Object> getV8Fields()  {
        return getV8Fields(false, 0, 0, null);
    }
    @SuppressWarnings({"unchecked"})
    public @NonNull <T> Map<String, T> getV8FieldsTyped(int flags, Class<T> returnType) {
        return (Map<String, T>)getV8Fields(false, flags, returnType.hashCode(), returnType);
    }
    @SuppressWarnings({"unchecked"})
    public @NonNull <T> Map<String, T> getV8FieldsTyped(Class<T> returnType) {
        return (Map<String, T>)getV8Fields(false, V8Flags.Default, returnType.hashCode(), returnType);
    }

    public @NonNull String[] getV8OwnKeys() {
        return getV8Keys(true);
    }
    public @NonNull Map<String,Object> getV8OwnFields() {
        return getV8Fields(true, 0, 0, null);
    }

    @SuppressWarnings({"unchecked"})
    public @NonNull <T> Map<String, T> getV8OwnFieldsTyped(int flags, Class<T> returnType) {
        return (Map<String, T>)getV8Fields(true, flags, returnType.hashCode(), returnType);
    }
    @SuppressWarnings({"unchecked"})
    public @NonNull <T> Map<String, T> getV8OwnFieldsTyped(Class<T> returnType) {
        return (Map<String, T>)getV8Fields(true, V8Flags.Default, returnType.hashCode(), returnType);
    }

    public native void setV8Field(@NonNull String name, @Nullable Object value);
    public native void setV8Fields(@NonNull Map< String, Object> fields);

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

    public static boolean isInstanceOf(Object obj, JNIV8Function constructor) {
        if(!(obj instanceof JNIV8Object)) return false;
        return ((JNIV8Object)obj).isInstanceOf(constructor);
    }

    public static boolean isInstanceOf(Object obj, String name) {
        if(!(obj instanceof JNIV8Object)) return false;
        return ((JNIV8Object)obj).isInstanceOf(name);
    }

    protected native void adjustJSExternalMemory(long change);

    //------------------------------------------------------------------------
    // internal fields & methods
    private V8Engine _engine;

    public JNIV8Object(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(true);
        _engine = engine;
        initNativeJNIV8Object(getClass().getName(), engine, jsObjPtr);
        initAutomaticDisposure();
    }

    public JNIV8Object(V8Engine engine) {
        super(true);
        _engine = engine;
        initNativeJNIV8Object(getClass().getName(), engine, 0);
        initAutomaticDisposure();
    }

    private native boolean hasV8Field(String name, boolean ownOnly);
    private native String[] getV8Keys(boolean ownOnly);
    private native Map<String,Object> getV8Fields(boolean ownOnly, int flags, int type, Class returnType);
    private native void initNativeJNIV8Object(String canonicalName, V8Engine engine, long jsObjPtr);
}
