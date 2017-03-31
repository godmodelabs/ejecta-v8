package ag.boersego.bgjs;

/**
 * V8BackedJavaClass
 * This is the base class for all Java stubs around a native (=JNI) class. The class assures the cleanup of the persistent v8 handle on finalize
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 *
 **/

public class V8BackedJavaClass {

	protected long mNativePtr;
	
	protected V8BackedJavaClass(long nativePtr) {
		mNativePtr = nativePtr;
	}

    public long getNativePtr () {
        return mNativePtr;
    }
	
	@Override
	protected void finalize() throws Throwable {
        super.finalize();
        cleanup();
	}

	public synchronized void cleanup() {
        if (mNativePtr != 0) {
            ClientAndroid.cleanupNativeFnPtr(V8Engine.getCachedInstance().getNativePtr(), mNativePtr);
            mNativePtr = 0;
        }
	}
}
