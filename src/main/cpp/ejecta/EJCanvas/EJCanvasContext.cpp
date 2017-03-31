#include "EJCanvasContext.h"
#include "EJFont.h"

#include "mallocdebug.h"

#include "NdkMisc.h"
#define LOG_TAG	"EJCanvasContext"
#include <stdio.h>
#include <string.h>

EJVertex CanvasVertexBuffer[EJ_CANVAS_VERTEX_BUFFER_SIZE];

#undef EJ_MSAA

// #define SIMPLE	1

EJCanvasContext::EJCanvasContext (short widthp, short heightp) {
	std::memset(stateStack, 0, sizeof(stateStack));
	stateIndex = 0;
	state = &stateStack[stateIndex];
	state->globalAlpha = 1;
	state->fontName = NULL;
	state->globalCompositeOperation = kEJCompositeOperationSourceOver;
	state->transform = CGAffineTransformIdentity;
	state->lineWidth = 1;
	state->lineCap = kEJLineCapButt;
	state->lineJoin = kEJLineJoinMiter;
	state->miterLimit = 10;
	state->textBaseline = kEJTextBaselineAlphabetic;
	state->textAlign = kEJTextAlignStart;
	state->fontName = "Arial";
	state->fontSize = 10;
	vertexBufferBound = false;

	bufferWidth = viewportWidth = width = widthp;
	bufferHeight = viewportHeight = height = heightp;

	path = new EJPath();
	backingStoreRatio = 1;
	_font = NULL;

	// TODO: Font
	// fontCache = [[NSCache alloc] init];
	// fontCache.countLimit = 8;

	msaaEnabled = NO;
	msaaSamples = 2;
	// scaleX(1, 1);
}

static void checkGlError(const char* op) {
#ifdef DEBUG_GL
	for (GLint error = glGetError(); error; error = glGetError()) {
		LOGI("after %s() glError (0x%x)\n", op, error);
	}
#endif
}

EJCanvasContext* EJCanvasContext::initWithWidth (short widthp, short heightp) {
	EJCanvasContext* self = new EJCanvasContext(widthp, heightp);

	memset(stateStack, 0, sizeof(stateStack));
	stateIndex = 0;
	state = &stateStack[stateIndex];
	state->globalAlpha = 1;
	state->globalCompositeOperation = kEJCompositeOperationSourceOver;
	state->transform = CGAffineTransformIdentity;
	state->lineWidth = 1;
	state->fontName = NULL;
	state->lineCap = kEJLineCapButt;
	state->lineJoin = kEJLineJoinMiter;
	state->miterLimit = 10;
	state->textBaseline = kEJTextBaselineAlphabetic;
	state->textAlign = kEJTextAlignStart;
	state->fontName = "Arial";
	state->fontSize = 10;
	vertexBufferBound = false;

	bufferWidth = viewportWidth = width = widthp;
	bufferHeight = viewportHeight = height = heightp;

	path = new EJPath();
	backingStoreRatio = 1;
	_font = NULL;

	// TODO: Font
	// fontCache = [[NSCache alloc] init];
	// fontCache.countLimit = 8;

	msaaEnabled = NO;
	msaaSamples = 2;
	// scaleX(1, 1);
	return self;
}

EJCanvasContext::~EJCanvasContext() {
	// TODO: Font
	/* if (fontCache) {
		delete fontCache;
	}

	// Release all fonts from the stack
	for( int i = 0; i < stateIndex + 1; i++ ) {
		[stateStack[i].font release];
	} */

	if( viewFrameBuffer ) { COMPAT_glDeleteFramebuffers( 1, &viewFrameBuffer); }
	if( viewRenderBuffer ) { COMPAT_glDeleteRenderbuffers(1, &viewRenderBuffer); }
	if( msaaFrameBuffer ) {	COMPAT_glDeleteFramebuffers( 1, &msaaFrameBuffer); }
	if( msaaRenderBuffer ) { COMPAT_glDeleteRenderbuffers(1, &msaaRenderBuffer); }
	if( stencilBuffer ) { COMPAT_glDeleteRenderbuffers(1, &stencilBuffer); }

	delete path;
}

void EJCanvasContext::create () {
#ifdef EJ_MSAA
	if( msaaEnabled ) {
		glGenFramebuffers(1, &msaaFrameBuffer);
		glBindFramebuffer(COMPAT_GL_FRAMEBUFFER, msaaFrameBuffer);

		glGenRenderbuffers(1, &msaaRenderBuffer);
		glBindRenderbuffer(COMPAT_GL_RENDERBUFFER, msaaRenderBuffer);

		glRenderbufferStorageMultisampleAPPLE(COMPAT_GL_RENDERBUFFER, msaaSamples, GL_RGBA8_OES, bufferWidth, bufferHeight);
		glFramebufferRenderbuffer(COMPAT_GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, COMPAT_GL_RENDERBUFFER, msaaRenderBuffer);
	}
#endif

#ifndef SIMPLE_STENCIL
	// COMPAT_glGenFramebuffers(1, &viewFrameBuffer);
	// COMPAT_glBindFramebuffer(COMPAT_GL_FRAMEBUFFER, viewFrameBuffer);
	viewFrameBuffer = (GLuint)0;

	/* COMPAT_glGenRenderbuffers(1, &viewRenderBuffer);
	COMPAT_glBindRenderbuffer(COMPAT_GL_RENDERBUFFER, viewRenderBuffer); */
	viewRenderBuffer = (GLuint)0;
	scaleX(1, 1);
#endif
}

void EJCanvasContext::createStencilBufferOnce () {
	if( stencilBuffer ) { return; }

	#ifndef SIMPLE_STENCIL
	COMPAT_glGenRenderbuffers(1, &stencilBuffer);
	checkGlError("COMPAT_glGenRenderbuffers(createStencilBufferOnce)");
	COMPAT_glBindRenderbuffer(COMPAT_GL_RENDERBUFFER, stencilBuffer);
	checkGlError("COMPAT_glBindRenderbuffer(createStencilBufferOnce)");

#ifdef EJ_MSAA
	if( msaaEnabled ) {
		glRenderbufferStorageMultisampleAPPLE(COMPAT_GL_RENDERBUFFER, msaaSamples, GL_STENCIL_INDEX8_OES, bufferWidth, bufferHeight);
	}
	else {
#endif
		glRenderbufferStorageOES(COMPAT_GL_RENDERBUFFER, GL_STENCIL_INDEX8_OES, bufferWidth, bufferHeight);
#ifdef EJ_MSAA
	}
#endif
	COMPAT_glFramebufferRenderbuffer(COMPAT_GL_FRAMEBUFFER, COMPAT_GL_STENCIL_ATTACHMENT, COMPAT_GL_RENDERBUFFER, stencilBuffer);
	checkGlError("COMPAT_glFramebufferRenderbuffer(createStencilBufferOnce)");

	// COMPAT_glBindRenderbuffer(COMPAT_GL_RENDERBUFFER, msaaEnabled ? msaaRenderBuffer : viewRenderBuffer );

	glClear(GL_STENCIL_BUFFER_BIT);
	checkGlError("glClear(createStencilBufferOnce)");
#endif
}

void EJCanvasContext::bindVertexBuffer() {
	glVertexPointer(2, GL_FLOAT, sizeof(EJVertex), &CanvasVertexBuffer[0].pos.x);
	checkGlError("glVertexPointer(bindVertexBuffer)");
	glTexCoordPointer(2, GL_FLOAT, sizeof(EJVertex), &CanvasVertexBuffer[0].uv.x);
	checkGlError("glTexCoordPointer(bindVertexBuffer)");
	glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(EJVertex), &CanvasVertexBuffer[0].color);
	checkGlError("glColorPointer(bindVertexBuffer)");

	glEnableClientState(GL_VERTEX_ARRAY);
	checkGlError("glEnableClientState(bindVertexBuffer)");
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	checkGlError("glEnableClientState(bindVertexBuffer)");
	glEnableClientState(GL_COLOR_ARRAY);
	checkGlError("glEnableClientState(bindVertexBuffer)");

	vertexBufferIndex = 0;
}

void EJCanvasContext::setState() {
	EJCompositeOperation op = state->globalCompositeOperation;
	glBlendFunc( EJCompositeOperationFuncs[op].source, EJCompositeOperationFuncs[op].destination );
	checkGlError("glBlendFunc(prepare)");
}

void EJCanvasContext::prepare() {
	// Bind the frameBuffer and vertexBuffer array
#ifndef SIMPLE_STENCIL
	/* COMPAT_glBindFramebuffer(COMPAT_GL_FRAMEBUFFER, msaaEnabled ? msaaFrameBuffer : viewFrameBuffer );
	COMPAT_glBindRenderbuffer(COMPAT_GL_RENDERBUFFER, msaaEnabled ? msaaRenderBuffer : viewRenderBuffer ); */
#endif

	glViewport(0, 0, viewportWidth, viewportHeight);
	LOGD("prepare. New viewport %ux%u for projection %ux%u", viewportWidth, viewportHeight, width, height);
	checkGlError("glViewport(prepare)");

	glMatrixMode(GL_PROJECTION);
	checkGlError("glMatrixMode(prepare)");
	glLoadIdentity();
	checkGlError("glLoadIdentity(prepare)");
	glOrthof(0, width, 0, height, -1, 1);
	checkGlError("glOrthof(prepare)");

	glMatrixMode(GL_MODELVIEW);
	checkGlError("glMatrixMode(prepare)");
	glLoadIdentity();
	checkGlError("glLoadIdentity(prepare)");

#ifndef SIMPLE
	EJCompositeOperation op = state->globalCompositeOperation;
	glBlendFunc( EJCompositeOperationFuncs[op].source, EJCompositeOperationFuncs[op].destination );
	checkGlError("glBlendFunc(prepare)");
#endif
	glDisable(GL_TEXTURE_2D);
	checkGlError("glDisable(prepare)");
	currentTexture = NULL;

	if (!vertexBufferBound) {
		this->bindVertexBuffer();
		vertexBufferBound = true;
	}
}

void EJCanvasContext::setTexture (EJTexture *newTexture) {
	if( currentTexture == newTexture ) { return; }

	this->flushBuffers();

	if( !newTexture && currentTexture ) {
		// Was enabled; should be disabled
		glDisable(GL_TEXTURE_2D);
	}
	else if( newTexture && !currentTexture ) {
		// Was disabled; should be enabled
		glEnable(GL_TEXTURE_2D);
	}

	currentTexture = newTexture;
	if (currentTexture) {
		currentTexture->bind();
	}
}

void EJCanvasContext::pushTriX1(float x1, float y1, float x2, float y2, float x3, float y3, EJColorRGBA color, CGAffineTransform transform)	{
	if( vertexBufferIndex >= EJ_CANVAS_VERTEX_BUFFER_SIZE - 3 ) {
		this->flushBuffers();
	}

	EJVector2 d1 = { x1, y1 };
	EJVector2 d2 = { x2, y2 };
	EJVector2 d3 = { x3, y3 };

	if( !CGAffineTransformIsIdentity(transform) ) {
		d1 = EJVector2ApplyTransform( d1, transform );
		d2 = EJVector2ApplyTransform( d2, transform );
		d3 = EJVector2ApplyTransform( d3, transform );
	}

	EJVertex * vb = &CanvasVertexBuffer[vertexBufferIndex];
	vb[0] = (EJVertex) { d1, {0.5, 1}, color };
	vb[1] = (EJVertex) { d2, {0.5, 0.5}, color };
	vb[2] = (EJVertex) { d3, {0.5, 1}, color };

	vertexBufferIndex += 3;
}

void EJCanvasContext::pushQuadV1 (EJVector2 v1, EJVector2 v2, EJVector2 v3, EJVector2 v4, EJVector2 t1, EJVector2 t2, EJVector2 t3, EJVector2 t4, EJColorRGBA color, CGAffineTransform transform) {
	if( vertexBufferIndex >= EJ_CANVAS_VERTEX_BUFFER_SIZE - 6 ) {
		this->flushBuffers();
	}

	if( !CGAffineTransformIsIdentity(transform) ) {
		v1 = EJVector2ApplyTransform( v1, transform );
		v2 = EJVector2ApplyTransform( v2, transform );
		v3 = EJVector2ApplyTransform( v3, transform );
		v4 = EJVector2ApplyTransform( v4, transform );
	}

	EJVertex * vb = &CanvasVertexBuffer[vertexBufferIndex];
	vb[0] = (EJVertex) { v1, t1, color };
	vb[1] = (EJVertex) { v2, t2, color };
	vb[2] = (EJVertex) { v3, t3, color };
	vb[3] = (EJVertex) { v2, t2, color };
	vb[4] = (EJVertex) { v3, t3, color };
	vb[5] = (EJVertex) { v4, t4, color };

	vertexBufferIndex += 6;
}

void EJCanvasContext::pushRectX(float x, float y, float w, float h, float tx, float ty, float tw, float th, EJColorRGBA color, CGAffineTransform transform) {
	if( vertexBufferIndex >= EJ_CANVAS_VERTEX_BUFFER_SIZE - 6 ) {
		this->flushBuffers();
	}

	EJVector2 d11 = { x, y };
	EJVector2 d21 = { x+w, y };
	EJVector2 d12 = { x, y+h };
	EJVector2 d22 = { x+w, y+h };

	if( !CGAffineTransformIsIdentity(transform) ) {
		d11 = EJVector2ApplyTransform( d11, transform );
		d21 = EJVector2ApplyTransform( d21, transform );
		d12 = EJVector2ApplyTransform( d12, transform );
		d22 = EJVector2ApplyTransform( d22, transform );
	}

	EJVertex * vb = &CanvasVertexBuffer[vertexBufferIndex];
	vb[0] = (EJVertex) { d11, {tx, ty}, color };	// top left
	vb[1] = (EJVertex) { d21, {tx+tw, ty}, color };	// top right
	vb[2] = (EJVertex) { d12, {tx, ty+th}, color };	// bottom left

	vb[3] = (EJVertex) { d21, {tx+tw, ty}, color };	// top right
	vb[4] = (EJVertex) { d12, {tx, ty+th}, color };	// bottom left
	vb[5] = (EJVertex) { d22, {tx+tw, ty+th}, color };// bottom right

	vertexBufferIndex += 6;
}

void EJCanvasContext::flushBuffers() {
	if( vertexBufferIndex == 0 ) { return; }

	glDrawArrays(GL_TRIANGLES, 0, vertexBufferIndex);
	vertexBufferIndex = 0;
}

void EJCanvasContext::setGlobalCompositeOperation (EJCompositeOperation op) {
	this->flushBuffers();
	glBlendFunc( EJCompositeOperationFuncs[op].source, EJCompositeOperationFuncs[op].destination );
	state->globalCompositeOperation = op;
}

EJCompositeOperation EJCanvasContext::globalCompositeOperation ( ){
	return state->globalCompositeOperation;
}

// TODO: Font

bool EJCanvasContext::setFont (char* font) {
	// TODO: If we use dynamic fonts later, destroy the old font object here
	/* if (state->fontName) {
		free(state->fontName);
	} */
	state->fontName = font;
	return false;
}

char* EJCanvasContext::getFont() {
	return state->fontName;
}


void EJCanvasContext::save () {
	if( stateIndex == EJ_CANVAS_STATE_STACK_SIZE-1 ) {
		LOGI("Warning: EJ_CANVAS_STATE_STACK_SIZE (%d) reached", EJ_CANVAS_STATE_STACK_SIZE);
		return;
	}
	stateStack[stateIndex+1] = stateStack[stateIndex];
	stateIndex++;
	state = &stateStack[stateIndex];
	// TODO: Font
	// [state->font retain];
}

void EJCanvasContext::restore() {
	if( stateIndex == 0 ) {
		LOGI("Warning: Can't pop stack at index 0");
		return;
	}
	EJCompositeOperation oldCompositeOp = state->globalCompositeOperation;

	/* if (state->fontName) {
		free(state->fontName);
		state->fontName = NULL;
	} */

	stateIndex--;
	state = &stateStack[stateIndex];

    path->transform = state->transform;

	if( state->globalCompositeOperation != oldCompositeOp ) {
		this->setGlobalCompositeOperation (state->globalCompositeOperation);
	}
}

void EJCanvasContext::rotate (float angle) {
	state->transform = CGAffineTransformRotate( state->transform, angle );
    path->transform = state->transform;
}

void EJCanvasContext::translateX (float x, float y) {
	state->transform = CGAffineTransformTranslate( state->transform, x, y );
    path->transform = state->transform;
}

void EJCanvasContext::scaleX (float x, float y) {
	state->transform = CGAffineTransformScale( state->transform, x, y);
	path->transform = state->transform;
}

void EJCanvasContext::transformM11(float m11, float m12, float m21, float m22, float dx, float dy) {
	CGAffineTransform t = CGAffineTransformMake( m11, m12, m21, m22, dx, dy );
	state->transform = CGAffineTransformConcat( state->transform, t );
	path->transform = state->transform;
}

void EJCanvasContext::setTransformM11(float m11, float m12, float m21, float m22, float dx, float dy) {
	state->transform = CGAffineTransformMake( m11, m12, m21, m22, dx, dy );
	path->transform = state->transform;
}

void EJCanvasContext::drawImage (EJTexture *texture, float sx, float sy,
		float sw, float sh, float dx, float dy, float dw, float dh) {

	float tw = texture->realWidth;
	float th = texture->realHeight;

	EJColorRGBA color = {{255, 255, 255, (unsigned char)(255 * state->globalAlpha)}};
	this->setTexture(texture);
	this->pushRectX(dx, dy, dw, dh, sx/tw, sy/th, sw/tw, sh/th, color, state->transform);
}

void EJCanvasContext::fillRectX (float x, float y, float w, float h) {
	this->setTexture(NULL);

	EJColorRGBA color = state->fillColor;
	color.rgba.a = (float)color.rgba.a * state->globalAlpha;
	// LOGD("fillRect. (%f, %f) w %f h %f  rgba(%d,%d,%d,%.3f)", x, y, w, h, color.rgba.r, color.rgba.g, color.rgba.b, (float)color.rgba.a/255.0f);
	this->pushRectX (x, y, w, h, 0, 0, 0, 0, color, state->transform);
	// [self pushRectX:x y:y w:w h:h tx:0 ty:0 tw:0 th:0 color:color withTransform:state->transform];
}

void EJCanvasContext::strokeRectX (float x, float y, float w, float h) {
	// strokeRect should not affect the current path, so we create
	// a new, tempPath instead.
	EJPath * tempPath = new EJPath();
	tempPath->transform = state->transform;

	tempPath->moveToX (x, y);
	tempPath->lineToX (x+w, y);
	tempPath->lineToX (x+w, y+h);
	tempPath->lineToX (x, y+h);
	tempPath->close();

	tempPath->drawLinesToContext(this);
	delete tempPath;

	/* [tempPath moveToX:x y:y];
	[tempPath lineToX:x+w y:y];
	[tempPath lineToX:x+w y:y+h];
	[tempPath lineToX:x y:y+h];
	[tempPath close];

	[tempPath drawLinesToContext:self];
	[tempPath release]; */
}

void EJCanvasContext::clearRectX (float x, float y, float w, float h) {
	this->setTexture(NULL);

	EJCompositeOperation oldOp = state->globalCompositeOperation;
	this->setGlobalCompositeOperation(kEJCompositeOperationDestinationOut);

	static EJColorRGBA white = { {255, 255, 255, 255 }};
	this->pushRectX (x, y, w, h, 0, 0, 0, 0, white, state->transform);
	// [self pushRectX:x y:y w:w h:h tx:0 ty:0 tw:0 th:0 color:white withTransform:state->transform];

	this->setGlobalCompositeOperation(oldOp);
}

EJImageData* EJCanvasContext::getImageDataSx (float sx, float sy, float sw, float sh) {
	this->flushBuffers();
	GLubyte * pixels = (GLubyte*)malloc( sw * sh * 4 * sizeof(GLubyte));
	glReadPixels(sx, sy, sw, sh, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	EJImageData *data = EJImageData::initWithWidth(sw, sh, pixels);
	return data;
	// return [[[EJImageData alloc] initWithWidth:sw height:sh pixels:pixels] autorelease];
}

void EJCanvasContext::putImageData (EJImageData* imageData, float dx, float dy) {
	EJTexture * texture = imageData->getTexture();
	this->setTexture(texture);

	short tw = texture->realWidth;
	short th = texture->realHeight;

	static EJColorRGBA white = { { 255, 255, 255, 255 } };
	this->pushRectX (dx, dy, tw, th, 0, 0, 1, 1, white, CGAffineTransformIdentity);

	// [self pushRectX:dx y:dy w:tw h:th tx:0 ty:0 tw:1 th:1 color:white withTransform:CGAffineTransformIdentity];
	this->flushBuffers();
}

void EJCanvasContext::beginPath () {
	path->reset();
}

void EJCanvasContext::closePath () {
	path->close();
}

void EJCanvasContext::fill () {
	path->drawPolygonsToContext(this);
}

void EJCanvasContext::stroke () {
	path->drawLinesToContext(this);
}

void EJCanvasContext::moveToX (float x, float y) {
	path->moveToX(x,y);
}

void EJCanvasContext::lineToX (float x, float y) {
	path->lineToX (x, y);
}

void EJCanvasContext::bezierCurveToCpx1(float cpx1, float cpy1, float cpx2, float cpy2, float x, float y) {
	float scale = CGAffineTransformGetScale( state->transform );
	path->bezierCurveToCpx1 (cpx1, cpy1, cpx2, cpy2, x, y, scale);
	// [path bezierCurveToCpx1:cpx1 cpy1:cpy1 cpx2:cpx2 cpy2:cpy2 x:x y:y scale:scale];
}

void EJCanvasContext::quadraticCurveToCpx (float cpx, float cpy, float x, float y) {
	float scale = CGAffineTransformGetScale( state->transform );
	path->quadraticCurveToCpx (cpx, cpy, x, y, scale);
	// [path quadraticCurveToCpx:cpx cpy:cpy x:x y:y scale:scale];
}

void EJCanvasContext::rectX (float x, float y, float w, float h) {
	path->moveToX (x, y);
	path->lineToX (x+w, y);
	path->lineToX (x+w, y+h);
	path->lineToX (x, y+h);
	path->close();}

void EJCanvasContext::arcToX1 (float x1, float y1, float x2, float y2, float radius) {
	path->arcToX1 (x1, y1, x2, y2, radius);
}

void EJCanvasContext::arcX (float x, float y, float radius, float startAngle, float endAngle, bool antiClockwise) {
	path->arcX (x, y, radius, startAngle, endAngle, antiClockwise);
	// [path arcX:x y:y radius:radius startAngle:startAngle endAngle:endAngle antiClockwise:antiClockwise];
}

EJFont* EJCanvasContext::acquireFont (char* fontName, float pointSize, bool fill, float contentScale) {

	if (state->fontName && strcmp (fontName, state->fontName) == 0 && pointSize - 0.99 <= state->fontSize && pointSize + 0.99 >= state->fontSize && _font) {
		return _font;
	}
	setFont(fontName);
	state->fontSize = pointSize;

	// This font is different, recreate
	if (_font) {
		delete(_font);
	}
	_font = new EJFont(fontName, pointSize, fill, contentScale);
	return _font;

	// TODO: Caching
	/* NSString * cacheKey = [NSString stringWithFormat:@"%@_%.2f_%d_%.2f", fontName, pointSize, fill, contentScale];
	EJFont * font = [fontCache objectForKey:cacheKey];
	if( !font ) {
		font = [[EJFont alloc] initWithFont:fontName size:pointSize fill:fill contentScale:contentScale];
		[fontCache setObject:font forKey:cacheKey];
		[font autorelease];
	}
	return font; */
}

void EJCanvasContext::fillText (const char* text, float x, float y) {
	EJFont* font = acquireFont(state->fontName, state->fontSize, true, backingStoreRatio);
	font->drawString(text, this, x, y);
	/* EJFont *font = [self acquireFont:state->font.fontName size:state->font.pointSize fill:YES contentScale:backingStoreRatio];
	[font drawString:text toContext:self x:x y:y]; */
}

void EJCanvasContext::strokeText (const char* text, float x, float y) {
	EJFont* font = acquireFont(state->fontName, state->fontSize, false, backingStoreRatio);
	font->drawString(text, this, x, y);
	/* EJFont *font = [self acquireFont:state->font.fontName size:state->font.pointSize fill:NO contentScale:backingStoreRatio];
	[font drawString:text toContext:self x:x y:y]; */
}

float EJCanvasContext::measureText (const char* text) {
	EJFont* font = acquireFont(state->fontName, state->fontSize, true, backingStoreRatio);
	return font->measureString(text);
	/* EJFont *font = [self acquireFont:state->font.fontName size:state->font.pointSize fill:YES contentScale:backingStoreRatio];
	return [font measureString:text]; */
}
