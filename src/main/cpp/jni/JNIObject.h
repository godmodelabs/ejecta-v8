//
// Created by Martin Kleinhans on 18.04.17.
//

#ifndef __JNIOBJECT_H
#define __JNIOBJECT_H

#import <string>
#include <jni.h>
#include <mutex>
#include "JNIBase.h"

class JNIObjectRef;

/**
 * Base class for all native classes associated with a java object
 * constructor should never be called manually; if you want to create a new instance
 * use JNIWrapper::createObject<Type>(env) instead
 */
class JNIObject : public JNIBase {
    friend class JNIWrapper;
    friend class JNIObjectRef;
public:
    JNIObject(jobject obj, JNIClassInfo *info);
    virtual ~JNIObject();

    /**
     * returns the shared_ptr for this instance
     * usage of direct pointers to this object should be avoided at all cost
     */
    std::shared_ptr<JNIObject> getSharedPtr();

    /**
     * returns the referenced java object
     */
    const jobject getJObject();

    /**
     * calls the specified java object method
     */
    void callJavaVoidMethod(const char* name, ...);
    jlong callJavaLongMethod(const char* name, ...);
    jboolean callJavaBooleanMethod(const char* name, ...);
    jbyte callJavaByteMethod(const char* name, ...);
    jchar callJavaCharMethod(const char* name, ...);
    jdouble callJavaDoubleMethod(const char* name, ...);
    jfloat callJavaFloatMethod(const char* name, ...);
    jint callJavaIntMethod(const char* name, ...);
    jshort callJavaShortMethod(const char* name, ...);
    jobject callJavaObjectMethod(const char* name, ...);

protected:
    void retainJObject();
    void releaseJObject();

private:
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void jniRegisterClass(JNIEnv *env, jobject obj, jstring derivedClass, jstring baseClass);

    std::mutex _mutex;
    //pthread_mutex_t _mutex;
    jobject _jniObject;
    jweak _jniObjectWeak;
    std::atomic<uint8_t> _atomicJniObjectRefCount;
    std::weak_ptr<JNIObject> _weakPtr;
};

BGJS_JNI_LINK_DEF(JNIObject)

//template<typename T>
//class JNIObjectRef {
//private:
//    struct Counter {
//        std::atomic<uint8_t> num;
//    };
//    T *_obj;
//    Counter *_cnt;
//    bool _retaining;
//public:
//    JNIObjectRef(const JNIObjectRef &ref)
//    {
//        _retaining = true;
//        if(ref._retaining) {
//            _cnt = ref._cnt;
//        } else {
//            _cnt = new Counter();
//            _cnt->num = 0;
//            if (_obj->isPersistent()) {
//                _obj->retainJObject();
//            }
//        }
//        _obj = ref._obj;
//        _cnt->num++;
//    }
//    JNIObjectRef(JNIObjectRef &ref) : JNIObjectRef((const JNIObjectRef&)ref) {}
//
//    JNIObjectRef(T *obj) {
//        _cnt = new Counter();
//        _cnt->num = 0;
//        _obj = obj;
//        _retaining = !obj->isPersistent();
//    }
//    ~JNIObjectRef() {
//        uint8_t refs = --_cnt->num;
//        if(refs > 0) return;
//
//        delete _cnt;
//
//        if(_retaining) {
//            if (_obj->isPersistent()) {
//                _obj->releaseJObject();
//            } else {
//                delete _obj;
//            }
//        }
//    }
//    T& operator*() const {
//        return &_obj;
//    }
//    T* operator->() const {
//        return _obj;
//    }
//    T* get() const {
//        return _obj;
//    }
//};

#endif //__OBJECT_H
