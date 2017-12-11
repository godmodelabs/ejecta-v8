//
// Created by Martin Kleinhans on 18.04.17.
//

#include "jni_assert.h"

#include "JNIObject.h"
#include "JNIWrapper.h"

BGJS_JNI_LINK(JNIObject, "ag/boersego/bgjs/JNIObject");

JNIObject::JNIObject(jobject obj, JNIClassInfo *info) : JNIBase(info) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    if(info->type == JNIObjectType::kPersistent) {
        // persistent objects are owned by the java side: they are destroyed once the java side is garbage collected
        // => as long as there are no references to the c object, the java reference is weak.
        _jniObjectWeak = env->NewWeakGlobalRef(obj);
        _jniObject = nullptr;
    } else {
        // non-persistent objects are owned by the c side. they do not exist in this form on the java side
        // => as long as the object exists, the java reference should always be strong
        // theoretically we could use the same logic here, and make it non-weak on demand, but it simply is not necessary
        _jniObject = env->NewGlobalRef(obj);
        _jniObjectWeak = nullptr;
    }
    _atomicJniObjectRefCount = 0;

    // store pointer to native instance in "nativeHandle" field
    // actually type will never be kAbstract here, because JNIClassInfo will be provided for the subclass!
    // however, this gets rid of the "never used" warning for the constant, and works just fine as well.
    if(info->type != JNIObjectType::kTemporary) {
        JNIClassInfo *info = _jniClassInfo;
        while(info && info->baseClassInfo) info = info->baseClassInfo;
        auto it = info->fieldMap.at("nativeHandle");
        env->SetLongField(getJObject(), it.id, reinterpret_cast<jlong>(this));
    }
}

JNIObject::~JNIObject() {
    JNI_ASSERT(_atomicJniObjectRefCount==0, "JNIObject was deleted while retaining java object");
    if(_jniObject) {
        // this should/can never happen for persistent objects
        // if there is a strong ref to the JObject, then the native object must not be deleted!
        // it can however happen for non-persistent objects!
        JNIWrapper::getEnvironment()->DeleteGlobalRef(_jniObject);
    } else if(_jniObjectWeak) {
        JNIWrapper::getEnvironment()->DeleteWeakGlobalRef(_jniObjectWeak);
    }
    _jniObjectWeak = _jniObject = nullptr;
}

void JNIObject::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerNativeMethod("RegisterClass", "(Ljava/lang/String;Ljava/lang/String;)V", (void*)JNIObject::jniRegisterClass);
}

const jobject JNIObject::getJObject() {
    // persistents always have a weak reference to the jobject
    // the strong reference only exists if they are retained by native code
    // to avoid having to synchronize read/write, we simply ALWAYS use the weak ref if we can
    if(isPersistent()) {
        return _jniObjectWeak;
    }
    // non persistents always have a strong reference as long as they exist
    return _jniObject;
}

std::shared_ptr<JNIObject> JNIObject::getSharedPtr() {
    if(isPersistent()) {
        retainJObject();
    }
    return std::shared_ptr<JNIObject>(this, [=](JNIObject *cls) {
        if (!cls->isPersistent()) {
            // non persistent objects need to be deleted once they are not referenced anymore
            // wrapping an object again will return a new native instance!
            delete cls;
        } else {
            // ownership of persistent objects is held by the java side
            // object is only deleted if the java object is garbage collected
            // when there are no more references from C we need to make the reference to the java object weak again!
            cls->releaseJObject();
        }
    });
}

void JNIObject::retainJObject() {
    // optimized for the case where an object is likely to be retained for longer times
    // => just update the atomic counter, no locking
    // if retain counter never rises above one it could be faster to always use a mutex and non-atomic counters

    JNI_ASSERT(isPersistent(), "Attempt to retain non-persistent native object");
    if(++_atomicJniObjectRefCount == 1) {
        std::lock_guard<std::mutex> guard(_mutex);

        // while this thread A was waiting for the mutex, the counter might have changed
        // or another thread B could possible even already have created the object
        // e.g. (0->1(A)->0->1(B), executed as 1->0->1 (B)(A)
        // => check state here, guarded by mutex, and possibly do nothing
        if(_atomicJniObjectRefCount==0 || _jniObject) return;

        JNIEnv *env = JNIWrapper::getEnvironment();
        _jniObject = env->NewGlobalRef(_jniObjectWeak);
    }
}

void JNIObject::releaseJObject() {
    // optimized for the case where an object is likely to be retained for longer times
    // => just update the atomic counter, no locking
    // if retain counter never rises above one it could be faster to always use a mutex and non-atomic counters

    JNI_ASSERT(isPersistent(), "Attempt to release non-persistent native object");
    if(--_atomicJniObjectRefCount == 0) {
        std::lock_guard<std::mutex> guard(_mutex);

        // while this thread A was waiting for the mutex, the counter might have changed
        // or another thread B could possible even already have released the object
        // e.g. (1->0(A)->1->0(B), executed as 1->0->1 (B)(A)
        // => check state here, guarded by mutex, and possibly do nothing
        if(_atomicJniObjectRefCount > 0 || !_jniObject) return;

        JNIEnv *env = JNIWrapper::getEnvironment();

        // this method is executed automatically if a shared_ptr falls of the stack
        // if an exception was thrown after the shared_ptr was created, we can
        // not call any JNI functions without clearing the exception first and then rethrowing it
        // In most cases this could be done in the method itself, but it is tedious and likely to be forgotten
        // also, there are cases
        // e.g. when an exception is thrown in a method that is not immediately called from Java/JNI, but the shared_ptr is used in a method further upp the call stack
        // when it is very hard or even impossible to do this
        // => handle this here!
        jthrowable exc = nullptr;
        if(env->ExceptionCheck()) {
            exc = env->ExceptionOccurred();
            env->ExceptionClear();
        }

        env->DeleteGlobalRef(_jniObject);
        _jniObject = nullptr;

        if(exc) env->Throw(exc);
    }
}

//--------------------------------------------------------------------------------------------------
// Methods
//--------------------------------------------------------------------------------------------------
#define METHOD(TypeName, JNITypeName) \
JNITypeName JNIObject::callJava##TypeName##Method(const char* name, ...) {\
    JNIEnv* env = JNIWrapper::getEnvironment();\
    auto it = _jniClassInfo->methodMap.find(name);\
    JNI_ASSERTF(it != _jniClassInfo->methodMap.end(), "Attempt to call unregistered method '%s'", name);\
    JNI_ASSERTF(!it->second.isStatic, "Attempt to call non-static method '%s' as static", name);\
    va_list args;\
    JNITypeName res;\
    va_start(args, name);\
    res = env->Call##TypeName##MethodV(getJObject(), it->second.id, args);\
    va_end(args);\
    return res;\
}

void JNIObject::callJavaVoidMethod(const char* name, ...) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    auto it = _jniClassInfo->methodMap.find(name);
    JNI_ASSERTF(it != _jniClassInfo->methodMap.end(), "Attempt to call unregistered method '%s'", name);
    JNI_ASSERTF(!it->second.isStatic, "Attempt to call non-static method '%s' as static", name);
    va_list args;
    va_start(args, name);\
    env->CallVoidMethodV(getJObject(), it->second.id, args);
    va_end(args);
}

METHOD(Long, jlong)
METHOD(Boolean, jboolean)
METHOD(Byte, jbyte)
METHOD(Char, jchar)
METHOD(Double, jdouble)
METHOD(Float, jfloat)
METHOD(Int, jint)
METHOD(Short, jshort)
METHOD(Object, jobject)

void JNIObject::jniRegisterClass(JNIEnv *env, jobject obj, jstring derivedClass, jstring baseClass) {
    std::string strDerivedClass = JNIWrapper::jstring2string(derivedClass);
    std::replace(strDerivedClass.begin(), strDerivedClass.end(), '.', '/');

    std::string strBaseClass = JNIWrapper::jstring2string(baseClass);
    std::replace(strBaseClass.begin(), strBaseClass.end(), '.', '/');

    JNIWrapper::registerJavaObject(strDerivedClass, strBaseClass);
}

//--------------------------------------------------------------------------------------------------
// Exports
//--------------------------------------------------------------------------------------------------
extern "C" {
    JNIEXPORT void JNICALL Java_ag_boersego_bgjs_JNIObject_initNative(JNIEnv *env, jobject obj, jstring canonicalName) {
        JNIWrapper::initializeNativeObject(obj, canonicalName);
    }

    JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_JNIObjectReference_disposeNative(JNIEnv *env, jobject obj, jlong nativeHandle) {
        JNIObject *jniObject = reinterpret_cast<JNIObject*>(nativeHandle);
        delete jniObject;
        return true;
    }
}

