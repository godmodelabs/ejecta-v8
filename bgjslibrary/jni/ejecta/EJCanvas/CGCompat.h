#ifndef __CGCOMPAT

#define __CGCOMPAT 1

typedef float CGFloat;

struct CGAffineTransform_t {
	float a, b, c, d, tx, ty;
};

struct CGPoint {
  CGFloat x;
  CGFloat y;
};
typedef struct CGPoint CGPoint;


struct CGSize {
  CGFloat width;
  CGFloat height;
};
typedef struct CGSize CGSize;


struct CGRect {
  CGPoint origin;
  CGSize size;
};
typedef struct CGRect CGRect;

typedef struct CGAffineTransform_t CGAffineTransform;

#define TRUE 1
#define FALSE 0

#define YES 1
#define NO 0

#define CG_EXTERN
#define CG_INLINE inline

/* constants */
CG_EXTERN const CGAffineTransform CGAffineTransformIdentity = {1,0,0,1,0,0};;

/* functions */
CG_EXTERN CGAffineTransform CGAffineTransformMake (
        CGFloat a, CGFloat b,
        CGFloat c, CGFloat d, CGFloat tx, CGFloat ty);

CG_EXTERN CGAffineTransform CGAffineTransformMakeRotation(CGFloat angle);

CG_EXTERN CGAffineTransform CGAffineTransformMakeScale(CGFloat sx, CGFloat sy);

CG_EXTERN CGAffineTransform CGAffineTransformMakeTranslation(CGFloat tx, CGFloat ty);

/* Modifying Affine Transformations */
CG_EXTERN CGAffineTransform CGAffineTransformTranslate(CGAffineTransform t,
CGFloat tx, CGFloat ty);

CG_EXTERN CGAffineTransform CGAffineTransformScale(CGAffineTransform t,
  CGFloat sx, CGFloat sy);

CG_EXTERN CGAffineTransform CGAffineTransformRotate(CGAffineTransform t,
  CGFloat angle);

CG_EXTERN CGAffineTransform CGAffineTransformInvert(CGAffineTransform t);

CG_EXTERN CGAffineTransform CGAffineTransformConcat(CGAffineTransform t1,
  CGAffineTransform t2);

/* Applying Affine Transformations */

CG_EXTERN bool CGAffineTransformEqualToTransform(CGAffineTransform t1,
  CGAffineTransform t2);


/* Evaluating Affine Transforms */

CG_EXTERN bool CGAffineTransformIsIdentity(CGAffineTransform t);

/* inline functions */
CG_INLINE CGAffineTransform __CGAffineTransformMake(CGFloat a, CGFloat b,
 CGFloat c, CGFloat d, CGFloat tx, CGFloat ty)
{
  CGAffineTransform at;
  at.a = a; at.b = b; at.c = c; at.d = d;
  at.tx = tx; at.ty = ty;
  return at;
}
#define CGAffineTransformMake __CGAffineTransformMake

CG_INLINE CGPoint
__CGPointApplyAffineTransform(CGPoint point, CGAffineTransform t)
{
  CGPoint p;
  p.x = (CGFloat)((double)t.a * point.x + (double)t.c * point.y + t.tx);
  p.y = (CGFloat)((double)t.b * point.x + (double)t.d * point.y + t.ty);
  return p;
}
#define CGPointApplyAffineTransform __CGPointApplyAffineTransform

CG_INLINE CGSize
__CGSizeApplyAffineTransform(CGSize size, CGAffineTransform t)
{
  CGSize s;
  s.width  = (CGFloat)((double)t.a * size.width + (double)t.c * size.height);
  s.height = (CGFloat)((double)t.b * size.width + (double)t.d * size.height);
  return s;
}
#define CGSizeApplyAffineTransform __CGSizeApplyAffineTransform


#endif
