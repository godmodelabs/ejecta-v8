//
// Created by Martin Kleinhans on 20.04.17.
//

#include "stdlib.h"
#include <jni.h>
#include <cstdlib>

#include "JNIWrapper.h"

void JNIWrapper::init(JavaVM *vm) {
    _jniVM = vm;

    _registerObject(JNIObjectType::kAbstract,
                    JNIWrapper::getCanonicalName<JNIObject>(), "", initialize<JNIObject>, nullptr);
}

bool JNIWrapper::isInitialized() {
    return _jniVM != nullptr;
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
        if(info->type == JNIObjectType::kPersistent) {
            info->registerField("nativeHandle", "J");
        }

        // check if a default constructor without arguments is available
        jmethodID constructor = info->getMethodID("<init>","()V",false);
        if(constructor) {
            info->methodMap["<init>"] = {false, "<init>", "()V", constructor};
        }

        // call static initializer
        info->initializer(info, true);
    }
}

bool JNIWrapper::isObjectInstanceOf(JNIObject *obj, const std::string &canonicalName) {
    auto it = _objmap.find(canonicalName);
    if(it == _objmap.end()) return false;
    JNIClassInfo *info = it->second;
    JNIClassInfo *info2 = obj->_jniClassInfo;
    while(info2 != info) {
        info2 = info2->baseClassInfo;
        if(!info2) {
            return false;
        }
    }
    return true;
}

void JNIWrapper::_registerObject(JNIObjectType type,
                                 const std::string &canonicalName, const std::string &baseCanonicalName,
                                 ObjectInitializer i, ObjectConstructor c) {
    // canonicalName may be already registered
    // (e.g. when called from JNI_OnLoad; when using multiple linked libraries it is called once for each library)
    if(_objmap.find(canonicalName) != _objmap.end()) {
        return;
    }

    // baseCanonicalName must either be empty (not a derived object), or already registered
    std::map<std::string, JNIClassInfo*>::iterator it;

    JNIClassInfo *baseInfo = nullptr;
    if(!baseCanonicalName.empty()) {
        it = _objmap.find(baseCanonicalName);
        if (it == _objmap.end()) {
            return;
        }
        baseInfo = it->second;

        // pure java objects can only directly extend JNIObject if they are abstract!
        if(JNIWrapper::getCanonicalName<JNIObject>() == baseCanonicalName && !i && type != JNIObjectType::kAbstract) {
            return;
        }
    } else if(canonicalName != JNIWrapper::getCanonicalName<JNIObject>()) {
        // an empty base class is only allowed here for internally registering JNIObject itself
        return;
    }

    if(baseInfo) {
        if (type == JNIObjectType::kTemporary) {
            // temporary classes can only extend other temporary classes (or JNIObject directly)
            JNIClassInfo *baseInfo2 = baseInfo;
            do {
                if (baseInfo2->type != JNIObjectType::kTemporary && baseInfo2->baseClassInfo) {
                    return;
                }
                baseInfo2 = baseInfo->baseClassInfo;
            } while (baseInfo2);
        } else if (baseInfo->type == JNIObjectType::kTemporary &&
                   baseInfo->type != type) {
            // temporary classes can only be extended by other temporary classes!
            return;
        }
    }

    JNIEnv* env = JNIWrapper::getEnvironment();

    // check if class exists
    jclass clazz = env->FindClass(canonicalName.c_str());

    // class has to exist...
    assert(clazz != NULL);

    JNIClassInfo *info = new JNIClassInfo(type, clazz, canonicalName, i, c, baseInfo);
    _objmap[canonicalName] = info;

    // if it is a persistent class, and has no base class, register the field for storing the native class
    // => this is only for JNIObject! ALL other objects have a baseclass
    if(!baseInfo) {
        info->registerField("nativeHandle", "J");
    }

    // check if a default constructor without arguments is available
    jmethodID constructor = info->getMethodID("<init>","()V",false);
    if(constructor) {
        info->methodMap["<init>"] = {false, "<init>", "()V", constructor};
    }

    // call static initializer (for java derived classes it is empty)
    if(info->initializer) {
        info->initializer(info, false);
        // register methods
        if (info->methods.size()) {
            env->RegisterNatives(clazz, &info->methods[0], info->methods.size());

            // free pointers that were allocated in registerNativeMethod
            for (auto &entry : info->methods) {
                free((void *) entry.name);
                free((void *) entry.signature);
            }
            info->methods.clear();
        }
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

void JNIWrapper::initializeNativeObject(jobject object) {
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

    // if nothing was found, the class was not registered
    assert(it != _objmap.end());

    JNIClassInfo *info = it->second;
    info->constructor(object, info);
}

jobject JNIWrapper::_createObject(const std::string& canonicalName, const char* constructorAlias, va_list constructorArgs) {
    auto it = _objmap.find(canonicalName);
    if (it == _objmap.end()) {
        return nullptr;
    } else {
        JNIClassInfo *info = it->second;

        if(info->type == JNIObjectType::kAbstract) return nullptr;

        JNIEnv *env = JNIWrapper::getEnvironment();

        jclass clazz = env->FindClass(canonicalName.c_str());
        jmethodID constructor;
        if (!constructorAlias) {
            constructor = info->methodMap.at("<init>").id;
            return env->NewObject(clazz, constructor);
        } else {
            constructor = info->methodMap.at(constructorAlias).id;
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