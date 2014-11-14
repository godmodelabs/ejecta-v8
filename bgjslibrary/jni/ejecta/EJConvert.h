#ifndef __EJCONVERT_H

#define __EJCONVERT_H	1

#include "EJCanvas/EJCanvasTypes.h"
#include <v8.h>

EJColorRGBA JSValueToColorRGBA(v8::Local<v8::Value> value);
v8::Handle<v8::String> ColorRGBAToJSValue (EJColorRGBA c);

#endif
