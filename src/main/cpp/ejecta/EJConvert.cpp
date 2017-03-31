#include <stdlib.h>
#include <ctype.h>

#include "EJConvert.h"

#include "NdkMisc.h"
#define LOG_TAG	"EJConvert"

using namespace v8;

EJColorRGBA JSValueToColorRGBA(Local<Value> value) {
	EJColorRGBA c;
	c.hex = 0xff000000;
	if (!value->IsString()) {
		return c;
	}

	String::Utf8Value utf8(value);
	const char* jsc = *utf8;
	int length = utf8.length();

	char str[] = "ffffff";

	// #f0f format
	if( length == 4 ) {
		str[0] = str[1] = jsc[3];
		str[2] = str[3] = jsc[2];
		str[4] = str[5] = jsc[1];
		c.hex = 0xff000000 | strtol( str, NULL, 16 );
	}

	// #ff00ff format
	else if( length == 7 ) {
		str[0] = jsc[5];
		str[1] = jsc[6];
		str[2] = jsc[3];
		str[3] = jsc[4];
		str[4] = jsc[1];
		str[5] = jsc[2];
		c.hex = 0xff000000 | strtol( str, NULL, 16 );
	}

	// assume rgb(255,0,255) or rgba(255,0,255,0.5) format
	else {
		int current = 0;
		for( int i = 4; i < length-1 && current < 4; i++ ) {
			if( current == 3 ) {
				// If we have an alpha component, copy the rest of the wide
				// string into a char array and use atof() to parse it.
				char alpha[8] = { 0,0,0,0, 0,0,0,0 };
				for( int j = 0; i + j < length-1 && j < 7; j++ ) {
					alpha[j] = jsc[i+j];
				}
				c.components[current] = atof(alpha) * 255.0f;
				current++;
			}
			else if( isdigit(jsc[i]) ) {
				c.components[current] = c.components[current] * 10 + (jsc[i] - '0');
			}
			else if( jsc[i] == ',' || jsc[i] == ')' ) {
				current++;
			}
		}
	}

	// LOGD(" setFillStyle %s rgba(%d,%d,%d,%.3f)", jsc, c.rgba.r, c.rgba.g, c.rgba.b, (float)c.rgba.a/255.0f);
	return c;
}

Local<String> ColorRGBAToJSValue (Isolate* isolate, EJColorRGBA c) {
	EscapableHandleScope scope(isolate);
	static char buffer[32];
	sprintf(buffer, "rgba(%d,%d,%d,%.3f)", c.rgba.r, c.rgba.g, c.rgba.b, (float)c.rgba.a/255.0f );

	Local<String> string = String::NewFromUtf8(isolate, buffer);
	return scope.Escape(string);
}

