//
// Created by Martin Kleinhans on 18.04.17.
//

#ifndef __JNIOBJECT_H
#define __JNIOBJECT_H

#import <string>
#include <jni.h>
#include "JNIClass.h"

/**
 * Base class for all native classes associated with a java object
 * constructor should never be called manually; if you want to create a new instance
 * use JNIWrapper::createObject<Type>(env) instead
 */
class JNIObject : public JNIClass {
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
     * @TODO: do we need a way to call implementation of a specific type via template methods? (for super.foo())
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

    /**
     * non-virtually calls the specified java object method on the specified class
     * e.g. a java equivalent would be calling "super.foo()"
     */
    void callJavaVoidMethod(const std::string& canonicalName, const char* name, ...);
    jlong callJavaLongMethod(const std::string& canonicalName, const char* name, ...);
    jboolean callJavaBooleanMethod(const std::string& canonicalName, const char* name, ...);
    jbyte callJavaByteMethod(const std::string& canonicalName, const char* name, ...);
    jchar callJavaCharMethod(const std::string& canonicalName, const char* name, ...);
    jdouble callJavaDoubleMethod(const std::string& canonicalName, const char* name, ...);
    jfloat callJavaFloatMethod(const std::string& canonicalName, const char* name, ...);
    jint callJavaIntMethod(const std::string& canonicalName, const char* name, ...);
    jshort callJavaShortMethod(const std::string& canonicalName, const char* name, ...);
    jobject callJavaObjectMethod(const std::string& canonicalName, const char* name, ...);

    /**
     * retrieves the value of the specified static java object field
     */
    jlong getJavaLongField(const std::string& fieldName);
    jboolean getJavaBooleanField(const std::string& fieldName);
    jbyte getJavaByteField(const std::string& fieldName);
    jchar getJavaCharField(const std::string& fieldName);
    jdouble getJavaDoubleField(const std::string& fieldName);
    jfloat getJavaFloatField(const std::string& fieldName);
    jint getJavaIntField(const std::string& fieldName);
    jshort getJavaShortField(const std::string& fieldName);
    jobject getJavaObjectField(const std::string& fieldName);

    /**
     * sets the value of the specified static java object field
     */
    void setJavaLongField(const std::string& fieldName, jlong value);
    void setJavaBooleanField(const std::string& fieldName, jboolean value);
    void setJavaByteField(const std::string& fieldName, jbyte value);
    void setJavaCharField(const std::string& fieldName, jchar value);
    void setJavaDoubleField(const std::string& fieldName, jdouble value);
    void setJavaFloatField(const std::string& fieldName, jfloat value);
    void setJavaIntField(const std::string& fieldName, jint value);
    void setJavaShortField(const std::string& fieldName, jshort value);
    void setJavaObjectField(const std::string& fieldName, jobject value);

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
