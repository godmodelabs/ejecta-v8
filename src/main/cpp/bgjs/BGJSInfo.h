#ifndef __BGJSINFO_H
#define __BGJSINFO_H	1

#include <v8.h>
// #include "BGJSContext.h"

class BGJSContext;

#include "os-detection.h"

class BGJSInfo {
public:
	static v8::Persistent<v8::Context>* _context;
	static v8::Eternal<v8::ObjectTemplate> _global;
	static BGJSContext* _jscontext;
};

#endif
