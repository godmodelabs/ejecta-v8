#ifndef __BGJSCLASS_H
#define __BGJSCLASS_H  1

#include "os-detection.h"
#include <v8.h>

// class BGJSContext;

#define NODE_SET_METHOD(obj, name, callback)                              \
  obj->Set(v8::String::NewFromUtf8(isolate, name),                                   \
           callback->GetFunction())

#define LOG_TAG "BGJSClass"

class BGJSClass {
public:

	v8::Local<v8::FunctionTemplate> makeStaticCallableFunc(
			v8::FunctionCallback func);
	v8::Local<v8::External> classPtrToExternal();
	// See: http://stackoverflow.com/questions/3171418/v8-functiontemplate-class-instance

	////////////////////////////////////////////////////////////////////////
	//
	// Converts an External to a V8TutorialBase pointer. This assumes that the
	// data inside the v8::External is a "this" pointer that was wrapped by
	// makeStaticCallableFunc
	//
	// \parameter data Shoudld be v8::Arguments::Data()
	//
	// \return "this" pointer inside v8::Arguments::Data() on success, NULL otherwise
	//
	////////////////////////////////////////////////////////////////////////
	template<typename T>
	static T *externalToClassPtr(v8::Local<v8::Value> data) {
		if (data.IsEmpty()) {
			LOGE("Data empty");
		} else if (!data->IsExternal()) {
			LOGE("Data not external");
		} else {
			return static_cast<T *>(v8::External::Cast(*(data))->Value());
		}

		//If function gets here, one of the checks above failed
		return NULL;
	}
};

#undef LOG_TAG

#endif
