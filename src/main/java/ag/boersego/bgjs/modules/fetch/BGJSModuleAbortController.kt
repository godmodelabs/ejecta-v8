package ag.boersego.bgjs.modules.fetch

import ag.boersego.bgjs.JNIV8Object
import ag.boersego.bgjs.V8Engine
import ag.boersego.v8annotations.V8Function
import ag.boersego.v8annotations.V8Getter


/**
 * Created by dseifert on 17.September.2019
 */

class BGJSModuleAbortController @JvmOverloads constructor(v8Engine: V8Engine, jsPtr: Long = 0, args: Array<Any>? = null) : JNIV8Object(v8Engine, jsPtr, args) {

    val signal = BGJSModuleAbortSignal(v8Engine)
        @V8Getter get

    @V8Function
    fun abort() {
        signal.abort()
    }

}