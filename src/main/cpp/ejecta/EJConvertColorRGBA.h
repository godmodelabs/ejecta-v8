// JSValueToColorRGBA and ColorRGBAToJSValue convert from JavaScript strings
// to EJColorRGBA and vice versa.

// JSValueToColorRGBA accepts colors in hex, rgb(a) and hsl(a) formats and
// all 140 HTML color names. E.g.:
// "#f0f", "#ff00ff", "rgba(255, 0, 255, 1)", "magenta"

// ColorRGBAToJSValue always converts the color to an rgba string, because I'm
// lazy.

#include <v8.h>
#import "EJCanvas/EJCanvasTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

EJColorRGBA JSValueToColorRGBA(v8::Local<v8::Value> value);
v8::Local<v8::String> ColorRGBAToJSValue (v8::Isolate* isolate, EJColorRGBA c);
EJColorRGBA bufferToColorRBGA(const char *jsc, size_t length);


#ifdef __cplusplus
}
#endif
