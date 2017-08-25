package ag.boersego.bgjs;

/**
 * Created by martin on 18.04.17.
 */

public class JNIObject {
    private long nativeHandle;
    private native void initNative();
    private native void disposeNative(long nativeHandle);

    private static native void initBinding();
    static {
        initBinding();
    }

    public JNIObject() {
        initNative();
    }

    public void dispose() {
        if(nativeHandle == 0) return;
        disposeNative(nativeHandle);
        nativeHandle = 0;
    }

    public boolean isDisposed() {
        return nativeHandle == 0;
    }

    @Override
    protected void finalize() throws Throwable {
        dispose();
        super.finalize();
    }
}
