package ag.boersego.bgjs;

import ag.boersego.v8annotations.V8Function;

import java.util.Iterator;

public class JNIV8Iterator extends JNIV8Object {
    private Iterator<Object> iterator;

    public JNIV8Iterator(V8Engine engine, Iterator<Object> iterator) {
        super(engine);
        this.iterator = iterator;
    }

    public JNIV8Iterator(V8Engine engine, Iterable<Object> iterable) {
        super(engine);
        this.iterator = iterable.iterator();
    }

    @V8Function
    public JNIV8GenericObject next() {
        boolean hasNext = iterator.hasNext();

        JNIV8GenericObject object = JNIV8GenericObject.Create(this.getV8Engine());
        object.setV8Field("done", !hasNext);
        if(hasNext) {
            object.setV8Field("value", iterator.next());
        }

        return object;
    }
}
