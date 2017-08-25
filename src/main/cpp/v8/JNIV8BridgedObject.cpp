//
// Created by Martin Kleinhans on 24.08.17.
//

#include "JNIV8BridgedObject.h"

void JNIV8BridgedObject::initializeJNIBindings(JNIClassInfo *info, bool isReload) {

}

void JNIV8BridgedObject::initializeV8Bindings(V8ClassInfo *info) {
    info->registerConstructor((JNIV8ObjectConstructorCallback)&JNIV8BridgedObject::constructorV8Callback);
    info->registerAccessor("propertyName",
                           (JNIV8ObjectAccessorGetterCallback)&JNIV8BridgedObject::getterV8Callback,
                           (JNIV8ObjectAccessorSetterCallback)&JNIV8BridgedObject::setterV8Callback);
    info->registerMethod("methodName", (JNIV8ObjectMethodCallback)&JNIV8BridgedObject::methodV8Callback);
}


void JNIV8BridgedObject::methodV8Callback(const std::string &methodName, const v8::FunctionCallbackInfo<v8::Value>& args) {
}

void JNIV8BridgedObject::constructorV8Callback(const v8::FunctionCallbackInfo<v8::Value>& args) {

}

void JNIV8BridgedObject::getterV8Callback(const std::string &propertyName, const v8::PropertyCallbackInfo<v8::Value> &info) {

}

void JNIV8BridgedObject::setterV8Callback(const std::string &propertyName, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info) {

}