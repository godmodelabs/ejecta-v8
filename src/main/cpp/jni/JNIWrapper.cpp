//
// Created by Martin Kleinhans on 20.04.17.
//

#include "stdlib.h"
#include <jni.h>
#include <cstdlib>

#include "JNIWrapper.h"

void JNIWrapper::init(JavaVM *vm) {
    _jniVM = vm;

    _registerObject(typeid(JNIObject).hash_code(), JNIObjectType::kAbstract,
                    JNIWrapper::getCanonicalName<JNIObject>(), "", initialize<JNIObject>, nullptr);
}

bool JNIWrapper::isInitialized() {
    return _jniVM != nullptr;
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

void JNIWrapper::_registerObject(size_t hashCode, JNIObjectType type,
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

        // pure java objects can not directly extend JNIObject
        if(JNIWrapper::getCanonicalName<JNIObject>() == baseCanonicalName && !i) {
            JNI_ASSERT(0, "Pure java objects must not directly extend JNIObject");
            return;
        }
    } else if(canonicalName != JNIWrapper::getCanonicalName<JNIObject>()) {
        // an empty base class is only allowed here for internally registering JNIObject itself
        JNI_ASSERT(0, "Attempt to register an object without super class");
        return;
    }

    if(baseInfo) {
        if (type == JNIObjectType::kTemporary) {
            // temporary classes can only extend other temporary classes (or JNIObject directly)
            JNIClassInfo *baseInfo2 = baseInfo;
            do {
                if (baseInfo2->type != JNIObjectType::kTemporary && baseInfo2->baseClassInfo) {
                    JNI_ASSERT(0, "Temporary classes can only extend JNIObject or other temporary classes");
                    return;
                }
                baseInfo2 = baseInfo->baseClassInfo;
            } while (baseInfo2);
        } else if (baseInfo->type == JNIObjectType::kTemporary &&
                   baseInfo->type != type) {
            // temporary classes can only be extended by other temporary classes!
            JNI_ASSERT(0, "Temporary classes can only be extended by other temporary classes");
            return;
        }
    }

    JNIEnv* env = JNIWrapper::getEnvironment();

    // check if class exists
    jclass clazz = env->FindClass(canonicalName.c_str());

    // class has to exist...
    JNI_ASSERTF(clazz != NULL, "Class '%s' not found", canonicalName.c_str());

    JNIClassInfo *info = new JNIClassInfo(hashCode, type, clazz, canonicalName, i, c, baseInfo);
    _objmap[canonicalName] = info;

    info->inherit();

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
        JNI_ASSERT(_jniVM, "JNI VM not available");

        int r = _jniVM->GetEnv((void**)&_jniEnv, JNI_VERSION_1_6);
        JNI_ASSERT(r == JNI_OK, "JNI Env not available"); (void)r;
    }
    int r = _jniVM->AttachCurrentThread(&_jniEnv, nullptr);
    JNI_ASSERT(r == JNI_OK, "Failed to attach thread to JVM"); (void)r;
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
    JNI_ASSERT(it != _objmap.end(), "Encountered unknown class during initialization");

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

/**
 * convert a jstring to a std::string
 */
std::string JNIWrapper::jstring2string(jstring string) {
    // string pointers can also be null but there is no real way to express that in a std::string
    if(_jniEnv->IsSameObject(string, NULL)) {
        return "";
    }

    JNI_ASSERT(_jniEnv, "JNI Environment not initialized");
    if(!_jniStringClass) {
        _jniStringClass = _jniEnv->FindClass("java/lang/String");
        _jniStringGetBytes = _jniEnv->GetMethodID(_jniStringClass, "getBytes",
                                                  "(Ljava/lang/String;)[B");
    }

    const jstring charsetName = _jniEnv->NewStringUTF("UTF-8");
    const jbyteArray stringJbytes = (jbyteArray) _jniEnv->CallObjectMethod(string, _jniStringGetBytes, charsetName);
    _jniEnv->DeleteLocalRef(charsetName);

    const jsize length = _jniEnv->GetArrayLength(stringJbytes);
    jbyte* pBytes = _jniEnv->GetByteArrayElements(stringJbytes, NULL);

    std::string ret = std::string((char *)pBytes, (unsigned long)length);

    _jniEnv->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);
    _jniEnv->DeleteLocalRef(stringJbytes);

    return ret;
}

/**
 * convert a std::string to a jstring
 */
jstring JNIWrapper::string2jstring(const std::string& string) {
    JNI_ASSERT(_jniEnv, "JNI Environment not initialized");
    return _jniEnv->NewStringUTF(string.c_str());
}

std::map<std::string, JNIClassInfo*> JNIWrapper::_objmap;
jmethodID JNIWrapper::_jniCanonicalNameMethodID = nullptr;
JavaVM* JNIWrapper::_jniVM = nullptr;
JNIEnv* JNIWrapper::_jniEnv = nullptr;
jclass JNIWrapper::_jniStringClass = nullptr;
jmethodID JNIWrapper::_jniStringGetBytes = nullptr;