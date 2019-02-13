package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8GenericObject
import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.JNIV8Promise
import ag.boersego.bgjs.V8Engine
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter

abstract open class BGJSModuleFetchBody @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    var body: String? = null
        internal set
        @V8Getter get

    var bodyUsed = false
        internal set
        @V8Getter get

    @V8Function
    fun json(): JNIV8Object {
        // TODO: parse
        // TODO: create promise
        return JNIV8GenericObject.Create(v8Engine)
    }

    @V8Function
    fun text(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        v8Engine.enqueueOnNextTick {
            if (body != null) {
                resolver.resolve(body)
            } else {
                resolver.reject("")
            }
        }


        return resolver.promise
    }
}