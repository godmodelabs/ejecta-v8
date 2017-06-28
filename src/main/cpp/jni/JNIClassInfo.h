//
// Created by Martin Kleinhans on 21.04.17.
//

#ifndef __JNICLASSINFO_H
#define __JNICLASSINFO_H

#include <string>
#include <vector>
#include <map>

class JNIClassInfo;
class JNIObject;

typedef JNIObject*(*ObjectConstructor)(jobject obj, JNIClassInfo *info);
typedef void(*ObjectInitializer)(JNIClassInfo *info, bool isReload);
struct JNIClassInfo {
    friend class JNIClass;
    friend class JNIObject;
    friend class JNIWrapper;
public:
    void registerNativeMethod(const std::string &name, const std::string &signature, void* fnPtr);

    void registerConstructor(const std::string& signature, const std::string& alias);
    void registerMethod(const std::string& methodName, const std::string& signature, const std::string& alias = "");
    void registerField(const std::string& fieldName, const std::string& signature);

    void registerStaticMethod(const std::string& methodName, const std::string& signature, const std::string& alias = "");
    void registerStaticField(const std::string& fieldName, const std::string& signature);

    jmethodID getMethodID(const std::string& methodName,
                          const std::string& signature,
                          bool isStatic);

    jfieldID getFieldID(const std::string& fieldName,
                        const std::string& signature,
                        bool isStatic);
private:
    JNIClassInfo(bool persistent, const std::string& canonicalName, ObjectInitializer i, ObjectConstructor c);

    bool persistent;
    ObjectInitializer initializer;
    ObjectConstructor constructor;
    std::vector<JNINativeMethod> methods;
    jclass jniClassRef;
    std::string canonicalName;

    std::map<std::string, jmethodID> methodMap;
    std::map<std::string, jfieldID> fieldMap;
};


#endif //__JNICLASSINFO_H
