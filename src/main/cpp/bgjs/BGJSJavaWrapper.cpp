#include "BGJSContext.h"
#include "BGJSJavaWrapper.h"
#include "mallocdebug.h"

#include <math.h>
#include <assert.h>

using namespace v8;

/**
 * BGJSJavaWrapper
 * Helper class to wrap v8 objects in Java objects
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BGJSJavaWrapper"

#define DEBUG	1

BGJSJavaWrapper::BGJSJavaWrapper() {

}

BGJSJavaWrapper::~BGJSJavaWrapper () {
	cleanUp(JNU_GetEnv());
}

void BGJSJavaWrapper::cleanUp(JNIEnv* env) {
	if (_javaObject != NULL) {
#ifdef DEBUG
		LOGD("cleanUp. Cleaning up javaObj global ref %p", _javaObject);
#endif
		env->DeleteGlobalRef(_javaObject);
#ifdef DEBUG
		LOGD("cleanUp. Cleaned up javaObj global ref %p", _javaObject);
#endif
		_javaObject = NULL;
	}
#ifdef DEBUG
	LOGD("cleanUp. Cleaning up jsObject pers");
#endif
	BGJS_CLEAR_PERSISTENT(_jsObject);
#ifdef DEBUG
	LOGD("cleanUp. Cleaned up jsObject pers");
#endif
	for (std::vector<Persistent<Function>*>::iterator it = _v8FuncsPersisted.begin(); it != _v8FuncsPersisted.end(); ++it) {
#ifdef DEBUG
        LOGD("BGJSJavaWrapper cleanUp func callback %p", *it);
#endif
        Persistent<Function>* resetMe = *it;
		BGJS_CLEAR_PERSISTENT((*resetMe));
#ifdef DEBUG
		LOGD("BGJSJavaWrapper cleaned func callback %p", *it);
#endif
	}
	for (std::vector<Persistent<Object>*>::iterator it = _v8ObsPersisted.begin(); it != _v8ObsPersisted.end(); ++it) {
#ifdef DEBUG
        LOGD("BGJSJavaWrapper cleanUp obj callback %p", *it);
#endif
        Persistent<Object>* resetMe = *it;
		BGJS_CLEAR_PERSISTENT((*resetMe));
#ifdef DEBUG
		LOGD("BGJSJavaWrapper cleaned obj callback %p", *it);
#endif
	}
}
/*
extern "C" {
JNIEXPORT jlong JNICALL Java_ag_boersego_js_ClientAndroid_callJSPtr(
		JNIEnv * env, jobject obj, jstring fnName, jobject params);
}

JNIEXPORT jlong JNICALL Java_ag_boersego_js_ClientAndroid_callJSPtr(
		JNIEnv * env, jobject obj, jstring fnName, jobject params) {

} */

BGJSJavaWrapper::BGJSJavaWrapper (const BGJSContext* context, JNIEnv* env, jobject javaObject) {
	_context = context;
	_javaObject = env->NewGlobalRef(javaObject);
	_jStringClass = env->FindClass("java/lang/String");
}

int BGJSJavaWrapper::getArgCount (const char* argsSpec, const int argsStrLen) {
	int length = argsStrLen;

	for (int i = 0; i < argsStrLen; i++) {
		char spec = argsSpec[i];
		if (spec != '[') {
			length++;
		}
	}
	return length;
}

bool BGJSJavaWrapper::coerceArgToString (JNIEnv* env, jobject &object, jclass& clazz, const char *keyName, const char *valAsString) {
	if (env->ExceptionCheck()) {
		LOGI ("coerceArgToString: exception in JNI pending before");
		loC(env);
	}

	jfieldID fieldId = env->GetFieldID(clazz, keyName, "Ljava/lang/String;");
	if (fieldId) {
		jstring javaStr = env->NewStringUTF(valAsString);
		env->SetObjectField(object, fieldId, javaStr);
		env->DeleteLocalRef(javaStr);
		return true;
	}
	if (env->ExceptionCheck()) {
		env->ExceptionClear();
	}
	return false;
}

void BGJSJavaWrapper::loC (JNIEnv* env) {
	jthrowable e = env->ExceptionOccurred();
	if (e != NULL)
	{
		env->ExceptionClear();

		jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
		jstring estring = (jstring) env->CallObjectMethod(e, toString);

		jboolean isCopy;
		const char* message = env->GetStringUTFChars(estring, &isCopy);
		LOGI("Exception occurred: %s", message);
		env->ReleaseStringUTFChars(estring, message);
		env->DeleteLocalRef(estring);
	}
}

bool BGJSJavaWrapper::apply (Local<Object> &v8obj, jobject &object, jclass &clazz, const char* className, JNIEnv* env,
		char* errBuf, int errBufLen, char* containingObject, bool (*f)(const char*, const char*, int, char**)) {

	Local<Array> props = v8obj->GetPropertyNames();

	// Iterate through args, adding each element to our list
	for(unsigned int j = 0; j < props->Length(); j++) {
		Handle<Value> propName = props->Get(j);
		String::Utf8Value keyName (propName->ToString());
		Local<Value> val = v8obj->Get(propName);

		if (env->ExceptionCheck()) {
			snprintf (errBuf, errBufLen, "jsToJava: exception in JNI pending while setting attributes for l paramenter");
			return false;
		}
		bool isArray = val->IsArray();

		if (val->IsObject() || isArray) {
			// First we need the java class name for this object
			char *javaClassName = NULL;
			bool found = (*f)(containingObject, *keyName, 0, &javaClassName);
			if (!found) {
				LOGI("Cannot get Java className from cb for field %s of type object", *keyName);
			} else {
				int javaStrLen = strlen(javaClassName) + 6;
				char *fieldIdStr = new char[javaStrLen];
				if (isArray) {
					snprintf (fieldIdStr, javaStrLen-1, "[L%s;", javaClassName);
				} else {
					snprintf (fieldIdStr, javaStrLen-1, "L%s;", javaClassName);
				}
				// LOGD("Subobject of %s has fieldId %s", *keyName, fieldIdStr);
				jfieldID fieldId = env->GetFieldID(clazz, *keyName, fieldIdStr);
				if (!fieldId) {
					if (env->ExceptionCheck()) {
						env->ExceptionClear();
					}
					LOGI ("Cannot find field '%s' of type Object of java class '%s' in object of class %s", *keyName, fieldIdStr, className);
				} else {
					/* jobject subobject = env->GetObjectField(object, fieldId);
					jclass subclazz = env->GetObjectClass(subobject);
					jvalue newJv; */

					// TODO: Disparity between java method field ID (L..., [L...) and the name used to construct the class.
					// Also, why twice the call to *f?

					// Now recursively call apply to instantiate the object and everything beneath it
					// Local<Object> subv8 = Local<Object>::Cast(v8obj);
					char *subClassName = NULL;
					jobject newSubObject = copyObject(isArray, val, &javaClassName, env, errBuf, errBufLen, 0, *keyName, f);
					if (newSubObject) {
						env->SetObjectField(object, fieldId, newSubObject);
						if (subClassName) free (subClassName);
					} else {
						LOGE("apply: no jobject returned for attribute %s for class %s", *keyName, javaClassName);
					}
					// apply (subv8, subobject, subclazz, javaClassName, env, *keyName, f);
				}
				delete[] (fieldIdStr);
				if (javaClassName) {
					free (javaClassName);
					javaClassName = NULL;
				}
			}
		} else if (val->IsBoolean()) {
			jfieldID fieldId = env->GetFieldID(clazz, *keyName, "Z");
			bool boolVal = val->BooleanValue();
			if (fieldId) {
				env->SetBooleanField(object, fieldId, boolVal);
			} else {
				if (env->ExceptionCheck()) {
					env->ExceptionClear();
				}
				jfieldID fieldId = env->GetFieldID(clazz, *keyName, "I");
				if (fieldId) {
					env->SetIntField(object, fieldId, boolVal ? 1 : 0);
				} else {
					if (env->ExceptionCheck()) {
						env->ExceptionClear();
					}
					if (!coerceArgToString(env, object, clazz, *keyName, boolVal?"true":"false")) {
						LOGI ("Cannot find field %s of type boolean or String in object of class %s", *keyName, className);
					}
				}

			}
		} else if (val->IsString()) {
			jfieldID fieldId = env->GetFieldID(clazz, *keyName, "Ljava/lang/String;");
			if (fieldId) {
				Local<v8::String> stringArg = val->ToString();
				String::Utf8Value stringArgUtf(stringArg);
				jstring javaStr = env->NewStringUTF(*stringArgUtf);
				env->SetObjectField(object, fieldId, javaStr);
				env->DeleteLocalRef(javaStr);
			} else {
				LOGI ("Cannot find field %s of type String in object of class %s", *keyName, className);
			}
		} else if (val->IsInt32()) {
			jfieldID fieldId = env->GetFieldID(clazz, *keyName, "I");
			int intVal = val->Int32Value();
			if (fieldId) {
				env->SetIntField(object, fieldId, intVal);
			} else {
				if (env->ExceptionCheck()) {
					env->ExceptionClear();
				}
				char scratchBuf[100];
				snprintf(scratchBuf, 99, "%i", intVal);
				if (!coerceArgToString(env, object, clazz, *keyName, scratchBuf)) {
					LOGI ("Cannot find field %s of type int or String in object of class %s", *keyName, className);
				}
			}
		} else if (val->IsNumber() || val->IsNull() || val->IsUndefined()) {
			jfieldID fieldId = env->GetFieldID(clazz, *keyName, "I");
			double doubleVal = val->NumberValue();
			if (fieldId) {
				env->SetIntField(object, fieldId, (int)doubleVal);
			} else {
				if (env->ExceptionCheck()) {
					env->ExceptionClear();
				}
				jfieldID fieldId = env->GetFieldID(clazz, *keyName, "L");
				if (fieldId) {
					env->SetLongField(object, fieldId, (long)doubleVal);
				} else {
					if (env->ExceptionCheck()) {
						env->ExceptionClear();
					}
					jfieldID fieldId = env->GetFieldID(clazz, *keyName, "D");
					if (fieldId) {
						env->SetDoubleField(object, fieldId, doubleVal);
					} else {
						if (env->ExceptionCheck()) {
							env->ExceptionClear();
						}
						jfieldID fieldId = env->GetFieldID(clazz, *keyName, "F");
						if (fieldId) {
							env->SetFloatField(object, fieldId, (float)doubleVal);
						} else {
							if (env->ExceptionCheck()) {
								env->ExceptionClear();
							}
							char scratchBuf[100];
							// Check if the number is a fraction
							// Source: http://stackoverflow.com/questions/1521607/check-double-variable-if-it-contains-an-integer-and-not-floating-point
							double integralPart = 0.0;
							if (modf(doubleVal, &integralPart) == 0.0) {
								snprintf(scratchBuf, 99, "%lli", (long long int)doubleVal);
							} else {
								snprintf(scratchBuf, 99, "%f", doubleVal);
							}
							if (!coerceArgToString(env, object, clazz, *keyName, scratchBuf)) {
								LOGI ("Cannot find field %s of type int/long/double/float or String in object of class %s", *keyName, className);
							}
						}
					}
				}
			}
		} else {
			LOGI ("Don't know what to do with field %i named %s", j, *keyName);
		}
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
		}
	}
	return true;
}

jobject BGJSJavaWrapper::copyObject (bool isArray, Local<Value> &arg, char **classNameUsed, JNIEnv* env,
		char* errBuf, int errBufLen, int argNum,
		char* containingObject, bool (*f)(const char*, const char*, int, char**)) {
	jclass clazz;
	jobject javaObject = NULL;
	char* className = *classNameUsed;
	if (!f) {
		snprintf (errBuf, errBufLen, "jsToJavaVal: No className callback given but parameter %u needs it", argNum);
		return NULL;
	}

	if (className == NULL) {
		// Call the callback, stating that we are in the base object and so only have argument numbers
		bool found = (*f)(NULL, NULL, argNum, &className);

		if (!found) {
			snprintf (errBuf, errBufLen, "jsToJavaVal: No className from callback for base class for param %u", argNum);
			return NULL;
		}
	}
	/* if (!containingObject) {
		LOGD("className for root %s", className);
	} else {
		LOGD("className for container %s is %s", containingObject, className);
	} */
	clazz = env->FindClass(className);
	if (!clazz) {
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
		}
		snprintf (errBuf, errBufLen, "jsToJavaVal: Cannot find class %s for base class for param %u", className, argNum);
		if (className) {
			free (className);
			className = NULL;
		}
		return NULL;
	}

	jmethodID constructor = env->GetMethodID(clazz, "<init>", "()V");
	if (!constructor) {
		snprintf (errBuf, errBufLen, "jsToJavaVal: Cannot find empty constructor for class %s for base class for param %u", className, argNum);
		if (env->ExceptionCheck()) {
			env->ExceptionClear();
		}
		if (className) {
			free (className);
			className = NULL;
		}
		return NULL;
	}
	if (isArray) {
		Local<Array> array = arg->ToObject().As<Array>();
		const int arLength = array->Length();
		jobjectArray javaArray = env->NewObjectArray(arLength, clazz, NULL);

		for (int j = 0; j < arLength; j++) {
			Local<Object> v8obj = Local<Object>::Cast(array->Get(j));

			jobject newObject = env->NewObject(clazz, constructor);
			if (!apply(v8obj, newObject, clazz, (const char*)className, env, errBuf, errBufLen, containingObject, f)) {
				return NULL;
			}

			if (env->ExceptionCheck()) {
				LOGE("copyObject: Exception before setObjectArrayElement");
				return NULL;
			}
			env->SetObjectArrayElement (javaArray, j, newObject);
			if (env->ExceptionCheck()) {
				LOGE("copyObject: Exception after setObjectArrayElement");
				return NULL;
			}
			env->DeleteLocalRef(newObject);
		}
		javaObject = javaArray;
	} else {
		jobject object = env->NewObject(clazz, constructor);
		javaObject = object;

		Local<Object> v8obj = Local<Object>::Cast(arg);

		if (!apply(v8obj, object, clazz, className, env, errBuf, errBufLen, containingObject, f)) {
			return NULL;
		}
	}
	*classNameUsed = className;
	return javaObject;
}

bool BGJSJavaWrapper::jsValToJavaVal (char spec, jvalue &jv, Local<Value> &arg, bool isArray, JNIEnv* env,
		std::string &javaSig, int argNum, char* errBuf, int errBufLen, bool isOptional, bool (*f)(const char*, const char*, int, char**)) {
	switch (spec) {
			case 'l': { // V8 object copied into a Java object
				jobject jobj;
				char *className = NULL;
				jobj = copyObject(isArray, arg, &className, env, errBuf, errBufLen, argNum, NULL, f);
				if (!jobj && !isOptional) {
					return false;
				}
				jv.l = jobj;

				javaSig.append("L").append(className).append(";");
				if (className) {
					free (className);
					className = NULL;
				}

				// TODO: Release the object????
				break;
			}
			case 'o': {		// BGJSJavawrapped object whose java instance will be passed
				if (!arg->IsObject() && isOptional) {
					jv.l = NULL;
					javaSig.append("L/java/lang/Object;");
				} else {
					BGJSJavaWrapper *obj = BGJSClass::externalToClassPtr<BGJSJavaWrapper>(arg->ToObject()->GetInternalField(0));
					jclass clazz = env->GetObjectClass(obj->_javaObject);
					assert(clazz);
					jmethodID mid_getClass = env->GetMethodID(clazz, "getClass", "()Ljava/lang/Class;");
					assert(mid_getClass);
					jobject clazzObj = env->CallObjectMethod(obj->_javaObject, mid_getClass);
					jclass classClazz = env->GetObjectClass(clazzObj);
					jmethodID mid_getName = env->GetMethodID(classClazz, "getName", "()Ljava/lang/String;");
					assert(mid_getName);
					jstring name = (jstring)env->CallObjectMethod(clazzObj, mid_getName);
					assert(name);
					const char *nameStr = env->GetStringUTFChars(name, NULL);
					char *nameCopy = strdup(nameStr);
					const int length = strlen(nameCopy);
					for (int i = 0; i < length; i++) {
						if (nameCopy[i] == '.') {
							nameCopy[i] = '/';
						}
					}
					javaSig.append("L").append(nameCopy).append(";");
					free(nameCopy);
					env->ReleaseStringUTFChars(name, nameStr);
					env->DeleteLocalRef(name);
					jv.l = obj->_javaObject;
				}
				break;
			}
			case 'f': {	// V8 function
				javaSig.append("J");
				if (!(arg->IsFunction())) {
					if (isOptional) {
						jv.j = 0;
						break;
					}
					snprintf (errBuf, errBufLen, "jsToJavaVoid: Param %u is not of type Function", argNum);
					return false;
				}
				Local<v8::Function> callback = Local<Function>::Cast(arg);
				Persistent<Function>* thisFn = new Persistent<Function>(Isolate::GetCurrent(), callback);
				BGJS_NEW_PERSISTENT_PTR(thisFn);
				_v8FuncsPersisted.push_back(thisFn);
				jlong thisPtr = (jlong)(thisFn);
				jv.j = thisPtr;
				break;
			}
			case 't': { // This object

				break;
			}
			case '[': {	// Array
				javaSig.append("[");
				break;
			}
			case 's': { // string
				javaSig.append("Ljava/lang/String;");
				if (!isArray) {
					/* if (!(arg->IsString())) {
						if (isOptional) {
							jv.l = NULL;
							break;
						}
						snprintf (errBuf, errBufLen, "jsToJavaVoid: Param %u is not of type String", argNum);
						return false;
					} */
					Local<v8::String> stringArg = arg->ToString();
					String::Utf8Value stringArgUtf(stringArg);
					jstring javaStr = env->NewStringUTF(*stringArgUtf);
					jv.l = javaStr;
				} else {
					Local<Array> array = arg->ToObject().As<Array>();
					const int arLength = array->Length();
					jobjectArray javaArray = env->NewObjectArray(arLength, env->FindClass("java/lang/String"), env->NewStringUTF(""));

					for (int j = 0; j < arLength; j++) {
						Local<v8::String> stringArg = array->Get(j)->ToString();
						String::Utf8Value stringArgUtf(stringArg);
						jstring javaStr = env->NewStringUTF(*stringArgUtf);
						env->SetObjectArrayElement (javaArray, j, javaStr);
						env->DeleteLocalRef(javaStr);
					}
					jv.l = javaArray;
				}
				break;
			}
			case 'I': { // integer
				javaSig.append("I");
				if ((!arg->IsNumber())) {
					if (isOptional) {
						jv.i = 0;
						break;
					}
					snprintf (errBuf, errBufLen, "jsToJavaVoid: Param %u is not of type Number", argNum);
					return false;
				}
				int intVal = Local<Integer>::Cast(arg)->Value();
				jv.i = (jint)intVal;

				break;
			}
			case 'Z': { // boolean
				javaSig.append("Z");
				if ((!arg->IsBoolean())) {
					if (isOptional) {
						jv.z = false;
						break;
					}
					snprintf (errBuf, errBufLen, "jsToJavaVoid: Param %u is not of type Boolean", argNum);
					return false;
				}
				bool boolVal = arg->ToBoolean()->Value();
				jv.z = (jboolean)boolVal;
				break;
			}
			default: {
				snprintf (errBuf, errBufLen, "jsToJavaVoid: Param %u is of unknown argSpec %c", argNum, spec);
				return false;
			}
		}
	return true;
}

void BGJSJavaWrapper::jsToJava (const char *argsSpec, const char* javaMethodName, const char* returnType,
		const bool isStatic, const v8::FunctionCallbackInfo<v8::Value>& args, bool (*f)(const char*, const char*, int, char**)) {
	v8::Isolate* isolate = Isolate::GetCurrent();
	EscapableHandleScope scope(isolate);
	int len = strlen(argsSpec);
	int argCount = getArgCount (argsSpec, len);

	JNIEnv* env = JNU_GetEnv();
	if (env == NULL) {
		LOGE("Cannot execute jsToJava with no envCache");
		args.GetReturnValue().SetUndefined();
		return;
	}
	if (env->PushLocalFrame(100) != 0) {
		LOGD ("jsToJava: Cannot create local stack frame");
		env->ExceptionClear();
	}

	if (_javaObject == NULL) {
		LOGE("We have no global object ref, maybe out of memory situation?");
		args.GetReturnValue().SetUndefined();
		return;
	}
	char errBuf[512];

	jvalue* javaArgs = (jvalue*)malloc (sizeof(jvalue) * argCount);
	std::string javaSig("(");

	// Points to the next v8 argument. We need this separately because a valid Java argument is the This object
	int argIndex = 0;
	// Because arrays consist of two chars in the argSpec, we need to track the index into the java args separately
	int javaIndex = 0;
	bool isArray = false;
	bool isOptional = false;

	for (int i = 0; i < len; i++) {
		char spec = argsSpec[i];
		jvalue jv;
		Local<Value> arg;
		if (argIndex >= argCount) {
			arg.Clear();
		} else {
			arg = args[argIndex];
		}

		// Handle type checking for all array items
		if (isArray) {
			if (!(arg->IsArray())) {
				snprintf (errBuf, 511, "jsToJavaVoid: Param %u is not of type Array", i);
				isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, errBuf)));
				args.GetReturnValue().SetUndefined();
				return;
			}
		}

		if (spec == 't') {	// The This object, only makes sense for v8 argument lists
			javaSig.append("J");
			if (args.This().IsEmpty()) {
				snprintf (errBuf, 511, "jsToJavaVoid: Param %u wants This object, but it is empty", i);
				isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, errBuf)));
				args.GetReturnValue().SetUndefined();
                return;
			}
			Persistent<Object>* thisObj = new Persistent<Object>(isolate, args.This());
			BGJS_NEW_PERSISTENT_PTR(thisObj);
			_v8ObsPersisted.push_back(thisObj);
			jlong thisPtr = (jlong)(thisObj);
			jv.j = thisPtr;
			javaArgs[javaIndex] = jv;
		} else if (spec == ']') {
			// This parameter is optional
		} else {
			bool res = jsValToJavaVal (spec, jv, arg, isArray, env, javaSig, argIndex, errBuf, 512, isOptional, f);

			if (!res) {
				if (!isOptional) {
				    // TODO: This errBuf seemed uninitialed, added error message
					isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, errBuf)));
					return;
				}
			}
		}

		// Don't move on to next argument index for the This object
		if (spec == '[') {
			isArray = true;
		} else if (spec == ']') {
			// the next parameter is optional
			isOptional = true;
		} else {
			javaArgs[javaIndex] = jv;
			if (spec != 't') {
				argIndex++;
			}
			isArray = false;
			isOptional = false;
			javaIndex++;
		}

	}

	Local<Value> result = v8::Undefined(isolate);


	if (env->ExceptionCheck()) {
		LOGE("jsToJava: Exception before GetObjectClass");
		isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "jsToJava: Exception before GetObjectClass")));
		args.GetReturnValue().SetUndefined();
        return;
	}
	jclass clazz = env->GetObjectClass(_javaObject);

	if (env->ExceptionCheck()) {
		LOGE("jsToJava: Exception after GetObjectClass");
		isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "jsToJava: Exception after GetObjectClass")));
		args.GetReturnValue().SetUndefined();
        return;
	}

	if (strlen(returnType) == 1) {
		switch (returnType[0]) {
			case 'V': { // void
				javaSig.append(")V");

				jmethodID methodId;
				if (isStatic) {
					methodId = env->GetStaticMethodID(clazz, javaMethodName, javaSig.c_str());
				} else {
					methodId = env->GetMethodID(clazz, javaMethodName, javaSig.c_str());
				}
				if (!methodId) {
					snprintf (errBuf, 511, "jsToJavaVoid: Cannot find java method of name %s with signature %s", javaMethodName, javaSig.c_str());
					if (env->ExceptionCheck()) {
						env->ExceptionClear();
					}
					isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, errBuf)));
					args.GetReturnValue().SetUndefined();
                    return;
				}

				if (isStatic) {
					env->CallStaticVoidMethodA(clazz, methodId, javaArgs);
				} else {
					env->CallVoidMethodA(_javaObject, methodId, javaArgs);
				}
				break;
			}
			case 'Z': {	// boolean
				javaSig.append(")Z");

				jmethodID methodId = env->GetMethodID(clazz, javaMethodName, javaSig.c_str());
				if (!methodId) {
					snprintf (errBuf, 511, "jsToJavaVoid: Cannot find java method of name %s", javaMethodName);
					if (env->ExceptionCheck()) {
						env->ExceptionClear();
					}
					isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, errBuf)));
					args.GetReturnValue().SetUndefined();
                    return;
				}

				jboolean res;
				if (isStatic) {
					res = env->CallStaticBooleanMethodA(clazz, methodId, javaArgs);
				} else {
					res = env->CallBooleanMethodA(_javaObject, methodId, javaArgs);
				}
				result = Boolean::New(isolate, res);
				break;
			}
			case 'f': { // long as pointer to v8 function
				javaSig.append(")J");

				jmethodID methodId = env->GetMethodID(clazz, javaMethodName, javaSig.c_str());
				if (!methodId) {
					snprintf (errBuf, 511, "jsToJavaVoid: Cannot find java method of name %s", javaMethodName);
					if (env->ExceptionCheck()) {
						env->ExceptionClear();
					}
					isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, errBuf)));
					args.GetReturnValue().SetUndefined();
                    return;
				}

				jlong ptr = 0;
				if (isStatic) {
					ptr = env->CallStaticLongMethodA(clazz, methodId, javaArgs);
				} else {
					ptr = env->CallLongMethodA(_javaObject, methodId, javaArgs);
				}

				if (env->ExceptionCheck()) {
					LOGE("jsToJava: Exception after CallMethod");
					isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "jsToJava: Exception after CallMethod")));
					args.GetReturnValue().SetUndefined();
                    return;
				}

				//LOGD("Returned long %llu", ptr);

				void* realPtr = (void*)ptr;

				BGJSJavaWrapper* wrapper = (BGJSJavaWrapper*)realPtr;
				result = Local<Object>::New(isolate, wrapper->_jsObject);
				break;
			}
			default: {
				snprintf (errBuf, 511, "jsToJavaVoid: Don't know how to return type %s", returnType);
				isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, errBuf)));
				args.GetReturnValue().SetUndefined();
                return;
			}
		}
		if (env->ExceptionCheck()) {
			LOGE("jsToJava: Exception after CallMethod");
			isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "jsToJava: Exception after CallMethod")));
			args.GetReturnValue().SetUndefined();
            return;
		}
	}


	env->PopLocalFrame(NULL);
	free (javaArgs);

	args.GetReturnValue().Set(scope.Escape(result));
}
