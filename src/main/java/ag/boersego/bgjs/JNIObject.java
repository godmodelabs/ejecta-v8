package ag.boersego.bgjs;

import java.lang.ref.ReferenceQueue;

/**
 * Created by martin on 18.04.17.
 */

abstract public class JNIObject {
    private static ReferenceQueue<JNIObject> referenceQueue;
    private static final Thread finalizingThread;

    private JNIObjectReference reference;
    private long nativeHandle;
    private native void initNative();

    private static native void initBinding();
    static {
        referenceQueue = new ReferenceQueue<>();

        finalizingThread = new Thread(new JNIObjectFinalizerRunnable(referenceQueue));
        finalizingThread.setName("EjectaV8FinalizingDaemon");
        finalizingThread.start();
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
        reference = new JNIObjectReference(this, nativeHandle, referenceQueue);
    }

    protected void dispose() throws RuntimeException {
        if(nativeHandle == 0) {
            throw new RuntimeException("Object must not be disposed twice");
        }
        if(!reference.cleanup()) {
            throw new RuntimeException("Object is strongly referenced from native side and must not be disposed manually");
        }
        nativeHandle = 0;
        reference = null;
    }

    public boolean isDisposed() {
        return nativeHandle == 0;
    }
}
