//
// Created by Martin Kleinhans on 19.09.17.
//

#ifndef ANDROID_TRADINGLIB_SAMPLE_JNISCOPE_H
#define ANDROID_TRADINGLIB_SAMPLE_JNISCOPE_H

class JNIObject;
class JNIWrapper;
class JNIClassInfo;

/*
 * -------------------------------------------------------------------------------------------------
 * This class is to be used as an "intermediate" or "mixin" layer when extending Java+Native tuples
 * It provides methods for accessing fields (instance+static) and calling static and super methods
 * with the correct implicit "java inheritance scope".
 *
 * Suppose there is a Class Hierarchy A -> B -> C
 * callJavaSuperVoidMethod("foo") should do diferent this depending on which level it is called
 * e.g. when called from B it should do C::foo(), but on A it should do B::foo()
 * The same applies to fields and static methods which can be overridden on every layer.
 *
 * By making A extend JNIScope<A, B> instead of directly extending B provides a set of methods that
 * implicitly always use the correct scope. To access different scopes from inside or outside of an instance
 * simply convert it's pointer before calling a method.
 *
 * A : public JNIScope<A, B> {}
 *
 * Usage: For example, if the java field "test" is defined in both A and B, the following calls will access the two different versions:
 *
 * C++:
 * --------
 * A* obj;
 * obj->getJavaLongField("test");
 * ((B*)obj)->getJavaLongField("test");
 *
 * Java:
 * --------
 * class A extends B {
 *     public long test;
 * }
 * class B {
 *     public long test;
 * }
 * -------------------------------------------------------------------------------------------------
*/
#define CLASS_SCOPE() \
size_t scope = typeid(ScopeClass).hash_code(); \
BaseClass* jniObject = static_cast<BaseClass*>(this); \
JNIClassInfo *info = jniObject->_jniClassInfo; \
while(info->hashCode != scope && info) info = info->baseClassInfo; \
assert(info);

#define STATIC_METHOD(TypeName, JNITypeName) \
JNITypeName callJavaStatic##TypeName##Method(const char* name, ...) {\
    CLASS_SCOPE() \
    JNIEnv* env = JNIWrapper::getEnvironment();\
    auto it = info->methodMap.find(name); \
    assert(it != info->methodMap.end());\
    assert(it->second.isStatic);\
    va_list args;\
    JNITypeName res;\
    va_start(args, name);\
    res = env->CallStatic##TypeName##MethodV(info->jniClassRef, it->second.id, args);\
    va_end(args);\
    return res;\
}

#define SUPER_METHOD(TypeName, JNITypeName) \
JNITypeName callJavaSuper##TypeName##Method(const char* name, ...) {\
    CLASS_SCOPE() \
    assert(info->baseClassInfo);\
    info = info->baseClassInfo;\
    JNIEnv* env = JNIWrapper::getEnvironment();\
    auto it = info->methodMap.find(name);\
    assert(it != info->methodMap.end());\
    assert(!it->second.isStatic);\
    va_list args;\
    JNITypeName res;\
    va_start(args, name);\
    res = env->CallNonvirtual##TypeName##MethodV(jniObject->getJObject(), info->jniClassRef, it->second.id, args);\
    va_end(args);\
    return res;\
}

#define GETTER(TypeName, JNITypeName) \
JNITypeName getJava##TypeName##Field(const std::string& fieldName) {\
    CLASS_SCOPE() \
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = info->fieldMap.find(fieldName);\
    assert(it != info->fieldMap.end());\
    assert(!it->second.isStatic);\
    return env->Get##TypeName##Field(jniObject->getJObject(), it->second.id); \
}

#define SETTER(TypeName, JNITypeName) \
void setJava##TypeName##Field(const std::string& fieldName, JNITypeName value) {\
    CLASS_SCOPE() \
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = info->fieldMap.find(fieldName);\
    assert(it != info->fieldMap.end());\
    assert(!it->second.isStatic);\
    return env->Set##TypeName##Field(jniObject->getJObject(), it->second.id, value); \
}

#define STATIC_GETTER(TypeName, JNITypeName) \
JNITypeName getJavaStatic##TypeName##Field(const std::string& fieldName) {\
    CLASS_SCOPE() \
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = info->fieldMap.find(fieldName);\
    assert(it != info->fieldMap.end());\
    assert(it->second.isStatic);\
    return env->GetStatic##TypeName##Field(info->jniClassRef, it->second.id); \
}

#define STATIC_SETTER(TypeName, JNITypeName) \
void setJavaStatic##TypeName##Field(const std::string& fieldName, JNITypeName value) {\
    CLASS_SCOPE() \
    JNIEnv* env = JNIWrapper::getEnvironment(); \
    auto it = info->fieldMap.find(fieldName);\
    assert(it != info->fieldMap.end());\
    assert(it->second.isStatic);\
    return env->SetStatic##TypeName##Field(info->jniClassRef, it->second.id, value); \
}

template<class ScopeClass, class BaseClass = JNIObject> class JNIScope : public BaseClass {
protected:
    /**
     * calls the super classes implementation of the specified java method
     * NOTE: throws an exception if method is not implemented on superclass!
     * Naming Scheme: callJavaSuper<TYPE>Method()
     */
    void callJavaSuperVoidMethod(const char* name, ...) {
        CLASS_SCOPE()
        JNIEnv* env = JNIWrapper::getEnvironment();
        assert(info->baseClassInfo);
        info = info->baseClassInfo;
        auto it = info->methodMap.find(name);
        assert(it != info->methodMap.end());
        assert(!it->second.isStatic);
        va_list args;
        va_start(args, name);
        env->CallNonvirtualVoidMethodV(jniObject->getJObject(), info->jniClassRef, it->second.id, args);
        va_end(args);
    }

    SUPER_METHOD(Long, jlong)
    SUPER_METHOD(Boolean, jboolean)
    SUPER_METHOD(Byte, jbyte)
    SUPER_METHOD(Char, jchar)
    SUPER_METHOD(Double, jdouble)
    SUPER_METHOD(Float, jfloat)
    SUPER_METHOD(Int, jint)
    SUPER_METHOD(Short, jshort)
    SUPER_METHOD(Object, jobject)

public:
    JNIScope(jobject obj, JNIClassInfo *info) : BaseClass(obj, info) {};

    /**
     * calls the specified static java method
     * * Naming Scheme: callJavaStatic<TYPE>Method()
     */
    void callJavaStaticVoidMethod(const char* name, ...) {
        CLASS_SCOPE()
        JNIEnv* env = JNIWrapper::getEnvironment();
        auto it = info->methodMap.find(name);
        assert(it != info->methodMap.end());
        assert(it->second.isStatic);
        va_list args;
        va_start(args, name);
        env->CallStaticVoidMethodV(info->jniClassRef, it->second.id, args);
        va_end(args);
    }

    STATIC_METHOD(Boolean, jboolean)
    STATIC_METHOD(Byte, jbyte)
    STATIC_METHOD(Char, jchar)
    STATIC_METHOD(Double, jdouble)
    STATIC_METHOD(Float, jfloat)
    STATIC_METHOD(Int, jint)
    STATIC_METHOD(Short, jshort)
    STATIC_METHOD(Object, jobject)

    /**
     * retrieves the value of the specified static java object field
     * Naming Scheme: getJavaStatic<TYPE>Field()
     */
    STATIC_GETTER(Long, jlong)
    STATIC_GETTER(Boolean, jboolean)
    STATIC_GETTER(Byte, jbyte)
    STATIC_GETTER(Char, jchar)
    STATIC_GETTER(Double, jdouble)
    STATIC_GETTER(Float, jfloat)
    STATIC_GETTER(Int, jint)
    STATIC_GETTER(Short, jshort)
    STATIC_GETTER(Object, jobject)

    /**
     * sets the value of the specified static java object field
     * Naming Scheme: setJavaStatic<TYPE>Field()
     */
    STATIC_SETTER(Long, jlong)
    STATIC_SETTER(Boolean, jboolean)
    STATIC_SETTER(Byte, jbyte)
    STATIC_SETTER(Char, jchar)
    STATIC_SETTER(Double, jdouble)
    STATIC_SETTER(Float, jfloat)
    STATIC_SETTER(Int, jint)
    STATIC_SETTER(Short, jshort)
    STATIC_SETTER(Object, jobject)

    /**
     * retrieves the value of the specified java object field
     * Naming Scheme: getJava<TYPE>Field()
     */
    GETTER(Long, jlong)
    GETTER(Boolean, jboolean)
    GETTER(Byte, jbyte)
    GETTER(Char, jchar)
    GETTER(Double, jdouble)
    GETTER(Float, jfloat)
    GETTER(Int, jint)
    GETTER(Short, jshort)
    GETTER(Object, jobject)

    /**
     * retrieves the value of the specified java object field
     * Naming Scheme: setJava<TYPE>Field()
     */
    SETTER(Long, jlong)
    SETTER(Boolean, jboolean)
    SETTER(Byte, jbyte)
    SETTER(Char, jchar)
    SETTER(Double, jdouble)
    SETTER(Float, jfloat)
    SETTER(Int, jint)
    SETTER(Short, jshort)
    SETTER(Object, jobject)

protected:
    virtual ~JNIScope() = default;
};


#endif //ANDROID_TRADINGLIB_SAMPLE_JNISCOPE_H
