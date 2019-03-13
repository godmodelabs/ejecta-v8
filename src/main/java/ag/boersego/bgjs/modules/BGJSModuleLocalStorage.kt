package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import ag.boersego.v8annotations.V8Getter
import android.content.Context
import android.content.SharedPreferences

/**
 * Created by Kevin Read <me@kevin-read.com> on 23.11.17 for myrmecophaga-2.0.
 * Copyright (c) 2017 BörseGo AG. All rights reserved.
 */

class BGJSModuleLocalStorage (applicationContext: Context) : JNIV8Module("localStorage") {

    private var mPref: SharedPreferences
    val length: Int
        @V8Getter get() = mPref.all.size

    init {
        mPref = applicationContext.getSharedPreferences(TAG, 0)
    }
    //TODO: implement version of key() method
    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        var exports = JNIV8GenericObject.Create(engine)

        exports.setV8Field("clear", JNIV8Function.Create(engine) { receiver, arguments ->
            if (!arguments.isEmpty()) {
                throw IllegalArgumentException("clear needs no arguments")
            }
            mPref.edit().clear().apply()
            JNIV8Undefined.GetInstance()
        })

        exports.setV8Field("getItem", JNIV8Function.Create(engine) { receiver, arguments ->
            if (arguments.isEmpty() || arguments[0] !is String) {
                throw IllegalArgumentException("getItem needs one parameter of type String")
            }
            val key = arguments[0] as String

            mPref.getString(key, null)
        })

        exports.setV8Field("removeItem", JNIV8Function.Create(engine) { receiver, arguments ->
            if (arguments.isEmpty() || arguments[0] !is String) {
                throw IllegalArgumentException("removeItem needs one parameter of type String")
            }
            val key = arguments[0] as String
            mPref.edit().remove(key).apply()

            JNIV8Undefined.GetInstance()
        })

        exports.setV8Field("setItem", JNIV8Function.Create(engine) { _, arguments ->
            if (arguments.size < 2 || arguments[0] !is String || arguments[1] !is String) {
                throw IllegalArgumentException("setItem needs two parameters of type String")
            }
            val key = arguments[0] as String
            val value = arguments[1] as String
            mPref.edit().putString(key, value).apply()

            JNIV8Undefined.GetInstance()
        })

        module?.setV8Field("exports", exports)
    }

    companion object {
        private val TAG = BGJSModuleLocalStorage::class.java.simpleName
    }

}