#ifndef __BGJSCANVASCONTEXT_H
#define __BGJSCANVASCONTEXT_H	1

#define MSAA_SAMPLES 2

#include "../ejecta/EJCanvas/EJCanvasContext.h"
#include "../ejecta/EJCanvas/CGCompat.h"

/**
 * BGJSCanvasContext
 * Manages a JS Canvas object and its OpenGL state. Loosely based on EJCanvasContext
 *
 * Copyright 2014 Kevin Read <me@kevin-read.com> and BörseGo AG (https://github.com/godmodelabs/ejecta-v8/)
 * Licensed under the MIT license.
 */

typedef struct {
	CGRect clipRect;
	bool clip;
} BGJSCanvasState;

class BGJSCanvasContext : public EJCanvasContext {
	BGJSCanvasState stateStack2[EJ_CANVAS_STATE_STACK_SIZE];
	BGJSCanvasState * state2;

public:
	BGJSCanvasContext(int width, int height);
	void resize(int width, int height, bool resizeOnly);
	void save();
	void restore();
	void clipY (float y, float y2);
	void clipRect (CGRect rect);
	void startRendering();
	void endRendering();

	bool _isRendering;
};

#endif
