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
 * use JNIWrapper::createObject<Type>(env) inestead
 */
class JNIObject : public JNIClass {
public:
    JNIObject(jobject obj, JNIClassInfo *info);
    virtual ~JNIObject();

    const jobject getJObject() const;

    /**
     * calls the specified static java object method
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
    jobject _jniObject;
};


#endif //__OBJECT_H
