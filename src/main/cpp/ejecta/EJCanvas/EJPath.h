
#ifndef __EJPATH_H__
#define __EJPATH_H__ 	1

#include "EJCanvasTypes.h"
#include "CGCompat.h"
#include "GLcompat.h"
#include <cmath>
#include <cfloat>

#include <vector>

#define EJ_PATH_RECURSION_LIMIT 8
#define EJ_PATH_DISTANCE_EPSILON 1.0f
#define EJ_PATH_COLLINEARITY_EPSILON FLT_EPSILON
#define EJ_PATH_STEPS_FOR_CIRCLE 48.0f

class EJCanvasContext;

// We're using the C++ std::vector here to store our points. Boxing and unboxing
// so many EJVectors to NSValue types seemed wasteful.
typedef std::vector<EJVector2> subpath_t;
typedef std::vector<subpath_t> path_t;

class EJPath {
public:
	EJPath();
	~EJPath();
	void reset();
	void close();
	void endSubPath();
	void moveToX (float x, float y);
	void lineToX (float x, float y);
	void bezierCurveToCpx1 (float cpx1, float cpy1, float cpx2, float cpy2, float x, float y, float scale);
	void recursiveBezierX1 (float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, int level);
	void quadraticCurveToCpx (float cpx, float cpy, float x, float y, float scale);
	void recursiveQuadraticX1 (float x1, float y1, float x2, float y2, float x3, float y3, int level);
	void arcToX1 (float x1, float y1, float x2, float y2, float radius);
	void arcX (float x, float y, float radius, float startAngle, float endAngle, bool antiClockwise);

	void drawPolygonsToContext (EJCanvasContext *context);
	void drawArcToContext (EJCanvasContext *context, EJVector2 point, EJVector2 p1, EJVector2 p2, EJColorRGBA color);
	void drawLinesToContext (EJCanvasContext *context);

	CGAffineTransform transform;
private:
	// attributes
	EJVector2 currentPos, startPos;
	int longestSubPath;
	GLubyte stencilMask;

	EJVector2 *vertexBuffer;
	int vertexBufferLength;

	float distanceTolerance;

	subpath_t currentPath;
	path_t paths;

	// methods

};

/*
@interface EJPath : NSObject {
	EJVector2 currentPos, startPos;
	int longestSubpath;

	GLubyte stencilMask;

	EJVector2 * vertexBuffer;
	int vertexBufferLength;

	float distanceTolerance;

    CGAffineTransform transform;
}

@property (nonatomic,assign) CGAffineTransform transform;;

- (void)reset;
- (void)close;
- (void)endSubPath;
- (void)moveToX:(float)x y:(float)y;
- (void)lineToX:(float)x y:(float)y;
- (void)bezierCurveToCpx1:(float)cpx1 cpy1:(float)cpy1 cpx2:(float)cpx2 cpy2:(float)cpy2 x:(float)x y:(float)y scale:(float)scale;
- (void)recursiveBezierX1:(float)x1 y1:(float)y1 x2:(float)x2 y2:(float)y2 x3:(float)x3 y3:(float)y3 x4:(float)x4 y4:(float)y4 level:(int)level;
- (void)quadraticCurveToCpx:(float)cpx cpy:(float)cpy x:(float)x y:(float)y scale:(float)scale;
- (void)recursiveQuadraticX1:(float)x1 y1:(float)y1 x2:(float)x2 y2:(float)y2 x3:(float)x3 y3:(float)y3	level:(int)level;
- (void)arcToX1:(float)x1 y1:(float)y1 x2:(float)x2 y2:(float)y2 radius:(float)radius;
- (void)arcX:(float)x y:(float)y radius:(float)radius startAngle:(float)startAngle endAngle:(float)endAngle	antiClockwise:(BOOL)antiClockwise;

- (void)drawPolygonsToContext:(EJCanvasContext *)context;
- (void)drawLinesToContext:(EJCanvasContext *)context;

@end

*/

#endif
