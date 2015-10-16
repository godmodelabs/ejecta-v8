/**
 *    Z   z                                  //////////////_            Anteater
 *          Z   O               __\\\\@   //^^        _-    \///////      eats
 *       Z    z   o       _____((_     \-/ ____/ /   {   { \\       }     ants
 *                  o    0__________\\\---//____/----//__|-^\\\\\\\\
 */

#include "BGJSContext.h"

#include "mallocdebug.h"
#include <assert.h>
#include <sstream>

#undef DEBUG
// #define DEBUG 1

// #define ENABLE_DEBUGGER_SUPPORT = 1

#ifdef ENABLE_DEBUGGER_SUPPORT
#include <v8-debug.h>
#endif

#include "BGJSGLView.h"

#define LOG_TAG	"BGJSContext-jni"

using namespace v8;

v8::Persistent<v8::Context> BGJSInfo::_context;
v8::Persistent<v8::ObjectTemplate> BGJSInfo::_global;
BGJSContext* BGJSInfo::_jscontext;

#ifdef ENABLE_DEBUGGER_SUPPORT
v8::Persistent<v8::Context> debug_message_context;

void DispatchDebugMessages() {
	// We are in some random thread. We should already have v8::Locker acquired
	// (we requested this when registered this callback). We was called
	// because new debug messages arrived; they may have already been processed,
	// but we shouldn't worry about this.
	//
	// All we have to do is to set context and call ProcessDebugMessages.
	//
	// We should decide which V8 context to use here. This is important for
	// "evaluate" command, because it must be executed some context.
	// In our sample we have only one context, so there is nothing really to
	// think about.
	v8::Context::Scope scope(debug_message_context);

	v8::Debug::ProcessDebugMessages();
}
#endif  // ENABLE_DEBUGGER_SUPPORT
// Extracts a C string from a V8 Utf8Value.
const char* ToCString(const v8::String::Utf8Value& value) {
	return *value ? *value : "<string conversion failed>";
}

static Handle<Value> LogCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->log(LOG_INFO, args);
	return v8::Undefined();
}

static Handle<Value> DebugCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	BGJSContext *ctx = BGJSInfo::_jscontext;
	ctx->log(LOG_DEBUG, args);
	return v8::Undefined();
}

static Handle<Value> InfoCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->log(LOG_INFO, args);
	return v8::Undefined();
}

static Handle<Value> ErrorCallback(const Arguments& args) {
	if (args.Length() < 1)
		return v8::Undefined();

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->log(LOG_ERROR, args);
	return v8::Undefined();
}

void LogStackTrace(Local<StackTrace>& str) {
	HandleScope scope;
	int count = str->GetFrameCount();
	for (int i = 0; i < count; i++) {
		Local<StackFrame> frame = str->GetFrame(i);
		String::AsciiValue fnName(frame->GetFunctionName());
		String::AsciiValue scriptName(frame->GetScriptName());
		LOGI("%s:%u (%s)", *fnName, frame->GetLineNumber(), *scriptName);
	}
}

/* static Handle<Value> AssertCallback(const Arguments& args) {
 if (args.Length() < 1)
 return v8::Undefined();

 BGJSContext::log(LOG_ERROR, args);
 return v8::Undefined();
 } */

/* BGJSContext& BGJSContext::getInstance()
 {
 static BGJSContext    instance; // Guaranteed to be destroyed.
 // Instantiated on first use.
 return instance;
 } */

// Register
bool BGJSContext::registerModule(const char* name,
		void (*requireFn)(v8::Handle<v8::Object> target)) {
	v8::Locker l;
	HandleScope scope;
	// module->initWithContext(this);
	_modules[name] = requireFn;
	// _modules.insert(std::pair<char const*, BGJSModule*>(module->getName(), module));

#ifdef DEBUG
	LOGI(
			"register %s %p %p %p %i", name, requireFn, _modules[name], _modules["ajax"], _modules.size());
#endif
	return true;
}

//////////////////////////
// Require

void BGJSContext::CloneObject(Handle<Object> recv, Handle<Value> source,
		Handle<Value> target) {
	HandleScope scope;

	Handle<Value> args[] = { source, target };

	// Init
	if (this->cloneObjectMethod.IsEmpty()) {
		Local<Function> cloneObjectMethod_ =
				Local<Function>::Cast(
						Script::Compile(
								String::New(
										"(function(source, target) {\n\
           Object.getOwnPropertyNames(source).forEach(function(key) {\n\
           try {\n\
             var desc = Object.getOwnPropertyDescriptor(source, key);\n\
             if (desc.value === source) desc.value = target;\n\
             Object.defineProperty(target, key, desc);\n\
           } catch (e) {\n\
            // Catch sealed properties errors\n\
           }\n\
         });\n\
        })"),
								String::New("binding:script"))->Run());
		this->cloneObjectMethod = Persistent<Function>::New(cloneObjectMethod_);
	}

	this->cloneObjectMethod->Call(recv, 2, args);
}

Handle<Value> BGJSContext::JsonParse(Handle<Object> recv,
		Handle<String> source) {
	HandleScope scope;
	TryCatch trycatch;

	Handle<Value> args[] = { source };

	// Init
	// if (jsonParseMethod.IsEmpty()) {
	Local<Function> jsonParseMethod_ =
			Local<Function>::Cast(
					Script::Compile(
							String::New(
									"(function(source) {\n \
           try {\n\
             var a = JSON.parse(source); return a;\
           } catch (e) {\n\
            console.log('json parsing error', e);\n\
           }\n\
         });"),
							String::New("binding:script"))->Run());
	// jsonParseMethod = Persistent<Function>::New(jsonParseMethod_);
	//}

	Handle<Value> result = jsonParseMethod_->Call(recv, 1, args);

	if (result.IsEmpty()) {
		LOGE("JsonParse exception");
		BGJSContext::ReportException(&trycatch);
		//abort();
	}

	return scope.Close(result);
}

Handle<Value> BGJSContext::executeJS(const char* src) {
	v8::Locker l;
	HandleScope scope;
	TryCatch trycatch;

	Handle<Value> result = Script::Compile(String::New(src),
			String::New("binding:script"))->Run();
	if (result.IsEmpty()) {
		LOGE("callFunction exception");
		BGJSContext::ReportException(&trycatch);
		//abort();
	}
	return scope.Close(result);
}

Handle<Value> BGJSContext::callFunction(Handle<Object> recv, const char* name,
		int argc, Handle<Value> argv[]) {
	v8::Locker l;
	HandleScope scope;
	TryCatch trycatch;

	Handle<Function> fn = Handle<Function>::Cast(recv->Get(String::New(name)));
			String::Utf8Value value(fn);
	Handle<Value> result = fn->Call(recv, argc, argv);
	if (result.IsEmpty()) {
		LOGE("callFunction exception");
		BGJSContext::ReportException(&trycatch);
		return v8::Undefined();
	}
	return scope.Close(result);
}

std::vector<std::string> &split(const std::string &s, char delim,
		std::vector<std::string> &elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

std::string BGJSContext::getPathName(std::string& path) {
	size_t found = path.find_last_of("/");

	if (found == string::npos) {
		return path;
	}
	return path.substr(0, found);
}

std::string BGJSContext::normalize_path(std::string& path) {
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
		// LOGD("normalize_path. Part %i = %s, %i", i + 1, pathPart.c_str(), pathPart.compare("."));
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

Handle<Value> BGJSContext::normalizePath(const Arguments& args) {
	if (args.Length() < 1) {
		return v8::Undefined();
	}

	v8::Locker l;
	HandleScope scope;

	Local<String> filename = args[0]->ToString();

	String::AsciiValue basename(filename);

	// Catch errors
	TryCatch try_catch;
	std::string baseNameStr = std::string(*basename);

	Local<Value> dirVal = Context::GetEntered()->GetData();
	Local<String> dirName;
	if (dirVal->IsUndefined()) {
		dirName = String::New("js");
	} else {
		dirName = dirVal->ToString();
	}
	String::AsciiValue dirNameC(dirName);
#ifdef DEBUG
	LOGD("Dirname %s", *dirNameC);
#endif

	// Check if this is an internal module
	requireHook module = _modules[baseNameStr];

#ifdef DEBUG
	LOGI("normalizePath %s %p %i", *basename, module, _modules.size());
#endif

	if (module) {
		// For modules just return the name again
		return scope.Close(filename);
	}
	std::string pathName;
	std::string pathTemp = std::string(*dirNameC).append("/").append(baseNameStr);
#ifdef DEBUG
	LOGD("Pathtemp %s", pathTemp.c_str());
#endif
	pathName = normalize_path(pathTemp);
	Local<String> normalizedPath = String::New(pathName.c_str());

	return scope.Close(normalizedPath);

}

Handle<Value> BGJSContext::require(const Arguments& args) {
	if (args.Length() < 1) {
		return v8::Undefined();
	}

	v8::Locker l;
	HandleScope scope;

	// Source of JS file if external code
	Handle<String> source;
	const char* buf = 0;
	// Result of internal or external invocation
	Handle<Value> result;
	Local<String> filename = args[0]->ToString();

	String::AsciiValue basename(filename);

	// Catch errors
	TryCatch try_catch;
	std::string baseNameStr = std::string(*basename);
	bool isJson = false;
	Local<Object> sandbox = args[1]->ToObject();
	Handle<String> dirNameStr = String::New("__dirname");

	Local<Value> dirVal = Context::GetEntered()->GetData();
	Local<String> dirName;
	if (dirVal->IsUndefined()) {
		dirName = String::New("js");
	} else {
		dirName = dirVal->ToString();
	}
	String::AsciiValue dirNameC(dirName);
#ifdef DEBUG
	LOGD("Dirname %s", *dirNameC);
#endif

	// Check if this is an internal module
	requireHook module = _modules[baseNameStr];

#ifdef DEBUG
	LOGI("require %s %p %i", *basename, module, _modules.size());
#endif

	if (module) {
		Local<Object> exports = Object::New();
		module(exports);
		CloneObject(args.This(), exports, sandbox);
		return scope.Close(exports);
	}
	std::string basePath = std::string(baseNameStr);
	std::string pathName = std::string(basePath);
	

	// Check if this is a directory containing index.js or package.json
	pathName.append("/package.json");
	buf = this->_client->loadFile(pathName.c_str());
#ifdef DEBUG
	LOGD("Opening file %s", pathName.c_str());
#endif
	if (!buf) {
		// It might be a directory with an index.js
		std::string indexPath (basePath);
		indexPath.append("/index.js");
		
		buf = this->_client->loadFile(indexPath.c_str());

#ifdef DEBUG
		LOGD("Opening file %s", indexPath.c_str());
#endif

		if (!buf) {
			// So it might just be a js file
			std::string jsPath (basePath);
			jsPath.append(".js");
			buf = this->_client->loadFile(jsPath.c_str());
#ifdef DEBUG
			LOGD("Opening file %s", jsPath.c_str());
#endif

			if (!buf) {
				// No JS file, but maybe JSON?
				std::string jsonPath (basePath);
				basePath = std::string(baseNameStr);
				jsonPath.append(".json");
#ifdef DEBUG
				LOGD("Opening file %s", jsonPath.c_str());
#endif
				buf = this->_client->loadFile(jsonPath.c_str());
				if (buf) {
					isJson = true;
					basePath = getPathName(jsPath);
				}
			} else {
				basePath = getPathName(jsPath);
			}
		}
	} else {
		// Parse the package.json
		// Create a string containing the JSON source
		source = String::New(buf);
		Handle<Object> res = BGJSContext::JsonParse(
				BGJSContext::_context->Global(), source)->ToObject();
		Handle<String> mainStr = String::New("main");
		if (res->Has(mainStr)) {
			Handle<String> jsFileName = res->Get(mainStr)->ToString();
			String::AsciiValue jsFileNameC(jsFileName);
#ifdef DEBUG
			LOGD("Main JS file %s", *jsFileNameC);
#endif
			std::string indexPath (basePath);
			indexPath.append("/").append(*jsFileNameC);
			if (buf) {
				free((void*) buf);
				buf = NULL;
			}

			// It might be a directory with an index.js
			buf = this->_client->loadFile(indexPath.c_str());
		} else {
			LOGE("%s doesn't have a main object: %s", pathName.c_str(), buf);
			if (buf) {
				free((void*) buf);
				buf = NULL;
			}
		}
	}
	
	if (!buf) {
		std::string pathTemp = std::string(baseNameStr);
		basePath = getPathName(pathTemp);
#ifdef DEBUG
		LOGD("Pathtemp %s", pathTemp.c_str());
#endif
		// pathName = normalize_path(baseNameStr);
		buf = this->_client->loadFile(pathTemp.c_str());

		// Check if we are opening a JSON file
		const int length = baseNameStr.length();
		if (length > 5 && baseNameStr[length - 1] == 'n'
				&& baseNameStr[length - 2] == 'o' && baseNameStr[length - 3] == 's'
				&& baseNameStr[length - 4] == 'j'
				&& baseNameStr[length - 5] == '.') {
			// The file is a json file, just open it and parse it
			isJson = true;
		}
	}

	if (isJson) {
		if (!buf) {
			LOGE("Cannot find file %s", *basename);
			log(LOG_ERROR, args);
			return v8::Undefined();
		}
		// Create a string containing the JSON source
		source = String::New(buf);
		Handle<Value> res = BGJSContext::JsonParse(
				BGJSContext::_context->Global(), source);
		// CloneObject(args.This(), res, BGJSContext::_context->Global());
		if (buf) {
			free((void*) buf);
			buf = NULL;
		}
		return scope.Close(res);
	}

	Persistent<Context> context = Context::New(NULL, BGJSInfo::_global);

	Local<Array> keys;
	// Enter the context
	context->Enter();

	context->SetData(String::New(basePath.c_str()));

	sandbox->Set(dirNameStr, String::New(basePath.c_str()));
#ifdef DEBUG
	LOGD("New __dirname %s", basePath.c_str());
#endif

	// Copy everything from the passed in sandbox (either the persistent
	// context for runInContext(), or the sandbox arg to runInNewContext()).
	CloneObject(args.This(), sandbox, context->Global()->GetPrototype());

	context->Global()->GetPrototype()->ToObject()->Set(dirNameStr, String::New(basePath.c_str()));

	if (buf) {
		// Create a string containing the JavaScript source code.
		source = String::New(buf);

		// well, here WrappedScript::New would suffice in all cases, but maybe
		// Compile has a little better performance where possible
		Handle<Script> scriptR = Script::Compile(source, filename);
		if (scriptR.IsEmpty()) {

			context->DetachGlobal();
			context->Exit();
			context.Dispose();

			// FIXME UGLY HACK TO DISPLAY SYNTAX ERRORS.
			BGJSContext::ReportException(&try_catch);

			free((void*) buf);
			buf = NULL;

			// Hack because I can't get a proper stacktrace on SyntaxError
			return try_catch.ReThrow();
		}

		result = scriptR->Run();
		if (result.IsEmpty()) {
			context->DetachGlobal();
			context->Exit();
			context.Dispose();
			/* ReThrow doesn't re-throw TerminationExceptions; a null exception value
			 * is re-thrown instead (see Isolate::PropagatePendingExceptionToExternalTryCatch())
			 * so we re-propagate a TerminationException explicitly here if necesary. */
			if (try_catch.CanContinue())
				return try_catch.ReThrow();
			v8::V8::TerminateExecution();

			free((void*) buf);
			buf = NULL;

			return v8::Undefined();
		}
	} else {
		LOGE("Cannot find file %s", *basename);
		log(LOG_ERROR, args);
		return v8::Undefined();
	}

	// success! copy changes back onto the sandbox object.
	CloneObject(args.This(), context->Global()->GetPrototype(), sandbox);
	sandbox->Set(dirNameStr, String::New(basePath.c_str()));
	// Clean up, clean up, everybody everywhere!
	context->DetachGlobal();
	context->Exit();
	context.Dispose();
	if (buf) {
		free((void*) buf);
		buf = NULL;
	}

#ifdef INTERNAL_REQUIRE_CACHE
	Persistent<Value> cacheObj = Persistent<Value>::New(result);
	_requireCache[baseNameStr] = *cacheObj;
#endif

	return scope.Close(result);
}

static Handle<Value> RequireCallback(const Arguments& args) {

	BGJSContext *ctx = BGJSInfo::_jscontext;

	return (ctx->require(args));
}

static Handle<Value> NormalizePathCallback(const Arguments& args) {

	BGJSContext *ctx = BGJSInfo::_jscontext;

	return (ctx->normalizePath(args));
}

void BGJSContext::setClient(ClientAbstract* client) {
	this->_client = client;
}

ClientAbstract* BGJSContext::getClient() {
	return this->_client;
}

void BGJSContext::cancelAnimationFrame(int id) {
#ifdef DEBUG
	LOGD("Canceling animation frame %i", id);
#endif
	for (std::set<BGJSGLView*>::iterator it = _glViews.begin();
			it != _glViews.end(); ++it) {
		int i = 0;
		while (i < MAX_FRAME_REQUESTS) {
			AnimationFrameRequest *request = &((*it)->_frameRequests[i]);
			if (request->requestId == id && request->valid
					&& request->view != NULL) {
				request->valid = false;
				request->callback.Dispose();
#ifdef DEBUG
				LOGD("cancelAnimationFrame cancelled id %d", request->requestId);
#endif
				break;
			}
			i++;
		}
	}
}

void BGJSContext::registerGLView(BGJSGLView* view) {
	_glViews.insert(view);
}

void BGJSContext::unregisterGLView(BGJSGLView* view) {
	_glViews.erase(view);
}

bool BGJSContext::runAnimationRequests(BGJSGLView* view) {
	v8::Locker l;
	HandleScope scope;

	Context::Scope context_scope(BGJSContext::_context);

	TryCatch trycatch;
	bool didDraw = false;

	AnimationFrameRequest *request;
	int index = view->_firstFrameRequest, nextIndex = view->_nextFrameRequest,
			startFrame = view->_firstFrameRequest;
#ifdef DEBUG
	LOGD("runAnimation %d to %d", view->_firstFrameRequest, view->_nextFrameRequest);
#endif
	while (index != nextIndex) {
		request = &(view->_frameRequests[index]);

		if (request->valid) {
			didDraw = true;
			request->view->prepareRedraw();
			Handle<Value> args[0];
			Handle<Value> result = request->callback->CallAsFunction(
					_context->Global(), 0, args);

			if (result.IsEmpty()) {
				LOGE("Exception occured while running runAnimationRequest cb");
				BGJSContext::ReportException(&trycatch);
			}

			String::Utf8Value fnName(request->callback->ToString());
#ifdef DEBUG
			LOGD("runAnimation number %d %s", index, *fnName);
#endif
			request->callback.Dispose();

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
		view->call(view->_cbRedraw);
	}
	return didDraw;
}

Handle<Value> BGJSContext::js_global_getLocale(Local<String> property,
		const AccessorInfo &info) {
	HandleScope scope;
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (ctx->_jscontext->_locale) {
		// LOGD("Returning locale %s", ctx->_jscontext->_locale);
		return scope.Close(String::New(ctx->_jscontext->_locale));
	} else {
		return v8::Null();
	}
}

void BGJSContext::js_global_setLocale(Local<String> property,
		Local<Value> value, const AccessorInfo &info) {

}

Handle<Value> BGJSContext::js_global_getLang(Local<String> property,
		const AccessorInfo &info) {
	HandleScope scope;
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (ctx->_jscontext->_locale) {
		// LOGD("Returning lang %s", ctx->_jscontext->_lang);
		return scope.Close(String::New(ctx->_jscontext->_lang));
	} else {
		return v8::Null();
	}
}

void BGJSContext::js_global_setLang(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {

}

Handle<Value> BGJSContext::js_global_getTz(Local<String> property,
		const AccessorInfo &info) {
	HandleScope scope;
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (ctx->_jscontext->_locale) {
		// LOGD("Returning tz %s", ctx->_jscontext->_tz);
		return scope.Close(String::New(ctx->_jscontext->_tz));
	} else {
		return v8::Null();
	}
}

void BGJSContext::js_global_setTz(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {

}

Handle<Value> BGJSContext::js_global_requestAnimationFrame(
		const Arguments& args) {
	v8::Locker l;
	HandleScope scope;
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (args.Length() >= 2 && args[0]->IsFunction() && args[1]->IsObject()) {
		Persistent<Object> func = Persistent<Object>::New(args[0]->ToObject());
		Handle<Object> objRef = args[1]->ToObject();
		BGJSGLView* view = static_cast<BGJSGLView *>(External::Unwrap(
				objRef->GetInternalField(0)));
		if (func->IsFunction()) {
			int id = view->requestAnimationFrameForView(func,
					(ctx->_nextTimerId)++);
			return scope.Close(Integer::New(id));
		}
	} else {
		LOGI("requestAnimationFrame: Wrong number or type of parameters");
		return scope.Close(Integer::New(-1));
		// return v8::ThrowException(v8::Exception::ReferenceError(v8::String::New("Wrong number of parameters")));
	}
	return scope.Close(Integer::New(-1));
}

Handle<Value> BGJSContext::js_global_cancelAnimationFrame(
		const Arguments& args) {
	v8::Locker l;
	HandleScope scope;
	if (args.Length() >= 1 && args[0]->IsNumber()) {
		BGJSContext *ctx = BGJSInfo::_jscontext;
		int id = (int) (Local<Number>::Cast(args[0])->Value());
		ctx->cancelAnimationFrame(id);
	}

	return v8::Undefined();
}

Handle<Value> BGJSContext::js_global_setTimeout(const Arguments& args) {
	v8::Locker l;
	HandleScope scope;
	return scope.Close(setTimeoutInt(args, false));
}

Handle<Value> BGJSContext::js_global_setInterval(const Arguments& args) {
	v8::Locker l;
	HandleScope scope;
	return scope.Close(setTimeoutInt(args, true));
}

Handle<Value> BGJSContext::setTimeoutInt(const Arguments& args,
		bool recurring) {
	HandleScope scope;
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (args.Length() == 2 && args[0]->IsFunction() && args[1]->IsNumber()) {
		Local<v8::Function> callback = Local<Function>::Cast(args[0]);
		Persistent<Object> thisObj = Persistent<Object>::New(args.This());

		WrapPersistentFunc* ws = new WrapPersistentFunc();
		ws->callbackFunc = Persistent<Function>::New(callback);
		WrapPersistentObj* wo = new WrapPersistentObj();
		wo->obj = Persistent<Object>::New(thisObj);

		jlong timeout = (jlong)(Local<Number>::Cast(args[1])->Value());

		ClientAndroid* client = (ClientAndroid*) (BGJSInfo::_jscontext->_client);
		JNIEnv* env = JNU_GetEnv();
		if (env == NULL) {
			LOGE("Cannot execute setTimeout with no envCache");
			return v8::Undefined();
		}

		jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
		jmethodID pushMethod = env->GetStaticMethodID(clazz, "setTimeout",
				"(JJJZ)I");
		assert(pushMethod);
		assert(clazz);
		int subId = env->CallStaticIntMethod(clazz, pushMethod, (jlong) ws,
				(jlong) wo, timeout, (jboolean) recurring);
#ifdef DEBUG
		LOGI("SetTimeout gave id %i", subId);
#endif
		return scope.Close(Integer::New(subId));
	} else {
		return v8::ThrowException(
				v8::Exception::ReferenceError(
						v8::String::New("Wrong number of parameters")));
	}
	return scope.Close(Integer::New(-1));
}

Handle<Value> BGJSContext::js_global_clearInterval(const Arguments& args) {
	clearTimeoutInt(args);
	return v8::Undefined();
}

Handle<Value> BGJSContext::js_global_clearTimeout(const Arguments& args) {
	clearTimeoutInt(args);
	return v8::Undefined();
}

void BGJSContext::clearTimeoutInt(const Arguments& args) {
	v8::Locker l;
	HandleScope scope;
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (args.Length() == 1) {

		int id = (int) ((Local<Integer>::Cast(args[0]))->Value());

		if (id == 0) {
			return;
		}

#ifdef DEBUG
		LOGI("clearTimeout for timeout %u", id);
#endif

		ClientAndroid* client = (ClientAndroid*) (BGJSInfo::_jscontext->_client);
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
		LOGE("Wrong arguments for clearTimeout");
	}
}

Persistent<Script> BGJSContext::load(const char* path) {
	v8::Locker l;
	// Create a stack-allocated handle scope.
	HandleScope handle_scope;
	v8::TryCatch try_catch;

	Context::Scope context_scope(BGJSContext::_context);

	const char* buf = this->_client->loadFile(path);

	// Create a string containing the JavaScript source code.
	Handle<String> source = String::New(buf);

	// Compile the source code.
	Handle<Script> scr = Script::Compile(source);
	Persistent<Script> pers = Persistent<Script>::New(scr);

	if (scr.IsEmpty()) {
		LOGE("Error compiling script %s\n", buf);
		free((void*) buf);
		// Print errors that happened during compilation.
		BGJSContext::ReportException(&try_catch);
	}

#ifdef DEBUG
	LOGI("Compiled\n");
#endif
	free((void*) buf);
	return pers;
}

BGJSContext::BGJSContext() {
	_client = NULL;
	_nextTimerId = 1;
	_locale = NULL;

	v8::Locker l;
	// Create a stack-allocated handle scope.
	v8::HandleScope handle_scope;

	// Create a template for the global object where we set the
	// built-in global functions.
	v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

	// Add methods to console function
	v8::Local<v8::FunctionTemplate> console = v8::FunctionTemplate::New();
	console->Set("log", v8::FunctionTemplate::New(LogCallback));
	console->Set("debug", v8::FunctionTemplate::New(DebugCallback));
	console->Set("info", v8::FunctionTemplate::New(InfoCallback));
	console->Set("error", v8::FunctionTemplate::New(ErrorCallback));
	console->Set("warn", v8::FunctionTemplate::New(ErrorCallback));
	// console->Set("assert", v8::FunctionTemplate::New(AssertCallback)); // TODO
	// Set the created FunctionTemplate as console in global object
	global->Set(v8::String::New("console"), console);
	// Set the int_require function in the global object
	global->Set("int_require", v8::FunctionTemplate::New(RequireCallback));
	global->Set("int_normalizePath", v8::FunctionTemplate::New(NormalizePathCallback));

	global->SetAccessor(String::New("_locale"), BGJSContext::js_global_getLocale,
			BGJSContext::js_global_setLocale);
	global->SetAccessor(String::New("_lang"), BGJSContext::js_global_getLang,
			BGJSContext::js_global_setLang);
	global->SetAccessor(String::New("_tz"), BGJSContext::js_global_getTz,
			BGJSContext::js_global_setTz);

	global->Set(String::New("requestAnimationFrame"),
			v8::FunctionTemplate::New(
					BGJSContext::js_global_requestAnimationFrame));
	global->Set(String::New("cancelAnimationFrame"),
			v8::FunctionTemplate::New(
					BGJSContext::js_global_cancelAnimationFrame));
	global->Set(String::New("setTimeout"),
			v8::FunctionTemplate::New(BGJSContext::js_global_setTimeout));
	global->Set(String::New("setInterval"),
			v8::FunctionTemplate::New(BGJSContext::js_global_setInterval));
	global->Set(String::New("clearTimeout"),
			v8::FunctionTemplate::New(BGJSContext::js_global_clearTimeout));
	global->Set(String::New("clearInterval"),
			v8::FunctionTemplate::New(BGJSContext::js_global_clearInterval));

	// Also, persist the global object template so we can add stuff here later when calling require
	BGJSInfo::_global = v8::Persistent<v8::ObjectTemplate>::New(global);

	LOGD("v8 version %s", V8::GetVersion());
}

void BGJSContext::createContext() {

#ifdef ENABLE_DEBUGGER_SUPPORT
	int port_number = 1337;
	bool wait_for_connection = true;
	bool support_callback = true;
#endif  // ENABLE_DEBUGGER_SUPPORT
	v8::Locker l;
	// Create a stack-allocated handle scope.
	v8::HandleScope handle_scope;
	// Create a new context.
	BGJSInfo::_context = v8::Context::New(NULL, BGJSInfo::_global);
	BGJSInfo::_jscontext = this;

#ifdef ENABLE_DEBUGGER_SUPPORT
	debug_message_context = v8::Persistent<v8::Context>::New(BGJSInfo::_context);

	v8::Locker locker;

	if (support_callback) {
		v8::Debug::SetDebugMessageDispatchHandler(DispatchDebugMessages, true);
	}

	if (port_number != -1) {
		v8::Debug::EnableAgent("v8Engine", port_number, wait_for_connection);
	}
#endif  // ENABLE_DEBUGGER_SUPPORT
}

void BGJSContext::ReportException(v8::TryCatch* try_catch) {
	v8::HandleScope handle_scope;
	v8::String::Utf8Value exception(try_catch->Exception());
	const char* exception_string = ToCString(exception);
	v8::Handle<v8::Message> message = try_catch->Message();

/* #ifdef ANDROID
	jstring dataStr, excStr, methodStr;
	ClientAndroid* client = (ClientAndroid*)(_bgjscontext->_client);
	JNIEnv* env = JNU_GetEnv();
	if (env != NULL) {
		LOGE("Cannot log to Java with no envCache");
	} else {

		jclass clazz = env->FindClass("ag/boersego/bgjs/V8Engine");
		jmethodID ajaxMethod = env->GetStaticMethodID(clazz,
				"logException", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
		assert(ajaxMethod);
		assert(clazz);
	}
	env->CallStaticVoidMethod(clazz, ajaxMethod, urlStr, dataStr, methodStr, (jboolean)processData);

#endif */

	if (message.IsEmpty()) {
		// V8 didn't provide any extra information about this error; just
		// print the exception.
		LOGI("Exception: %s", exception_string);
	} else {
		// Print (filename):(line number): (message).
		v8::String::Utf8Value filename(message->GetScriptResourceName());
		const char* filename_string = ToCString(filename);
		int linenum = message->GetLineNumber();
		LOGI("Exception: %s:%i: %s\n", filename_string, linenum, exception_string);
		// Print line of source code.
		v8::String::Utf8Value sourceline(message->GetSourceLine());
		const char* sourceline_string = ToCString(sourceline);
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
		v8::String::Utf8Value stack_trace(try_catch->StackTrace());
		if (stack_trace.length() > 0) {
			const char* stack_trace_string = ToCString(stack_trace);
			LOGI("%s", stack_trace_string);
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

void BGJSContext::log(int debugLevel, const Arguments& args) {
	v8::Locker locker;
	HandleScope scope;

	std::stringstream str;
	int l = args.Length();
	for (int i = 0; i < l; i++) {
		String::Utf8Value value(args[i]);
		str << " " << ToCString(value);
	}

	LOG(debugLevel, "%s", str.str().c_str());
}

int BGJSContext::run() {
	v8::Locker l;
	if (_script.IsEmpty()) {
		LOGE("run called when no valid script loaded");
	}
	// Create a stack-allocated handle scope.
	HandleScope handle_scope;
	v8::TryCatch try_catch;
	Context::Scope context_scope(BGJSInfo::_context);

	// Run the android.js file
	Persistent<Script> res = load("js/android.js");
	if (res.IsEmpty()) {
		LOGE("Cannot find android.js");
		return 0;
	}
	Handle<Value> result = res->Run();
	res.Dispose();

	if (result.IsEmpty()) {
		// Print errors that happened during execution.
		ReportException(&try_catch);
		return false;
	}

	// Run the script to get the result.
	result = _script->Run();

	if (result.IsEmpty()) {
		// Print errors that happened during execution.
		ReportException(&try_catch);
		return false;
	}

	_nextTimerId = 1;

	// BGJSInfo::_context->Exit();

	return 1;
}

void BGJSContext::setLocale(const char* locale, const char* lang,
		const char* tz) {
	_locale = strdup(locale);
	_lang = strdup(lang);
	_tz = strdup(tz);
}

BGJSContext::~BGJSContext() {
	LOGI("Cleaning up");
	if (!_global.IsEmpty()) {
		_global.Dispose();
	}
	if (!_script.IsEmpty()) {
		_script.Dispose();
	}
	if (!cloneObjectMethod.IsEmpty()) {
		cloneObjectMethod.Dispose();
	}
	if (!jsonParseMethod.IsEmpty()) {
		jsonParseMethod.Dispose();
	}

	// Dispose the persistent context.
	_context.Dispose();

	if (_locale) {
		free(_locale);
	}
}

