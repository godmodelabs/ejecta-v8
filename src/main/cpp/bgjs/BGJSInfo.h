#ifndef __BGJSINFO_H
#define __BGJSINFO_H	1

#include <v8.h>
#include <string>
// #include "BGJSContext.h"

class BGJSContext;

#include "os-detection.h"

class BGJSInfo {
public:
	static v8::Persistent<v8::Context>* _context;
	static BGJSContext* _jscontext;
};


class BGJSInfo2 {
public:
	v8::Persistent<v8::Context>* _context;
	BGJSContext* _jscontext;
	std::string _module;
    std::string _dir;
};

#endif
