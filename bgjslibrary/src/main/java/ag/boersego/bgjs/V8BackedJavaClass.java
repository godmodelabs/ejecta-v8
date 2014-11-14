package ag.boersego.bgjs;

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
		if (mNativePtr != 0) {
			ClientAndroid.cleanupNativeFnPtr(mNativePtr);
		}
	}
}
