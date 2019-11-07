package ag.boersego.bgjs;

/**
 * Created by martin on 05.10.17.
 */

import android.util.Log;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;

/**
 * This class is used for holding the reference to the native pointers present in JNIObjects.
 * This is required as phantom references cannot access the original objects for this value.
 * The phantom references will be stored in a double linked list to avoid the reference itself gets GCed. When the
 * referent get GCed, the reference will be added to the ReferenceQueue. Loop in the daemon thread will retrieve the
 * phantom reference from the ReferenceQueue then dealloc the native object and remove the reference from the double linked
 * list.
 * Linked-list approach based on realm-java (https://github.com/realm/realm-java)
 */
final class JNIObjectReference extends PhantomReference<JNIObject> {
    // Linked list to keep the reference of the PhantomReference
    private static class ReferencePool {
        JNIObjectReference head;
        int length = 0;

        synchronized void add(JNIObjectReference ref) {
            ref.prev = null;
            ref.next = head;
            if (head != null) {
                head.prev = ref;
            }
            head = ref;
            length++;
        }

        synchronized void remove(JNIObjectReference ref) {
            JNIObjectReference next = ref.next;
            JNIObjectReference prev = ref.prev;
            ref.next = null;
            ref.prev = null;
            if (prev != null) {
                prev.next = next;
            } else {
                head = next;
            }
            if (next != null) {
                next.prev = prev;
            }
            length--;
        }
    }

    public JNIObjectReference next, prev;
    private long nativeHandle;

    private static ReferencePool referencePool = new ReferencePool();

    protected static native boolean disposeNative(long nativeHandle);

    public JNIObjectReference(JNIObject obj, long nativeHandle, ReferenceQueue<JNIObject> referenceQueue) {
        super(obj, referenceQueue);
        this.nativeHandle = nativeHandle;
        referencePool.add(this);
    }

    public boolean cleanup() {
        if(!disposeNative(nativeHandle)) return false;

        referencePool.remove(this);
        clear();

        if(referencePool.length == 0) {
            Log.d("JNIObject", "reference pool was completely drained!");
        }
        return true;
    }
}
