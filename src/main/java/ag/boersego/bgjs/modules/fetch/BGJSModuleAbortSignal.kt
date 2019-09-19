package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.V8Engine
import ag.boersego.bgjs.V8JSException
import ag.boersego.v8annotations.V8Getter


/**
 * Created by dseifert on 17.September.2019
 */

class BGJSModuleAbortSignal @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    var aborted = false
        private set
        @V8Getter get

    var eventListeners = ArrayList<EventListener>()

    init {
        if (args != null) {
            // Was directly constructed in JS
            throw V8JSException(v8Engine, "TypeError", "AbortSignal cannot be constructed directly")
        }
    }

    fun abort() {
        if (aborted) return
        aborted = true
        for (cb in eventListeners) {
          cb.onEvent("abort")
        }
    }


    fun addEventListener (cb: EventListener) {
        eventListeners.add(cb)
    }

    fun removeListener (cb: EventListener) {
        eventListeners = eventListeners.filter { it != cb } as ArrayList<EventListener>
    }

}

interface EventListener {
    fun onEvent(context: String?)
}