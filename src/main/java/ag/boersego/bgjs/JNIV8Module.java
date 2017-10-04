package ag.boersego.bgjs;

import ag.boersego.bgjs.JNIV8GenericObject;

/**
 * Created by martin on 29.09.17.
 */

abstract public class JNIV8Module {
    private String name;

    public JNIV8Module(String name) {
        this.name = name;
    }

    public String getName() {
        return name;
    }

    abstract public void Require(V8Engine engine, JNIV8GenericObject module);
}
