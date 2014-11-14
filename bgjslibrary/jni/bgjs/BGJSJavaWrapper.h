#ifndef	__BGJSJAVAWRAPPER_H
#define	__BGJSJAVAWRAPPER_H		1

#include <v8.h>
#include <jni.h>
#include <string>

class BGJSJavaWrapper {
	jclass _jStringClass;
public:
	v8::Persistent<v8::Object> _jsObject;
	BGJSJavaWrapper ();
	BGJSJavaWrapper (BGJSContext* context, JNIEnv* env, jobject javaObject);

	bool coerceArgToString (JNIEnv* env, jobject &object, jclass& clazz, const char *keyName, const char *valAsString);

	void loC (JNIEnv* env);

	bool apply (v8::Local<v8::Object> &v8obj, jobject &object, jclass &clazz, const char* className, JNIEnv* env,
			char* errBuf, int errBufLen, char* containingObject, bool (*f)(const char*, const char*, int, char**));
	jobject copyObject (bool isArray, v8::Local<v8::Value> &arg, char **classNameUsed, JNIEnv* env, char* errBuf, int errBufLen, int argNum,
			char* containingObject, bool (*f)(const char*, const char*, int, char**));
	bool jsValToJavaVal (char spec, jvalue &jv, v8::Local<v8::Value> &arg, bool isArray, JNIEnv* env,
			std::string &javaSig, int argNum, char* errBuf, int errBufLen, bool isOptional, bool (*f)(const char*, const char*, int, char**));
	v8::Handle<v8::Value> jsToJava (const char* argsSpec, const char* javaMethodName,
			const char* returnType, const bool isStatic, const v8::Arguments& args, bool (*f)(const char*, const char*, int, char**));
	int getArgCount (const char* argsSpec, const int argsStrLen);
	~BGJSJavaWrapper();
	BGJSContext* _context;
	jobject _javaObject;
};


#endif
