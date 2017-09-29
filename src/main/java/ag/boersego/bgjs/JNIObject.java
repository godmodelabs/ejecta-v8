package ag.boersego.bgjs;

/**
 * Created by martin on 18.04.17.
 */

abstract public class JNIObject {
    private long nativeHandle;
    private native void initNative();
    private native void disposeNative(long nativeHandle);

    private static native void initBinding();
    static {
        initBinding();
    }

    static public void RegisterClass(Class<? extends JNIObject> derivedClass) {
        RegisterClass(derivedClass, JNIObject.class);
    }
    static public void RegisterClass(Class<? extends JNIObject> derivedClass, Class<? extends JNIObject> baseClass) {
        RegisterClass(derivedClass.getCanonicalName(), baseClass.getCanonicalName());
    }
    static private native void RegisterClass(String derivedClass, String baseClass);

    public JNIObject() {
        initNative();
    }

    private void dispose() {
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
