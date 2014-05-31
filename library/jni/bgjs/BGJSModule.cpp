#include "BGJSModule.h"
#include "BGJSContext.h"
#include "BGJSJavaWrapper.h"

#include <assert.h>

using namespace v8;

BGJSContext* BGJSModule::_bgjscontext = NULL;

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "BGJSModule"



BGJSModule::BGJSModule(const char* modName) {
	this->name = std::string (modName);
}

std::string BGJSModule::getName() const {
	return name;
}

void BGJSModule::doRegister (BGJSContext* context) {
	BGJSModule::_bgjscontext = context;
	BGJSModule::_context = context->_context;
	BGJSModule::_global = context->_global;
}

BGJSModule::~BGJSModule() {
}

void BGJSModule::javaToJsField (const char* fieldName, const char fieldType, JNIEnv *env, jobject &jobj, v8::Handle<v8::Object> &jsObj) {
	jclass clazz = env->GetObjectClass(jobj);
	switch (fieldType) {
		case 's': {
			jfieldID fieldId = env->GetFieldID(clazz, fieldName, "Ljava/lang/String;");
			if (fieldId) {
				jstring javaString = (jstring)env->GetObjectField(jobj, fieldId);
				if (javaString) {
					const char *nativeString = env->GetStringUTFChars(javaString, 0);
					jsObj->Set(String::New(fieldName), String::New(nativeString));
					env->ReleaseStringUTFChars(javaString, nativeString);
				} else {
					LOGD("Couldn't retrieve java string for field %s", fieldName);
				}
			} else {
				LOGD("Couldn't find fieldId for string field %s", fieldName);
			}
			break;
		}
		case 'I': {
			jfieldID fieldId = env->GetFieldID(clazz, fieldName, "I");
			if (fieldId) {
				jint data = env->GetIntField(jobj, fieldId);
				if (env->ExceptionCheck()) {
					LOGD("Couldn't get int field for field %s", fieldName);
				}
				jsObj->Set(String::New(fieldName), Integer::New((int)data));
			} else {
				LOGD("Couldn't find fieldId for int field %s", fieldName);
			}

			break;
		}
	}
}

extern "C" {
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupNativeFnPtr (JNIEnv * env, jobject obj, jlong nativePtr);
	JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupPersistentFunction (JNIEnv * env, jobject obj, jlong nativePtr);
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupNativeFnPtr (JNIEnv * env, jobject obj, jlong nativePtr) {
	if (nativePtr) {
		BGJSJavaWrapper* wrapper = (BGJSJavaWrapper*)nativePtr;
		delete (wrapper);
	}
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_cleanupPersistentFunction (JNIEnv * env, jobject obj, jlong nativePtr) {
	if (nativePtr) {
		Persistent<Function> func = static_cast<Function*>((void*)nativePtr);
		func.Dispose();
	}
}

