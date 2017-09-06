//
// Created by Martin Kleinhans on 18.04.17.
//

#include <assert.h>

#include "JNIObject.h"
#include "JNIWrapper.h"

BGJS_JNIOBJECT_LINK(JNIObject, "ag/boersego/bgjs/JNIObject");

JNIObject::JNIObject(jobject obj, JNIClassInfo *info) : JNIClass(info) {

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
    if(info->type == JNIObjectType::kPersistent) {
        setJavaLongField("nativeHandle", reinterpret_cast<jlong>(this));
    }
}

JNIObject::~JNIObject() {
    // not really necessary, since this should only happen if the java object is deleted, but maybe to avoid errors?
    if(_jniClassInfo->type == JNIObjectType::kPersistent) {
        setJavaLongField("nativeHandle", reinterpret_cast<jlong>(nullptr));
    }
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

}

const jobject JNIObject::getJObject() const {
    if(_jniObject) {
        return _jniObject;
    }
    return _jniObjectWeak;
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
    assert(isPersistent());
    if(_jniObjectRefCount==0) {
        JNIEnv *env = JNIWrapper::getEnvironment();
        _jniObject = env->NewGlobalRef(_jniObjectWeak);
        env->DeleteWeakGlobalRef(_jniObjectWeak);
        _jniObjectWeak = nullptr;
    }
    _jniObjectRefCount++;
}

void JNIObject::releaseJObject() {
    assert(isPersistent());
    if(_jniObjectRefCount==1) {
        JNIEnv *env = JNIWrapper::getEnvironment();
        _jniObjectWeak = env->NewWeakGlobalRef(_jniObject);
        env->DeleteGlobalRef(_jniObject);
        _jniObject = nullptr;
    }
    _jniObjectRefCount--;
}

//--------------------------------------------------------------------------------------------------
// Fields Getter
//--------------------------------------------------------------------------------------------------
#define GETTER(TypeName, JNITypeName) \
JNITypeName JNIObject::getJava##TypeName##Field(const std::string& fieldName) {\
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = _jniClassInfo->fieldMap.find(fieldName);\
    assert(it != _jniClassInfo->fieldMap.end());\
    return env->Get##TypeName##Field(_jniObject, it->second); \
}

GETTER(Long, jlong)
GETTER(Boolean, jboolean)
GETTER(Byte, jbyte)
GETTER(Char, jchar)
GETTER(Double, jdouble)
GETTER(Float, jfloat)
GETTER(Int, jint)
GETTER(Short, jshort)
GETTER(Object, jobject)

//--------------------------------------------------------------------------------------------------
// Fields Setter
//--------------------------------------------------------------------------------------------------
#define SETTER(TypeName, JNITypeName) \
void JNIObject::setJava##TypeName##Field(const std::string& fieldName, JNITypeName value) {\
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = _jniClassInfo->fieldMap.find(fieldName);\
    assert(it != _jniClassInfo->fieldMap.end());\
    return env->Set##TypeName##Field(getJObject(), it->second, value); \
}

SETTER(Long, jlong)
SETTER(Boolean, jboolean)
SETTER(Byte, jbyte)
SETTER(Char, jchar)
SETTER(Double, jdouble)
SETTER(Float, jfloat)
SETTER(Int, jint)
SETTER(Short, jshort)
SETTER(Object, jobject)

//--------------------------------------------------------------------------------------------------
// Methods
//--------------------------------------------------------------------------------------------------
#define METHOD(TypeName, JNITypeName) \
JNITypeName JNIObject::callJava##TypeName##Method(const char* name, ...) {\
    JNIEnv* env = JNIWrapper::getEnvironment();\
    auto it = _jniClassInfo->methodMap.find(name);\
    assert(it != _jniClassInfo->methodMap.end());\
    va_list args;\
    JNITypeName res;\
    va_start(args, name);\
    res = env->Call##TypeName##MethodV(getJObject(), it->second, args);\
    va_end(args);\
    return res;\
}

void JNIObject::callJavaVoidMethod(const char* name, ...) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    auto it = _jniClassInfo->methodMap.find(name);
    assert(it != _jniClassInfo->methodMap.end());
    va_list args;
    va_start(args, name);\
    env->CallVoidMethodV(getJObject(), it->second, args);
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

//--------------------------------------------------------------------------------------------------
// Exports
//--------------------------------------------------------------------------------------------------
extern "C" {
    JNIEXPORT void JNICALL Java_ag_boersego_bgjs_JNIObject_initBinding(JNIEnv *env) {
        JNIWrapper::reloadBindings();
    }

    JNIEXPORT void JNICALL Java_ag_boersego_bgjs_JNIObject_initNative(JNIEnv *env, jobject obj) {
        JNIWrapper::initializeNativeObject(obj);
    }

    JNIEXPORT void JNICALL Java_ag_boersego_bgjs_JNIObject_disposeNative(JNIEnv *env, jobject obj, jlong nativeHandle) {
        delete reinterpret_cast<JNIObject*>(nativeHandle);
    }
}

