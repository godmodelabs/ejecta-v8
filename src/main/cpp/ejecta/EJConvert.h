#ifndef __EJCONVERT_H

#define __EJCONVERT_H	1

#include "EJCanvas/EJCanvasTypes.h"
#include <v8.h>

EJColorRGBA JSValueToColorRGBA(v8::Local<v8::Value> value);
v8::Local<v8::String> ColorRGBAToJSValue (v8::Isolate* isolate, EJColorRGBA c);

#endif
