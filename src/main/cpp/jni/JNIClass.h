//
// Created by Martin Kleinhans on 21.04.17.
//

#ifndef __JNICLASS_H
#define __JNICLASS_H

#import <string>
#include <jni.h>

class JNIClassInfo;
class JNIWrapper;

/**
 * Base class for all native classes associated with a java object
 * constructor should never be called manually
 * instead you should use
 * JNIWrapper::wrapClass<Type>() - to get the native wrapper for a java class (access to static fields/methods)
 * JNIWrapper::wrapObject<Type>(obj) - to get the native wrapper for a java object (access to static & instance fields/methods)
 * JNIWrapper::createObject<Type>() - to create a new instance of a Java+Native object
 */
class JNIClass {
    friend class JNIWrapper;
public:
    JNIClass(JNIClassInfo *info);
    virtual ~JNIClass();

    const std::string& getCanonicalName() const;
    const jclass getJClass() const;
    const std::string getSignature() const;

    bool isPersistent() const;

    /**
     * calls the specified static java object method
     */
    void callJavaStaticVoidMethod(const char* name, ...);
    jlong callJavaStaticLongMethod(const char* name, ...);
    jboolean callJavaStaticBooleanMethod(const char* name, ...);
    jbyte callJavaStaticByteMethod(const char* name, ...);
    jchar callJavaStaticCharMethod(const char* name, ...);
    jdouble callJavaStaticDoubleMethod(const char* name, ...);
    jfloat callJavaStaticFloatMethod(const char* name, ...);
    jint callJavaStaticIntMethod(const char* name, ...);
    jshort callJavaStaticShortMethod(const char* name, ...);
    jobject callJavaStaticObjectMethod(const char* name, ...);

    /**
     * retrieves the value of the specified static java object field
     */
    jlong getJavaStaticLongField(const std::string& fieldName);
    jboolean getJavaStaticBooleanField(const std::string& fieldName);
    jbyte getJavaStaticByteField(const std::string& fieldName);
    jchar getJavaStaticCharField(const std::string& fieldName);
    jdouble getJavaStaticDoubleField(const std::string& fieldName);
    jfloat getJavaStaticFloatField(const std::string& fieldName);
    jint getJavaStaticIntField(const std::string& fieldName);
    jshort getJavaStaticShortField(const std::string& fieldName);
    jobject getJavaStaticObjectField(const std::string& fieldName);

    /**
     * sets the value of the specified static java object field
     */
    void setJavaStaticLongField(const std::string& fieldName, jlong value);
    void setJavaStaticBooleanField(const std::string& fieldName, jboolean value);
    void setJavaStaticByteField(const std::string& fieldName, jbyte value);
    void setJavaStaticCharField(const std::string& fieldName, jchar value);
    void setJavaStaticDoubleField(const std::string& fieldName, jdouble value);
    void setJavaStaticFloatField(const std::string& fieldName, jfloat value);
    void setJavaStaticIntField(const std::string& fieldName, jint value);
    void setJavaStaticShortField(const std::string& fieldName, jshort value);
    void setJavaStaticObjectField(const std::string& fieldName, jobject value);

protected:
    JNIClassInfo *_jniClassInfo;
};

#endif //__JNICLASS_H
