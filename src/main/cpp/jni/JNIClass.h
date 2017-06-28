//
// Created by Martin Kleinhans on 21.04.17.
//

#ifndef __JNICLASS_H
#define __JNICLASS_H

#import <string>
#include <jni.h>

class JNIClassInfo;

/**
 * Base class for all native classes associated with a java object
 * constructor should never be called manually
 * instead you should use
 * JNIWrapper::wrapClass<Type>() - to get the native wrapper for a java class (access to static fields/methods)
 * JNIWrapper::wrapObject<Type>(obj) - to get the native wrapper for a java object (access to static & instance fields/methods)
 * JNIWrapper::createObject<Type>() - to create a new instance of a Java+Native object
 */
class JNIClass {
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
    void callStaticVoidMethod(const char* name, ...);
    jlong callStaticLongMethod(const char* name, ...);
    jboolean callStaticBooleanMethod(const char* name, ...);
    jbyte callStaticByteMethod(const char* name, ...);
    jchar callStaticCharMethod(const char* name, ...);
    jdouble callStaticDoubleMethod(const char* name, ...);
    jfloat callStaticFloatMethod(const char* name, ...);
    jint callStaticIntMethod(const char* name, ...);
    jshort callStaticShortMethod(const char* name, ...);
    jobject callStaticObjectMethod(const char* name, ...);

    /**
     * retrieves the value of the specified static java object field
     */
    jlong getStaticLongField(const std::string& fieldName);
    jboolean getStaticBooleanField(const std::string& fieldName);
    jbyte getStaticByteField(const std::string& fieldName);
    jchar getStaticCharField(const std::string& fieldName);
    jdouble getStaticDoubleField(const std::string& fieldName);
    jfloat getStaticFloatField(const std::string& fieldName);
    jint getStaticIntField(const std::string& fieldName);
    jshort getStaticShortField(const std::string& fieldName);
    jobject getStaticObjectField(const std::string& fieldName);

    /**
     * sets the value of the specified static java object field
     */
    void setStaticLongField(const std::string& fieldName, jlong value);
    void setStaticBooleanField(const std::string& fieldName, jboolean value);
    void setStaticByteField(const std::string& fieldName, jbyte value);
    void setStaticCharField(const std::string& fieldName, jchar value);
    void setStaticDoubleField(const std::string& fieldName, jdouble value);
    void setStaticFloatField(const std::string& fieldName, jfloat value);
    void setStaticIntField(const std::string& fieldName, jint value);
    void setStaticShortField(const std::string& fieldName, jshort value);
    void setStaticObjectField(const std::string& fieldName, jobject value);

protected:
    JNIClassInfo *_jniClassInfo;
};

#endif //__JNICLASS_H
