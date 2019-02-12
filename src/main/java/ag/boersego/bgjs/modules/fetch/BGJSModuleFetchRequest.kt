package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.JNIV8Function
import ag.boersego.bgjs.JNIV8GenericObject
import ag.boersego.bgjs.JNIV8Iterator
import ag.boersego.bgjs.V8Engine
import ag.boersego.bgjs.modules.BGJSModuleFetchBody
import ag.boersego.bgjs.modules.BGJSModuleFetchHeaders
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter
import ag.boersego.v8annotations.V8Symbols
import android.util.Log

/**
 * Created by Kevin Read <me@kevin-read.com> on 06.02.19 for JSNext-android.
 * Copyright (c) 2019 BörseGo AG. All rights reserved.
 */

class BGJSModuleFetchRequest @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : BGJSModuleFetchBody(v8Engine, jsPtr, args) {
/*
    val iterator : JNIV8Function
        @V8Getter(symbol = V8Symbols.ITERATOR) get() {
            return JNIV8Function.Create(v8Engine) { _, _ ->
                return JNIV8Iterator(this.v8Engine, setOf("pen", "cup", "dog", "spectacles"))
            }
        }
*/
    @V8Function(symbol = V8Symbols.ITERATOR)
    fun iterator() : JNIV8Iterator {
        return JNIV8Iterator(this.v8Engine, setOf("pen", "cup", "dog", "spectacles"))
    }

    var cache = "default"
        internal set(value) {
            if (value == "default" || value == "no-store" || value == "reload" || value == "no-cache"
                || value == "force-cache" || value == "only-if-cached") {
                field = value
            } else {
                throw RuntimeException("invalid cache: '$value'")
            }
        }
        @V8Getter get

    var context = ""
        internal set
        @V8Getter get

    var credentials = "omit"
        internal set(value) {
            if (value == "omit" || value == "same-origin" || value == "include") {
                field = value
            } else {
                throw RuntimeException("invalid credentials: '$value'")
            }
        }
        @V8Getter get

    var destination = ""
        internal set
        @V8Getter get

    var headers: BGJSModuleFetchHeaders? = null
        internal set
        @V8Getter get

    var method = "GET"
        internal set
        @V8Getter get

    var mode = "no-cors"
        internal set(value) {
            if (value == "same-origin" || value == "cors" || value == "no-cors" || value == "navigate" || value == "websocket") {
                field = value
            } else {
                throw RuntimeException("invalid mode: '$value'")
            }
        }
        @V8Getter get

    var redirect = "follow"
        internal set(value) {
            if (value == "follow" || value == "error" || value == "manual") {
                field = value
            } else {
                throw RuntimeException("invalid redirect: '$value'")
            }
        }
        @V8Getter get

    var referrer = "client"
        internal set
        @V8Getter get

    var referrerPolicy = ""
        internal set
        @V8Getter get

    var url = ""
        internal set
        @V8Getter get

    /**
     * The clone() method of the Request interface creates a copy of the current Request object.
     *
     * clone() throws a TypeError if the response Body has already been used. In fact, the main reason clone()
     * exists is to allow multiple uses of Body objects (when they are one-use only.)
     * @return cloned request object
     */
    @V8Function
    fun clone(): BGJSModuleFetchRequest {
        if (bodyUsed) {
            throw RuntimeException("TypeError: cannot clone because body already used")
        }
        val clone = BGJSModuleFetchRequest(v8Engine)
        clone.cache = cache
        clone.context = context
        clone.credentials = credentials
        clone.destination = destination
        // TODO: check if we need to clone headers
        clone.headers = headers
        clone.method = method
        clone.mode = mode
        clone.redirect = redirect
        clone.referrer = referrer
        clone.referrerPolicy = referrerPolicy
        clone.url = url

        return clone
    }
}