#include "BGJSGLModule.h"

#include "../BGJSContext.h"
#include "../BGJSGLView.h"

#include "v8.h"
#include "../ejecta/EJConvert.h"

// #define DEBUG 1

#include <GLES/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <android/bitmap.h>

#include "../jniext.h"

#define LOG_TAG "BGJSGLModule"

using namespace v8;

// Fetch the canvascontext from the context2d function in a FunctionTemplate
#define CONTEXT_FETCH()  	v8::Locker l; \
HandleScope scope; \
if (!args.This()->IsObject()) { \
	LOGE("context method '%s' got no this object", __PRETTY_FUNCTION__);  \
	return v8::ThrowException(v8::Exception::ReferenceError(v8::String::New("Can't run as static function"))); \
} \
BGJSCanvasContext *__context = BGJSClass::externalToClassPtr<BGJSContext2dGL>(args.This()->ToObject()->GetInternalField(0))->context; \
if (!__context->_isRendering) { \
	LOGI("Context is not in rendering phase in method '%s'", __PRETTY_FUNCTION__); \
	return v8::ThrowException(v8::Exception::ReferenceError(v8::String::New("Can't run when not in rendering phase"))); \
}

// Bail if not exactly n parameters were passed
#define REQUIRE_PARAMS(n)		if (args.Length() != n) { \
	LOGI("Context method '%s' requires %i parameters", __PRETTY_FUNCTION__, n); \
	return v8::ThrowException(v8::Exception::ReferenceError(v8::String::New("Wrong number of parameters"))); \
}

// Fetch the canvascontext from the context2d function for Accessors
#define CONTEXT_FETCH_VAR v8::Locker l; \
HandleScope scope; \
Local<Object> self = info.Holder(); \
Local<External> wrap = Local<External>::Cast(self->GetInternalField(0)); \
void* ptr = wrap->Value(); \
BGJSCanvasContext *__context = static_cast<BGJSContext2dGL*>(ptr)->context;

class BGJSContext2dGL {
public:
	Persistent<Object> _jsValue;
	BGJSCanvasContext* context;
};

class BGJSCanvasGL {
public:
	BGJSGLView* _view;
	BGJSContext2dGL* _context2d;
	BGJSContext* _context;

	static Handle<Value> getWidth(Local<String> property,
			const AccessorInfo &info);
	static Handle<Value> getHeight(Local<String> property,
			const AccessorInfo &info);
    static Handle<Value> getPixelRatio(Local<String> property,
			const AccessorInfo &info);
	static void setWidth(Local<String> property, Local<Value> value,
			const AccessorInfo& info);
	static void setHeight(Local<String> property, Local<Value> value,
			const AccessorInfo& info);
	static void setPixelRatio(Local<String> property, Local<Value> value,
    			const AccessorInfo& info);
};

v8::Persistent<v8::Function> BGJSGLModule::g_classRefCanvasGL;
v8::Persistent<v8::Function> BGJSGLModule::g_classRefContext2dGL;

bool BGJSGLModule::initialize() {

	return true;
}

Handle<Value> js_context_get_fillStyle(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	return scope.Close(ColorRGBAToJSValue(__context->state->fillColor));
}

void js_context_set_fillStyle(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	__context->state->fillColor = JSValueToColorRGBA(value);
	 // LOGD(" setFillStyle rgba(%d,%d,%d,%.3f)", __context->state->fillColor.rgba.r, __context->state->fillColor.rgba.g, __context->state->fillColor.rgba.b, (float)__context->state->fillColor.rgba.a/255.0f);
}

Handle<Value> js_context_get_strokeStyle(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	return scope.Close(ColorRGBAToJSValue(__context->state->strokeColor));
}

void js_context_set_strokeStyle(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	__context->state->strokeColor = JSValueToColorRGBA(value);
}

static Handle<Value> js_context_get_textAlign(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	Handle<String> stringRef;
	switch (__context->state->textAlign) {
	case kEJTextAlignStart:
		stringRef = String::New("start");
		break;
	case kEJTextAlignEnd:
		stringRef = String::New("end");
		break;
	case kEJTextAlignLeft:
		stringRef = String::New("left");
		break;
	case kEJTextAlignRight:
		stringRef = String::New("right");
		break;
	case kEJTextAlignCenter:
		stringRef = String::New("center");
		break;
	}

	return scope.Close(stringRef);
}

static void js_context_set_textAlign(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	// "start", "end", "left", "right", "center"
	CONTEXT_FETCH_VAR;
	if (!value->IsString()) {
		LOGE("set textAlign expects string");
		return;
	}
	String::Utf8Value utf8(value);

	const char *str = *utf8;

	if (strcmp(str, "left") == 0) {
		__context->state->textAlign = kEJTextAlignLeft;
	} else if (strcmp(str, "right") == 0) {
		__context->state->textAlign = kEJTextAlignRight;
	} else if (strcmp(str, "start") == 0) {
		__context->state->textAlign = kEJTextAlignStart;
	} else if (strcmp(str, "end") == 0) {
		__context->state->textAlign = kEJTextAlignEnd;
	} else if (strcmp(str, "center") == 0) {
		__context->state->textAlign = kEJTextAlignCenter;
	}
}

static Handle<Value> js_context_get_globalCompositeOperation(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	Handle<String> stringRef;
	switch (__context->state->globalCompositeOperation) {
	case kEJCompositeOperationLighter:
		stringRef = String::New("lighter");
		break;
	case kEJCompositeOperationSourceOver:
		stringRef = String::New("source-over");
		break;
	/* case kEJTextAlignLeft:
		stringRef = String::New("left");
		break;
	case kEJTextAlignRight:
		stringRef = String::New("right");
		break;
	case kEJTextAlignCenter:
		stringRef = String::New("center");
		break; */
	}

	return scope.Close(stringRef);
}

static void js_context_set_globalCompositeOperation(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	// "start", "end", "left", "right", "center"
	CONTEXT_FETCH_VAR;
	if (!value->IsString()) {
		LOGE("set globalCompositeOperation expects string");
		return;
	}
	String::Utf8Value utf8(value);

	const char *str = *utf8;

	if (strcmp(str, "source-over") == 0) {
		__context->setGlobalCompositeOperation (kEJCompositeOperationSourceOver);
	} else if (strcmp(str, "lighter") == 0) {
		__context->setGlobalCompositeOperation (kEJCompositeOperationLighter);
	} else if (strcmp(str, "start") == 0) {
		__context->state->textAlign = kEJTextAlignStart;
	} else if (strcmp(str, "end") == 0) {
		__context->state->textAlign = kEJTextAlignEnd;
	} else if (strcmp(str, "center") == 0) {
		__context->state->textAlign = kEJTextAlignCenter;
	}
}

static Handle<Value> js_context_get_textBaseline(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	Handle<String> stringRef;
	switch (__context->state->textBaseline) {
	case kEJTextBaselineAlphabetic:
		stringRef = String::New("alphabetic");
		break;
	case kEJTextBaselineIdeographic:
		stringRef = String::New("ideographic");
		break;
	case kEJTextBaselineHanging:
		stringRef = String::New("hanging");
		break;
	case kEJTextBaselineTop:
		stringRef = String::New("top");
		break;
	case kEJTextBaselineBottom:
		stringRef = String::New("bottom");
		break;
	case kEJTextBaselineMiddle:
		stringRef = String::New("middle");
		break;
	}

	return scope.Close(stringRef);
}

static void js_context_set_textBaseline(Local<String> property,
		Local<Value> value, const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	if (!value->IsString()) {
		LOGE("set textAlign expects string");
		return;
	}

	String::Utf8Value utf8(value);
	const char *str = *utf8;

	// top, hanging, middle, alphabetic, ideographic, bottom

	if (strcmp(str, "top") == 0) {
		__context->state->textBaseline = kEJTextBaselineTop;
	} else if (strcmp(str, "bottom") == 0) {
		__context->state->textBaseline = kEJTextBaselineBottom;
	} else if (strcmp(str, "middle") == 0) {
		__context->state->textBaseline = kEJTextBaselineMiddle;
	} else if (strcmp(str, "hanging") == 0) {
		__context->state->textBaseline = kEJTextBaselineHanging;
	} else if (strcmp(str, "alphabetic") == 0) {
		__context->state->textBaseline = kEJTextBaselineAlphabetic;
	} else if (strcmp(str, "ideographic") == 0) {
		__context->state->textBaseline = kEJTextBaselineIdeographic;
	} else if (strcmp(str, "bottom") == 0) {
		__context->state->textBaseline = kEJTextBaselineBottom;
	}
}

static Handle<Value> js_context_get_font(Local<String> property,
		const AccessorInfo &info) {
	HandleScope scope;
	return scope.Close(String::New(""));
}

static void js_context_set_font(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	if (!value->IsString()) {
#ifdef DEBUG
		LOGI("context setFont expects string");
#endif
		return;
	}

	String::Utf8Value utf8(value);
	const char *str = *utf8;

	__context->setFont((char*)str);
}

static Handle<Value> js_context_get_lineWidth(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	return scope.Close(Number::New(__context->state->lineWidth));
}

static void js_context_set_lineWidth(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	if (!value->IsNumber()) {
		LOGI("context set lineWidth expects number");
		return;
	}
	float num = Local<Number>::Cast(value)->Value();
	__context->state->lineWidth = num;
}

static Handle<Value> js_context_get_globalAlpha(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	return scope.Close(Number::New(__context->state->globalAlpha));
}

static void js_context_set_globalAlpha(Local<String> property,
		Local<Value> value, const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	if (!value->IsNumber()) {
		LOGI("context set lineWidth expects number");
		return;
	}
	float num = Local<Number>::Cast(value)->Value();
	__context->state->globalAlpha = num;
}

static Handle<Value> js_context_get_lineJoin(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	Handle<String> stringRef;

	switch (__context->state->lineJoin) {
	case kEJLineJoinRound:
		stringRef = String::New("round");
		break;
	case kEJLineJoinBevel:
		stringRef = String::New("bevel");
		break;
	case kEJLineJoinMiter:
		stringRef = String::New("miter");
		break;
	}

	return scope.Close(stringRef);
}

static void js_context_set_lineJoin(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	if (!value->IsString()) {
		LOGE("set textAlign expects string");
		return;
	}

	String::Utf8Value utf8(value);
	const char *str = *utf8;

	if (strcmp(str, "round") == 0) {
		__context->state->lineJoin = kEJLineJoinRound;
	} else if (strcmp(str, "bevel")) {
		__context->state->lineJoin = kEJLineJoinBevel;
	} else if (strcmp(str, "miter")) {
		__context->state->lineJoin = kEJLineJoinMiter;
	}
}

static Handle<Value> js_context_get_lineCap(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	Handle<String> stringRef;

	switch (__context->state->lineCap) {
	case kEJLineCapButt:
		stringRef = String::New("butt");
		break;
	case kEJLineCapRound:
		stringRef = String::New("round");
		break;
	case kEJLineCapSquare:
		stringRef = String::New("square");
		break;
	}
	return scope.Close(stringRef);
}

static void js_context_set_lineCap(Local<String> property, Local<Value> value,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;

	if (!value->IsString()) {
		LOGE("set textAlign expects string");
		return;
	}

	String::Utf8Value utf8(value);
	const char *str = *utf8;

	if (strcmp(str, "butt") == 0) {
		__context->state->lineCap = kEJLineCapButt;
	} else if (strcmp(str, "round") == 0) {
		__context->state->lineCap = kEJLineCapRound;
	} else if (strcmp(str, "square") == 0) {
		__context->state->lineCap = kEJLineCapSquare;
	}
}

static Handle<Value> js_context_get_miterLimit(Local<String> property,
		const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	return scope.Close(Number::New(__context->state->miterLimit));
}

static void js_context_set_miterLimit(Local<String> property,
		Local<Value> value, const AccessorInfo &info) {
	CONTEXT_FETCH_VAR;
	if (!value->IsNumber()) {
		LOGI("context set lineWidth expects number");
		return;
	}
	float num = Local<Number>::Cast(value)->Value();
	__context->state->miterLimit = num;
}

Handle<Value> BGJSCanvasGL::getWidth(Local<String> property,
		const AccessorInfo &info) {
	HandleScope scope;
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	int value = static_cast<BGJSCanvasGL*>(ptr)->_view->width;
#ifdef DEBUG
	LOGD("getWidth %d", value);
#endif
	return scope.Close(Integer::New(value));
}

Handle<Value> BGJSCanvasGL::getHeight(Local<String> property,
		const AccessorInfo &info) {
	HandleScope scope;
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	int value = static_cast<BGJSCanvasGL*>(ptr)->_view->height;
#ifdef DEBUG
	LOGD("getHeight %d", value);
#endif
	return scope.Close(Integer::New(value));
}

Handle<Value> BGJSCanvasGL::getPixelRatio(Local<String> property,
		const AccessorInfo &info) {
	HandleScope scope;
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	float value = static_cast<BGJSCanvasGL*>(ptr)->_view->context2d->backingStoreRatio;
	LOGD("getPixelRatio %f", value);
	return scope.Close(Number::New(value));
}

void BGJSCanvasGL::setHeight(Local<String> property, Local<Value> value,
		const AccessorInfo& info) {
	// NOP
}

void BGJSCanvasGL::setWidth(Local<String> property, Local<Value> value,
		const AccessorInfo& info) {
	// NOP
}

void BGJSCanvasGL::setPixelRatio(Local<String> property, Local<Value> value,
		const AccessorInfo& info) {
	// NOP
}

static Handle<Value> js_context_beginPath(const Arguments& args) {
	CONTEXT_FETCH();
	__context->beginPath();
	return v8::Undefined();
}

static Handle<Value> js_context_closePath(const Arguments& args) {
	CONTEXT_FETCH();
	__context->closePath();
	return v8::Undefined();
}

static Handle<Value> js_context_moveTo(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->moveToX(x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_lineTo(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->lineToX(x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_stroke(const Arguments& args) {
	CONTEXT_FETCH();
	__context->stroke();
	return v8::Undefined();
}

static Handle<Value> js_context_fill(const Arguments& args) {
	CONTEXT_FETCH();
	__context->fill();
	return v8::Undefined();
}

static Handle<Value> js_context_save(const Arguments& args) {
	CONTEXT_FETCH();
	__context->save();
	return v8::Undefined();
}

static Handle<Value> js_context_restore(const Arguments& args) {
	CONTEXT_FETCH();
	__context->restore();
	return v8::Undefined();
}

static Handle<Value> js_context_scale(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->scaleX(x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_rotate(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(1);
	float degrees = Local<Number>::Cast(args[0])->Value();
	__context->rotate(degrees);
	return v8::Undefined();
}

static Handle<Value> js_context_translate(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->translateX(x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_transform(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("transform: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_setTransform(const Arguments& args) {

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("scrollPathIntoView: unimplemented stub!");
	return v8::Undefined();

}

static Handle<Value> js_context_createLinearGradient(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("createLinearGradient: unimplemented stub!");
	return v8::False();
}

static Handle<Value> js_context_createPattern(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("createPattern: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_scrollPathIntoView(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("scrollPathIntoView: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_clearRect(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->clearRectX(x, y, w, h);
	// LOGD("clearRect %f %f . w %f h %f", x, y, w, h);
	return v8::Undefined();
}

static Handle<Value> js_context_fillRect(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->fillRectX(x, y, w, h);
	// LOGD("fillRect %f %f . w %f h %f", x, y, w, h);
	return v8::Undefined();
}

static Handle<Value> js_context_strokeRect(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->strokeRectX(x, y, w, h);
	return v8::Undefined();
}

static Handle<Value> js_context_quadraticCurveTo(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float cpx = Local<Number>::Cast(args[0])->Value();
	float cpy = Local<Number>::Cast(args[1])->Value();
	float x = Local<Number>::Cast(args[2])->Value();
	float y = Local<Number>::Cast(args[3])->Value();
	__context->quadraticCurveToCpx(cpx, cpy, x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_bezierCurveTo(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(6);
	float cpx1 = Local<Number>::Cast(args[0])->Value();
	float cpy1 = Local<Number>::Cast(args[1])->Value();
	float cpx2 = Local<Number>::Cast(args[2])->Value();
	float cpy2 = Local<Number>::Cast(args[3])->Value();
	float x = Local<Number>::Cast(args[4])->Value();
	float y = Local<Number>::Cast(args[5])->Value();

	__context->bezierCurveToCpx1(cpx1, cpy1, cpx2, cpy2, x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_arcTo(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(5);
	float x1 = Local<Number>::Cast(args[0])->Value();
	float y1 = Local<Number>::Cast(args[1])->Value();
	float x2 = Local<Number>::Cast(args[2])->Value();
	float y2 = Local<Number>::Cast(args[3])->Value();
	float radius = Local<Number>::Cast(args[5])->Value();

	__context->arcToX1(x1, y1, x2, y2, radius);
	return v8::Undefined();
}

static Handle<Value> js_context_rect(const Arguments& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->rectX(x, y, w, h);
	return v8::Undefined();
}

static Handle<Value> js_context_arc(const Arguments& args) {

	/*
	 void arc(in double x, in double y, in double radius, in double startAngle, in double endAngle, in optional boolean anticlockwise);
	 */
	CONTEXT_FETCH();
	REQUIRE_PARAMS(6);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float radius = Local<Number>::Cast(args[2])->Value();
	float startAngle = Local<Number>::Cast(args[3])->Value();
	float endAngle = Local<Number>::Cast(args[4])->Value();
	bool antiClockWise = args[5]->BooleanValue();
	__context->arcX(x, y, radius, startAngle, endAngle, antiClockWise);
	// [__context arcX:(float)JSValueToNumber(ctx,arguments[0],exception) y:(float)JSValueToNumber(ctx,arguments[1],exception) radius:(float)JSValueToNumber(ctx,arguments[2],exception) startAngle:(float)JSValueToNumber(ctx,arguments[3],exception) endAngle:(float)JSValueToNumber(ctx,arguments[4],exception) antiClockwise:JSValueToBoolean(ctx,arguments[5])];
	return v8::Undefined();
}

static Handle<Value> js_context_drawSystemFocusRing(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void drawSystemFocusRing(in Element element);
	 */
	//assert(argumentCount==1);
	LOGI("drawSystemFocusRing: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_drawCustomFocusRing(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void drawCustomFocusRing(in Element element);
	 */
	//assert(argumentCount==1);
	LOGI("drawCustomFocusRing: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_clipY(const Arguments& args) {
	CONTEXT_FETCH();

	REQUIRE_PARAMS(2);
	float y1 = Local<Number>::Cast(args[0])->Value();
	float y2 = Local<Number>::Cast(args[1])->Value();

	__context->clipY(y1, y2);

	return v8::Undefined();
}

static Handle<Value> js_context_clip(const Arguments& args) {
	/*
	 void clip();
	 */
	//assert(argumentCount==0);
	return v8::Undefined();
}

static Handle<Value> js_context_isPointInPath(const Arguments& args) {
	CONTEXT_FETCH();

	REQUIRE_PARAMS(5);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();

	bool isInside = NO;

	return scope.Close(Boolean::New(isInside));
}

static Handle<Value> js_context_strokeText(const Arguments& args) {
	CONTEXT_FETCH();

	/*
	 void fillText(in DOMString text, in double x, in double y, in optional double maxWidth);
	 */
	//assert(argumentCount==3||argumentCount==4);
	Handle<String> text = Local<String>::Cast(args[0]);
	String::Utf8Value utf8(text);
	float x = Local<Number>::Cast(args[1])->Value();
	float y = Local<Number>::Cast(args[2])->Value();

	__context->strokeText(*utf8, x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_fillText(const Arguments& args) {
	CONTEXT_FETCH();

	/*
	 void fillText(in DOMString text, in double x, in double y, in optional double maxWidth);
	 */
	//assert(argumentCount==3||argumentCount==4);
	Handle<String> text = Local<String>::Cast(args[0]);
	String::Utf8Value utf8(text);
	float x = Local<Number>::Cast(args[1])->Value();
	float y = Local<Number>::Cast(args[2])->Value();

	__context->fillText(*utf8, x, y);
	return v8::Undefined();
}

static Handle<Value> js_context_measureText(const Arguments& args) {
	/*
	 void measureText(in DOMString text);
	 */
	//assert(argumentCount==1);
	CONTEXT_FETCH();

	Handle<String> text = Local<String>::Cast(args[0]);
	String::Utf8Value utf8(text);
	float stringWidth = __context->measureText(*utf8);

	Handle<Object> objRef = Object::New();
	objRef->Set(String::New("width"), Number::New(stringWidth));

	return scope.Close(objRef);
}

static Handle<Value> js_context_drawImage(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void drawImage(in HTMLImageElement image, in double dx, in double dy);
	 void drawImage(in HTMLImageElement image, in double dx, in double dy, in double dw, in double dh);
	 void drawImage(in HTMLImageElement image, in double sx, in double sy, in double sw, in double sh, in double dx, in double dy, in double dw, in double dh);
	 void drawImage(in HTMLCanvasElement image, in double dx, in double dy);
	 void drawImage(in HTMLCanvasElement image, in double dx, in double dy, in double dw, in double dh);
	 void drawImage(in HTMLCanvasElement image, in double sx, in double sy, in double sw, in double sh, in double dx, in double dy, in double dw, in double dh);
	 void drawImage(in HTMLVideoElement image, in double dx, in double dy);
	 void drawImage(in HTMLVideoElement image, in double dx, in double dy, in double dw, in double dh);
	 void drawImage(in HTMLVideoElement image, in double sx, in double sy, in double sw, in double sh, in double dx, in double dy, in double dw, in double dh);
	 */
	//assert(argumentCount==3||argumentCount==5||argumentCount==9);
	LOGI("drawImage: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_createImageData(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 ImageData createImageData(in double sw, in double sh);
	 ImageData createImageData(in ImageData imagedata);
	 */
	//assert(argumentCount==1||argumentCount==2);
	LOGI("createImageData: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_getImageData(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 ImageData getImageData(in double sx, in double sy, in double sw, in double sh);
	 */
	//assert(argumentCount==4);
	LOGI("getImageData: unimplemented stub!");
	return v8::Undefined();
}

static Handle<Value> js_context_putImageData(const Arguments& args) {
	//CONTEXT_FETCH();

	/*
	 void putImageData(in ImageData imagedata, in double dx, in double dy);
	 void putImageData(in ImageData imagedata, in double dx, in double dy, in double dirtyX, in double dirtyY, in double dirtyWidth, in double dirtyHeight);

	 */
	//assert(argumentCount==3||argumentCount==7);
	LOGI("putImageData: unimplemented stub!");
	return v8::Undefined();
}

Handle<Value> BGJSGLModule::js_canvas_constructor(const Arguments& args) {
	v8::Locker l;
	HandleScope scope;

	// BGJSGLModule *objPtr = externalToClassPtr<BGJSGLModule>(args.Data());
	// The constructor gets a BGJSView object as first parameter
	if (args.Length() < 1) {
		LOGE("js_canvas_constructor got no arguments");
		return v8::Undefined();
	}
	if (!args[0]->IsObject()) {
		LOGE(
				"js_canvas_constructor got non-object as first type: %s", *(String::Utf8Value(args[0]->ToString())));
		return v8::Undefined();
	}
	Handle<Object> obj = args[0]->ToObject();
	BGJSCanvasGL* canvas = new BGJSCanvasGL();
	canvas->_context = BGJSGLModule::_bgjscontext;
	Local<Value> external = obj->GetInternalField(0);
	canvas->_view = externalToClassPtr<BGJSGLView>(external);

	Handle<Object> fn = BGJSGLModule::g_classRefCanvasGL->NewInstance();
	fn->SetInternalField(0, External::New(canvas));
	return scope.Close(fn);
}

Handle<Value> BGJSGLModule::js_canvas_getContext(const Arguments& args) {
	v8::Locker l;
	HandleScope scope;
	if (!args.This()->IsObject()) {
		LOGE("js_canvas_getContext got no this object");
		return v8::Undefined();
	}
	BGJSCanvasGL *canvas = externalToClassPtr<BGJSCanvasGL>(
			args.This()->ToObject()->GetInternalField(0));

	if (canvas->_context2d) {
		return canvas->_context2d->_jsValue;
	}

	BGJSContext2dGL *context2d = new BGJSContext2dGL();
	Local<Object> jsObj = BGJSGLModule::g_classRefContext2dGL->NewInstance();
	context2d->_jsValue = Persistent<Object>::New(jsObj);
	context2d->_jsValue.MakeWeak(NULL, BGJSGLModule::js_context_destruct);
	context2d->context = canvas->_view->context2d;
	jsObj->SetInternalField(0, External::New(context2d));
	canvas->_context2d = context2d;

	return scope.Close(jsObj);
}

void BGJSGLModule::js_context_destruct(v8::Persistent<v8::Value> value,
		void *data) {
	v8::HandleScope scope;

	BGJSContext2dGL *obj = static_cast<BGJSContext2dGL*>(data);
	// assert(value.IsNearDeath());
	delete obj;
}

void BGJSGLModule::doRequire(v8::Handle<v8::Object> target) {
	HandleScope scope;

	// Handle<Object> exports = Object::New();

	// Create the template for the fake HTMLCanvasElement
	v8::Local<v8::FunctionTemplate> bgjshtmlft = v8::FunctionTemplate::New();
	bgjshtmlft->SetClassName(String::New("HTMLCanvasElement"));
	v8::Local<v8::ObjectTemplate> bgjshtmlit = bgjshtmlft->InstanceTemplate();
	bgjshtmlit->SetInternalFieldCount(1);
	bgjshtmlit->SetAccessor(String::New("width"), BGJSCanvasGL::getWidth,
			BGJSCanvasGL::setWidth);
	bgjshtmlit->SetAccessor(String::New("height"), BGJSCanvasGL::getHeight,
			BGJSCanvasGL::setHeight);
    bgjshtmlit->SetAccessor(String::New("devicePixelRatio"), BGJSCanvasGL::getPixelRatio,
			BGJSCanvasGL::setPixelRatio);

	// Call on construction
	bgjshtmlit->SetCallAsFunctionHandler(BGJSGLModule::js_canvas_constructor);
	bgjshtmlit->Set(String::New("getContext"),
			FunctionTemplate::New(BGJSGLModule::js_canvas_getContext));

	// bgjsgl->Set(String::New("log"), FunctionTemplate::New(BGJSGLModule::log));
	Local<Function> instance = bgjshtmlft->GetFunction();
	BGJSGLModule::g_classRefCanvasGL = Persistent<Function>::New(instance);
	Handle<Object> exports = instance->NewInstance();

	// Create the template for Canvas objects
	Local<FunctionTemplate> canvasft = FunctionTemplate::New();
	canvasft->SetClassName(String::New("CanvasRenderingContext2D"));
	Local<ObjectTemplate> canvasot = canvasft->InstanceTemplate();
	canvasot->SetInternalFieldCount(1);

	// Attributes
	canvasot->SetAccessor(String::New("fillStyle"), js_context_get_fillStyle,
			js_context_set_fillStyle);
	canvasot->SetAccessor(String::New("strokeStyle"),
			js_context_get_strokeStyle, js_context_set_strokeStyle);
	canvasot->SetAccessor(String::New("textAlign"), js_context_get_textAlign,
			js_context_set_textAlign);
	canvasot->SetAccessor(String::New("textBaseline"),
			js_context_get_textBaseline, js_context_set_textBaseline);
	canvasot->SetAccessor(String::New("font"), js_context_get_font,
			js_context_set_font);
	canvasot->SetAccessor(String::New("lineWidth"), js_context_get_lineWidth,
			js_context_set_lineWidth);
	canvasot->SetAccessor(String::New("globalAlpha"),
			js_context_get_globalAlpha, js_context_set_globalAlpha);
	canvasot->SetAccessor(String::New("lineJoin"), js_context_get_lineJoin,
			js_context_set_lineJoin);
	canvasot->SetAccessor(String::New("lineCap"), js_context_get_lineCap,
			js_context_set_lineCap);
	canvasot->SetAccessor(String::New("miterLimit"), js_context_get_miterLimit,
			js_context_set_miterLimit);
	canvasot->SetAccessor(String::New("globalCompositeOperation"), js_context_get_globalCompositeOperation, js_context_set_globalCompositeOperation);

	// Functions
	canvasot->Set(String::New("beginPath"),
			FunctionTemplate::New(js_context_beginPath));
	canvasot->Set(String::New("closePath"),
			FunctionTemplate::New(js_context_closePath));
	canvasot->Set(String::New("moveTo"),
			FunctionTemplate::New(js_context_moveTo));
	canvasot->Set(String::New("lineTo"),
			FunctionTemplate::New(js_context_lineTo));
	canvasot->Set(String::New("stroke"),
			FunctionTemplate::New(js_context_stroke));
	canvasot->Set(String::New("fill"), FunctionTemplate::New(js_context_fill));
	canvasot->Set(String::New("save"), FunctionTemplate::New(js_context_save));
	canvasot->Set(String::New("restore"),
			FunctionTemplate::New(js_context_restore));
	canvasot->Set(String::New("scale"),
			FunctionTemplate::New(js_context_scale));
	canvasot->Set(String::New("rotate"),
			FunctionTemplate::New(js_context_rotate));
	canvasot->Set(String::New("translate"),
			FunctionTemplate::New(js_context_translate));
	canvasot->Set(String::New("transform"),
			FunctionTemplate::New(js_context_transform));
	canvasot->Set(String::New("setTransform"),
			FunctionTemplate::New(js_context_setTransform));
	canvasot->Set(String::New("createLinearGradient"),
			FunctionTemplate::New(js_context_createLinearGradient));
	canvasot->Set(String::New("createPattern"),
			FunctionTemplate::New(js_context_createPattern));
	canvasot->Set(String::New("clearRect"),
			FunctionTemplate::New(js_context_clearRect));
	canvasot->Set(String::New("fillRect"),
			FunctionTemplate::New(js_context_fillRect));
	canvasot->Set(String::New("strokeRect"),
			FunctionTemplate::New(js_context_strokeRect));
	canvasot->Set(String::New("quadraticCurveTo"),
			FunctionTemplate::New(js_context_quadraticCurveTo));
	canvasot->Set(String::New("bezierCurveTo"),
			FunctionTemplate::New(js_context_bezierCurveTo));
	canvasot->Set(String::New("arcTo"),
			FunctionTemplate::New(js_context_arcTo));
	canvasot->Set(String::New("rect"), FunctionTemplate::New(js_context_rect));
	canvasot->Set(String::New("arc"), FunctionTemplate::New(js_context_arc));
	canvasot->Set(String::New("drawSystemFocusRing"),
			FunctionTemplate::New(js_context_drawSystemFocusRing));
	canvasot->Set(String::New("drawCustomFocusRing"),
			FunctionTemplate::New(js_context_drawCustomFocusRing));
	canvasot->Set(String::New("scrollPathIntoView"),
			FunctionTemplate::New(js_context_scrollPathIntoView));
	canvasot->Set(String::New("clip"), FunctionTemplate::New(js_context_clip));
	canvasot->Set(String::New("isPointInPath"),
			FunctionTemplate::New(js_context_isPointInPath));
	canvasot->Set(String::New("fillText"),
			FunctionTemplate::New(js_context_fillText));
	canvasot->Set(String::New("strokeText"),
			FunctionTemplate::New(js_context_strokeText));
	canvasot->Set(String::New("measureText"),
			FunctionTemplate::New(js_context_measureText));
	canvasot->Set(String::New("drawImage"),
			FunctionTemplate::New(js_context_drawImage));
	canvasot->Set(String::New("createImageData"),
			FunctionTemplate::New(js_context_createImageData));
	canvasot->Set(String::New("getImageData"),
			FunctionTemplate::New(js_context_getImageData));
	canvasot->Set(String::New("putImageData"),
			FunctionTemplate::New(js_context_putImageData));
	canvasot->Set(String::New("clipY"),
			FunctionTemplate::New(js_context_clipY));

	g_classRefContext2dGL = Persistent<Function>::New(canvasft->GetFunction());

	// g_classRefContext2dGL

	target->Set(String::New("exports"), exports);
}

Handle<Value> BGJSGLModule::initWithContext(BGJSContext* context) {
	doRegister(context);

	this->_canvasContext = new BGJSCanvasContext(500, 500);
	return v8::Undefined();
}

BGJSGLModule::BGJSGLModule() :
		BGJSModule("bgjsgl") {

}

BGJSGLModule::~BGJSGLModule() {

}

static void checkGlError(const char* op) {
	for (GLint error = glGetError(); error; error = glGetError()) {
		LOGI("after %s() glError (0x%x)\n", op, error);
	}
}

JNIEXPORT jint JNICALL Java_ag_boersego_bgjs_ClientAndroid_createGL(JNIEnv * env,
		jobject obj, jlong ctxPtr, jobject javaGlView, jfloat pixelRatio, jboolean noClearOnFlip) {

	v8::Locker l;
	HandleScope scope;

	BGJSContext* ct = (BGJSContext*) ctxPtr;
	Context::Scope context_scope(ct->_context);

	BGJSGLView *view = new BGJSGLView(BGJSGLModule::_bgjscontext, pixelRatio);
	view->noClearOnFlip = noClearOnFlip;
	view->setJavaGl(env, env->NewGlobalRef(javaGlView));

	// Register GLView with context so that cancelAnimationRequest works.
	ct->registerGLView(view);

	return (jint) view;
}

JNIEXPORT int JNICALL Java_ag_boersego_bgjs_ClientAndroid_init(JNIEnv * env,
		jobject obj, jlong ctxPtr, jint objPtr, jint width, jint height, jstring callbackName) {
	v8::Locker l;
	HandleScope scope;

#ifdef DEBUG
	printGLString("Version", GL_VERSION);
	printGLString("Vendor", GL_VENDOR);
	printGLString("Renderer", GL_RENDERER);
	printGLString("Extensions", GL_EXTENSIONS);

	LOGI("setupGraphics(%d, %d)", width, height);
#endif
	BGJSContext* ct = (BGJSContext*) ctxPtr;

	Context::Scope context_scope(ct->_context);

	BGJSGLView *view = (BGJSGLView*) objPtr;

	if (width != view->width || height != view->height) {
#ifdef DEBUG
		LOGD("Resizing from %dx%d to %dx%d, resizeOnly %i", view->width, view->height, width, height, (int)(view->opened));
#endif
		view->resize(width, height, view->opened);
	}
	Handle<Value> uiObj;

	// Only call this once!
	if (!view->opened) {
		const char* cbStr = env->GetStringUTFChars(callbackName, NULL);

		LOGI("setupGraphics(%s)", cbStr);
		Handle<Value> res = view->startJS(cbStr, NULL, v8::Undefined(), 0);
		view->opened = true;
		env->ReleaseStringUTFChars(callbackName, cbStr);

		if (res->IsNumber()) {
			return res->ToNumber()->Value();
		}
	}
	return -1;
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_close(JNIEnv * env,
		jobject obj, jlong ctxPtr, jint objPtr) {
	v8::Locker l;
	HandleScope scope;

	BGJSContext* ct = (BGJSContext*) ctxPtr;
	Context::Scope context_scope(ct->_context);

	BGJSGLView *view = (BGJSGLView*) objPtr;

	ct->unregisterGLView(view);
	view->close();
	env->DeleteGlobalRef(view->_javaGlView);
	delete (view);
}

const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f };

JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_step(JNIEnv * env,
		jobject obj, jlong ctxPtr, jint jsPtr) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	BGJSGLView *view = (BGJSGLView*) jsPtr;
	return BGJSGLModule::_bgjscontext->runAnimationRequests(view);
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_redraw(JNIEnv * env,
		jobject obj, jlong ctxPtr, jint jsPtr) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	BGJSGLView *view = (BGJSGLView*) jsPtr;
	return view->call(view->_cbRedraw);
}


JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_sendTouchEvent(
		JNIEnv * env, jobject obj, jlong ctxPtr, jint objPtr, jstring typeStr,
		jfloatArray xArr, jfloatArray yArr, jfloat scale) {
	v8::Locker l;
	HandleScope scope;

	float* x = env->GetFloatArrayElements(xArr, NULL);
	float* y = env->GetFloatArrayElements(yArr, NULL);
	const int count = env->GetArrayLength(xArr);
	const char *type = env->GetStringUTFChars(typeStr, 0);
	if (x == NULL || y == NULL) {
		LOGE("sendTouchEvent: Cannot access point copies: %p %p", x, y);
		return;
	}

	/* if (count == 0) {
		LOGI("sendTouchEvent: Empty array");
		return;
	} */

	BGJSContext* ct = (BGJSContext*) ctxPtr;

	Context::Scope context_scope(ct->_context);
	BGJSGLView *view = (BGJSGLView*) objPtr;

	// Create event object
	Handle<Object> eventObjRef = Object::New();
	eventObjRef->Set(String::New("type"), String::New(type));
	eventObjRef->Set(String::New("scale"), Number::New(scale));

	Handle<String> pageX = String::New("clientX");
	Handle<String> pageY = String::New("clientY");

	Handle<Array> touchesArray = Array::New(count);

	// Populate touches array
	for (int i = 0; i < count; i++) {
		Handle<Object> touchObjRef = Object::New();
		touchObjRef->Set(pageX, Number::New(x[i]));
		touchObjRef->Set(pageY, Number::New(y[i]));
		touchesArray->Set(Number::New(i), touchObjRef);
	}

	eventObjRef->Set(String::New("touches"), touchesArray);

	view->sendEvent(eventObjRef);

	// Cleanup JNI stuff
	env->ReleaseFloatArrayElements(xArr, x, 0);
	env->ReleaseFloatArrayElements(yArr, y, 0);
	env->ReleaseStringUTFChars(typeStr, type);
}

