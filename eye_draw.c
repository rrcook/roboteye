/*
 * eye_draw.c - Pure CoreGraphics renderer for the robot face.
 *
 * Style: just a cute face floating on a dark background - two glowing
 * blue oval eyes and a blue mouth that morphs between smile, frown and
 * a surprised "O".
 *
 * Only uses APIs available on Mac OS X 10.4 (CGShading for gradients,
 * since CGGradient/NSGradient are 10.5+).
 */
#include "eye_draw.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Geometry (fractions)                                               */
/* ------------------------------------------------------------------ */

#define FACE_H_FRAC   0.62f   /* face box height / min(screen w,h)      */
#define FACE_ASPECT   1.55f   /* face box width / face box height       */

#define EYE_X         0.25f   /* eye centre offset / face width         */
#define EYE_Y         0.08f   /* eye centre offset / face height       */
#define EYE_W         0.135f  /* eye width / face width                 */
#define EYE_H         0.40f   /* eye height / face height              */
#define GAZE_X        0.09f   /* max eye travel / face width            */
#define GAZE_Y        0.09f   /* max eye travel / face height          */
#define EYE_LEAN      0.30f   /* max tilt toward the gaze, radians     */
#define EYE_TURN_X    0.20f   /* narrowing when looking sideways        */
#define EYE_TURN_Y    0.12f   /* shortening when looking up or down     */
#define EYE_DEPTH     0.08f   /* near/far eye size difference           */

#define MOUTH_Y      -0.24f   /* mouth centre offset / face height     */

/* ------------------------------------------------------------------ */
/* Colours                                                            */
/* ------------------------------------------------------------------ */

#define GLOW_R    0.30f       /* eye / mouth blue */
#define GLOW_G    0.72f
#define GLOW_B    1.00f

#define MAX_STOPS 5

typedef struct { CGFloat pos, r, g, b, a; } Stop;
typedef struct { int n; Stop s[MAX_STOPS]; } Ramp;

static const Ramp kBackgroundRamp = { 2, {
    { 0.00f, 0.13f, 0.16f, 0.21f, 1.0f },
    { 1.00f, 0.02f, 0.03f, 0.05f, 1.0f } } };

static const Ramp kGlowRamp = { 3, {
    { 0.00f, GLOW_R, GLOW_G, GLOW_B, 0.45f },
    { 0.45f, GLOW_R, GLOW_G, GLOW_B, 0.18f },
    { 1.00f, GLOW_R, GLOW_G, GLOW_B, 0.00f } } };

static const Ramp kEyeRamp = { 3, {              /* centre -> edge */
    { 0.00f, 0.62f, 0.90f, 1.00f, 1.0f },
    { 0.60f, GLOW_R, GLOW_G, GLOW_B, 1.0f },
    { 1.00f, 0.20f, 0.58f, 0.95f, 1.0f } } };

/* ------------------------------------------------------------------ */
/* Shading helpers                                                    */
/* ------------------------------------------------------------------ */

static CGColorSpaceRef RGBSpace(void)
{
    static CGColorSpaceRef space = NULL;
    if (space == NULL)
        space = CGColorSpaceCreateDeviceRGB();
    return space;
}

static void RampEvaluate(void *info, const CGFloat *in, CGFloat *out)
{
    const Ramp *rp = (const Ramp *)info;
    const Stop *a, *b;
    CGFloat t = in[0], f, span;
    int i;

    if (t <= rp->s[0].pos) {
        a = &rp->s[0];
        out[0] = a->r; out[1] = a->g; out[2] = a->b; out[3] = a->a;
        return;
    }
    for (i = 1; i < rp->n; i++) {
        if (t <= rp->s[i].pos) {
            a = &rp->s[i - 1];
            b = &rp->s[i];
            span = b->pos - a->pos;
            f = (span > 0) ? (t - a->pos) / span : 0;
            out[0] = a->r + (b->r - a->r) * f;
            out[1] = a->g + (b->g - a->g) * f;
            out[2] = a->b + (b->b - a->b) * f;
            out[3] = a->a + (b->a - a->a) * f;
            return;
        }
    }
    a = &rp->s[rp->n - 1];
    out[0] = a->r; out[1] = a->g; out[2] = a->b; out[3] = a->a;
}

static void RampRelease(void *info)
{
    free(info);
}

static CGFunctionRef MakeRampFunction(const Ramp *ramp)
{
    static const CGFloat domain[2] = { 0, 1 };
    static const CGFloat range[8] = { 0, 1, 0, 1, 0, 1, 0, 1 };
    CGFunctionCallbacks callbacks = { 0, RampEvaluate, RampRelease };
    Ramp *copy = (Ramp *)malloc(sizeof(Ramp));

    memcpy(copy, ramp, sizeof(Ramp));
    return CGFunctionCreate(copy, 1, domain, 4, range, &callbacks);
}

/* Radial gradient between two circles (paints only the swept area). */
static void Radial(CGContextRef ctx, CGPoint c0, CGFloat r0,
                   CGPoint c1, CGFloat r1, const Ramp *ramp)
{
    CGFunctionRef fn = MakeRampFunction(ramp);
    CGShadingRef sh = CGShadingCreateRadial(RGBSpace(), c0, r0, c1, r1,
                                            fn, false, false);
    CGContextDrawShading(ctx, sh);
    CGShadingRelease(sh);
    CGFunctionRelease(fn);
}

/* Elliptical radial gradient centred on c with radii rx, ry. */
static void EllipticalRadial(CGContextRef ctx, CGPoint c, CGFloat rx, CGFloat ry,
                             const Ramp *ramp)
{
    if (rx <= 0 || ry <= 0)
        return;
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, c.x, c.y);
    CGContextScaleCTM(ctx, 1, ry / rx);
    Radial(ctx, CGPointMake(0, 0), 0, CGPointMake(0, 0), rx, ramp);
    CGContextRestoreGState(ctx);
}

static CGRect CenteredRect(CGPoint c, CGFloat w, CGFloat h)
{
    return CGRectMake(c.x - w * 0.5f, c.y - h * 0.5f, w, h);
}

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */

/* The face box: eyes and mouth are placed in fractions of it. */
typedef struct {
    CGPoint vc;         /* face centre      */
    CGFloat vw, vh;     /* face box size    */
} Layout;

static void DoLayout(CGRect b, Layout *L)
{
    CGFloat m = (b.size.width < b.size.height) ? b.size.width : b.size.height;

    L->vh = m * FACE_H_FRAC;
    L->vw = L->vh * FACE_ASPECT;
    if (L->vw > b.size.width * 0.95f) {      /* very narrow screens */
        L->vw = b.size.width * 0.95f;
        L->vh = L->vw / FACE_ASPECT;
    }
    L->vc = CGPointMake(b.origin.x + b.size.width * 0.5f,
                        b.origin.y + b.size.height * 0.5f);
}

/* ------------------------------------------------------------------ */
/* Scene parts                                                        */
/* ------------------------------------------------------------------ */

/* lx, ly: gaze direction (unit disk); gx, gy: gaze offset in pixels. */
static void DrawEyes(CGContextRef ctx, const Layout *L, const EyeState *s,
                     CGFloat lx, CGFloat ly, CGFloat gx, CGFloat gy)
{
    CGFloat grow = 1.0f + 0.15f * s->surprise;
    CGFloat ax = (CGFloat)fabs(lx), ay = (CGFloat)fabs(ly);
    CGFloat ew = L->vw * EYE_W * grow * (1.0f - EYE_TURN_X * ax);
    CGFloat eh = L->vh * EYE_H * grow * (1.0f - EYE_TURN_Y * ay);
    CGFloat lean = -lx * EYE_LEAN;       /* look right -> tops lean right */
    CGFloat open = 1.0f - s->blink;
    CGFloat h, depth, w;
    CGPoint o = CGPointMake(0, 0);
    int side;

    for (side = -1; side <= 1; side += 2) {
        CGPoint ec = CGPointMake(L->vc.x + side * L->vw * EYE_X + gx,
                                 L->vc.y + L->vh * EYE_Y + gy);
        CGRect eye;

        /* the eye on the side we look toward is a little farther away */
        depth = 1.0f - EYE_DEPTH * lx * side;
        w = ew * depth;

        /* a blink squashes the oval down to a thin bar */
        h = eh * depth * open;
        if (h < w * 0.22f)
            h = w * 0.22f;
        eye = CenteredRect(o, w, h);

        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, ec.x, ec.y);
        CGContextRotateCTM(ctx, lean);

        EllipticalRadial(ctx, o, w * 1.25f, h * 0.5f + w * 0.75f, &kGlowRamp);

        CGContextBeginPath(ctx);
        CGContextAddEllipseInRect(ctx, eye);
        CGContextClip(ctx);
        CGContextSetRGBFillColor(ctx, GLOW_R, GLOW_G, GLOW_B, 1);
        CGContextFillRect(ctx, eye);
        EllipticalRadial(ctx, o, w * 0.5f, h * 0.5f, &kEyeRamp);
        CGContextRestoreGState(ctx);
    }
}

/*
 * The mouth is a closed shape: a top and a bottom cubic between two
 * corner points. When top and bottom coincide it is a curved line
 * (smile or frown); pulled apart it becomes an "O".
 */
static void DrawMouth(CGContextRef ctx, const Layout *L, const EyeState *s,
                      CGFloat gx, CGFloat gy)
{
    CGFloat smile = s->smile, e = s->surprise;
    CGFloat lineW = L->vw * (0.13f + 0.03f * (smile > 0 ? smile : 0));
    CGFloat lineCtl = -(smile * L->vh * 0.075f) / 0.75f;  /* mid dips by 0.075vh */
    CGFloat oW = L->vw * 0.055f, oH = L->vh * 0.085f;
    CGFloat w   = lineW + (oW - lineW) * e;
    CGFloat a   = 0.55f + (1.0f - 0.55f) * e;
    CGFloat top = lineCtl + (oH * 4.0f / 3.0f - lineCtl) * e;
    CGFloat bot = lineCtl + (-oH * 4.0f / 3.0f - lineCtl) * e;
    CGFloat cx = L->vc.x + gx * 0.6f;
    CGFloat cy = L->vc.y + L->vh * MOUTH_Y + gy * 0.6f - L->vh * 0.02f * e
                 + ((smile < 0) ? smile * L->vh * 0.02f : 0);
    CGFloat stroke = L->vh * 0.040f;

    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, cx - w, cy);
    CGContextAddCurveToPoint(ctx, cx - w * a, cy + top, cx + w * a, cy + top, cx + w, cy);
    CGContextAddCurveToPoint(ctx, cx + w * a, cy + bot, cx - w * a, cy + bot, cx - w, cy);
    CGContextClosePath(ctx);

    CGContextSetLineCap(ctx, kCGLineCapRound);
    CGContextSetLineJoin(ctx, kCGLineJoinRound);
    CGContextSetLineWidth(ctx, stroke);
    CGContextSetRGBStrokeColor(ctx, GLOW_R, GLOW_G, GLOW_B, 1);
    CGContextStrokePath(ctx);
}

/* ------------------------------------------------------------------ */
/* Public                                                             */
/* ------------------------------------------------------------------ */

CGRect EyeSocketBounds(CGRect bounds)
{
    Layout L;
    DoLayout(bounds, &L);
    /* generous margin so the eye glow is always inside */
    return CGRectIntegral(CGRectInset(CenteredRect(L.vc, L.vw, L.vh),
                                      -L.vw * 0.05f, -L.vh * 0.10f));
}

void EyeDraw(CGContextRef ctx, CGRect bounds, const EyeState *s)
{
    Layout L;
    CGFloat lx = s->lookX, ly = s->lookY, mag, far, gx, gy;

    DoLayout(bounds, &L);
    mag = (CGFloat)sqrt(lx * lx + ly * ly);
    if (mag > 1) { lx /= mag; ly /= mag; }
    gx = lx * L.vw * GAZE_X;
    gy = ly * L.vh * GAZE_Y;
    far = (CGFloat)sqrt(bounds.size.width * bounds.size.width +
                        bounds.size.height * bounds.size.height) * 0.5f;

    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, bounds);

    CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
    CGContextFillRect(ctx, bounds);
    Radial(ctx, L.vc, 0, L.vc, far, &kBackgroundRamp);

    DrawEyes(ctx, &L, s, lx, ly, gx, gy);
    DrawMouth(ctx, &L, s, gx, gy);

    CGContextRestoreGState(ctx);
}
