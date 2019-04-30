package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import java.util.*


/**
 * Created by dseifert on 30.April.2019
 */

class BGJSModulePlatform(applicationContext: Context, v8Engine: V8Engine) : JNIV8Module("platform") {
    private var isTablet = applicationContext.resources.getBoolean(R.bool.isTablet)
    private val intentFilter = IntentFilter().apply {
        addAction(Intent.ACTION_LOCALE_CHANGED)
        addAction(Intent.ACTION_TIMEZONE_CHANGED)
    }

    private lateinit var eventBus: JNIV8GenericObject

    private var localeChangedReciever = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Intent.ACTION_LOCALE_CHANGED -> eventBus.callV8Method("dispatch", "platform:locale", Locale.getDefault().toString())
                Intent.ACTION_TIMEZONE_CHANGED -> eventBus.callV8Method("dispatch", "platform:timeZone", TimeZone.getDefault().id)
            }
        }
    }

    init {
        v8Engine.addStatusHandler {
            eventBus = v8Engine.require("./js/core/EventBus.js") as JNIV8GenericObject
            applicationContext.registerReceiver(localeChangedReciever, intentFilter)
        }
    }

    override fun Require(engine: V8Engine, module: JNIV8GenericObject?) {
        var exports = JNIV8GenericObject.Create(engine)

        exports.setV8Field("environment", "BGJSContext")
        exports.setV8Field("type", "android")
        exports.setV8Field("debug", BuildConfig.DEBUG)
        exports.setV8Field("isStoreBuild", BuildConfig.BUILD_TYPE.startsWith("release"))
        exports.setV8Field("locale", JNIV8Function.Create(engine) { receiver, arguments ->
             return@Create Locale.getDefault().toString()
        })
        exports.setV8Field("language", JNIV8Function.Create(engine) { _, _ ->
            return@Create Locale.getDefault().language
        })
        exports.setV8Field("timezone", JNIV8Function.Create(engine) { _, _ ->
            return@Create TimeZone.getDefault().id
        })
        if (isTablet) {
            exports.setV8Field("deviceClass", "tablet")
        } else {
            exports.setV8Field("deviceClass", "phone")
        }

        module?.setV8Field("exports", exports)
    }

    companion object {
        private val TAG = BGJSModulePlatform::class.java.simpleName
    }

}