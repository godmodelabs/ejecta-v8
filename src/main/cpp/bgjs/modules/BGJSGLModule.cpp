#include "BGJSGLModule.h"

#include "../BGJSContext.h"
#include "../BGJSGLView.h"

#include "v8.h"
#include "../ejecta/EJConvert.h"

#include <GLES/gl.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <android/bitmap.h>

#include "../jniext.h"

// #define DEBUG 1
#undef DEBUG

#define LOG_TAG "BGJSGLModule"

using namespace v8;

/**
 * BGJSGLModule
 * Canvas BGJS extension. This is the glue between Ejectas Canvas and OpenGL draw code, BGJSGLViews context handling and v8.
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */


#define CREATE_ESCAPABLE_CONTEXT    	v8::Isolate* isolate = Isolate::GetCurrent(); \
EscapableHandleScope scope(isolate);

#define CREATE_UNESCAPABLE_CONTEXT   	v8::Isolate* isolate = Isolate::GetCurrent(); \
HandleScope scope(isolate);

// Fetch the canvascontext from the context2d function in a FunctionTemplate
#define CONTEXT_FETCH_BASE v8::Locker l(isolate); \
if (!args.This()->IsObject()) { \
	LOGE("context method '%s' got no this object", __PRETTY_FUNCTION__);  \
	isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "Can't run as static function"))); \
} \
BGJSCanvasContext *__context = BGJSClass::externalToClassPtr<BGJSContext2dGL>(args.This()->ToObject()->GetInternalField(0))->context; \
if (!__context->_isRendering) { \
	LOGI("Context is not in rendering phase in method '%s'", __PRETTY_FUNCTION__); \
	isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "Can't run when not in rendering phase"))); \
}


#define CONTEXT_FETCH_ESCAPABLE()  CREATE_ESCAPABLE_CONTEXT \
CONTEXT_FETCH_BASE

#define CONTEXT_FETCH()  CREATE_UNESCAPABLE_CONTEXT \
CONTEXT_FETCH_BASE

// Bail if not exactly n parameters were passed
#define REQUIRE_PARAMS(n)		if (args.Length() != n) { \
	LOGI("Context method '%s' requires %i parameters", __PRETTY_FUNCTION__, n); \
	isolate->ThrowException(v8::Exception::ReferenceError(v8::String::NewFromUtf8(isolate, "Wrong number of parameters"))); \
}

// Fetch the canvascontext from the context2d function for Accessors
#define CONTEXT_FETCH_VAR               v8::Isolate* isolate = Isolate::GetCurrent(); \
v8::Locker l(isolate); \
HandleScope scope(isolate); \
Local<Object> self = info.Holder(); \
Local<External> wrap = Local<External>::Cast(self->GetInternalField(0)); \
void* ptr = wrap->Value(); \
BGJSCanvasContext *__context = static_cast<BGJSContext2dGL*>(ptr)->context;

#define CONTEXT_FETCH_VAR_ESCAPABLE       v8::Isolate* isolate = Isolate::GetCurrent(); \
v8::Locker l(isolate); \
EscapableHandleScope scope(isolate); \
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
	const BGJSContext2dGL* _context2d;
	const BGJSContext* _context;

	static void getWidth(Local<String> property,
			const v8::PropertyCallbackInfo<Value>& info);
	static void getHeight(Local<String> property,
			const v8::PropertyCallbackInfo<Value>& info);
    static void getPixelRatio(Local<String> property,
			const v8::PropertyCallbackInfo<Value>& info);
	static void setWidth(Local<String> property, Local<Value> value,
			const v8::PropertyCallbackInfo<void>& info);
	static void setHeight(Local<String> property, Local<Value> value,
			const v8::PropertyCallbackInfo<void>& info);
	static void setPixelRatio(Local<String> property, Local<Value> value,
    			const v8::PropertyCallbackInfo<void>& info);
};

v8::Persistent<v8::Function> BGJSGLModule::g_classRefCanvasGL;
v8::Persistent<v8::Function> BGJSGLModule::g_classRefContext2dGL;

bool BGJSGLModule::initialize() {

	return true;
}

void js_context_get_fillStyle(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR_ESCAPABLE;
	info.GetReturnValue().Set(scope.Escape(ColorRGBAToJSValue(isolate, __context->state->fillColor)));
}

void js_context_set_fillStyle(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
	CONTEXT_FETCH_VAR;
	__context->state->fillColor = JSValueToColorRGBA(value);
	 // LOGD(" setFillStyle rgba(%d,%d,%d,%.3f)", __context->state->fillColor.rgba.r, __context->state->fillColor.rgba.g, __context->state->fillColor.rgba.b, (float)__context->state->fillColor.rgba.a/255.0f);
}

void js_context_get_strokeStyle(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR_ESCAPABLE;
	info.GetReturnValue().Set(scope.Escape(ColorRGBAToJSValue(isolate, __context->state->strokeColor)));
}

void js_context_set_strokeStyle(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
	CONTEXT_FETCH_VAR;
	__context->state->strokeColor = JSValueToColorRGBA(value);
}

static void js_context_get_textAlign(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR_ESCAPABLE;

	Local<String> stringRef;
	switch (__context->state->textAlign) {
	case kEJTextAlignStart:
		stringRef = String::NewFromUtf8(isolate, "start");
		break;
	case kEJTextAlignEnd:
		stringRef = String::NewFromUtf8(isolate, "end");
		break;
	case kEJTextAlignLeft:
		stringRef = String::NewFromUtf8(isolate, "left");
		break;
	case kEJTextAlignRight:
		stringRef = String::NewFromUtf8(isolate, "right");
		break;
	case kEJTextAlignCenter:
		stringRef = String::NewFromUtf8(isolate, "center");
		break;
	}

	info.GetReturnValue().Set(scope.Escape(stringRef));
}

static void js_context_set_textAlign(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
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

static void js_context_get_globalCompositeOperation(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR_ESCAPABLE;

	Local<String> stringRef;
	switch (__context->state->globalCompositeOperation) {
	case kEJCompositeOperationLighter:
		stringRef = String::NewFromUtf8(isolate, "lighter");
		break;
	case kEJCompositeOperationSourceOver:
		stringRef = String::NewFromUtf8(isolate, "source-over");
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

	info.GetReturnValue().Set(scope.Escape(stringRef));
}

static void js_context_set_globalCompositeOperation(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
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

static void js_context_get_textBaseline(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR_ESCAPABLE;

	Local<String> stringRef;
	switch (__context->state->textBaseline) {
	case kEJTextBaselineAlphabetic:
		stringRef = String::NewFromUtf8(isolate, "alphabetic");
		break;
	case kEJTextBaselineIdeographic:
		stringRef = String::NewFromUtf8(isolate, "ideographic");
		break;
	case kEJTextBaselineHanging:
		stringRef = String::NewFromUtf8(isolate, "hanging");
		break;
	case kEJTextBaselineTop:
		stringRef = String::NewFromUtf8(isolate, "top");
		break;
	case kEJTextBaselineBottom:
		stringRef = String::NewFromUtf8(isolate, "bottom");
		break;
	case kEJTextBaselineMiddle:
		stringRef = String::NewFromUtf8(isolate, "middle");
		break;
	}

	info.GetReturnValue().Set(scope.Escape(stringRef));
}

static void js_context_set_textBaseline(Local<String> property,
		Local<Value> value, const v8::PropertyCallbackInfo<void>& info) {
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

static void js_context_get_font(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	EscapableHandleScope scope(Isolate::GetCurrent());
	scope.Escape(String::NewFromUtf8(Isolate::GetCurrent(), ""));
}

static void js_context_set_font(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
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

static void js_context_get_lineWidth(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR;
	info.GetReturnValue().Set(__context->state->lineWidth);
}

static void js_context_set_lineWidth(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
	CONTEXT_FETCH_VAR;

	if (!value->IsNumber()) {
		LOGI("context set lineWidth expects number");
		return;
	}
	float num = Local<Number>::Cast(value)->Value();
	__context->state->lineWidth = num;
}

static void js_context_get_globalAlpha(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR;
	info.GetReturnValue().Set(__context->state->globalAlpha);
}

static void js_context_set_globalAlpha(Local<String> property,
		Local<Value> value, const v8::PropertyCallbackInfo<void>& info) {
	CONTEXT_FETCH_VAR;

	if (!value->IsNumber()) {
		LOGI("context set lineWidth expects number");
		return;
	}
	float num = Local<Number>::Cast(value)->Value();
	__context->state->globalAlpha = num;
}

static void js_context_get_lineJoin(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR_ESCAPABLE;

	Local<String> stringRef;

	switch (__context->state->lineJoin) {
	case kEJLineJoinRound:
		stringRef = String::NewFromUtf8(isolate, "round");
		break;
	case kEJLineJoinBevel:
		stringRef = String::NewFromUtf8(isolate, "bevel");
		break;
	case kEJLineJoinMiter:
		stringRef = String::NewFromUtf8(isolate, "miter");
		break;
	}

	info.GetReturnValue().Set(scope.Escape(stringRef));
}

static void js_context_set_lineJoin(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
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

static void js_context_get_lineCap(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR_ESCAPABLE;

	Local<String> stringRef;

	switch (__context->state->lineCap) {
	case kEJLineCapButt:
		stringRef = String::NewFromUtf8(isolate, "butt");
		break;
	case kEJLineCapRound:
		stringRef = String::NewFromUtf8(isolate, "round");
		break;
	case kEJLineCapSquare:
		stringRef = String::NewFromUtf8(isolate, "square");
		break;
	}
	info.GetReturnValue().Set(scope.Escape(stringRef));
}

static void js_context_set_lineCap(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
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

static void js_context_get_miterLimit(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	CONTEXT_FETCH_VAR;
	info.GetReturnValue().Set(__context->state->miterLimit);
}

static void js_context_set_miterLimit(Local<String> property,
		Local<Value> value, const v8::PropertyCallbackInfo<void>& info) {
	CONTEXT_FETCH_VAR;
	if (!value->IsNumber()) {
		LOGI("context set lineWidth expects number");
		return;
	}
	float num = Local<Number>::Cast(value)->Value();
	__context->state->miterLimit = num;
}

void BGJSCanvasGL::getWidth(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	HandleScope scope(Isolate::GetCurrent());
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	int value = static_cast<BGJSCanvasGL*>(ptr)->_view->width;
#ifdef DEBUG
	LOGD("getWidth %d", value);
#endif
	info.GetReturnValue().Set(value);
}

void BGJSCanvasGL::getHeight(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	HandleScope scope(Isolate::GetCurrent());
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	int value = static_cast<BGJSCanvasGL*>(ptr)->_view->height;
#ifdef DEBUG
	LOGD("getHeight %d", value);
#endif
	info.GetReturnValue().Set(value);
}

void BGJSCanvasGL::getPixelRatio(Local<String> property,
		const v8::PropertyCallbackInfo<Value>& info) {
	HandleScope scope(Isolate::GetCurrent());
	Local<Object> self = info.Holder();
	Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
	void* ptr = wrap->Value();
	float value = static_cast<BGJSCanvasGL*>(ptr)->_view->context2d->backingStoreRatio;
	LOGD("getPixelRatio %f", value);
	info.GetReturnValue().Set(value);
}

void BGJSCanvasGL::setHeight(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
	// NOP
}

void BGJSCanvasGL::setWidth(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
	// NOP
}

void BGJSCanvasGL::setPixelRatio(Local<String> property, Local<Value> value,
		const v8::PropertyCallbackInfo<void>& info) {
	// NOP
}

static void js_context_beginPath(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	__context->beginPath();
	args.GetReturnValue().SetUndefined();
}

static void js_context_closePath(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	__context->closePath();
	args.GetReturnValue().SetUndefined();
}

static void js_context_moveTo(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->moveToX(x, y);
	args.GetReturnValue().SetUndefined();
}

static void js_context_lineTo(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->lineToX(x, y);
	args.GetReturnValue().SetUndefined();
}

static void js_context_stroke(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	__context->stroke();
	args.GetReturnValue().SetUndefined();
}

static void js_context_fill(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	__context->fill();
	args.GetReturnValue().SetUndefined();
}

static void js_context_save(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	__context->save();
	args.GetReturnValue().SetUndefined();
}

static void js_context_restore(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	__context->restore();
	args.GetReturnValue().SetUndefined();
}

static void js_context_scale(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->scaleX(x, y);
	args.GetReturnValue().SetUndefined();
}

static void js_context_rotate(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(1);
	float degrees = Local<Number>::Cast(args[0])->Value();
	__context->rotate(degrees);
	args.GetReturnValue().SetUndefined();
}

static void js_context_translate(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(2);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	__context->translateX(x, y);
	args.GetReturnValue().SetUndefined();
}

static void js_context_transform(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("transform: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

static void js_context_setTransform(const v8::FunctionCallbackInfo<v8::Value>& args) {

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("scrollPathIntoView: unimplemented stub!");
	args.GetReturnValue().SetUndefined();

}

static void js_context_createLinearGradient(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("createLinearGradient: unimplemented stub!");
	args.GetReturnValue().Set(false);
}

static void js_context_createPattern(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("createPattern: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

static void js_context_scrollPathIntoView(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 void scrollPathIntoView();
	 */
	//assert(argumentCount==0);
	LOGI("scrollPathIntoView: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

static void js_context_clearRect(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->clearRectX(x, y, w, h);
	// LOGD("clearRect %f %f . w %f h %f", x, y, w, h);
	args.GetReturnValue().SetUndefined();
}

static void js_context_fillRect(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->fillRectX(x, y, w, h);
	// LOGD("fillRect %f %f . w %f h %f", x, y, w, h);
	args.GetReturnValue().SetUndefined();
}

static void js_context_strokeRect(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->strokeRectX(x, y, w, h);
	args.GetReturnValue().SetUndefined();
}

static void js_context_quadraticCurveTo(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float cpx = Local<Number>::Cast(args[0])->Value();
	float cpy = Local<Number>::Cast(args[1])->Value();
	float x = Local<Number>::Cast(args[2])->Value();
	float y = Local<Number>::Cast(args[3])->Value();
	__context->quadraticCurveToCpx(cpx, cpy, x, y);
	args.GetReturnValue().SetUndefined();
}

static void js_context_bezierCurveTo(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(6);
	float cpx1 = Local<Number>::Cast(args[0])->Value();
	float cpy1 = Local<Number>::Cast(args[1])->Value();
	float cpx2 = Local<Number>::Cast(args[2])->Value();
	float cpy2 = Local<Number>::Cast(args[3])->Value();
	float x = Local<Number>::Cast(args[4])->Value();
	float y = Local<Number>::Cast(args[5])->Value();

	__context->bezierCurveToCpx1(cpx1, cpy1, cpx2, cpy2, x, y);
	args.GetReturnValue().SetUndefined();
}

static void js_context_arcTo(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(5);
	float x1 = Local<Number>::Cast(args[0])->Value();
	float y1 = Local<Number>::Cast(args[1])->Value();
	float x2 = Local<Number>::Cast(args[2])->Value();
	float y2 = Local<Number>::Cast(args[3])->Value();
	float radius = Local<Number>::Cast(args[5])->Value();

	__context->arcToX1(x1, y1, x2, y2, radius);
	args.GetReturnValue().SetUndefined();
}

static void js_context_rect(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();
	REQUIRE_PARAMS(4);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();
	__context->rectX(x, y, w, h);
	args.GetReturnValue().SetUndefined();
}

static void js_context_arc(const v8::FunctionCallbackInfo<v8::Value>& args) {

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
	args.GetReturnValue().SetUndefined();
}

static void js_context_drawSystemFocusRing(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 void drawSystemFocusRing(in Element element);
	 */
	//assert(argumentCount==1);
	LOGI("drawSystemFocusRing: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

static void js_context_drawCustomFocusRing(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 void drawCustomFocusRing(in Element element);
	 */
	//assert(argumentCount==1);
	LOGI("drawCustomFocusRing: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

static void js_context_clipY(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();

	REQUIRE_PARAMS(2);
	float y1 = Local<Number>::Cast(args[0])->Value();
	float y2 = Local<Number>::Cast(args[1])->Value();

	__context->clipY(y1, y2);

#ifdef DEBUG
	LOGD("js clipY called from %f to %f", y1, y2);
#endif

	args.GetReturnValue().SetUndefined();
}

static void js_context_clipRect(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();

	REQUIRE_PARAMS(4);
	float inputX = Local<Number>::Cast(args[0])->Value();
	float inputY = Local<Number>::Cast(args[1])->Value();
	float w = Local<Number>::Cast(args[2])->Value();
	float h = Local<Number>::Cast(args[3])->Value();

#ifdef DEBUG
	LOGD("js clipRect called from %f, %f to %f, %f", inputX, inputY, w, h);
#endif

	CGRect rect;
	rect.origin.x = inputX;
	rect.origin.y = inputY;
	rect.size.width = w;
	rect.size.height = h;

	__context->clipRect(rect);

	args.GetReturnValue().SetUndefined();
}

static void js_context_clip(const v8::FunctionCallbackInfo<v8::Value>& args) {
	/*
	 void clip();
	 */
	//assert(argumentCount==0);
#ifdef DEBUG
	LOGD("js clip called: not implemented");
#endif

	args.GetReturnValue().SetUndefined();
}

static void js_context_isPointInPath(const v8::FunctionCallbackInfo<v8::Value>& args) {
	CONTEXT_FETCH();

	REQUIRE_PARAMS(5);
	float x = Local<Number>::Cast(args[0])->Value();
	float y = Local<Number>::Cast(args[1])->Value();

	bool isInside = NO;
	// TODO: Incomplete

	args.GetReturnValue().Set(isInside);
}

static void js_context_strokeText(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
	args.GetReturnValue().SetUndefined();
}

static void js_context_fillText(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
	args.GetReturnValue().SetUndefined();
}

static void js_context_measureText(const v8::FunctionCallbackInfo<v8::Value>& args) {
	/*
	 void measureText(in DOMString text);
	 */
	//assert(argumentCount==1);
	CONTEXT_FETCH_ESCAPABLE();

	Local<String> text = Local<String>::Cast(args[0]);
	String::Utf8Value utf8(text);
	float stringWidth = __context->measureText(*utf8);

	Local<Object> objRef = Object::New(isolate);
	objRef->Set(String::NewFromUtf8(isolate, "width"), Number::New(isolate, stringWidth));

	args.GetReturnValue().Set(scope.Escape(objRef));
}

static void js_context_drawImage(const v8::FunctionCallbackInfo<v8::Value>& args) {
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
	args.GetReturnValue().SetUndefined();
}

static void js_context_createImageData(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 ImageData createImageData(in double sw, in double sh);
	 ImageData createImageData(in ImageData imagedata);
	 */
	//assert(argumentCount==1||argumentCount==2);
	LOGI("createImageData: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

static void js_context_getImageData(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 ImageData getImageData(in double sx, in double sy, in double sw, in double sh);
	 */
	//assert(argumentCount==4);
	LOGI("getImageData: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

static void js_context_putImageData(const v8::FunctionCallbackInfo<v8::Value>& args) {
	//CONTEXT_FETCH();

	/*
	 void putImageData(in ImageData imagedata, in double dx, in double dy);
	 void putImageData(in ImageData imagedata, in double dx, in double dy, in double dirtyX, in double dirtyY, in double dirtyWidth, in double dirtyHeight);

	 */
	//assert(argumentCount==3||argumentCount==7);
	LOGI("putImageData: unimplemented stub!");
	args.GetReturnValue().SetUndefined();
}

void BGJSGLModule::js_canvas_constructor(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);

	// BGJSGLModule *objPtr = externalToClassPtr<BGJSGLModule>(args.Data());
	// The constructor gets a BGJSView object as first parameter
	if (args.Length() < 1) {
		LOGE("js_canvas_constructor got no arguments");
		args.GetReturnValue().SetUndefined();
	}
	if (!args[0]->IsObject()) {
		LOGE(
				"js_canvas_constructor got non-object as first type: %s", *(String::Utf8Value(args[0]->ToString())));
		args.GetReturnValue().SetUndefined();
	}
	Local<Object> obj = args[0]->ToObject();
	BGJSCanvasGL* canvas = new BGJSCanvasGL();
	canvas->_context = BGJSGLModule::_bgjscontext;
	Local<Value> external = obj->GetInternalField(0);
	canvas->_view = externalToClassPtr<BGJSGLView>(external);

	Local<Object> fn = Local<Function>::New(isolate, BGJSGLModule::g_classRefCanvasGL)->NewInstance();
	fn->SetInternalField(0, External::New(isolate, canvas));
	args.GetReturnValue().Set(scope.Escape(fn));
}

void BGJSGLModule::js_canvas_getContext(const v8::FunctionCallbackInfo<v8::Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);
	if (!args.This()->IsObject()) {
		LOGE("js_canvas_getContext got no this object");
		args.GetReturnValue().SetUndefined();
		return;
	}
	BGJSCanvasGL *canvas = externalToClassPtr<BGJSCanvasGL>(
			args.This()->ToObject()->GetInternalField(0));

	if (canvas->_context2d) {
	    args.GetReturnValue().Set(scope.Escape(Local<Object>::New(isolate, canvas->_context2d->_jsValue)));
	    return;
	}

	BGJSContext2dGL *context2d = new BGJSContext2dGL();
	// Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(BGJSContext::_context));
	Local<Function> gClassRefLocal = *reinterpret_cast<Local<Function>*>(&BGJSGLModule::g_classRefContext2dGL);
	Local<Object> jsObj = gClassRefLocal->NewInstance();
	BGJS_RESET_PERSISTENT(isolate, context2d->_jsValue, jsObj);
	context2d->_jsValue.SetWeak<BGJSContext2dGL>(context2d, js_context_destruct, WeakCallbackType::kInternalFields);
	context2d->context = canvas->_view->context2d;
	jsObj->SetInternalField(0, External::New(isolate, context2d));
	canvas->_context2d = context2d;

	args.GetReturnValue().Set(scope.Escape(jsObj));
}

void BGJSGLModule::js_context_destruct(const v8::WeakCallbackInfo<BGJSContext2dGL>& data) {
	LOGD("js context destruct %p", data);
	// delete (BGJSContext2dGL*)data.GetInternalField(0);
	BGJS_CLEAR_PERSISTENT(data.GetParameter()->_jsValue);
	delete (data.GetParameter());
	/* BGJSContext2dGL *context2d = data.GetParameter();
	context2d->_jsValue.Reset();
	// assert(value.IsNearDeath());
	delete context2d; */
	// TODO: Also destroy canvas instance?
}

void BGJSGLModule::doRequire(v8::Isolate* isolate, v8::Handle<v8::Object> target) {
	v8::Locker l(isolate);
	HandleScope scope(isolate);

	// Handle<Object> exports = Object::New();

	// Create the template for the fake HTMLCanvasElement
	v8::Local<v8::FunctionTemplate> bgjshtmlft = v8::FunctionTemplate::New(isolate);
	bgjshtmlft->SetClassName(String::NewFromUtf8(isolate, "HTMLCanvasElement"));
	v8::Local<v8::ObjectTemplate> bgjshtmlit = bgjshtmlft->InstanceTemplate();
	bgjshtmlit->SetInternalFieldCount(1);
	bgjshtmlit->SetAccessor(String::NewFromUtf8(isolate, "width"), BGJSCanvasGL::getWidth,
			BGJSCanvasGL::setWidth);
	bgjshtmlit->SetAccessor(String::NewFromUtf8(isolate, "height"), BGJSCanvasGL::getHeight,
			BGJSCanvasGL::setHeight);
    bgjshtmlit->SetAccessor(String::NewFromUtf8(isolate, "devicePixelRatio"), BGJSCanvasGL::getPixelRatio,
			BGJSCanvasGL::setPixelRatio);

	// Call on construction
	bgjshtmlit->SetCallAsFunctionHandler(BGJSGLModule::js_canvas_constructor);
	bgjshtmlit->Set(String::NewFromUtf8(isolate, "getContext"),
			FunctionTemplate::New(isolate, BGJSGLModule::js_canvas_getContext));

	// bgjsgl->Set(String::NewFromUtf8(isolate, "log"), FunctionTemplate::New(BGJSGLModule::log));
	Local<Function> instance = bgjshtmlft->GetFunction();
	BGJS_RESET_PERSISTENT(isolate, BGJSGLModule::g_classRefCanvasGL, instance);
	Handle<Object> exports = instance->NewInstance();

	// Create the template for Canvas objects
	Local<FunctionTemplate> canvasft = FunctionTemplate::New(isolate);
	canvasft->SetClassName(String::NewFromUtf8(isolate, "CanvasRenderingContext2D"));
	Local<ObjectTemplate> canvasot = canvasft->InstanceTemplate();
	canvasot->SetInternalFieldCount(1);

	// Attributes
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "fillStyle"), js_context_get_fillStyle,
			js_context_set_fillStyle);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "strokeStyle"),
			js_context_get_strokeStyle, js_context_set_strokeStyle);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "textAlign"), js_context_get_textAlign,
			js_context_set_textAlign);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "textBaseline"),
			js_context_get_textBaseline, js_context_set_textBaseline);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "font"), js_context_get_font,
			js_context_set_font);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "lineWidth"), js_context_get_lineWidth,
			js_context_set_lineWidth);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "globalAlpha"),
			js_context_get_globalAlpha, js_context_set_globalAlpha);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "lineJoin"), js_context_get_lineJoin,
			js_context_set_lineJoin);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "lineCap"), js_context_get_lineCap,
			js_context_set_lineCap);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "miterLimit"), js_context_get_miterLimit,
			js_context_set_miterLimit);
	canvasot->SetAccessor(String::NewFromUtf8(isolate, "globalCompositeOperation"), js_context_get_globalCompositeOperation, js_context_set_globalCompositeOperation);

	// Functions
	canvasot->Set(String::NewFromUtf8(isolate, "beginPath"),
			FunctionTemplate::New(isolate, js_context_beginPath));
	canvasot->Set(String::NewFromUtf8(isolate, "closePath"),
			FunctionTemplate::New(isolate, js_context_closePath));
	canvasot->Set(String::NewFromUtf8(isolate, "moveTo"),
			FunctionTemplate::New(isolate, js_context_moveTo));
	canvasot->Set(String::NewFromUtf8(isolate, "lineTo"),
			FunctionTemplate::New(isolate, js_context_lineTo));
	canvasot->Set(String::NewFromUtf8(isolate, "stroke"),
			FunctionTemplate::New(isolate, js_context_stroke));
	canvasot->Set(String::NewFromUtf8(isolate, "fill"), FunctionTemplate::New(isolate, js_context_fill));
	canvasot->Set(String::NewFromUtf8(isolate, "save"), FunctionTemplate::New(isolate, js_context_save));
	canvasot->Set(String::NewFromUtf8(isolate, "restore"),
			FunctionTemplate::New(isolate, js_context_restore));
	canvasot->Set(String::NewFromUtf8(isolate, "scale"),
			FunctionTemplate::New(isolate, js_context_scale));
	canvasot->Set(String::NewFromUtf8(isolate, "rotate"),
			FunctionTemplate::New(isolate, js_context_rotate));
	canvasot->Set(String::NewFromUtf8(isolate, "translate"),
			FunctionTemplate::New(isolate, js_context_translate));
	canvasot->Set(String::NewFromUtf8(isolate, "transform"),
			FunctionTemplate::New(isolate, js_context_transform));
	canvasot->Set(String::NewFromUtf8(isolate, "setTransform"),
			FunctionTemplate::New(isolate, js_context_setTransform));
	canvasot->Set(String::NewFromUtf8(isolate, "createLinearGradient"),
			FunctionTemplate::New(isolate, js_context_createLinearGradient));
	canvasot->Set(String::NewFromUtf8(isolate, "createPattern"),
			FunctionTemplate::New(isolate, js_context_createPattern));
	canvasot->Set(String::NewFromUtf8(isolate, "clearRect"),
			FunctionTemplate::New(isolate, js_context_clearRect));
	canvasot->Set(String::NewFromUtf8(isolate, "fillRect"),
			FunctionTemplate::New(isolate, js_context_fillRect));
	canvasot->Set(String::NewFromUtf8(isolate, "strokeRect"),
			FunctionTemplate::New(isolate, js_context_strokeRect));
	canvasot->Set(String::NewFromUtf8(isolate, "quadraticCurveTo"),
			FunctionTemplate::New(isolate, js_context_quadraticCurveTo));
	canvasot->Set(String::NewFromUtf8(isolate, "bezierCurveTo"),
			FunctionTemplate::New(isolate, js_context_bezierCurveTo));
	canvasot->Set(String::NewFromUtf8(isolate, "arcTo"),
			FunctionTemplate::New(isolate, js_context_arcTo));
	canvasot->Set(String::NewFromUtf8(isolate, "rect"), FunctionTemplate::New(isolate, js_context_rect));
	canvasot->Set(String::NewFromUtf8(isolate, "arc"), FunctionTemplate::New(isolate, js_context_arc));
	canvasot->Set(String::NewFromUtf8(isolate, "drawSystemFocusRing"),
			FunctionTemplate::New(isolate, js_context_drawSystemFocusRing));
	canvasot->Set(String::NewFromUtf8(isolate, "drawCustomFocusRing"),
			FunctionTemplate::New(isolate, js_context_drawCustomFocusRing));
	canvasot->Set(String::NewFromUtf8(isolate, "scrollPathIntoView"),
			FunctionTemplate::New(isolate, js_context_scrollPathIntoView));
	canvasot->Set(String::NewFromUtf8(isolate, "clip"), FunctionTemplate::New(isolate, js_context_clip));
	canvasot->Set(String::NewFromUtf8(isolate, "clipRect"), FunctionTemplate::New(isolate, js_context_clipRect));
	canvasot->Set(String::NewFromUtf8(isolate, "isPointInPath"),
			FunctionTemplate::New(isolate, js_context_isPointInPath));
	canvasot->Set(String::NewFromUtf8(isolate, "fillText"),
			FunctionTemplate::New(isolate, js_context_fillText));
	canvasot->Set(String::NewFromUtf8(isolate, "strokeText"),
			FunctionTemplate::New(isolate, js_context_strokeText));
	canvasot->Set(String::NewFromUtf8(isolate, "measureText"),
			FunctionTemplate::New(isolate, js_context_measureText));
	canvasot->Set(String::NewFromUtf8(isolate, "drawImage"),
			FunctionTemplate::New(isolate, js_context_drawImage));
	canvasot->Set(String::NewFromUtf8(isolate, "createImageData"),
			FunctionTemplate::New(isolate, js_context_createImageData));
	canvasot->Set(String::NewFromUtf8(isolate, "getImageData"),
			FunctionTemplate::New(isolate, js_context_getImageData));
	canvasot->Set(String::NewFromUtf8(isolate, "putImageData"),
			FunctionTemplate::New(isolate, js_context_putImageData));
	canvasot->Set(String::NewFromUtf8(isolate, "clipY"),
			FunctionTemplate::New(isolate, js_context_clipY));

	BGJS_RESET_PERSISTENT(isolate, g_classRefContext2dGL, canvasft->GetFunction());
	// g_classRefContext2dGL

	target->Set(String::NewFromUtf8(isolate, "exports"), exports);
}

v8::Local<v8::Value> BGJSGLModule::initWithContext(Isolate* isolate, const BGJSContext* context) {
	v8::Locker l(isolate);
	EscapableHandleScope scope(isolate);

	doRegister(isolate, context);

	return v8::Undefined(isolate);
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

JNIEXPORT jlong JNICALL Java_ag_boersego_bgjs_ClientAndroid_createGL(JNIEnv * env,
		jobject obj, jlong ctxPtr, jobject javaGlView, jfloat pixelRatio, jboolean noClearOnFlip, jint width, jint height) {
    LOGD("createGL started");
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	Isolate* isolate = ct->getIsolate();
    LOGD("createGL: isolate is %p", isolate);
    v8::Locker l(isolate);
	Isolate::Scope isolate_scope(isolate);
    HandleScope scope(isolate);
    Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(ct->_context));

	BGJSGLView *view = new BGJSGLView(isolate, BGJSGLModule::_bgjscontext, pixelRatio, noClearOnFlip, width, height);
	view->setJavaGl(env, env->NewGlobalRef(javaGlView));

	// Register GLView with context so that cancelAnimationRequest works.
	ct->registerGLView(view);

	return (jlong) view;
}

JNIEXPORT int JNICALL Java_ag_boersego_bgjs_ClientAndroid_init(JNIEnv * env,
		jobject obj, jlong ctxPtr, jlong objPtr, jint width, jint height, jstring callbackName) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	Isolate* isolate = ct->getIsolate();
	v8::Locker l(isolate);
	Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);

#ifdef DEBUG
	LOGI("setupGraphics(%d, %d)", width, height);
#endif
    Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(ct->_context));

	BGJSGLView *view = (BGJSGLView*) objPtr;

	if (width != view->width || height != view->height || !view->opened) {
#ifdef DEBUG
		LOGD("Resizing from %dx%d to %dx%d, resizeOnly %i", view->width, view->height, width, height, (int)(view->opened));
#endif
		view->resize(isolate, width, height, view->opened);
	}
	Handle<Value> uiObj;

	// Only call this once!
	if (!view->opened) {
		const char* cbStr = env->GetStringUTFChars(callbackName, NULL);

		LOGI("setupGraphics(%s)", cbStr);
		Local<Value> res = view->startJS(isolate, cbStr, NULL, v8::Undefined(isolate), 0, false);
		view->opened = true;
		env->ReleaseStringUTFChars(callbackName, cbStr);

		if (res->IsNumber()) {
			return res->ToNumber()->Value();
		}
	}
	return -1;
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_close(JNIEnv * env,
		jobject obj, jlong ctxPtr, jlong objPtr) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	Isolate* isolate = ct->getIsolate();
	v8::Locker l(isolate);
	Isolate::Scope isolateScope(isolate);
    HandleScope scope(isolate);

	Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(ct->_context));

	BGJSGLView *view = (BGJSGLView*) objPtr;
	view->close();

	ct->unregisterGLView(view);
	env->DeleteGlobalRef(view->_javaGlView);
	delete (view);
}

const GLfloat gTriangleVertices[] = { 0.0f, 0.5f, -0.5f, -0.5f, 0.5f, -0.5f };

JNIEXPORT bool JNICALL Java_ag_boersego_bgjs_ClientAndroid_step(JNIEnv * env,
		jobject obj, jlong ctxPtr, jlong jsPtr) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	BGJSGLView *view = (BGJSGLView*) jsPtr;
	return BGJSGLModule::_bgjscontext->runAnimationRequests(view);
}

JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_redraw(JNIEnv * env,
		jobject obj, jlong ctxPtr, jlong jsPtr) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	BGJSGLView *view = (BGJSGLView*) jsPtr;
	return view->call(ct->getIsolate(), view->_cbRedraw);
}


JNIEXPORT void JNICALL Java_ag_boersego_bgjs_ClientAndroid_sendTouchEvent(
		JNIEnv * env, jobject obj, jlong ctxPtr, jlong objPtr, jstring typeStr,
		jfloatArray xArr, jfloatArray yArr, jfloat scale) {
	BGJSContext* ct = (BGJSContext*) ctxPtr;
	Isolate* isolate = ct->getIsolate();
	v8::Locker l(isolate);
	Isolate::Scope isolateScope(isolate);
	Context::Scope context_scope(*reinterpret_cast<Local<Context>*>(ct->_context));
    HandleScope scope(isolate);

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

    // Local<Context> v8Context = ct->_context.Get(isolate);
	BGJSGLView *view = (BGJSGLView*) objPtr;

	// Create event object
	Handle<Object> eventObjRef = Object::New(isolate);
	eventObjRef->Set(String::NewFromUtf8(isolate, "type"), String::NewFromUtf8(isolate, type));
	eventObjRef->Set(String::NewFromUtf8(isolate, "scale"), Number::New(isolate, scale));

	Handle<String> pageX = String::NewFromUtf8(isolate, "clientX");
	Handle<String> pageY = String::NewFromUtf8(isolate, "clientY");

	Handle<Array> touchesArray = Array::New(isolate, count);

	// Populate touches array
	for (int i = 0; i < count; i++) {
		Handle<Object> touchObjRef = Object::New(isolate);
		touchObjRef->Set(pageX, Number::New(isolate, x[i]));
		touchObjRef->Set(pageY, Number::New(isolate, y[i]));
		touchesArray->Set(Number::New(isolate, i), touchObjRef);
	}

	eventObjRef->Set(String::NewFromUtf8(isolate, "touches"), touchesArray);

	view->sendEvent(isolate, eventObjRef);

	// Cleanup JNI stuff
	env->ReleaseFloatArrayElements(xArr, x, 0);
	env->ReleaseFloatArrayElements(yArr, y, 0);
	env->ReleaseStringUTFChars(typeStr, type);
}

