
#include "V8TestClass2.h"

using namespace v8;

BGJS_JNIV8OBJECT_LINK(V8TestClass2, "ag/boersego/bgjs/V8TestClass2");

#define LOG_TAG	"V8TestClass2"

void _V8TestClass2_overwriteMe(JNIEnv *env, jobject object, jlong l, jfloat f, jdouble d, jstring s) {
    auto pInstance = JNIWrapper::wrapObject<V8TestClass2>(object);
    pInstance->overwriteMe();
}
void V8TestClass2::overwriteMe() {
    callJavaSuperVoidMethod("overwriteMe");
}

void V8TestClass2::initializeV8Bindings(V8ClassInfo *info) {
    info->registerConstructor((JNIV8ObjectConstructorCallback)&V8TestClass2::initFromJS);
    info->registerMethod("methodName2", (JNIV8ObjectMethodCallback)&V8TestClass2::testMember);
    info->registerAccessor("propertyName2", (JNIV8ObjectAccessorGetterCallback)&V8TestClass2::getter,
                           (JNIV8ObjectAccessorSetterCallback)&V8TestClass2::setter);
}

void V8TestClass2::testMember(const std::string &methodName, const v8::FunctionCallbackInfo<v8::Value>& args) {
    HandleScope scope(args.GetIsolate());
    Local<Object> obj = (Local<Object>::Cast(args[0]));
    auto cls = JNIV8Wrapper::wrapObject<V8TestClass>(obj);
    cls->test();
    LOGE("(V8TestClass2) shadowField %ld", getJavaLongField("shadowField"));
    LOGE("(V8TestClass2) staticShadowField %ld", getJavaStaticLongField("staticShadowField"));
    LOGE("(V8TestClass) shadowField %ld", ((V8TestClass*)this)->getJavaLongField("shadowField"));
    LOGE("(V8TestClass) staticShadowField %ld", ((V8TestClass*)this)->getJavaStaticLongField("staticShadowField"));
}

void V8TestClass2::getter(const std::string &propertyName, const v8::PropertyCallbackInfo<v8::Value> &info) {

}

void V8TestClass2::setter(const std::string &propertyName, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info) {

}

void V8TestClass2::initFromJS(const v8::FunctionCallbackInfo<v8::Value>& args) {
}

void V8TestClass2::initializeJNIBindings(JNIClassInfo *info, bool isReload) {
    info->registerField("shadowField","J");
    info->registerStaticField("staticShadowField","J");

    info->registerNativeMethod("overwriteMe", "()V", (void*)_V8TestClass2_overwriteMe);
}
