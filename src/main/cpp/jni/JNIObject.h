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
    void callVoidMethod(const char* name, ...);
    jlong callLongMethod(const char* name, ...);
    jboolean callBooleanMethod(const char* name, ...);
    jbyte callByteMethod(const char* name, ...);
    jchar callCharMethod(const char* name, ...);
    jdouble callDoubleMethod(const char* name, ...);
    jfloat callFloatMethod(const char* name, ...);
    jint callIntMethod(const char* name, ...);
    jshort callShortMethod(const char* name, ...);
    jobject callObjectMethod(const char* name, ...);

    /**
     * retrieves the value of the specified static java object field
     */
    jlong getLongField(const std::string& fieldName);
    jboolean getBooleanField(const std::string& fieldName);
    jbyte getByteField(const std::string& fieldName);
    jchar getCharField(const std::string& fieldName);
    jdouble getDoubleField(const std::string& fieldName);
    jfloat getFloatField(const std::string& fieldName);
    jint getIntField(const std::string& fieldName);
    jshort getShortField(const std::string& fieldName);
    jobject getObjectField(const std::string& fieldName);

    /**
     * sets the value of the specified static java object field
     */
    void setLongField(const std::string& fieldName, jlong value);
    void setBooleanField(const std::string& fieldName, jboolean value);
    void setByteField(const std::string& fieldName, jbyte value);
    void setCharField(const std::string& fieldName, jchar value);
    void setDoubleField(const std::string& fieldName, jdouble value);
    void setFloatField(const std::string& fieldName, jfloat value);
    void setIntField(const std::string& fieldName, jint value);
    void setShortField(const std::string& fieldName, jshort value);
    void setObjectField(const std::string& fieldName, jobject value);
protected:
    jobject _jniObject;
};


#endif //__OBJECT_H
