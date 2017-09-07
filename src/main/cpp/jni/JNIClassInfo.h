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

enum class JNIObjectType {
    /**
     * Java + native class are permanently linked together
     * native class can store persistent state, and exists as long as the java class
     * wrapping the same java object will always yield the exact same native instance
     */
    kPersistent,
    /**
     * like kPersistent, but these objects can not be constructed
     */
    kAbstract,
    /**
     * native class acts as a temporary wrapper for a Java object
     * this can be used to write utility methods for working with existing Java classes, e.g. you could wrap String
     * temporary native class must NOT store persistent state, and will be deallocated once it is not directly referenced from native code anymore
     * wrapping the same java object will always yield a new native instance
     */
    kTemporary
};

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
    JNIClassInfo(JNIObjectType type, const std::string& canonicalName, ObjectInitializer i, ObjectConstructor c, JNIClassInfo *baseClassInfo);

    JNIClassInfo *baseClassInfo;
    JNIObjectType type;
    ObjectInitializer initializer;
    ObjectConstructor constructor;
    std::vector<JNINativeMethod> methods;
    jclass jniClassRef;
    std::string canonicalName;

    std::map<std::string, jmethodID> methodMap;
    std::map<std::string, jfieldID> fieldMap;
};


#endif //__JNICLASSINFO_H
