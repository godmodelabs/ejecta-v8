package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import android.content.Context
import android.content.SharedPreferences

/**
 * Created by Kevin Read <me@kevin-read.com> on 23.11.17 for myrmecophaga-2.0.
 * Copyright (c) 2017 ${ORGANIZATION_NAME}. All rights reserved.
 */

class BGJSModuleLocalStorage (applicationContext: Context) : JNIV8Module("localStorage") {

    private var mPref: SharedPreferences

    init {
        mPref = applicationContext.getSharedPreferences(TAG, 0)
    }

    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        var exports = JNIV8GenericObject.Create(engine)

        exports.setV8Field("getItem", JNIV8Function.Create(engine, JNIV8Function.Handler { receiver, arguments ->
            if (arguments.size < 1 || !(arguments[0] is String)) {
                throw IllegalArgumentException("getItem needs one parameter of type String") as Throwable
            }
            val key = arguments[0] as String

            mPref.getString(key, null)
        }))

        exports.setV8Field("setItem", JNIV8Function.Create(engine, JNIV8Function.Handler { receiver, arguments ->
            if (arguments.size < 2 || !(arguments[0] is String) || !(arguments[1] is String)) {
                throw IllegalArgumentException("setItem needs two parameters of type String")
            }
            val key = arguments[0] as String
            val value = arguments[1] as String
            mPref.edit().putString(key, value).apply()

            JNIV8Undefined.GetInstance()
        }))

        module?.setV8Field("exports", exports)
    }

    companion object {
        private val TAG = BGJSModuleLocalStorage::class.java.simpleName
    }

}