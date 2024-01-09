package ag.boersego.bgjs.modules

import ag.boersego.bgjs.*
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import java.util.*


/**
 * Created by dseifert on 30.April.2019
 */

class BGJSModulePlatform(applicationContext: Context, v8Engine: V8Engine, val appVersion: String) : JNIV8Module("platform") {
    private var isTablet = applicationContext.resources.getBoolean(R.bool.isTablet)
    private val intentFilter = IntentFilter().apply {
        addAction(Intent.ACTION_TIMEZONE_CHANGED)
        addAction(ACTION_LOCALE_CHANGED)
    }

    private lateinit var eventBus: JNIV8GenericObject

    private var localeChangedReciever = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                ACTION_LOCALE_CHANGED -> eventBus.callV8Method("dispatch", "platform:locale", Locale.getDefault().toString())
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
        val exports = JNIV8GenericObject.Create(engine)

        exports.setV8Field("environment", "BGJSContext")
        exports.setV8Field("type", "android")
        exports.setV8Field("version", appVersion)
        exports.setV8Field("debug", BuildConfig.DEBUG)
        exports.setV8Field("isStoreBuild", BuildConfig.BUILD_TYPE.startsWith("release"))
        exports.setV8Field("deviceClass", if (isTablet) "tablet" else "phone")
        exports.setV8Field("sdkVersion", Build.VERSION.SDK_INT)
        exports.setV8Accessor("locale", JNIV8Function.Create(engine) { _, _ -> Locale.getDefault().toString() }, null)
        exports.setV8Accessor("language", JNIV8Function.Create(engine) { _, _ -> Locale.getDefault().language }, null)
        exports.setV8Accessor("timezone", JNIV8Function.Create(engine) { _, _ -> TimeZone.getDefault().id }, null)

        module?.setV8Field("exports", exports)
    }

    companion object {
        const val ACTION_LOCALE_CHANGED = "actionLocaleChanged"
    }
}