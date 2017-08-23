//
// Created by Martin Kleinhans on 18.08.17.
//

#ifndef TRADINGLIB_SAMPLE_V8CLASSINFO_H
#define TRADINGLIB_SAMPLE_V8CLASSINFO_H

#include "../bgjs/BGJSV8Engine.h"

class V8ClassInfo;
class V8ClassInfoContainer;
class V8Object;

typedef void(*V8ObjectInitializer)(V8ClassInfo *info);
typedef std::shared_ptr<V8Object>(*V8ObjectCreator)(V8ClassInfo *info, v8::Persistent<v8::Object> *jsObj);

// this declaration uses V8Object, but the type is valid for methods on all subclasses, see:
// http://www.open-std.org/jtc1/sc22/WG21/docs/wp/html/nov97-2/expr.html#expr.static.cast
// there is no implicit conversion however, so methods of derived classes have to be casted manually
typedef void(V8Object::*V8ObjectMethodCallback)(const v8::FunctionCallbackInfo<v8::Value>& args);

struct V8ObjectCallbackHolder {
    V8ObjectCallbackHolder(V8ObjectMethodCallback cb) {
        callback = cb;
    }
    V8ObjectMethodCallback callback;
};

struct V8ClassInfo {
    friend class V8Object;
    friend class JNIV8Wrapper;
public:
    BGJSV8Engine* getEngine() const {
        return engine;
    };

    void registerMethod(const std::string& methodName, V8ObjectMethodCallback callback);
    v8::Local<v8::FunctionTemplate> getFunctionTemplate() const;
private:
    V8ClassInfo(V8ClassInfoContainer *container, BGJSV8Engine *engine);

    BGJSV8Engine *engine;
    V8ClassInfoContainer *container;
    v8::Persistent<v8::FunctionTemplate> functionTemplate;
};

struct V8ClassInfoContainer {
    friend class JNIV8Wrapper;
private:
    V8ClassInfoContainer(const std::string& canonicalName, V8ObjectInitializer i, V8ObjectCreator c);

    std::string canonicalName;
    V8ObjectInitializer initializer;
    V8ObjectCreator creator;
    std::vector<V8ClassInfo*> classInfos;
};

#endif //TRADINGLIB_SAMPLE_V8CLASSINFO_H
