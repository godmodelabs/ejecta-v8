package ag.boersego.bgjs;

import androidx.annotation.NonNull;

/**
 * Created by martin on 29.09.17.
 */

public abstract class JNIV8Module {

    /**
     * A JNIV8Module that wants to be informed by the V8Engine about engine suspension and resuming
     */
    public interface IJNIV8Suspendable {
        void onSuspend();

        void onResume();
    }

    private final String name;

    public JNIV8Module(String name) {
        this.name = name;
    }

    public final String getName() {
        return name;
    }

    /**
     * Implement class logic here. This is called when this module is required. Call setV8Field on
     * the module.
     *
     * @param engine
     * @param module
     */
    public abstract void Require(@NonNull V8Engine engine, JNIV8GenericObject module);
}
