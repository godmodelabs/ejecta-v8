#ifndef __CLIENTABSTRACT_H
#define __CLIENTABSTRACT_H	1

#include "v8.h"

class ClientAbstract {
public:
	virtual void on (const char* event, void* cbPtr, void *thisObjPtr) = 0;
	virtual ~ClientAbstract() = 0;
};


#if defined __ANDROID__
#include "ClientAndroid.h"
#elif defined __APPLE__
#include "ClientOSX.h"
#endif

#endif
