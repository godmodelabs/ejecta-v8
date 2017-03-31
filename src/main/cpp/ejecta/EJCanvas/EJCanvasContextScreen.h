#ifndef __EJCANVASCONTEXTSCREEN_H
#define __EJCANVASCONTEXTSCREEN_H 1

#include "EJCanvasContext.h"
#include "EJImageData.h"

typedef enum {
	kEJScalingModeNone,
	kEJScalingModeFitWidth,
	kEJScalingModeFitHeight
} EJScalingMode;

class EJCanvasContextScreen : public EJCanvasContext {
	GLuint colorRenderbuffer;

	bool useRetinaResolution;
	// UIDeviceOrientation orientation;
	EJScalingMode scalingMode;
	float backingStoreRatio;

public:
	EJCanvasContextScreen(short widthp, short heightp);
	void prepare();
	void present();
	void finish();
	void resetGLContext();
	EJImageData* getImageDataSx(float, float, float, float);
};


#endif
