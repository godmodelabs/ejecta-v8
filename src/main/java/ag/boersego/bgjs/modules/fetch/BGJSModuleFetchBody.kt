package ag.boersego.bgjs.modules

import ag.boersego.bgjs.JNIV8GenericObject
import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.JNIV8Promise
import ag.boersego.bgjs.V8Engine
import ag.boersego.v8annotations.V8Function

abstract open class BGJSModuleFetchBody @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    internal var body: String? = null

    open var bodyUsed = false
        internal set
        get

    @V8Function
    open fun json(): JNIV8Object {
        // TODO: parse
        // TODO: create promise
        return JNIV8GenericObject.Create(v8Engine)
    }

    @V8Function
    open fun text(): JNIV8Promise {
        val resolver = JNIV8Promise.CreateResolver(v8Engine)

        v8Engine.enqueueOnNextTick {
            if (body != null) {
                resolver.resolve(body)
            } else {
                resolver.reject(JNIV8Object.Create(v8Engine, "TypeError", "no body"))
            }
        }

        return resolver.promise
    }
}