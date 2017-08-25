//
// Created by Martin Kleinhans on 24.08.17.
//

#ifndef TRADINGLIB_SAMPLE_JNIV8BRIDGEDOBJECT_H
#define TRADINGLIB_SAMPLE_JNIV8BRIDGEDOBJECT_H


#include "../v8/JNIV8Wrapper.h"

class JNIV8BridgedObject : public JNIV8Object {
public:
    BGJS_JNIV8OBJECT_CONSTRUCTOR(JNIV8BridgedObject);
    static void initializeJNIBindings(JNIClassInfo *info, bool isReload);
    static void initializeV8Bindings(V8ClassInfo *info);

    void methodV8Callback(const std::string &methodName, const v8::FunctionCallbackInfo<v8::Value>& args);
    void constructorV8Callback(const v8::FunctionCallbackInfo<v8::Value>& args);
    void getterV8Callback(const std::string &propertyName, const v8::PropertyCallbackInfo<v8::Value> &info);
    void setterV8Callback(const std::string &propertyName, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void> &info);
};


#endif //TRADINGLIB_SAMPLE_JNIV8BRIDGEDOBJECT_H
