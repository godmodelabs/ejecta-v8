/**
 * BGJSV8Engine
 * Manages a v8 context and exposes script load and execute functions
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

#include "BGJSV8Engine.h"

#include "mallocdebug.h"
#include <assert.h>
#include <sstream>

#include "BGJSGLView.h"

#define LOG_TAG	"BGJSV8Engine-jni"

using namespace v8;

//-----------------------------------------------------------
// Utility functions
//-----------------------------------------------------------

std::vector<std::string> &split(const std::string &s, char delim,
								std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

std::string normalize_path(std::string& path) {
	std::vector < std::string > pathParts;
	std::stringstream ss(path);
	std::string item;
	while (std::getline(ss, item, '/')) {
		pathParts.push_back(item);
	}
	std::string outPath;

	int length = pathParts.size();
	int i = 0;
	if (length > 0 && pathParts.at(0).compare("..") == 0) {
		i = 1;
	}
	for (; i < length - 1; i++) {
		std::string pathPart = pathParts.at(i + 1);
		if (pathPart.compare("..") != 0) {
			std::string nextSegment = pathParts.at(i);
			if (nextSegment.compare(".") == 0) {
				continue;
			}
			if (outPath.length() > 0) {
				outPath.append("/");
			}
			outPath.append(pathParts.at(i));
		} else {
			i++;
		}
	}
	outPath.append("/").append(pathParts.at(length - 1));
	return outPath;
}

std::string getPathName(std::string& path) {
	size_t found = path.find_last_of("/");

	if (found == string::npos) {
		return path;
	}
	return path.substr(0, found);
}

void find_and_replace(std::string& source, std::string const& find, std::string const& replace)
{
	for(std::string::size_type i = 0; (i = source.find(find, i)) != std::string::npos;)
	{
		source.replace(i, find.length(), replace);
		i += replace.length();
	}
}

//-----------------------------------------------------------
// V8 function callbacks
//-----------------------------------------------------------

static void LogCallback(const v8::FunctionCallbackInfo<Value>& args) {
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();

	ctx->log(LOG_INFO, args);
}

static void DebugCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();
	ctx->log(LOG_DEBUG, args);
}

static void InfoCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();

	ctx->log(LOG_INFO, args);
}

static void ErrorCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();

	ctx->log(LOG_ERROR, args);
}

static void RequireCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
	Isolate *isolate = Isolate::GetCurrent();

	// argument must be exactly one string
	if (args.Length() < 1 || !args[0]->IsString()) {
		args.GetReturnValue().SetUndefined();
		return;
	}

	EscapableHandleScope scope(isolate);

	BGJSV8Engine *engine = BGJS_CURRENT_V8ENGINE();

	Local<Value> result = engine->require(BGJS_STRING_FROM_V8VALUE(args[0]));

	args.GetReturnValue().Set(scope.Escape(result));
}

//-----------------------------------------------------------
// V8Engine
//-----------------------------------------------------------

/* static Handle<Value> AssertCallback(const Arguments& args) {
 if (args.Length() < 1)
 return v8::Undefined();

 BGJSV8Engine::log(LOG_ERROR, args);
 return v8::Undefined();
 } */

/* BGJSV8Engine& BGJSV8Engine::getInstance()
 {
 static BGJSV8Engine    instance; // Guaranteed to be destroyed.
 // Instantiated on first use.
 return instance;
 } */

// Register
bool BGJSV8Engine::registerModule(const char* name,
		void (*requireFn)(v8::Isolate* isolate, v8::Handle<v8::Object> target)) {
	// v8::Locker l(Isolate::GetCurrent());
	// HandleScope scope(Isolate::GetCurrent());
	// module->initWithContext(this);
	_modules[name] = requireFn;
	// _modules.insert(std::pair<char const*, BGJSModule*>(module->getName(), module));

	return true;
}

//////////////////////////
// Require

Handle<Value> BGJSV8Engine::JsonParse(Handle<Object> recv,
		Handle<String> source) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	TryCatch trycatch;

	Handle<Value> args[] = { source };

	// Init
	// if (jsonParseMethod.IsEmpty()) {
	Local<Function> jsonParseMethod_ =
			Local<Function>::Cast(
					Script::Compile(
							String::NewFromOneByte(Isolate::GetCurrent(),
									(const uint8_t*)"(function(source) {\n \
           try {\n\
             var a = JSON.parse(source); return a;\
           } catch (e) {\n\
            console.log('json parsing error', e);\n\
           }\n\
         });"),
							String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:script"))->Run());
	// jsonParseMethod = Persistent<Function>::New(jsonParseMethod_);
	//}

	Local<Value> result = jsonParseMethod_->Call(recv, 1, args);

	if (result.IsEmpty()) {
		LOGE("JsonParse exception");
		BGJSV8Engine::ReportException(&trycatch);
		//abort();
	}

	return scope.Escape(result);
}

Handle<Value> BGJSV8Engine::callFunction(Isolate* isolate, Handle<Object> recv, const char* name,
		int argc, Handle<Value> argv[]) const {
	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);
	TryCatch trycatch;

	Local<Function> fn = Handle<Function>::Cast(recv->Get(String::NewFromUtf8(isolate, name)));
			String::Utf8Value value(fn);
	
	Local<Value> result = fn->Call(recv, argc, argv);
	if (result.IsEmpty()) {
		LOGE("callFunction exception");
		BGJSV8Engine::ReportException(&trycatch);
		return v8::Undefined(isolate);
	}
	return scope.Escape(result);
}

v8::Local<v8::Function> BGJSV8Engine::makeRequireFunction(std::string pathName) {
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);

    const char *szJSRequireCode =
            "(function(require, prefix) {"
            "   return function(path) {"
            "       return require(path.indexOf('./')===0?'./'+prefix+'/'+path.substr(2):path);"
            "   };"
            "})";

    Local<Function> requireFn =
            Local<Function>::Cast(
                    Script::Compile(
                            String::NewFromOneByte(_isolate, (const uint8_t*)szJSRequireCode),
                            String::NewFromOneByte(_isolate, (const uint8_t*)"binding:script")
                    )->Run()
            );
    if(_requireFn.IsEmpty()) {
        _requireFn.Reset(_isolate, v8::FunctionTemplate::New(_isolate, RequireCallback)->GetFunction());
    }

    Local<Function> baseRequireFn = Local<Function>::New(_isolate, _requireFn);

    Handle<Value> args[] = { baseRequireFn, String::NewFromUtf8(_isolate, pathName.c_str()) };
    Local<Value> result = requireFn->Call(context->Global(), 2, args);
    return handle_scope.Escape(Local<Function>::Cast(result));
}

Local<Value> BGJSV8Engine::require(std::string baseNameStr){
    Local<Context> context = _isolate->GetCurrentContext();
    EscapableHandleScope handle_scope(_isolate);

    Local<Value> result;
    // Catch errors
    TryCatch try_catch;

    if(baseNameStr.find("./") == 0) {
        baseNameStr = baseNameStr.substr(2);
        find_and_replace(baseNameStr, std::string("/./"), std::string("/"));
        baseNameStr = normalize_path(baseNameStr);
    }
    bool isJson = false;

    // Source of JS file if external code
    Handle<String> source;
    const char* buf = 0;

    // Check if this is an internal module
    requireHook module = _modules[baseNameStr];
	
    if (module) {
        Local<Object> exportsObj = Object::New(_isolate);
        Local<Object> moduleObj = Object::New(_isolate);
        moduleObj->Set(String::NewFromUtf8(_isolate, "exports"), exportsObj);

        module(_isolate, moduleObj);
        return handle_scope.Escape(moduleObj->Get(String::NewFromUtf8(_isolate, "exports")));
    }
    std::string fileName, pathName;

    fileName = baseNameStr;
    buf = this->_client->loadFile(fileName.c_str());
    if (!buf) {
        // Check if this is a directory containing index.js or package.json
        fileName = baseNameStr + "/package.json";
        buf = this->_client->loadFile(fileName.c_str());

        if (!buf) {
            // It might be a directory with an index.js
            fileName = baseNameStr + "/index.js";
            buf = this->_client->loadFile(fileName.c_str());

            if (!buf) {
                // So it might just be a js file
                fileName = baseNameStr + ".js";
                buf = this->_client->loadFile(fileName.c_str());

                if (!buf) {
                    // No JS file, but maybe JSON?
                    fileName = baseNameStr + ".json";
                    buf = this->_client->loadFile(fileName.c_str());
					
                    if (buf) {
                        isJson = true;
                    }
                }
            }
        } else {
            // Parse the package.json
            // Create a string containing the JSON source
            source = String::NewFromUtf8(_isolate, buf);
            Handle<Object> res = BGJSV8Engine::JsonParse(
                    _isolate->GetEnteredContext()->Global(), source)->ToObject();
            Handle<String> mainStr = String::NewFromUtf8(_isolate, (const char *) "main");
            if (res->Has(mainStr)) {
                Handle<String> jsFileName = res->Get(mainStr)->ToString();
                String::Utf8Value jsFileNameC(jsFileName);

                fileName = baseNameStr + "/" + *jsFileNameC;
                if (buf) {
                    free((void *) buf);
                    buf = NULL;
                }

                // It might be a directory with an index.js
                buf = this->_client->loadFile(fileName.c_str());
            } else {
                LOGE("%s doesn't have a main object: %s", baseNameStr.c_str(), buf);
                if (buf) {
                    free((void *) buf);
                    buf = NULL;
                }
            }
        }
    } else if(baseNameStr.find(".json") == baseNameStr.length() - 5) {
        isJson = true;
    }

    if (!buf) {
        LOGE("Cannot find file %s", baseNameStr.c_str());
        //log(LOG_ERROR, args);
        return Undefined(_isolate);
    }

    if (isJson) {
        // Create a string containing the JSON source
        source = String::NewFromUtf8(_isolate, buf);
        Local<Value> res = BGJSV8Engine::JsonParse(
                _isolate->GetEnteredContext()->Global(), source);
        free((void*) buf);
        return handle_scope.Escape(res);
    }

    pathName = getPathName(fileName);

    // wrap source in anonymous function to set up an isolated scope
    const char *szSourcePrefix = "(function (exports, require, module, __filename, __dirname) {";
    const char *szSourcePostfix = "})";
    source = String::Concat(
            String::Concat(
                    String::NewFromUtf8(_isolate, szSourcePrefix),
                    String::NewFromUtf8(_isolate, buf)
            ),
            String::NewFromUtf8(_isolate, szSourcePostfix)
    );
    free((void*) buf);

    // compile script
    Handle<Script> scriptR = Script::Compile(source, String::NewFromUtf8(_isolate, baseNameStr.c_str()));

    // check if compilation failed
    if (scriptR.IsEmpty()) {

        // FIXME UGLY HACK TO DISPLAY SYNTAX ERRORS.
        BGJSV8Engine::ReportException(&try_catch);

        free((void*) buf);
        buf = NULL;

        // Hack because I can't get a proper stacktrace on SyntaxError
        return handle_scope.Escape(try_catch.Exception());
    }

    // run script; this will effectively return a function if everything worked
    // if not, something went wrong
    result = scriptR->Run();
    if (result.IsEmpty() || !result->IsFunction()) {
        /* ReThrow doesn't re-throw TerminationExceptions; a null exception value
         * is re-thrown instead (see Isolate::PropagatePendingExceptionToExternalTryCatch())
         * so we re-propagate a TerminationException explicitly here if necesary. */
        if (try_catch.CanContinue())
            return Undefined(_isolate);
        v8::V8::TerminateExecution(_isolate);

        free((void*) buf);
        buf = NULL;

        return Undefined(_isolate);
    }

    Local<Function> requireFn = makeRequireFunction(pathName);

    Local<Object> exportsObj = Object::New(_isolate);
    Local<Object> moduleObj = Object::New(_isolate);
    moduleObj->Set(String::NewFromUtf8(_isolate, "exports"), exportsObj);

    Handle<Value> fnModuleInitializerArgs[] = {
            exportsObj,                                     // exports
            requireFn,                                      // require
            moduleObj,                                      // module
            String::NewFromUtf8(_isolate, fileName.c_str()), // __filename
            String::NewFromUtf8(_isolate, pathName.c_str())  // __dirname
    };
    Local<Function> fnModuleInitializer = Local<Function>::Cast(result);
    fnModuleInitializer->Call(context->Global(), 5, fnModuleInitializerArgs);

    result = moduleObj->Get(String::NewFromUtf8(_isolate, "exports"));
    return handle_scope.Escape(result);
}

void BGJSV8Engine::setClient(ClientAbstract* client) {
	this->_client = client;
}

ClientAbstract* BGJSV8Engine::getClient() const {
    return this->_client;
}

v8::Isolate* BGJSV8Engine::getIsolate() const {
    return this->_isolate;
}

v8::Local<v8::Context> BGJSV8Engine::getContext() const {
	EscapableHandleScope scope(_isolate);
	return scope.Escape(Local<Context>::New(_isolate, _context));
}

void BGJSV8Engine::cancelAnimationFrame(int id) {
	for (std::set<BGJSGLView*>::iterator it = _glViews.begin();
			it != _glViews.end(); ++it) {
		int i = 0;
		while (i < MAX_FRAME_REQUESTS) {
			AnimationFrameRequest *request = &((*it)->_frameRequests[i]);
			if (request->requestId == id && request->valid
					&& request->view != NULL) {
				request->valid = false;
				BGJS_CLEAR_PERSISTENT(request->callback);
				BGJS_CLEAR_PERSISTENT(request->thisObj);
				break;
			}
			i++;
		}
	}
}

void BGJSV8Engine::registerGLView(BGJSGLView* view) {
	_glViews.insert(view);
}

void BGJSV8Engine::unregisterGLView(BGJSGLView* view) {
	_glViews.erase(view);
}

bool BGJSV8Engine::runAnimationRequests(BGJSGLView* view) const  {
	v8::Locker l(_isolate);
    Isolate::Scope isolateScope(_isolate);
	HandleScope scope(_isolate);

	TryCatch trycatch;
	bool didDraw = false;

	AnimationFrameRequest *request;
	int index = view->_firstFrameRequest, nextIndex = view->_nextFrameRequest,
			startFrame = view->_firstFrameRequest;
	while (index != nextIndex) {
		request = &(view->_frameRequests[index]);

		if (request->valid) {
			didDraw = true;
			request->view->prepareRedraw();
			Handle<Value> args[0];

			Handle<Value> result = Local<Object>::New(_isolate, request->callback)->CallAsFunction(
					Local<Object>::New(_isolate, request->thisObj), 0, args);

			if (result.IsEmpty()) {
				LOGE("Exception occured while running runAnimationRequest cb");
				BGJSV8Engine::ReportException(&trycatch);
			}

			String::Utf8Value fnName(Local<Object>::New(_isolate, request->callback)->ToString());
			BGJS_CLEAR_PERSISTENT(request->callback);
			BGJS_CLEAR_PERSISTENT(request->thisObj);

			request->view->endRedraw();
			request->valid = false;
			request->view = NULL;
		}

		index = (index + 1) % MAX_FRAME_REQUESTS;
	}
	while (index != nextIndex)
		;

	view->_firstFrameRequest = nextIndex;

	// If we couldn't draw anything, request that we can the next time
	if (!didDraw) {
		view->call(_isolate, view->_cbRedraw);
	}
	return didDraw;
}

void BGJSV8Engine::js_global_getLocale(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();

	if (ctx->_locale) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_locale)));
	} else {
	    info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_setLocale(Local<String> property,
		Local<Value> value, const v8::PropertyCallbackInfo<void>& info) {

}

void BGJSV8Engine::js_global_getLang(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();

	if (ctx->_lang) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_lang)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_setLang(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {

}

void BGJSV8Engine::js_global_getTz(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();

	if (ctx->_tz) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_tz)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSV8Engine::js_global_setTz(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {

}

void BGJSV8Engine::js_global_requestAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();
	v8::Locker l(args.GetIsolate());
	HandleScope scope(args.GetIsolate());


	if (args.Length() >= 2 && args[0]->IsFunction() && args[1]->IsObject()) {
	    Local<Object> localFunc = args[0]->ToObject();
		Handle<Object> objRef = args[1]->ToObject();
		BGJSGLView* view = static_cast<BGJSGLView *>(v8::External::Cast(*(objRef->GetInternalField(0)))->Value());
		if (localFunc->IsFunction()) {
			int id = view->requestAnimationFrameForView(ctx->getIsolate(), localFunc, args.This(),
					(ctx->_nextTimerId)++);
			args.GetReturnValue().Set(id);
			return;
		} else {
			LOGI("requestAnimationFrame: Not a function");
		}
	} else {
	    LOGI("requestAnimationFrame: Wrong number or type of parameters (num %d, is function %d %d, is object %d %d, is null %d %d)",
	        args.Length(), args[0]->IsFunction(), args.Length() >= 2 ? args[1]->IsFunction() : false,
	        args[0]->IsObject(), args.Length() >= 2 ? args[1]->IsObject() : false,
	        args[0]->IsNull(), args.Length() >= 2 ? args[1]->IsNull() : false);
        ctx->getIsolate()->ThrowException(
				v8::Exception::ReferenceError(
					v8::String::NewFromUtf8(ctx->getIsolate(), "requestAnimationFrame: Wrong number or type of parameters")));
		return;
	}
	args.GetReturnValue().Set(-1);
}

void BGJSV8Engine::js_global_cancelAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();
	v8::Locker l(ctx->getIsolate());
    HandleScope scope(ctx->getIsolate());
	if (args.Length() >= 1 && args[0]->IsNumber()) {

		int id = (int) (Local<Number>::Cast(args[0])->Value());
		ctx->cancelAnimationFrame(id);
	}

	args.GetReturnValue().SetUndefined();
}

void BGJSV8Engine::js_global_setTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
	setTimeoutInt(args, false);
}

void BGJSV8Engine::js_global_setInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
	setTimeoutInt(args, true);
}

void BGJSV8Engine::setTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& args,
		bool recurring) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();
	v8::Locker l(args.GetIsolate());
	HandleScope scope(args.GetIsolate());


	if (args.Length() == 2 && args[0]->IsFunction() && args[1]->IsNumber()) {
		Local<v8::Function> callback = Local<Function>::Cast(args[0]);

		WrapPersistentFunc* ws = new WrapPersistentFunc();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), ws->callbackFunc, callback);
		WrapPersistentObj* wo = new WrapPersistentObj();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), wo->obj, args.This());

		jlong timeout = (jlong)(Local<Number>::Cast(args[1])->Value());

		ClientAndroid* client = (ClientAndroid*) (BGJS_CURRENT_V8ENGINE()->_client);
		JNIEnv* env = JNU_GetEnv();
		if (env == NULL) {
			LOGE("Cannot execute setTimeout with no envCache");
			args.GetReturnValue().SetUndefined();
			return;
		}

		jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
		jmethodID pushMethod = env->GetStaticMethodID(clazz, "setTimeout",
				"(JJJZ)I");
		assert(pushMethod);
		assert(clazz);
		int subId = env->CallStaticIntMethod(clazz, pushMethod, (jlong) ws,
				(jlong) wo, timeout, (jboolean) recurring);
        args.GetReturnValue().Set(subId);
	} else {
        ctx->getIsolate()->ThrowException(
				v8::Exception::ReferenceError(
						v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong number of parameters")));
	}
	return;
}

void BGJSV8Engine::js_global_clearInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
	clearTimeoutInt(args);
}

void BGJSV8Engine::js_global_clearTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
	clearTimeoutInt(args);
}

void BGJSV8Engine::clearTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSV8Engine *ctx = BGJS_CURRENT_V8ENGINE();
	v8::Locker l(ctx->getIsolate());
    HandleScope scope(ctx->getIsolate());

	args.GetReturnValue().SetUndefined();

	if (args.Length() == 1) {

		int id = (int) ((Local<Integer>::Cast(args[0]))->Value());

		if (id == 0) {
			return;
		}

		ClientAndroid* client = (ClientAndroid*) (BGJS_CURRENT_V8ENGINE()->_client);
		JNIEnv* env = JNU_GetEnv();
		if (env == NULL) {
			LOGE("Cannot execute setTimeout with no envCache");
			return;
		}

		jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
		jmethodID pushMethod = env->GetStaticMethodID(clazz, "removeTimeout",
				"(I)V");
		assert(pushMethod);
		assert(clazz);
		env->CallStaticVoidMethod(clazz, pushMethod, (jint) id);
	} else {
        ctx->getIsolate()->ThrowException(
    				v8::Exception::ReferenceError(
    						v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong arguments for clearTimeout")));
		LOGE("Wrong arguments for clearTimeout");
	}
}

BGJSV8Engine::BGJSV8Engine(v8::Isolate* isolate) {
	_client = NULL;
	_nextTimerId = 1;
	_locale = NULL;

    this->_isolate = isolate;
}

void BGJSV8Engine::createContext() {

	v8::Locker l(_isolate);
	Isolate::Scope isolate_scope(_isolate);
	HandleScope scope(_isolate);

	// Create global object template
	v8::Local<v8::ObjectTemplate> globalObjTpl = v8::ObjectTemplate::New();

	// Add methods to console function
	v8::Local<v8::FunctionTemplate> console = v8::FunctionTemplate::New(_isolate);
	console->Set(String::NewFromUtf8(_isolate, "log"),
				 v8::FunctionTemplate::New(_isolate, LogCallback));
	console->Set(String::NewFromUtf8(_isolate, "debug"),
				 v8::FunctionTemplate::New(_isolate, DebugCallback));
	console->Set(String::NewFromUtf8(_isolate, "info"),
				 v8::FunctionTemplate::New(_isolate, InfoCallback));
	console->Set(String::NewFromUtf8(_isolate, "error"),
				 v8::FunctionTemplate::New(_isolate, ErrorCallback));
	console->Set(String::NewFromUtf8(_isolate, "warn"),
				 v8::FunctionTemplate::New(_isolate, ErrorCallback));
	// console->Set("assert", v8::FunctionTemplate::New(AssertCallback)); // TODO

	globalObjTpl->Set(v8::String::NewFromUtf8(_isolate, "console"), console);

	// environment variables
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "environment"), String::NewFromUtf8(_isolate, "BGJSContext"));

	globalObjTpl->SetAccessor(String::NewFromUtf8(_isolate, "_locale"),
						BGJSV8Engine::js_global_getLocale,
						BGJSV8Engine::js_global_setLocale);
	globalObjTpl->SetAccessor(String::NewFromUtf8(_isolate, "_lang"), BGJSV8Engine::js_global_getLang,
						BGJSV8Engine::js_global_setLang);
	globalObjTpl->SetAccessor(String::NewFromUtf8(_isolate, "_tz"), BGJSV8Engine::js_global_getTz,
						BGJSV8Engine::js_global_setTz);

	// global functions
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "requestAnimationFrame"),
				v8::FunctionTemplate::New(_isolate,
										  BGJSV8Engine::js_global_requestAnimationFrame));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "cancelAnimationFrame"),
				v8::FunctionTemplate::New(_isolate,
										  BGJSV8Engine::js_global_cancelAnimationFrame));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "setTimeout"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setTimeout));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "setInterval"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_setInterval));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearTimeout"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearTimeout));
	globalObjTpl->Set(String::NewFromUtf8(_isolate, "clearInterval"),
				v8::FunctionTemplate::New(_isolate, BGJSV8Engine::js_global_clearInterval));

	// Create a new context.
    Local<Context> context = v8::Context::New(_isolate, NULL, globalObjTpl);
	context->SetAlignedPointerInEmbedderData(EBGJSV8EngineEmbedderData::kContext, this);

	_context.Reset(_isolate, context);
}

void BGJSV8Engine::ReportException(v8::TryCatch* try_catch) {
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);
	Local<Context> context = isolate->GetCurrentContext();

	const char* exception_string = BGJS_CHAR_FROM_V8VALUE(try_catch->Exception());
	v8::Handle<v8::Message> message = try_catch->Message();

	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just
		// print the exception.
		LOGI("Exception: %s", exception_string);
	} else {
		// Print (filename):(line number): (message).
		const char* filename_string = BGJS_CHAR_FROM_V8VALUE(message->GetScriptResourceName());
		int linenum = message->GetLineNumber();
		LOGI("Exception: %s:%i: %s\n", filename_string, linenum, exception_string);
		// Print line of source code.
		const char* sourceline_string = BGJS_CHAR_FROM_V8VALUE(message->GetSourceLine());
		LOGI("%s\n", sourceline_string);
		std::stringstream str;
		// Print wavy underline (GetUnderline is deprecated).
		int start = message->GetStartColumn();
		for (int i = 0; i < start; i++) {
			str << " ";
		}
		int end = message->GetEndColumn();
		for (int i = start; i < end; i++) {
			str << "^";
		}
		LOGI("%s", str.str().c_str());

		MaybeLocal<Value> stack_trace = try_catch->StackTrace(context);
		if (!stack_trace.IsEmpty()) {
			LOGI("%s", BGJS_CHAR_FROM_V8VALUE(try_catch->StackTrace()));
		}

		/* excStr = env->NewStringUTF(exception_string);
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
		} */
	}
}

void BGJSV8Engine::log(int debugLevel, const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Locker locker(args.GetIsolate());
	HandleScope scope(args.GetIsolate());

	std::stringstream str;
	int l = args.Length();
	for (int i = 0; i < l; i++) {
		str << " " << BGJS_CHAR_FROM_V8VALUE(args[i]);
	}

	LOG(debugLevel, "%s", str.str().c_str());
}

int BGJSV8Engine::run(const char *path) {
    Locker locker(_isolate);
    Isolate::Scope isolate_scope(_isolate);

	// Create a stack-allocated handle scope.
	HandleScope scope(_isolate);
	v8::TryCatch try_catch;
    Local<Context> context = Local<Context>::New(_isolate, _context);
    Context::Scope context_scope(context);

    context->Global()->Set(String::NewFromUtf8(_isolate, "global"), context->Global());

	Local<Value> result = require(path);

	if (result.IsEmpty()) {
		// Print errors that happened during execution.
		ReportException(&try_catch);
		return false;
	}

    _nextTimerId = 1;

	return 1;
}

void BGJSV8Engine::setLocale(const char* locale, const char* lang,
		const char* tz) {
	_locale = strdup(locale);
	_lang = strdup(lang);
	_tz = strdup(tz);
}

BGJSV8Engine::~BGJSV8Engine() {
	LOGI("Cleaning up");

	// clear persistent context reference
	_context.Reset();

	if (_locale) {
		free(_locale);
	}
    this->_isolate->Exit();
}

