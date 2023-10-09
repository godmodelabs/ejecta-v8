package ag.boersego.bgjs;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;

import java.util.Map;
import java.util.Set;

/**
 * Created by martin on 26.09.17.
 */

public final class JNIV8GenericObject extends JNIV8Object {

    public static final String TAG = JNIV8GenericObject.class.getSimpleName();

    public static native JNIV8GenericObject Create(V8Engine engine);

    @Override
    public void dispose() throws RuntimeException {
        super.dispose();
    }

    //------------------------------------------------------------------------
    // internal fields & methods
    @Keep
    private JNIV8GenericObject(V8Engine engine, long jsObjPtr, Object[] arguments) {
        super(engine, jsObjPtr, arguments);
    }

    public static JNIV8GenericObject fromMap(final V8Engine engine, @NonNull final Map<String, Object> map) {
        final JNIV8GenericObject instance = Create(engine);

        final Set<Map.Entry<String, Object>> entrySet = map.entrySet();
        for (Map.Entry<String, Object> entry : entrySet) {
            final Object value = entry.getValue();
            // We can recursively process maps into JNIV8GenericObjects
            if (value instanceof Map) {
                final JNIV8GenericObject innerInstance = JNIV8GenericObject.fromMap(engine, (Map<String, Object>)value);
                instance.setV8Field(entry.getKey(), innerInstance);
            } else {
                instance.setV8Field(entry.getKey(), value);
            }
        }

        return instance;
    }
}
