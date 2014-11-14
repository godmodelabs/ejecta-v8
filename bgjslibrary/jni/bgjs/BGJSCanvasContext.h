#ifndef __BGJSCANVASCONTEXT_H
#define __BGJSCANVASCONTEXT_H	1

#define MSAA_SAMPLES 2

#include "../ejecta/EJCanvas/EJCanvasContext.h"

typedef struct {
	float clipY1,clipY2;
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
	void startRendering();
	void endRendering();

	bool _isRendering;
};

#endif
