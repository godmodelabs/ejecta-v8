//
// Created by Martin Kleinhans on 18.04.17.
//

#ifndef __JNIOBJECT_H
#define __JNIOBJECT_H

#import <string>
#include <jni.h>
#include "JNIBase.h"

/**
 * Base class for all native classes associated with a java object
 * constructor should never be called manually; if you want to create a new instance
 * use JNIWrapper::createObject<Type>(env) instead
 */
class JNIObject : public JNIBase {
    friend class JNIWrapper;
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
    const jobject getJObject() const;

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

    jobject _jniObject;
    jweak _jniObjectWeak;
    uint8_t _jniObjectRefCount;
    std::weak_ptr<JNIObject> _weakPtr;
};


#endif //__OBJECT_H
