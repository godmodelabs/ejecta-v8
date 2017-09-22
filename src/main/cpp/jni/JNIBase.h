//
// Created by Martin Kleinhans on 22.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIBASE_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIBASE_H

#import <string>
#include <jni.h>

class JNIClassInfo;
class JNIWrapper;
template<class ScopeClass, class BaseClass> class JNIScope;

class JNIBase {
    template<class ScopeClass, class BaseClass> friend class JNIScope;
public:
    JNIBase(JNIClassInfo *info);
    virtual ~JNIBase();

    const std::string& getCanonicalName() const;
    const jclass getJClass() const;
    const std::string getSignature() const;

    bool isPersistent() const;

protected:
    JNIClassInfo *_jniClassInfo;
};


#endif //ANDROID_TRADINGLIB_SAMPLE_JNIBASE_H
