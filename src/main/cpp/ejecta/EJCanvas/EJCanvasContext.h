#ifndef __EJCANVASCONTEXT_H

#define __EJCANVASCONTEXT_H 1

#include "GLcompat.h"

#include "EJTexture.h"
#include "EJImageData.h"
#include "EJPath.h"
#include "EJCanvasTypes.h"
#include "EJFont.h"

#define EJ_CANVAS_STATE_STACK_SIZE 16
#define EJ_CANVAS_VERTEX_BUFFER_SIZE 2048

extern EJVertex CanvasVertexBuffer[EJ_CANVAS_VERTEX_BUFFER_SIZE];

typedef enum {
	kEJLineCapButt,
	kEJLineCapRound,
	kEJLineCapSquare
} EJLineCap;

typedef enum {
	kEJLineJoinMiter,
	kEJLineJoinBevel,
	kEJLineJoinRound
} EJLineJoin;

typedef enum {
	kEJTextBaselineAlphabetic,
	kEJTextBaselineMiddle,
	kEJTextBaselineTop,
	kEJTextBaselineHanging,
	kEJTextBaselineBottom,
	kEJTextBaselineIdeographic
} EJTextBaseline;

typedef enum {
	kEJTextAlignStart,
	kEJTextAlignEnd,
	kEJTextAlignLeft,
	kEJTextAlignCenter,
	kEJTextAlignRight
} EJTextAlign;

typedef enum {
	kEJCompositeOperationSourceOver,
	kEJCompositeOperationLighter,
	kEJCompositeOperationDarker,
	kEJCompositeOperationDestinationOut,
	kEJCompositeOperationDestinationOver,
	kEJCompositeOperationSourceAtop,
	kEJCompositeOperationXOR
} EJCompositeOperation;

static const struct {
	GLenum source;
	GLenum destination;
} EJCompositeOperationFuncs[] = {
	{GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA},
	{GL_SRC_ALPHA, GL_ONE},
	{GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA},
	{GL_ZERO, GL_ONE_MINUS_SRC_ALPHA},
	{GL_ONE_MINUS_DST_ALPHA, GL_ONE},
	{GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA},
	{GL_ONE_MINUS_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA}
};


typedef struct {
	CGAffineTransform transform;

	EJCompositeOperation globalCompositeOperation;
	EJColorRGBA fillColor;
	EJColorRGBA strokeColor;
	float globalAlpha;

	float lineWidth;
	EJLineCap lineCap;
	EJLineJoin lineJoin;
	float miterLimit;

	EJTextAlign textAlign;
	EJTextBaseline textBaseline;
	char* fontName;
	float fontSize;
} EJCanvasState;


class EJCanvasContext {
protected:
	GLuint viewFrameBuffer, viewRenderBuffer;
	GLuint msaaFrameBuffer, msaaRenderBuffer;
	GLuint stencilBuffer;

	short viewportWidth, viewportHeight;
	short bufferWidth, bufferHeight;

	EJTexture * currentTexture;

	EJPath *path;
	EJFont *_font;

	int vertexBufferIndex;

	int stateIndex;
	EJCanvasState stateStack[EJ_CANVAS_STATE_STACK_SIZE];

	// TODO: Font
	//NSCache * fontCache;

public:
	~EJCanvasContext();
	EJCanvasContext* initWithWidth (short width, short height);
	EJCanvasContext (short width, short height);
	void create();
	void createStencilBufferOnce();
	void bindVertexBuffer();
	void setState();
	void prepare();
	void setTexture (EJTexture *newTexture);
	void pushTriX1 (float x1, float y1, float x2, float y2, float x3, float y3, EJColorRGBA color, CGAffineTransform transform);
	void pushQuadV1 (EJVector2 v1, EJVector2 v2, EJVector2 v3, EJVector2 v4, EJVector2 t1, EJVector2 t2, EJVector2 t3, EJVector2 t4, EJColorRGBA color, CGAffineTransform transform);
	void pushRectX (float x, float y, float w, float h, float tx, float ty, float tw, float th, EJColorRGBA color, CGAffineTransform transform);
	void flushBuffers();

	void save();
	void restore();
	void rotate (float angle);
	void translateX (float x, float y);
	void scaleX (float x, float y);
	void transformM11 (float m11, float m12, float m21, float m22, float dx, float dy);
	void setTransformM11 (float m11, float m12, float m21, float m22, float dx, float dy);
	void drawImage (EJTexture *texture, float sx, float sy,
			float sw, float sh, float dx, float dy, float dw, float dh);
	void fillRectX (float x, float y, float w, float h);
	void strokeRectX (float x, float y, float w, float h);
	void clearRectX (float x, float y, float w, float h);
	EJImageData* getImageDataSx (float sx, float sy, float sw, float sh);
	void putImageData (EJImageData* imageData, float dx, float dy);
	void beginPath();
	void closePath();
	void fill();
	void stroke();
	void moveToX (float x, float y);
	void lineToX (float x, float y);
	void rectX (float x, float y, float w, float h);
	void bezierCurveToCpx1 (float cpx1, float cpy1, float cpx2, float cpy2, float x, float y);
	void quadraticCurveToCpx (float cpx, float cpy, float x, float y);
	void arcToX1 (float x1, float y1, float x2, float y2, float radius);
	void arcX (float x, float y, float radius, float startAngle, float endAngle, bool antiClockwise);

	bool setFont (char* font) const;
	char* getFont() const;
	EJFont* acquireFont (char* fontName, float pointSize, bool fill, float contentScale);
	void fillText (const char* text, float x, float y);
	void strokeText (const char* text, float x, float y);
	float measureText (const char* text);

	// Synthesized
	void setGlobalCompositeOperation (EJCompositeOperation op);
	EJCompositeOperation globalCompositeOperation();

	// Attributes
	EJCanvasState * state;
	float backingStoreRatio;
	bool msaaEnabled;
	int msaaSamples;
	short width, height;
	bool vertexBufferBound;
};

/*
- (id)initWithWidth:(short)width height:(short)height;
- (void)create;
- (void)createStencilBufferOnce;
- (void)bindVertexBuffer;
- (void)prepare;
- (void)setTexture:(EJTexture *)newTexture;
- (void)pushTriX1:(float)x1 y1:(float)y1 x2:(float)x2 y2:(float)y2
			   x3:(float)x3 y3:(float)y3
			 color:(EJColorRGBA)color
	 withTransform:(CGAffineTransform)transform;
- (void)pushQuadV1:(EJVector2)v1 v2:(EJVector2)v2 v3:(EJVector2)v3 v4:(EJVector2)v4
	t1:(EJVector2)t1 t2:(EJVector2)t2 t3:(EJVector2)t3 t4:(EJVector2)t4
	color:(EJColorRGBA)color
	withTransform:(CGAffineTransform)transform;
- (void)pushRectX:(float)x y:(float)y w:(float)w h:(float)h
	tx:(float)tx ty:(float)ty tw:(float)tw th:(float)th
	color:(EJColorRGBA)color
	withTransform:(CGAffineTransform)transform;
- (void)flushBuffers;

- (void)save;
- (void)restore;
- (void)rotate:(float)angle;
- (void)translateX:(float)x y:(float)y;
- (void)scaleX:(float)x y:(float)y;
- (void)transformM11:(float)m11 m12:(float)m12 m21:(float)m21 m22:(float)m2 dx:(float)dx dy:(float)dy;
- (void)setTransformM11:(float)m11 m12:(float)m12 m21:(float)m21 m22:(float)m2 dx:(float)dx dy:(float)dy;
- (void)drawImage:(EJTexture *)image sx:(float)sx sy:(float)sy sw:(float)sw sh:(float)sh dx:(float)dx dy:(float)dy dw:(float)dw dh:(float)dh;
- (void)fillRectX:(float)x y:(float)y w:(float)w h:(float)h;
- (void)strokeRectX:(float)x y:(float)y w:(float)w h:(float)h;
- (void)clearRectX:(float)x y:(float)y w:(float)w h:(float)h;
- (EJImageData*)getImageDataSx:(float)sx sy:(float)sy sw:(float)sw sh:(float)sh;
- (void)putImageData:(EJImageData*)imageData dx:(float)dx dy:(float)dy;
- (void)beginPath;
- (void)closePath;
- (void)fill;
- (void)stroke;
- (void)moveToX:(float)x y:(float)y;
- (void)lineToX:(float)x y:(float)y;
- (void)rectX:(float)x y:(float)y w:(float)w h:(float)h;
- (void)bezierCurveToCpx1:(float)cpx1 cpy1:(float)cpy1 cpx2:(float)cpx2 cpy2:(float)cpy2 x:(float)x y:(float)y;
- (void)quadraticCurveToCpx:(float)cpx cpy:(float)cpy x:(float)x y:(float)y;
- (void)arcToX1:(float)x1 y1:(float)y1 x2:(float)x2 y2:(float)y2 radius:(float)radius;
- (void)arcX:(float)x y:(float)y radius:(float)radius startAngle:(float)startAngle endAngle:(float)endAngle antiClockwise:(BOOL)antiClockwise;

- (void)fillText:(NSString *)text x:(float)x y:(float)y;
- (void)strokeText:(NSString *)text x:(float)x y:(float)y;
- (float)measureText:(NSString *)text;

@property (nonatomic) EJCanvasState * state;
@property (nonatomic) EJCompositeOperation globalCompositeOperation;
@property (nonatomic, retain) UIFont * font;
@property (nonatomic, assign) float backingStoreRatio;
@property (nonatomic) BOOL msaaEnabled;
@property (nonatomic) int msaaSamples; */

/* TODO: not yet implemented:
	createLinearGradient(x0, y0, x1, y1)
	createRadialGradient(x0, y0, r0, x1, y1, r1)
	createPattern(image, repetition)
	shadowOffsetX
	shadowOffsetY
	shadowBlur
	shadowColor
	clip()
	isPointInPath(x, y)
*/


#endif
