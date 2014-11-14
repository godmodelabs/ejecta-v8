/*
 * v8Engine.h
 *
 *  Created on: Sep 3, 2012
 *      Author: kread@boerse-go.de
 */

#ifndef V8ENGINE_H_
#define V8ENGINE_H_

#include "v8.h"

class v8Engine {
	// As a singleton, constructor is private
public:
    static v8Engine& getInstance()
    {
        static v8Engine    instance; // Guaranteed to be destroyed.
                              // Instantiated on first use.
        return instance;
    }
	virtual ~v8Engine();
	int run(JNIEnv * env, jobject obj);
	static void log(int, const v8::Arguments& args);

	bool ajaxSuccess(JNIEnv * env, jobject obj,
			jstring data, jint responseCode, jint jsCbPtr, jint thisPtr);

	int load(JNIEnv * env, jobject obj, jstring path);
	int tick(int ms);
	v8::Handle<v8::Value> require(const v8::Arguments& args);
	v8::Handle<v8::Value> ajax(const v8::Arguments& args);
	static void ReportException(v8::TryCatch* try_catch);
	bool registerAssetManager(JNIEnv * env, jobject obj, jobject assetManager);

	v8::Persistent<v8::ObjectTemplate> global;
	JNIEnv *envCache;	// Only used when calling back from js, set to NULL when finished!
private:
	v8Engine();

	v8Engine(v8Engine const&) {};              // Don't Implement
	void operator=(v8Engine const&); // Don't implement

	bool validScriptLoaded;
	v8::Persistent<v8::Context> context;
	v8::Persistent<v8::Script> script;
	v8::Persistent<v8::Function> mockTimerTick;
	v8::Persistent<v8::Object> mockTimer;
	jobject assetManager;
};
#endif /* V8ENGINE_H_ */
