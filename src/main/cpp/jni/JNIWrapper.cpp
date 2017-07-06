//
// Created by Martin Kleinhans on 20.04.17.
//

#include <assert.h>
#include <jni.h>
#include <cstdlib>

#include "JNIWrapper.h"

void JNIWrapper::init(JavaVM *vm) {
    _jniVM = vm;
}

void JNIWrapper::reloadBindings() {
    static bool _firstCall = true;
    if(_firstCall) {
        _firstCall = false;
        return;
    }
    JNIEnv* env = JNIWrapper::getEnvironment();
    JNIClassInfo *info;

    _jniCanonicalNameMethodID = nullptr;
    
    for(auto &it : _objmap) {
        info = it.second;

        // empty cache
        info->methodMap.clear();
        info->fieldMap.clear();

        // update class ref
        env->DeleteGlobalRef(info->jniClassRef);
        jclass clazz = env->FindClass(info->canonicalName.c_str());
        info->jniClassRef = (jclass)env->NewGlobalRef(clazz);

        // if it is a persistent class, update the  field for storing the native handle
        if(info->persistent) {
            info->registerField("nativeHandle", "J");
        }

        // check if a default constructor without arguments is available
        jmethodID constructor = info->getMethodID("<init>","()V",false);
        if(constructor) {
            info->methodMap["<init>"] = constructor;
        }

        // call static initializer
        info->initializer(info, true);
    }
}

void JNIWrapper::_registerObject(bool persistent, const std::string &canonicalName, ObjectInitializer i,
                                 ObjectConstructor c) {
    if(_objmap.find(canonicalName) != _objmap.end()) {
        return;
    }

    JNIClassInfo *info = new JNIClassInfo(persistent, canonicalName, i, c);
    _objmap[canonicalName] = info;

    if(!_jniVM) return;

    JNIEnv* env = JNIWrapper::getEnvironment();

    // cache class
    jclass clazz = env->FindClass(canonicalName.c_str());
    info->jniClassRef = (jclass)env->NewGlobalRef(clazz);

    // if it is a persistent class, register the field for storing the native class
    if(persistent) {
        info->registerField("nativeHandle", "J");
    }

    // check if a default constructor without arguments is available
    jmethodID constructor = info->getMethodID("<init>","()V",false);
    if(constructor) {
        info->methodMap["<init>"] = constructor;
    }

    // call static initializer
    info->initializer(info, false);

    // register methods
    if(info->methods.size()) {
        env->RegisterNatives(clazz, &info->methods[0], info->methods.size());
        // free pointers that were allocated in registerNativeMethod
        for(auto &entry : info->methods) {
            free((void*)entry.name);
            free((void*)entry.signature);
        }
        info->methods.clear();
    }
}

JNIEnv* JNIWrapper::getEnvironment() {
    if(!_jniEnv) {
        assert(_jniVM);

        int r = _jniVM->GetEnv((void**)&_jniEnv, JNI_VERSION_1_6);
        assert(r == JNI_OK); (void)r;
    }
    int r = _jniVM->AttachCurrentThread(&_jniEnv, nullptr);
    assert(r == JNI_OK); (void)r;
    return _jniEnv;
}

JNIObject* JNIWrapper::initializeNativeObject(jobject object) {
    JNIEnv* env = JNIWrapper::getEnvironment();
    std::string canonicalName;

    // first determine canonical name of the class (required for mapping to native class)
    jclass jniClass = env->GetObjectClass(object);

    // we cache the getCanonicalName method locally to speed things up a little
    // theoretically we would have to make the Class class ref a global ref, but it will probably never
    // be unloaded anyways??!
    if(!_jniCanonicalNameMethodID) {
        jclass jniClassClass = env->GetObjectClass(jniClass);
        _jniCanonicalNameMethodID = env->GetMethodID(jniClassClass, "getCanonicalName", "()Ljava/lang/String;");
    }

    jstring className = (jstring) env->CallObjectMethod(jniClass, _jniCanonicalNameMethodID);

    // @TODO: method for converting jstring to std::string
    const char* szClassName = env->GetStringUTFChars(className, NULL);
    canonicalName = szClassName;
    std::replace(canonicalName.begin(), canonicalName.end(), '.', '/');

    env->ReleaseStringUTFChars(className, szClassName);

    // now retrieve registered native class
    auto it = _objmap.find(canonicalName);
    if (it == _objmap.end()){
        return nullptr;
    } else {
        JNIClassInfo *info = it->second;
        return info->constructor(object, info);
    }
}

jobject JNIWrapper::_createObject(const std::string& canonicalName, const char* constructorAlias, va_list constructorArgs) {
    auto it = _objmap.find(canonicalName);
    if (it == _objmap.end()) {
        return nullptr;
    } else {
        JNIClassInfo *info = it->second;

        JNIEnv *env = JNIWrapper::getEnvironment();

        jclass clazz = env->FindClass(canonicalName.c_str());
        jmethodID constructor;
        if (!constructorAlias) {
            constructor = info->methodMap.at("<init>");
            return env->NewObject(clazz, constructor);
        } else {
            constructor = info->methodMap.at(constructorAlias);
            return env->NewObjectV(clazz, constructor, constructorArgs);
        }
    }
}

std::shared_ptr<JNIClass> JNIWrapper::_wrapClass(const std::string& canonicalName) {
    auto it = _objmap.find(canonicalName);
    if (it == _objmap.end()){
        return nullptr;
    } else {
        JNIClassInfo *info = it->second;
        return std::shared_ptr<JNIClass>(new JNIClass(info));
    }
}

std::map<std::string, JNIClassInfo*> JNIWrapper::_objmap;
jmethodID JNIWrapper::_jniCanonicalNameMethodID = nullptr;
JavaVM* JNIWrapper::_jniVM = nullptr;
JNIEnv* JNIWrapper::_jniEnv = nullptr;