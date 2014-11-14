#include <assert.h>

#include "../BGJSContext.h"
#include "AjaxModule.h"

#include "../ClientAbstract.h"

#define LOG_TAG	"AjaxModule"

using namespace v8;

AjaxModule::AjaxModule() : BGJSModule ("ajax") {
}

static Handle<Value> BlaCallback(const Arguments& args) {

	return v8::Undefined();
}

bool AjaxModule::initialize() {

	return true;
}

void AjaxModule::doRequire (v8::Handle<v8::Object> target) {
	HandleScope scope;

	// Handle<Object> exports = Object::New();
	Handle<FunctionTemplate> ft = FunctionTemplate::New(ajax);

	// NODE_SET_METHOD(exports, "ajax", this->makeStaticCallableFunc(ajax));
	/* exports->Set(String::New("ajax"), ft->GetFunction());
	exports->Set(String::New("bla"), String::New("test")); */

	target->Set(String::New("exports"), ft->GetFunction());
}

Handle<Value> AjaxModule::initWithContext (BGJSContext* context)
{
	doRegister(context);
	return v8::Undefined();
}

AjaxModule::~AjaxModule() {

}

Handle<Value> AjaxModule::ajax(const Arguments& args) {
	v8::Locker l;
	if (args.Length() < 1) {
		LOGE("Not enough parameters for ajax");
		return v8::Undefined();
	}

	/* Local<StackTrace> str = StackTrace::CurrentStackTrace(15);
	LogStackTrace(str); */

	HandleScope scope;

	Local<v8::Object> options = args[0]->ToObject();

	// Get callbacks
	Local<Function> callback = Local<Function>::Cast(
			options->Get(String::New("success")));
	Local<Function> errCallback = Local<Function>::Cast(
				options->Get(String::New("error")));

	Persistent<Function> callbackPers = Persistent<Function>::New(callback);
	Persistent<Function> errorPers;
	if (!errCallback.IsEmpty() && errCallback != v8::Undefined()) {
		errorPers = Persistent<Function>::New(errCallback);
	}
	// Persist the this object
	Persistent<Object> thisObj = Persistent<Object>::New(args.This());

	// Get url
	Local<String> url = Local<String>::Cast(options->Get(String::New("url")));
	String::AsciiValue urlLocal(url);
	// Get data
	Local<Value> data = options->Get(String::New("data"));

	Local<String> method = Local<String>::Cast(options->Get(String::New("type")));

	Local<String> processKey = String::New("processData");
	bool processData = true;
	if (options->Has(processKey)) {
		processData = options->Get(processKey)->BooleanValue();
	}

#ifdef ANDROID
	jstring dataStr, urlStr, methodStr;
	ClientAndroid* client = (ClientAndroid*)(_bgjscontext->_client);
	JNIEnv* env = JNU_GetEnv();
	if (env == NULL) {
		LOGE("Cannot execute AJAX request with no envCache");
		return v8::Undefined();
	}
	urlStr = env->NewStringUTF(*urlLocal);
	if (data != v8::Undefined()) {
		String::Utf8Value dataLocal(data);
		dataStr = env->NewStringUTF(*dataLocal);
	} else {
		dataStr = 0;
	}

	if (method != v8::Undefined()) {
		String::Utf8Value methodLocal(method);
		methodStr = env->NewStringUTF(*methodLocal);
	} else {
		methodStr = 0;
	}

	jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
	jmethodID ajaxMethod = env->GetStaticMethodID(clazz,
			"doAjaxRequest", "(Ljava/lang/String;JJJLjava/lang/String;Ljava/lang/String;Z)V");
	assert(ajaxMethod);
	assert(clazz);
	/* std::stringstream str;
	 str << "Clazz " << clazz << ", " << urlStr << ", " << dataStr << ", cbPers" << (jint)&callbackPers;
	 LOGI(str.str().c_str()); */
	env->CallStaticVoidMethod(clazz, ajaxMethod, urlStr,
			(jlong) *callbackPers, (jlong) *thisObj, (jlong) *errorPers, dataStr, methodStr, (jboolean)processData);

#endif

	return v8::Undefined();
}

#ifdef ANDROID
extern "C" {
JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_ajaxDone(
		JNIEnv * env, jobject obj, jlong ctxPtr, jstring data, jint responseCode, jlong cbPtr,
		jlong thisPtr, jlong errorCb, jboolean success, jboolean processData);
};

JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_ajaxDone(
		JNIEnv * env, jobject obj, jlong ctxPtr, jstring dataStr, jint responseCode,
		jlong jsCbPtr, jlong thisPtr, jlong errorCb, jboolean success, jboolean processData) {

	BGJSContext* context = (BGJSContext*)ctxPtr;

	const char *nativeString = env->GetStringUTFChars(dataStr, 0);
	v8::Locker l;
	Context::Scope context_scope(context->_context);

	HandleScope scope;
	TryCatch trycatch;

	Persistent<Object> thisObj = static_cast<Object*>((void*)thisPtr);
	Persistent<Function> errorP;
	if (errorCb) {
		errorP = static_cast<Function*>((void*)errorCb);
	}
	Persistent<Function> callbackP = static_cast<Function*>((void*)jsCbPtr);

	Handle<Value> argarray[1];
	int argcount = 1;

	if (nativeString == 0 || dataStr == 0) {
		argarray[0] = v8::Null();
	} else {
		Handle<Value> resultObj;
		if (processData) {
			resultObj = context->JsonParse(thisObj,
				String::New(nativeString));
		} else {
			resultObj = String::New(nativeString);
		}

		argarray[0] = resultObj;
	}


	Handle<Value> result;

	if (success) {
		result = callbackP->Call(thisObj, argcount, argarray);
	} else {
		if (!errorP.IsEmpty()) {
			result = errorP->Call(thisObj, argcount, argarray);
		} else {
			LOGI("Error signaled by java code but no error callback set");
			result = v8::Undefined();
		}
	}
	if (result.IsEmpty()) {
		BGJSContext::ReportException(&trycatch);
	}
	env->ReleaseStringUTFChars(dataStr, nativeString);
	callbackP.Dispose();
	thisObj.Dispose();
	if (!errorP.IsEmpty()) {
		errorP.Dispose();
	}

	return true;
}

#endif
