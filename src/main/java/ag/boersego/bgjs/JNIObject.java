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
    native void initNative(String canonicalName);

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

    /**
     * default constructor; will always initialize the jni side of the object automatically
     */
    public JNIObject() {
        initNative(getClass().getCanonicalName());
        initAutomaticDisposure();
    }

    /**
     * WARNING: using this constructor with (true) will skip native initialization!
     *
     * it is solely intended for speeding up initialization of special derived classes that also do initialization in jni code
     * derived classes need to:
     * - call JNIWrapper::initializeNativeObject from jni
     * - call initAutomaticDisposure() immediately after the jni call
     */
    JNIObject(boolean skipInitNative) {
        if(skipInitNative) return;
        initNative(getClass().getCanonicalName());
        initAutomaticDisposure();
    }

    void initAutomaticDisposure() {
        reference = new JNIObjectReference(this, nativeHandle, referenceQueue);
    }

    /**
     * the jni side of some objects can be manually disposed before they are gc'd using this method
     * only possible for temporary wrapper objects, e.g. JNIV8GenericObject
     */
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

    /**
     * checks if the object has been manually disposed before
     */
    public boolean isDisposed() {
        return nativeHandle == 0;
    }
}
