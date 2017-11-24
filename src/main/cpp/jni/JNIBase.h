//
// Created by Martin Kleinhans on 22.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNIBASE_H
#define ANDROID_TRADINGLIB_SAMPLE_JNIBASE_H

#import <string>
#include <jni.h>
#include "jni_assert.h"

class JNIClassInfo;
class JNIWrapper;
template<class ScopeClass, class BaseClass> class JNIScope;

class JNIBase {
    template<class ScopeClass, class BaseClass> friend class JNIScope;
public:
    JNIBase(JNIClassInfo *info);
    virtual ~JNIBase();

    /**
     * returns the canonical name of the java class associated with the specified native object
     */
    template <typename T> static
    const std::string getCanonicalName() {
        JNI_ASSERT(0, "JNIBase::getCanonicalName called for unregistered class");
        return "<unknown>";
    }

    const std::string& getCanonicalName() const;
    const jclass getJClass() const;
    const std::string getSignature() const;

    bool isPersistent() const;

protected:
    JNIClassInfo *_jniClassInfo;
};

/**
 * macro to link native class with matching java counterpart
 * specify the native class, and the full canonical name of the associated java class
 * BGJS_JNI_LINK has to be implemented in the native classes .cpp file, and BGJS_JNI_LINK_DEF goes into the header to make the link known
 */
#define BGJS_JNI_LINK(type, canonicalName) template<> const std::string JNIBase::getCanonicalName<type>() { return canonicalName; };
#define BGJS_JNI_LINK_DEF(type) template<> const std::string JNIBase::getCanonicalName<type>();

#endif //ANDROID_TRADINGLIB_SAMPLE_JNIBASE_H
