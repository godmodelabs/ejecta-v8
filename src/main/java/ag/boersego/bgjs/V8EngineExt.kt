package ag.boersego.bgjs

/**
 * Created by Kevin Read <me@kevin-read.com> on 12.02.18 for myrmecophaga-2.0.
 * Copyright (c) 2018 BörseGo AG. All rights reserved.
 */

/**
 * Execute a block within a v8 level lock on this v8 engine and hence this v8 Isolate.
 * @param body the block to execute with the lock held
 * @return the result of the block
 */
fun <T> V8Engine.runLocked(body:() -> T): T {
    val lock = lock()
    try {
        val ret = body()
        return ret
    } finally {
        unlock(lock)
    }
}