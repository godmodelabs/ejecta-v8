/**
 * BGJSContext
 * Manages a v8 context and exposes script load and execute functions
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
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

v8::Persistent<v8::Context>* BGJSInfo::_context;
v8::Eternal<v8::ObjectTemplate> BGJSInfo::_global;
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

static void LogCallback(const v8::FunctionCallbackInfo<Value>& args) {
	if (args.Length() < 1) {
		return;
	}

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->log(LOG_INFO, args);
}

static void DebugCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSContext *ctx = BGJSInfo::_jscontext;
	ctx->log(LOG_DEBUG, args);
}

static void InfoCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->log(LOG_INFO, args);
}

static void ErrorCallback(const v8::FunctionCallbackInfo<Value>& args) {
    args.GetReturnValue().SetUndefined();
	if (args.Length() < 1) {
		return;
	}

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->log(LOG_ERROR, args);
}

void LogStackTrace(Local<StackTrace>& str, Isolate* isolate) {
	HandleScope scope (isolate);
	int count = str->GetFrameCount();
	for (int i = 0; i < count; i++) {
		Local<StackFrame> frame = str->GetFrame(i);
		String::Utf8Value fnName(frame->GetFunctionName());
		String::Utf8Value scriptName(frame->GetScriptName());
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
		void (*requireFn)(v8::Isolate* isolate, v8::Handle<v8::Object> target)) {
	// v8::Locker l(Isolate::GetCurrent());
	// HandleScope scope(Isolate::GetCurrent());
	// module->initWithContext(this);
	_modules[name] = requireFn;
	// _modules.insert(std::pair<char const*, BGJSModule*>(module->getName(), module));

#ifdef DEBUG
	LOGI(
			"register %s %p %p %p %i", name, requireFn, _modules[name], _modules["ajax"], _modules.size());
#endif
	return true;
}

template<class T>
inline Local<T> ToLocal(Persistent<T>* p_) {
    return *reinterpret_cast<Local<T>*>(p_);
}

//////////////////////////
// Require

void BGJSContext::CloneObject(Handle<Object> recv, Handle<Value> source,
		Handle<Value> target) {
	HandleScope scope(Isolate::GetCurrent());

	Handle<Value> args[] = { source, target };
    const char* cloneScript = "(function(source, target) {\n\
                                         Object.getOwnPropertyNames(source).forEach(function(key) {\n\
                                         try {\n\
                                           var desc = Object.getOwnPropertyDescriptor(source, key);\n\
                                           if (desc.value === source) desc.value = target;\n\
                                           Object.defineProperty(target, key, desc);\n\
                                         } catch (e) {\n\
                                          // Catch sealed properties errors\n\
                                         }\n\
                                       });\n\
                                      })";

	// Init
	if (this->cloneObjectMethod.IsEmpty()) {
		Local<Function> cloneObjectMethod_ =
				Local<Function>::Cast(
						Script::Compile(
								String::NewFromOneByte(Isolate::GetCurrent(),
								    (const uint8_t*)cloneScript),
								String::NewFromOneByte(Isolate::GetCurrent(), (const uint8_t*)"binding:script"))->Run());
		this->cloneObjectMethod.Reset(Isolate::GetCurrent(), cloneObjectMethod_);
	}

	Local<Function>::New(Isolate::GetCurrent(), this->cloneObjectMethod)->Call(recv, 2, args);
}

Handle<Value> BGJSContext::JsonParse(Handle<Object> recv,
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
		BGJSContext::ReportException(&trycatch);
		//abort();
	}

	return scope.Escape(result);
}

Handle<Value> BGJSContext::executeJS(const uint8_t* src) {
	v8::Locker l(Isolate::GetCurrent());
	EscapableHandleScope scope(Isolate::GetCurrent());
	TryCatch trycatch;

	Local<Value> result = Script::Compile(String::NewFromOneByte(Isolate::GetCurrent(), src),
			String::NewFromOneByte(Isolate::GetCurrent(), (uint8_t*)"binding:script"))->Run();
	if (result.IsEmpty()) {
		LOGE("executeJS exception");
		BGJSContext::ReportException(&trycatch);
		//abort();
	}
	return scope.Escape(result);
}

Handle<Value> BGJSContext::callFunction(Isolate* isolate, Handle<Object> recv, const char* name,
		int argc, Handle<Value> argv[]) const {
	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);
	TryCatch trycatch;

	Local<Function> fn = Handle<Function>::Cast(recv->Get(String::NewFromUtf8(isolate, name)));
			String::Utf8Value value(fn);
	
	Local<Value> result = fn->Call(recv, argc, argv);
	if (result.IsEmpty()) {
		LOGE("callFunction exception");
		BGJSContext::ReportException(&trycatch);
		return v8::Undefined(isolate);
	}
	return scope.Escape(result);
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
        #ifdef DEBUG
            LOGD("normalize_path step %i %s", i+1, pathPart.c_str());
        #endif
		// LOGD("normalize_path. Part %i = %s, %i", i + 1, pathPart.c_str(), pathPart.compare("."));
		if (pathPart.compare("..") != 0) {
			std::string nextSegment = pathParts.at(i);
			if (nextSegment.compare(".") == 0) {
                #ifdef DEBUG
                    LOGD("normalize_path skipping %i because is ..", i);
                #endif
				continue;
			}
			if (outPath.length() > 0) {
				outPath.append("/");
			}
			outPath.append(pathParts.at(i));
            #ifdef DEBUG
                LOGD("normalize_path outPath now %s", outPath.c_str());
            #endif
		} else {
			i++;
		}
	}
	outPath.append("/").append(pathParts.at(length - 1));
	return outPath;
}

void find_and_replace(std::string& source, std::string const& find, std::string const& replace)
{
    for(std::string::size_type i = 0; (i = source.find(find, i)) != std::string::npos;)
    {
        source.replace(i, find.length(), replace);
        i += replace.length();
    }
}

void BGJSContext::normalizePath(const v8::FunctionCallbackInfo<v8::Value>& args) {
	// Path in args is relative and may contain /./STH.js and ../FOLDER/STH.js
	// ie current folder and parent folder

	// This method will take the current working directory (from embedderdata or
	// in older V8 from Context->GetData() which were set by the last call to require())
	// and this relative path and return an absolute path
	if (args.Length() < 1) {
	    args.GetReturnValue().SetUndefined();
		return;
	}

	Isolate* isolate = Isolate::GetCurrent();

	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);

	// Get the relative path from the first argument
	Local<String> filename = args[0]->ToString();

	String::Utf8Value basename(filename);

	TryCatch try_catch;
	std::string baseNameStr = std::string(*basename);

	// Get absolute current working directory from current V8 context
	Local<Value> dirVal = isolate->GetEnteredContext()->GetEmbedderData(1);
	Local<String> dirName;
	if (dirVal->IsUndefined()) {
		// We don't have a cwd so assume /js
		dirName = String::NewFromUtf8(isolate, "js");
	} else {
		dirName = dirVal->ToString();
	}
	String::Utf8Value dirNameC(dirName);
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
		args.GetReturnValue().Set(scope.Escape(filename));
		return;
	}
	// Append cwd and input path
	std::string pathName;
	std::string pathTemp = std::string(*dirNameC).append("/").append(baseNameStr);
	// Remote /./ which is /
	find_and_replace(pathTemp, std::string("/./"), std::string("/"));
#ifdef DEBUG
	LOGD("Pathtemp %s", pathTemp.c_str());
#endif
	// Now flatten /../ into one absolute path
	pathName = normalize_path(pathTemp);
	Local<String> normalizedPath = String::NewFromUtf8(isolate, pathName.c_str());

	args.GetReturnValue().Set(scope.Escape(normalizedPath));

}

void BGJSContext::require(const v8::FunctionCallbackInfo<v8::Value>& args) {

	if (args.Length() < 1) {
	    args.GetReturnValue().SetUndefined();
		return;
	}

	Isolate* isolate = Isolate::GetCurrent();

	// LOGD("require. Current context %p", isolate->GetCurrentContext());

	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);

	// Source of JS file if external code
	Handle<String> source;
	const char* buf = 0;
	// Result of internal or external invocation
	Local<Value> result;
	Local<String> filename = args[0]->ToString();

	String::Utf8Value basename(filename);

	// Catch errors
	TryCatch try_catch;
	std::string baseNameStr = std::string(*basename);
	bool isJson = false;
	Local<Object> sandbox = args[1]->ToObject();
	Handle<String> dirNameStr = String::NewFromOneByte(isolate, (const uint8_t*)"__dirname");

	Local<Value> dirVal = isolate->GetEnteredContext()->GetEmbedderData(1);
	Local<String> dirName;
	if (dirVal->IsUndefined()) {
		dirName = String::NewFromUtf8(isolate, "js");
	} else {
		dirName = dirVal->ToString();
	}
	String::Utf8Value dirNameC(dirName);
#ifdef DEBUG
	LOGD("Dirname %s", *dirNameC);
#endif

	// Check if this is an internal module
	requireHook module = _modules[baseNameStr];

#ifdef DEBUG
	LOGI("require %s %p %i", *basename, module, _modules.size());
#endif

	if (module) {
		Local<Object> exports = Object::New(isolate);
		module(isolate, exports);
		CloneObject(args.This(), exports, sandbox);
		args.GetReturnValue().Set(scope.Escape(exports));
		return;
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
		source = String::NewFromUtf8(isolate, buf);
		Handle<Object> res = BGJSContext::JsonParse(
				isolate->GetEnteredContext()->Global(), source)->ToObject();
		Handle<String> mainStr = String::NewFromUtf8(isolate, (const char*)"main");
		if (res->Has(mainStr)) {
			Handle<String> jsFileName = res->Get(mainStr)->ToString();
			String::Utf8Value jsFileNameC(jsFileName);
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
			args.GetReturnValue().SetUndefined();
			return;
		}
		// Create a string containing the JSON source
		source = String::NewFromUtf8(isolate, buf);
		Local<Value> res = BGJSContext::JsonParse(
				isolate->GetEnteredContext()->Global(), source);
		// CloneObject(args.This(), res, BGJSContext::_context->Global());
		if (buf) {
			free((void*) buf);
			buf = NULL;
		}
		args.GetReturnValue().Set(scope.Escape(res));
		return;
	}

    Local<Context> context = Context::New(isolate, NULL, Local<ObjectTemplate>::New(isolate, BGJSInfo::_global.Get(isolate)));
    Persistent<Context, CopyablePersistentTraits<Value> > contextPersistent(isolate, context);
    BGJS_NEW_PERSISTENT(contextPersistent);
    context->SetEmbedderData(1, String::NewFromUtf8(isolate, basePath.c_str()));

	Local<Array> keys;
	// Enter the context
	{
		Context::Scope embeddedContext(context);

		sandbox->Set(dirNameStr, String::NewFromUtf8(isolate, basePath.c_str()));
	#ifdef DEBUG
		LOGD("New __dirname %s", basePath.c_str());
	#endif

		// Copy everything from the passed in sandbox (either the persistent
		// context for runInContext(), or the sandbox arg to runInNewContext()).
		CloneObject(args.This(), sandbox, context->Global()->GetPrototype());

		context->Global()->GetPrototype()->ToObject()->Set(dirNameStr, String::NewFromUtf8(isolate, basePath.c_str()));

		if (buf) {
			// Create a string containing the JavaScript source code.
			source = String::NewFromUtf8(isolate, buf);

			// well, here WrappedScript::New would suffice in all cases, but maybe
			// Compile has a little better performance where possible
			Handle<Script> scriptR = Script::Compile(source, filename);
			if (scriptR.IsEmpty()) {

				context->DetachGlobal();

				// FIXME UGLY HACK TO DISPLAY SYNTAX ERRORS.
				BGJSContext::ReportException(&try_catch);

				free((void*) buf);
				buf = NULL;

				// Hack because I can't get a proper stacktrace on SyntaxError
				args.GetReturnValue().Set(scope.Escape(try_catch.Exception()));
				return;
			}

			result = scriptR->Run();
			if (result.IsEmpty()) {
				context->DetachGlobal();
				BGJS_CLEAR_PERSISTENT(contextPersistent);
				/* ReThrow doesn't re-throw TerminationExceptions; a null exception value
				 * is re-thrown instead (see Isolate::PropagatePendingExceptionToExternalTryCatch())
				 * so we re-propagate a TerminationException explicitly here if necesary. */
				if (try_catch.CanContinue())
					return;
				v8::V8::TerminateExecution(isolate);

				free((void*) buf);
				buf = NULL;

	            args.GetReturnValue().SetUndefined();
				return;
			}
		} else {
			LOGE("Cannot find file %s", *basename);
			log(LOG_ERROR, args);
			context->DetachGlobal();
			BGJS_CLEAR_PERSISTENT(contextPersistent);
			args.GetReturnValue().SetUndefined();
			return;
		}

		// success! copy changes back onto the sandbox object.
		CloneObject(args.This(), context->Global()->GetPrototype(), sandbox);
		sandbox->Set(dirNameStr, String::NewFromUtf8(isolate, basePath.c_str()));
		// Clean up, clean up, everybody everywhere!
	}
	context->DetachGlobal();
	BGJS_CLEAR_PERSISTENT(contextPersistent);
	if (buf) {
		free((void*) buf);
		buf = NULL;
	}

#ifdef INTERNAL_REQUIRE_CACHE
	Persistent<Value>* cacheObj = new Persistent<Value>::Persistent(isolate, result);
	BGJS_NEW_PERSISTENT_PTR(cacheObj);
	_requireCache[baseNameStr] = *acheObj;
#endif
    args.GetReturnValue().Set(scope.Escape(result));
	return;
}

static void RequireCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->require(args);
}

static void NormalizePathCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {

	BGJSContext *ctx = BGJSInfo::_jscontext;

	ctx->normalizePath(args);
}

void BGJSContext::setClient(ClientAbstract* client) {
	this->_client = client;
}

ClientAbstract* BGJSContext::getClient() const {
    return this->_client;
}

v8::Isolate* BGJSContext::getIsolate() const {
    return this->_isolate;
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
				BGJS_CLEAR_PERSISTENT(request->callback);
				BGJS_CLEAR_PERSISTENT(request->thisObj);
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

bool BGJSContext::runAnimationRequests(BGJSGLView* view) const  {
	v8::Locker l(_isolate);
    Isolate::Scope isolateScope(_isolate);
	HandleScope scope(_isolate);

	// Context::Scope context_scope(BGJSContext::_context.Get(_isolate));

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
			// LOGD("BGJSC runAnimation call");
			Handle<Value> result = Local<Object>::New(_isolate, request->callback)->CallAsFunction(
					Local<Object>::New(_isolate, request->thisObj), 0, args);

			if (result.IsEmpty()) {
				LOGE("Exception occured while running runAnimationRequest cb");
				BGJSContext::ReportException(&trycatch);
			}

			String::Utf8Value fnName(Local<Object>::New(_isolate, request->callback)->ToString());
#ifdef DEBUG
			LOGD("runAnimation number %d %s", index, *fnName);
#endif
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

void BGJSContext::js_global_getLocale(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (ctx->_jscontext->_locale) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_jscontext->_locale)));
	} else {
	    info.GetReturnValue().SetNull();
	}
}

void BGJSContext::js_global_setLocale(Local<String> property,
		Local<Value> value, const v8::PropertyCallbackInfo<void>& info) {

}

void BGJSContext::js_global_getLang(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (ctx->_jscontext->_lang) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_jscontext->_lang)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSContext::js_global_setLang(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {

}

void BGJSContext::js_global_getTz(Local<String> property,
		const v8::PropertyCallbackInfo<v8::Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	BGJSContext *ctx = BGJSInfo::_jscontext;

	if (ctx->_jscontext->_tz) {
		info.GetReturnValue().Set(scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ctx->_jscontext->_tz)));
	} else {
		info.GetReturnValue().SetNull();
	}
}

void BGJSContext::js_global_setTz(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {

}

void BGJSContext::js_global_requestAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSContext *ctx = BGJSInfo::_jscontext;
	v8::Locker l(args.GetIsolate());
	HandleScope scope(args.GetIsolate());


	if (args.Length() >= 2 && args[0]->IsFunction() && args[1]->IsObject()) {
	    Local<Object> localFunc = args[0]->ToObject();
		Handle<Object> objRef = args[1]->ToObject();
		BGJSGLView* view = static_cast<BGJSGLView *>(v8::External::Cast(*(objRef->GetInternalField(0)))->Value());
		if (localFunc->IsFunction()) {
			#ifdef DEBUG
				LOGD("requestAnimationFrame: on BGJSGLView %p", view);
			#endif
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
		// return v8::ThrowException(v8::Exception::ReferenceError(v8::String::New("Wrong number of parameters")));
	}
	args.GetReturnValue().Set(-1);
}

void BGJSContext::js_global_cancelAnimationFrame(
		const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSContext *ctx = BGJSInfo::_jscontext;
	v8::Locker l(ctx->getIsolate());
    HandleScope scope(ctx->getIsolate());
	if (args.Length() >= 1 && args[0]->IsNumber()) {

		int id = (int) (Local<Number>::Cast(args[0])->Value());
		ctx->cancelAnimationFrame(id);
	}

	args.GetReturnValue().SetUndefined();
}

void BGJSContext::js_global_setTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
	setTimeoutInt(args, false);
}

void BGJSContext::js_global_setInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
	setTimeoutInt(args, true);
}

void BGJSContext::setTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& args,
		bool recurring) {
    BGJSContext *ctx = BGJSInfo::_jscontext;
	v8::Locker l(args.GetIsolate());
	HandleScope scope(args.GetIsolate());


	if (args.Length() == 2 && args[0]->IsFunction() && args[1]->IsNumber()) {
		Local<v8::Function> callback = Local<Function>::Cast(args[0]);

		WrapPersistentFunc* ws = new WrapPersistentFunc();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), ws->callbackFunc, callback);
		WrapPersistentObj* wo = new WrapPersistentObj();
		BGJS_RESET_PERSISTENT(ctx->getIsolate(), wo->obj, args.This());

		jlong timeout = (jlong)(Local<Number>::Cast(args[1])->Value());

		ClientAndroid* client = (ClientAndroid*) (BGJSInfo::_jscontext->_client);
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
#ifdef DEBUG
		LOGI("SetTimeout gave id %i", subId);
#endif
        args.GetReturnValue().Set(subId);
	} else {
        ctx->getIsolate()->ThrowException(
				v8::Exception::ReferenceError(
						v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong number of parameters")));
	}
	return;
}

void BGJSContext::js_global_clearInterval(const v8::FunctionCallbackInfo<v8::Value>& args) {
	clearTimeoutInt(args);
}

void BGJSContext::js_global_clearTimeout(const v8::FunctionCallbackInfo<v8::Value>& args) {
	clearTimeoutInt(args);
}

void BGJSContext::clearTimeoutInt(const v8::FunctionCallbackInfo<v8::Value>& args) {
    BGJSContext *ctx = BGJSInfo::_jscontext;
	v8::Locker l(ctx->getIsolate());
    HandleScope scope(ctx->getIsolate());

	args.GetReturnValue().SetUndefined();

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
        ctx->getIsolate()->ThrowException(
    				v8::Exception::ReferenceError(
    						v8::String::NewFromUtf8(ctx->getIsolate(), "Wrong arguments for clearTimeout")));
		LOGE("Wrong arguments for clearTimeout");
	}
}

Persistent<Script, CopyablePersistentTraits<Script> > BGJSContext::load(const char* path) {
    Isolate *isolate = this->_isolate;
    LOGI("Load: got Isolate %p", isolate);
	v8::Locker l(isolate);
	Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);
	v8::TryCatch try_catch;

	const char* buf = this->_client->loadFile(path);

	// Create a string containing the JavaScript source code.
	Handle<String> source = String::NewFromUtf8(isolate, buf);

	// Compile the source code.
	Handle<Script> scr = Script::Compile(source);
	Persistent<Script, CopyablePersistentTraits<Script> > pers(isolate, scr);

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

BGJSContext::BGJSContext(v8::Isolate* isolate) {
	_client = NULL;
	_nextTimerId = 1;
	_locale = NULL;

    // Create default isolate just to be sure.
    this->_isolate = isolate;
    /* if (!isolate) {
        Isolate::CreateParams create_params;
        isolate = Isolate::New(create_params);
        isolate->Enter();
    } */

	v8::Locker l(isolate);
	// Create a stack-allocated handle scope.
	HandleScope scope(isolate);

	// Create a template for the global object where we set the
	// built-in global functions.
	v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New();

	// Add methods to console function
	v8::Local<v8::FunctionTemplate> console = v8::FunctionTemplate::New(isolate);
	console->Set(String::NewFromUtf8(isolate, "log"), v8::FunctionTemplate::New(isolate, LogCallback));
	console->Set(String::NewFromUtf8(isolate, "debug"), v8::FunctionTemplate::New(isolate, DebugCallback));
	console->Set(String::NewFromUtf8(isolate, "info"), v8::FunctionTemplate::New(isolate, InfoCallback));
	console->Set(String::NewFromUtf8(isolate, "error"), v8::FunctionTemplate::New(isolate, ErrorCallback));
	console->Set(String::NewFromUtf8(isolate, "warn"), v8::FunctionTemplate::New(isolate, ErrorCallback));
	// console->Set("assert", v8::FunctionTemplate::New(AssertCallback)); // TODO
	// Set the created FunctionTemplate as console in global object
	global->Set(v8::String::NewFromUtf8(isolate, "console"), console);
	// Set the int_require function in the global object
	global->Set(String::NewFromUtf8(isolate, "int_require"), v8::FunctionTemplate::New(isolate, RequireCallback));
	global->Set(String::NewFromUtf8(isolate, "int_normalizePath"), v8::FunctionTemplate::New(isolate, NormalizePathCallback));

	global->SetAccessor(String::NewFromUtf8(isolate, "_locale"), BGJSContext::js_global_getLocale,
			BGJSContext::js_global_setLocale);
	global->SetAccessor(String::NewFromUtf8(isolate, "_lang"), BGJSContext::js_global_getLang,
			BGJSContext::js_global_setLang);
	global->SetAccessor(String::NewFromUtf8(isolate, "_tz"), BGJSContext::js_global_getTz,
			BGJSContext::js_global_setTz);

	global->Set(String::NewFromUtf8(isolate, "requestAnimationFrame"),
			v8::FunctionTemplate::New(isolate,
					BGJSContext::js_global_requestAnimationFrame));
	global->Set(String::NewFromUtf8(isolate, "cancelAnimationFrame"),
			v8::FunctionTemplate::New(isolate,
					BGJSContext::js_global_cancelAnimationFrame));
	global->Set(String::NewFromUtf8(isolate, "setTimeout"),
			v8::FunctionTemplate::New(isolate, BGJSContext::js_global_setTimeout));
	global->Set(String::NewFromUtf8(isolate, "setInterval"),
			v8::FunctionTemplate::New(isolate, BGJSContext::js_global_setInterval));
	global->Set(String::NewFromUtf8(isolate, "clearTimeout"),
			v8::FunctionTemplate::New(isolate, BGJSContext::js_global_clearTimeout));
	global->Set(String::NewFromUtf8(isolate, "clearInterval"),
			v8::FunctionTemplate::New(isolate, BGJSContext::js_global_clearInterval));

	// Also, persist the global object template so we can add stuff here later when calling require
	BGJSInfo::_global.Set(isolate, global);

	LOGD("v8 version %s", V8::GetVersion());
}

void BGJSContext::createContext() {

#ifdef ENABLE_DEBUGGER_SUPPORT
	int port_number = 1337;
	bool wait_for_connection = true;
	bool support_callback = true;
#endif  // ENABLE_DEBUGGER_SUPPORT
    Isolate::Scope isolate_scope(_isolate);
	v8::Locker l(_isolate);
    // Create a stack-allocated handle scope.
    HandleScope scope(_isolate);
	// Create a new context.
    Local<Context> context = v8::Context::New(_isolate, NULL,
                                        Local<ObjectTemplate>::New(_isolate, BGJSInfo::_global.Get(_isolate)));
    LOGI("createContext v8context is %p, BGJSContext is %p", context, this);
	BGJSInfo::_context = new Persistent<Context>(_isolate, context);
	BGJSInfo::_jscontext = this;

#ifdef ENABLE_DEBUGGER_SUPPORT
	debug_message_context = v8::Persistent<v8::Context>::Persistent(isolate, BGJSInfo::_context);

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
	HandleScope scope(Isolate::GetCurrent());
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

void BGJSContext::log(int debugLevel, const v8::FunctionCallbackInfo<v8::Value>& args) {
	v8::Locker locker(args.GetIsolate());
	HandleScope scope(args.GetIsolate());

	std::stringstream str;
	int l = args.Length();
	for (int i = 0; i < l; i++) {
		String::Utf8Value value(args[i]);
		str << " " << ToCString(value);
	}

	LOG(debugLevel, "%s", str.str().c_str());
}

int BGJSContext::run() {
    Isolate::Scope isolate_scope(_isolate);
	v8::Locker locker(this->_isolate);
	if (_script.IsEmpty()) {
		LOGE("run called when no valid script loaded");
	}
	// Create a stack-allocated handle scope.
	HandleScope scope(this->_isolate);
	v8::TryCatch try_catch;
    Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(BGJSContext::_context));
    // context->Enter();

	// Run the android.js file
	v8::Persistent<v8::Script, v8::CopyablePersistentTraits<v8::Script> > res = load("js/android.js");
	BGJS_NEW_PERSISTENT(res);
	if (res.IsEmpty()) {
		LOGE("Cannot find android.js");
		return 0;
	}

	Handle<Value> result = Local<Script>::New(this->_isolate, res)->Run();
	BGJS_CLEAR_PERSISTENT(res);

	if (result.IsEmpty()) {
		// Print errors that happened during execution.
		ReportException(&try_catch);
		return false;
	}
	// Run the script to get the result.
	result = Local<Script>::New(this->_isolate, _script)->Run();

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
	BGJS_CLEAR_PERSISTENT(_script);
	BGJS_CLEAR_PERSISTENT(cloneObjectMethod);

	if (_locale) {
		free(_locale);
	}
    this->_isolate->Exit();
}

