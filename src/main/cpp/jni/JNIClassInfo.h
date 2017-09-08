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

struct JNIMethodInfo{
    bool isStatic;
    std::string name, signature;
    jmethodID id;
};

struct JNIFieldInfo {
    bool isStatic;
    std::string name, signature;
    jfieldID id;
};

struct JNIClassInfo {
    friend class JNIClass;
    friend class JNIObject;
    friend class JNIWrapper;
public:
    /**
     * register the callback for a native method
     * the class instance can be obtained in the callback using JNIWrapper::wrapObject<Type>(obj)
     */
    void registerNativeMethod(const std::string &name, const std::string &signature, void* fnPtr);

    /**
     * register a constructor to be used with JNIWrapper::createObject later
     */
    void registerConstructor(const std::string& signature, const std::string& alias);

    /**
     * register a method to be called via JNIObject or JNIClass later
     * fields and methods have seperate registries, so aliases and names can be "used twice"
     *
     * Note: it is not possible to overwrite names used previously by subclasses
     *
     * Note: to register multiple overloads of the same method aliases can be used
     *
     * Note: overrides of instance methods in subclasses are handled automatically in the same way as in java:
     * all methods are considered virtual, actual instance type determines called method.
     * this means that subclasses must not register overrides manually!
     *
     * Note: names for static methods always refer to the static method of the class that registered the name.
     * subclasses implementing a static method with the same name as a baseclass should always use aliases (e.g. "Type:method")
     * following subclasses will also have to use this new alias to access the "latest" version of the field!
     */
    void registerMethod(const std::string& methodName, const std::string& signature, const std::string& alias = "");
    void registerStaticMethod(const std::string& methodName, const std::string& signature, const std::string& alias = "");
    /**
     * register a field to be accessed via JNIObject or JNIClass later
     * fields and methods have seperate registries, so aliases and names can be "used twice"
     *
     * Note: it is not possible to overwrite names used previously by subclasses
     * when registering fields that shadow a baseclasses field it is recommended to always use an alias (e.g. "Type:field") instead of the actual field name
     * because that might have been already registered by the baseclass, or might be needed to be registered with a later update.
     * following subclasses will also have to use this new alias to access the "latest" version of the field!
     *
     * Note: like static methods, names for fields always refer to the field of the class that registered the name, even if the field is shadowed by a subclass.
     * to access the subclasses shadowing field, use the alias provided by the subclass
     */
    void registerField(const std::string& fieldName, const std::string& signature, const std::string& alias = "");
    void registerStaticField(const std::string& fieldName, const std::string& signature, const std::string& alias = "");

private:
    JNIClassInfo(JNIObjectType type, jclass clazz, const std::string& canonicalName, ObjectInitializer i, ObjectConstructor c, JNIClassInfo *baseClassInfo);

    jmethodID getMethodID(const std::string& methodName,
                          const std::string& signature,
                          bool isStatic);

    jfieldID getFieldID(const std::string& fieldName,
                        const std::string& signature,
                        bool isStatic);

    JNIClassInfo *baseClassInfo;
    JNIObjectType type;
    ObjectInitializer initializer;
    ObjectConstructor constructor;
    std::vector<JNINativeMethod> methods;
    jclass jniClassRef;
    std::string canonicalName;

    std::map<std::string, JNIMethodInfo> methodMap;
    std::map<std::string, JNIFieldInfo> fieldMap;
};


#endif //__JNICLASSINFO_H
