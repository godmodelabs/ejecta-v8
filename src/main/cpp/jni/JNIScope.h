//
// Created by Martin Kleinhans on 19.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNISCOPE_H
#define ANDROID_TRADINGLIB_SAMPLE_JNISCOPE_H

#include "JNIObject.h"
#include "JNIClassInfo.h"

class JNIObject;

template<class ScopeClass, class BaseClass = JNIObject> class JNIScope : public BaseClass {
public:
    JNIScope(jobject obj, JNIClassInfo *info) : BaseClass(obj, info) {};
    void callJavaSuperVoidMethod(const char* name, ...) {
        size_t scope = typeid(ScopeClass).hash_code();
        JNIClassInfo *info = static_cast<BaseClass*>(this)->_jniClassInfo;
        while(info->hashCode != scope && info) info = info->baseClassInfo;
        assert(info);
    }

    void callJavaStaticVoidMethod() {

    }

    void getJavaStaticField() {}

    void setJavaStaticField() {}

    void getJavaField() {}

    void setJavaField() {}

    // => java only classes können nicht von extended werden! weder von native (macht keinenn sinn weil man dann an den scope ggf nicht drankommt..), noch von java (nicht nötig, diesen layer kann man "überspringen")
    // callmethod auf JNIObject, weil immer virtual!
    // JNIObject erbt nicht von mehr von JNIClass, sondern noch eine klasse dazwischen für static calls, oder implementiert in beiden?
    // JNIClass weiterhin für wrapClass gebraucht, verwendung etwas anders als bei JNIObject..

    // naming anders?
protected:
    virtual ~JNIScope() = default;
};


#endif //ANDROID_TRADINGLIB_SAMPLE_JNISCOPE_H
