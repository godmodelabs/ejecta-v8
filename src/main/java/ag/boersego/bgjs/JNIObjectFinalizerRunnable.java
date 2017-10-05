package ag.boersego.bgjs;

import android.util.Log;

import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;

/**
 * Running in the FinalizingDaemon thread (managed by JNIV8Object) to free native objects.
 */
final class JNIObjectFinalizerRunnable implements Runnable {
    private ReferenceQueue<JNIObject> referenceQueue;

    JNIObjectFinalizerRunnable(ReferenceQueue<JNIObject> referenceQueue) {
        this.referenceQueue = referenceQueue;
    }

    @Override
    public void run() {
        while (true) {
            try {
                JNIObjectReference reference = (JNIObjectReference) referenceQueue.remove();
                if(!reference.cleanup()) {
                    Log.e("JNIObject", "GCd JNIObject failed to free native resources");
                }
            } catch (InterruptedException e) {
                // Restores the interrupted status.
                Thread.currentThread().interrupt();

                Log.e("JNIObject", "The FinalizerRunnable thread has been interrupted." +
                        " Native resources cannot be freed anymore");
                break;
            }
        }
    }
}
