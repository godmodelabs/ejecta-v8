//
// Created by Martin Kleinhans on 18.04.17.
//

#include "jni_assert.h"

#include "JNIObject.h"
#include "JNIWrapper.h"

BGJS_JNIOBJECT_LINK(JNIObject, "ag/boersego/bgjs/JNIObject");

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
    _jniObjectRefCount = 0;

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

const jobject JNIObject::getJObject() const {
    JNIEnv *env = JNIWrapper::getEnvironment();
    // we always need to convert the object reference to local (both weak and global)
    // because the global reference could be deleted when the last shared ptr to this instance is released
    // e.g. "return instance->getJObject();" would then yield an invalid global ref!
    if(_jniObject) {
        return env->NewLocalRef(_jniObject);
    }
    return env->NewLocalRef(_jniObjectWeak);
}

std::shared_ptr<JNIObject> JNIObject::getSharedPtr() {
    // a private internal weak ptr is used as a basis for "synchronizing" creation of shared_ptr
    // all shared_ptr created from the weak_ptr use the same counter and deallocator!
    // theoretically we could use multiple separate instance with the retain/release logic, but this way we stay more flexible
    if(_weakPtr.use_count()) {
        return std::shared_ptr<JNIObject>(_weakPtr);
    }
    if(isPersistent()) {
        // we are handing out a reference to the native object here
        // as long as that reference is alive, the java object must not be gargabe collected
        retainJObject();
    }
    auto ptr = std::shared_ptr<JNIObject>(this,[=](JNIObject* cls) {
        if(!cls->isPersistent()) {
            // non persistent objects need to be deleted once they are not referenced anymore
            // wrapping an object again will return a new native instance!
            delete cls;
        } else {
            // ownership of persistent objects is held by the java side
            // object is only deleted if the java object is garbage collected
            // when there are no more references from C we make the reference to the java object weak again!
            cls->releaseJObject();
        }
    });
    _weakPtr = ptr;
    return ptr;
}

void JNIObject::retainJObject() {
    JNI_ASSERT(isPersistent(), "Attempt to retain non-persistent native object");
    if(_jniObjectRefCount==0) {
        JNIEnv *env = JNIWrapper::getEnvironment();
        _jniObject = env->NewGlobalRef(_jniObjectWeak);
        env->DeleteWeakGlobalRef(_jniObjectWeak);
        _jniObjectWeak = nullptr;
    }
    _jniObjectRefCount++;
}

void JNIObject::releaseJObject() {
    JNI_ASSERT(isPersistent(), "Attempt to release non-persistent native object");
    if(_jniObjectRefCount==1) {
        JNIEnv *env = JNIWrapper::getEnvironment();
        _jniObjectWeak = env->NewWeakGlobalRef(_jniObject);
        env->DeleteGlobalRef(_jniObject);
        _jniObject = nullptr;
    }
    _jniObjectRefCount--;
}

bool JNIObject::retainsJObject() const {
    return _jniObjectRefCount > 0;
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
        if(jniObject->retainsJObject()) {
            return false;
        }
        delete jniObject;
        return true;
    }
}

