#include "BGJSView.h"
#include "os-android.h"
#include "mallocdebug.h"
#include <v8.h>

#include "GLcompat.h"

#if defined __ANDROID__
#include "ClientAndroid.h"
#endif


// #define DEBUG_GL 1
#undef DEBUG_GL
// #define DEBUG 1
#undef DEBUG

#define LOG_TAG	"BGJSView"

using namespace v8;

static void checkGlError(const char* op) {
#ifdef DEBUG_GL
	for (GLint error = glGetError(); error; error = glGetError()) {
		LOGI("after %s() glError (0x%x)\n", op, error);
	}
#endif
}


Handle<Value> getWidth(Local<String> property, const AccessorInfo &info) {
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	int value = static_cast<BGJSView*>(ptr)->width;
#ifdef DEBUG
	LOGD("getWidth %u", value);
#endif
	return Integer::New(value);
}

void setWidth(Local<String> property, Local<Value> value,
		const AccessorInfo& info) {
}

Handle<Value> getHeight(Local<String> property, const AccessorInfo &info) {
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	int value = static_cast<BGJSView*>(ptr)->height;
#ifdef DEBUG
	LOGD("getHeight %u", value);
#endif
	return Integer::New(value);
}

Handle<Value> getPixelRatio(Local<String> property, const AccessorInfo &info) {
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	float value = static_cast<BGJSView*>(ptr)->pixelRatio;

#ifdef DEBUG
	LOGD("getPixelRatio %f", value);
#endif
	return Number::New(value);
}

void setPixelRatio(Local<String> property, Local<Value> value,
		const AccessorInfo& info) {
}

void setHeight(Local<String> property, Local<Value> value,
		const AccessorInfo& info) {
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
#ifdef DEBUG
	LOGI("Setting height to %d", value->Int32Value());
#endif
	static_cast<BGJSView*>(ptr)->height = value->Int32Value();
}

Handle<Value> BGJSView::js_view_on(const Arguments& args) {
	v8::Locker l;
	HandleScope scope;
	BGJSView *view = externalToClassPtr<BGJSView>(args.Data());
	if (args.Length() == 2 && args[0]->IsString() && args[1]->IsObject()) {
		Handle<Object> func = args[1]->ToObject();
		if (func->IsFunction()) {
			String::Utf8Value eventUtf8(args[0]->ToString());
			const char *event = *eventUtf8;
			if (strcmp(event, "event") == 0) {
				view->_cbEvent.push_back(Persistent<Object>::New(func));
			} else if (strcmp(event, "close") == 0) {
				view->_cbClose.push_back(Persistent<Object>::New(func));
			} else if (strcmp(event, "resize") == 0) {
				view->_cbResize.push_back(Persistent<Object>::New(func));
			} else if (strcmp(event, "redraw") == 0) {
				view->_cbRedraw.push_back(Persistent<Object>::New(func));
			}
		}
	}
	return v8::Undefined();
}

BGJSView::BGJSView(BGJSContext *ctx, float pixelRatio) {
	opened = false;
	_contentObj = 0;

	noClearOnFlip = false;

	this->pixelRatio = pixelRatio;

	// Create new JS BGJSView object
	this->_jsContext = ctx;
	v8::Local<v8::FunctionTemplate> bgjsglft = v8::FunctionTemplate::New();
	bgjsglft->SetClassName(String::New("BGJSView"));
	v8::Local<v8::ObjectTemplate> bgjsgl = bgjsglft->InstanceTemplate();
	bgjsgl->SetInternalFieldCount(1);
	bgjsgl->SetAccessor(String::New("width"), getWidth, setWidth);
	bgjsgl->SetAccessor(String::New("height"), getHeight, setHeight);
	bgjsgl->SetAccessor(String::New("devicePixelRatio"), getPixelRatio, setPixelRatio);

	// bgjsgl->SetAccessor(String::New("magnifierPoint"), getMagnifierPoint, setMagnifierPoint);

	NODE_SET_METHOD(bgjsgl, "on", makeStaticCallableFunc(BGJSView::js_view_on));

	this->jsViewOT = Persistent<v8::ObjectTemplate>::New(bgjsgl);
}

Handle<Value> BGJSView::startJS(const char* fnName, const char* configJson, Handle<Value> uiObj, long configId, bool hasIntradayQuotes) {
	HandleScope scope;

	Handle<Value> config;

	if (configJson) {
		config = String::New(configJson);
	} else {
		config = v8::Undefined();
	}

	// bgjsgl->Set(String::New("log"), FunctionTemplate::New(BGJSGLModule::log));
	Local<Object> objInstance = this->jsViewOT->NewInstance();
	objInstance->SetInternalField(0, External::New(this));
	// Local<Object> instance = bgjsglft->GetFunction()->NewInstance();
	this->_jsObj = Persistent<Object>::New(objInstance);

	Handle<Value> argv[5] = { uiObj, this->_jsObj, config, Number::New(configId), Number::New(hasIntradayQuotes) };

	Handle<Value> res = this->_jsContext->callFunction(_jsContext->_context->Global(), fnName, 5,
			argv);
	if (res->IsNumber()) {
		_contentObj = res->ToNumber()->Value();
#ifdef DEBUG
		LOGD ("startJS return id %d", _contentObj);
#endif
	} else {
		LOGI ("Did not receive a return id from startJS");
	}
	return scope.Close(res);
}

void BGJSView::sendEvent(Handle<Object> eventObjRef) {
	TryCatch trycatch;
	HandleScope scope;

	Handle<Value> args[] = { eventObjRef };

	/* if (!_cbEvent) {
		return;
	} */

	const int count = _cbEvent.size();

	// eventObjRef->Set(String::New("target"))


	for (std::vector<Persistent<Object> >::size_type i = 0; i < count; i++) {
		Handle<Value> result = _cbEvent[i]->CallAsFunction(_cbEvent[i], 1, args);
		if (result.IsEmpty()) {
			BGJSContext::ReportException(&trycatch);
		}
	}
}

void BGJSView::call(std::vector<Persistent<Object> > &list) {
	TryCatch trycatch;
	HandleScope scope;

	Handle<Value> args[] = { };

	const int count = list.size();

	for (std::vector<Persistent<Object> >::size_type i = 0; i < count; i++) {
		Handle<Value> result = list[i]->CallAsFunction(list[i], 0, args);
		if (result.IsEmpty()) {
			BGJSContext::ReportException(&trycatch);
		}
	}
}

BGJSView::~BGJSView() {
	// Dispose of permanent references to event listeners
	int count = _cbClose.size();
	for (std::vector<Persistent<Object> >::size_type i = 0; i < count; i++) {
		_cbClose[i].Dispose();
	}
	count = _cbResize.size();
	for (std::vector<Persistent<Object> >::size_type i = 0; i < count; i++) {
		_cbResize[i].Dispose();
	}
	count = _cbEvent.size();
	for (std::vector<Persistent<Object> >::size_type i = 0; i < count; i++) {
		_cbEvent[i].Dispose();
	}
	count = _cbRedraw.size();
	for (std::vector<Persistent<Object> >::size_type i = 0; i < count; i++) {
		_cbRedraw[i].Dispose();
	}
	if (*this->jsViewOT && !this->jsViewOT.IsEmpty()) {
		this->jsViewOT.Dispose();
	}
}
