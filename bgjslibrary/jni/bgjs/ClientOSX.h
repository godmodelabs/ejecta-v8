#ifndef __CLIENTOSX_H
#define __CLIENTOSX_H	1

#include "ClientAbstract.h"

class ClientOSX : public ClientAbstract {
public:
	const char* loadFile (const char* path);
	~ClientOSX();
};

#endif
