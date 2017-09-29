package ag.boersego.bgjs;

/**
 * Created by martin on 29.09.17.
 */

public class V8Module extends JNIV8Module {
    public V8Module() {
        super("javaModule");
    }

    @Override
    public void Require(V8Engine engine, JNIV8GenericObject module) {
        JNIV8GenericObject exports = JNIV8GenericObject.Create(engine);
        exports.setV8Field("TestClass", engine.getConstructor(V8TestClassDerived.class));
        module.setV8Field("exports", exports);
    }
}
