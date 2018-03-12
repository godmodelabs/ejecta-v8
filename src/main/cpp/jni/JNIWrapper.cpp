//
// Created by Martin Kleinhans on 20.04.17.
//

#include "stdlib.h"
#include <jni.h>
#include <cstdlib>
#include "JNIWrapper.h"

pthread_key_t JNIWrapper::_jniEnvKey = NULL;
pthread_key_t JNIWrapper::_jniDetachThreadKey = NULL;

void JNIWrapper::detachThread(void* _) {
    _jniVM->DetachCurrentThread();
}

void JNIWrapper::init(JavaVM *vm) {
    _jniVM = vm;

    pthread_key_create(&_jniEnvKey, NULL);
    pthread_key_create(&_jniDetachThreadKey, &detachThread);
    JNIEnv *env = JNIWrapper::getEnvironment();

    _jniStringClass = (jclass)env->NewGlobalRef(env->FindClass("java/lang/String"));
    _jniStringInit = env->GetMethodID(_jniStringClass, "<init>", "([BLjava/lang/String;)V");
    _jniStringGetBytes = env->GetMethodID(_jniStringClass, "getBytes",
                                          "(Ljava/lang/String;)[B");
    _jniCharsetName = (jstring)env->NewGlobalRef(env->NewStringUTF("UTF-8"));

    _registerObject(typeid(JNIObject).hash_code(), JNIObjectType::kAbstract,
                    JNIBase::getCanonicalName<JNIObject>(), "", initialize<JNIObject>, nullptr);
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

    JNI_ASSERT(baseCanonicalName != "<unknown>" && canonicalName != "<unknown>", "Could not resolve canonicalnames; missing BGJS_JNI_LINK_DEF?");

    // baseCanonicalName must either be empty (not a derived object), or already registered
    std::map<std::string, JNIClassInfo*>::iterator it;

    JNIClassInfo *baseInfo = nullptr;
    if(!baseCanonicalName.empty()) {
        it = _objmap.find(baseCanonicalName);
        if (it == _objmap.end()) {
            JNI_ASSERT(0, "Attempt to register objects with unknown super class");
            return;
        }
        baseInfo = it->second;

        // pure java objects can not directly extend JNIObject
        if(JNIBase::getCanonicalName<JNIObject>() == baseCanonicalName && !i) {
            JNI_ASSERT(0, "Pure java objects must not directly extend JNIObject");
            return;
        }
    } else if(canonicalName != JNIBase::getCanonicalName<JNIObject>()) {
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
        _jniNativeHandleFieldID = info->fieldMap["nativeHandle"].id;
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
            env->RegisterNatives(clazz, &info->methods[0], (jint)info->methods.size());

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
    JNIEnv* env = static_cast<JNIEnv*>(pthread_getspecific(_jniEnvKey));
    if (!env) {
        int r = _jniVM->GetEnv((void **) &env, JNI_VERSION_1_6);
        if (r != JNI_OK) {
            int r = _jniVM->AttachCurrentThread(&env, nullptr);
            JNI_ASSERT(r == JNI_OK, "Failed to attach thread to JVM");
            pthread_setspecific(_jniDetachThreadKey, (void*)(r < 0 ? 1 : r+1));   // Actual value doesn't matter as long as its non-NULL
        }
        pthread_setspecific(_jniEnvKey, (void*)env);
    }

    return env;
}

void JNIWrapper::initializeNativeObject(jobject object, jstring className) {
    JNIEnv* env = JNIWrapper::getEnvironment();

    auto canonicalName = JNIWrapper::jstring2string(className);
    std::replace(canonicalName.begin(), canonicalName.end(), '.', '/');

    // now retrieve registered native class
    auto it = _objmap.find(canonicalName);

    // if nothing was found, the class was not registered
    JNI_ASSERTF(it != _objmap.end(), "Encountered unknown class '%s' during initialization", canonicalName.c_str());

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

        jmethodID constructor;
        if (!constructorAlias) {
            constructor = info->methodMap.at("<init>").id;
            return env->NewObject(info->jniClassRef, constructor);
        } else {
            constructor = info->methodMap.at(constructorAlias).id;
            return env->NewObjectV(info->jniClassRef, constructor, constructorArgs);
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
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNI_ASSERT(env, "JNI Environment not initialized");

    // string pointers can also be null but there is no real way to express that in a std::string
    if(env->IsSameObject(string, NULL)) {
        return "";
    }

    const jbyteArray stringJbytes = (jbyteArray) env->CallObjectMethod(string, _jniStringGetBytes, _jniCharsetName);
    const jsize length = env->GetArrayLength(stringJbytes);
    jbyte* pBytes = env->GetByteArrayElements(stringJbytes, NULL);

    std::string ret = std::string((char *)pBytes, (unsigned long)length);

    env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);
    env->DeleteLocalRef(stringJbytes);

    return ret;
}

/**
 * convert a std::string to a jstring
 */
jstring JNIWrapper::string2jstring(const std::string& string) {
    JNIEnv *env = JNIWrapper::getEnvironment();
    JNI_ASSERT(env, "JNI Environment not initialized");

    jsize len =  (jsize)string.length();

    const jbyteArray bytes = env->NewByteArray(len);
    env->SetByteArrayRegion(bytes, 0, len, (jbyte*)string.c_str());

    jstring javaString = (jstring)env->NewObject(_jniStringClass, _jniStringInit, bytes, _jniCharsetName);

    env->DeleteLocalRef(bytes);

    return javaString;
}

std::map<std::string, JNIClassInfo*> JNIWrapper::_objmap;
jfieldID JNIWrapper::_jniNativeHandleFieldID = nullptr;
JavaVM* JNIWrapper::_jniVM = nullptr;
jstring JNIWrapper::_jniCharsetName = nullptr;
jclass JNIWrapper::_jniStringClass = nullptr;
jmethodID JNIWrapper::_jniStringInit = nullptr;
jmethodID JNIWrapper::_jniStringGetBytes = nullptr;