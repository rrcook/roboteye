/*
 * eye_draw.c - Pure CoreGraphics renderer for the robot eye.
 *
 * Style: glossy cartoon eye - thin silver rim, white "eyeball",
 * black-rimmed blue iris that moves around inside it - on a black body.
 *
 * Only uses APIs available on Mac OS X 10.4 (CGShading for gradients,
 * since CGGradient/NSGradient are 10.5+).
 */
#include "eye_draw.h"

#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Geometry (fractions)                                               */
/* ------------------------------------------------------------------ */

#define RIM_FRAC      0.40f   /* silver rim radius / min(screen w,h)      */
#define WHITE_FRAC    0.955f  /* white eyeball radius / rim radius        */
#define IRIS_FRAC     0.66f   /* black iris ring radius / eyeball radius  */
#define BLUE_FRAC     0.84f   /* blue iris radius / black ring radius     */
#define SQUASH        0.22f   /* iris foreshortening at full deflection   */

/* ------------------------------------------------------------------ */
/* Colour ramps                                                       */
/* ------------------------------------------------------------------ */

#define MAX_STOPS 6

typedef struct { CGFloat pos, r, g, b, a; } Stop;
typedef struct { int n; Stop s[MAX_STOPS]; } Ramp;

static const Ramp kBackgroundRamp = { 2, {
    { 0.00f, 0.11f, 0.11f, 0.12f, 1.0f },
    { 1.00f, 0.00f, 0.00f, 0.00f, 1.0f } } };

static const Ramp kRimRamp = { 3, {              /* top -> bottom */
    { 0.00f, 0.92f, 0.93f, 0.95f, 1.0f },
    { 0.50f, 0.62f, 0.63f, 0.66f, 1.0f },
    { 1.00f, 0.30f, 0.31f, 0.34f, 1.0f } } };

static const Ramp kWhiteRamp = { 3, {            /* centre -> edge */
    { 0.00f, 1.00f, 1.00f, 1.00f, 1.0f },
    { 0.70f, 0.97f, 0.97f, 0.98f, 1.0f },
    { 1.00f, 0.78f, 0.79f, 0.82f, 1.0f } } };

static const Ramp kBowlShadowRamp = { 2, {       /* inner edge of eyeball */
    { 0.00f, 0.0f, 0.0f, 0.05f, 0.00f },
    { 1.00f, 0.0f, 0.0f, 0.05f, 0.40f } } };

static const Ramp kCastShadowRamp = { 2, {       /* iris onto eyeball */
    { 0.00f, 0.0f, 0.0f, 0.05f, 0.40f },
    { 1.00f, 0.0f, 0.0f, 0.05f, 0.00f } } };

static const Ramp kBlackRingRamp = { 3, {        /* centre -> edge */
    { 0.00f, 0.00f, 0.00f, 0.00f, 1.0f },
    { 0.85f, 0.04f, 0.04f, 0.05f, 1.0f },
    { 1.00f, 0.20f, 0.21f, 0.24f, 1.0f } } };

/* Blue iris: light blue around the pupil, deep blue at the edge. */
static const Ramp kIrisRamp = { 5, {
    { 0.00f, 0.02f, 0.06f, 0.16f, 1.0f },
    { 0.28f, 0.20f, 0.48f, 0.80f, 1.0f },
    { 0.50f, 0.30f, 0.60f, 0.90f, 1.0f },
    { 0.80f, 0.12f, 0.36f, 0.68f, 1.0f },
    { 1.00f, 0.03f, 0.12f, 0.30f, 1.0f } } };

static const Ramp kPupilEdgeRamp = { 2, {
    { 0.00f, 0.01f, 0.03f, 0.10f, 0.75f },
    { 1.00f, 0.01f, 0.03f, 0.10f, 0.00f } } };

static const Ramp kHighlightRamp = { 3, {
    { 0.00f, 1.0f, 1.0f, 1.0f, 0.90f },
    { 0.35f, 1.0f, 1.0f, 1.0f, 0.55f },
    { 1.00f, 1.0f, 1.0f, 1.0f, 0.00f } } };

static const Ramp kLidRamp = { 3, {              /* outer -> edge */
    { 0.00f, 0.02f, 0.02f, 0.03f, 1.0f },
    { 0.80f, 0.09f, 0.09f, 0.10f, 1.0f },
    { 1.00f, 0.22f, 0.22f, 0.25f, 1.0f } } };

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

/* Linear gradient from p0 to p1, extended both ways (use with a clip). */
static void Axial(CGContextRef ctx, CGPoint p0, CGPoint p1, const Ramp *ramp)
{
    CGFunctionRef fn = MakeRampFunction(ramp);
    CGShadingRef sh = CGShadingCreateAxial(RGBSpace(), p0, p1, fn, true, true);
    CGContextDrawShading(ctx, sh);
    CGShadingRelease(sh);
    CGFunctionRelease(fn);
}

static CGRect CircleRect(CGPoint c, CGFloat r)
{
    return CGRectMake(c.x - r, c.y - r, 2 * r, 2 * r);
}

static void ClipToCircle(CGContextRef ctx, CGPoint c, CGFloat r)
{
    CGContextBeginPath(ctx);
    CGContextAddEllipseInRect(ctx, CircleRect(c, r));
    CGContextClip(ctx);
}

static void StrokeCircle(CGContextRef ctx, CGPoint c, CGFloat r, CGFloat width,
                         CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha)
{
    CGContextSetRGBStrokeColor(ctx, red, green, blue, alpha);
    CGContextSetLineWidth(ctx, width);
    CGContextStrokeEllipseInRect(ctx, CircleRect(c, r));
}

/* ------------------------------------------------------------------ */
/* Scene parts                                                        */
/* ------------------------------------------------------------------ */

static void DrawRim(CGContextRef ctx, CGPoint c, CGFloat R, CGFloat lw)
{
    CGContextSaveGState(ctx);
    ClipToCircle(ctx, c, R);
    Axial(ctx, CGPointMake(c.x, c.y + R), CGPointMake(c.x, c.y - R), &kRimRamp);
    CGContextRestoreGState(ctx);
    StrokeCircle(ctx, c, R, lw, 0, 0, 0, 0.8f);
}

static void DrawIris(CGContextRef ctx, CGPoint c, CGFloat Rw, CGFloat lw,
                     const EyeState *s)
{
    CGFloat Ri = Rw * IRIS_FRAC;
    CGFloat Rb = Ri * BLUE_FRAC;
    CGFloat travel = Rw - Ri - lw * 2;
    CGFloat lx = s->lookX, ly = s->lookY;
    CGFloat mag = (CGFloat)sqrt(lx * lx + ly * ly);
    CGFloat angle, Rp;
    CGPoint o = CGPointMake(0, 0);
    CGPoint hl;
    int i;

    if (mag > 1) { lx /= mag; ly /= mag; mag = 1; }
    angle = (CGFloat)atan2(ly, lx);

    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, c.x + lx * travel, c.y + ly * travel);
    /* foreshorten along the gaze direction, as if the eyeball turned */
    CGContextRotateCTM(ctx, angle);
    CGContextScaleCTM(ctx, 1.0f - SQUASH * mag, 1.0f);
    CGContextRotateCTM(ctx, -angle);

    /* soft shadow the iris casts on the white */
    Radial(ctx, CGPointMake(0, -Ri * 0.03f), Ri * 0.95f,
           CGPointMake(0, -Ri * 0.03f), Ri * 1.14f, &kCastShadowRamp);

    /* black ring */
    Radial(ctx, o, 0, o, Ri, &kBlackRingRamp);

    /* blue iris with faint fibres */
    Radial(ctx, o, 0, o, Rb, &kIrisRamp);
    CGContextSetLineWidth(ctx, lw * 0.6f);
    CGContextSetRGBStrokeColor(ctx, 0.55f, 0.80f, 1.0f, 0.16f);
    for (i = 0; i < 60; i++) {
        double a = i * (2 * M_PI / 60);
        CGFloat ca = (CGFloat)cos(a), sa = (CGFloat)sin(a);
        CGFloat r0 = Rb * ((i & 1) ? 0.42f : 0.36f);
        CGContextMoveToPoint(ctx, ca * r0, sa * r0);
        CGContextAddLineToPoint(ctx, ca * Rb * 0.92f, sa * Rb * 0.92f);
    }
    CGContextStrokePath(ctx);
    StrokeCircle(ctx, o, Rb, lw, 0.0f, 0.03f, 0.10f, 0.7f);

    /* pupil */
    Rp = Rb * (0.24f + 0.16f * s->pupil);
    Radial(ctx, o, Rp, o, Rp * 1.35f, &kPupilEdgeRamp);
    CGContextSetRGBFillColor(ctx, 0.01f, 0.02f, 0.05f, 1.0f);
    CGContextFillEllipseInRect(ctx, CircleRect(o, Rp));

    /* glossy highlights, upper right */
    hl = CGPointMake(Rb * 0.36f, Rb * 0.40f);
    Radial(ctx, hl, 0, hl, Rb * 0.30f, &kHighlightRamp);
    CGContextSetRGBFillColor(ctx, 1, 1, 1, 0.85f);
    CGContextFillEllipseInRect(ctx, CircleRect(CGPointMake(Rp * 0.42f, Rp * 0.38f),
                                               Rp * 0.14f));

    CGContextRestoreGState(ctx);
}

/* Black eyelids with a gently curved edge, closing from top and bottom. */
static void DrawLids(CGContextRef ctx, CGPoint c, CGFloat Rw, CGFloat lw, CGFloat blink)
{
    CGFloat topEdge = c.y + Rw - blink * Rw * 1.08f;
    CGFloat botEdge = c.y - Rw + blink * Rw * 0.92f;
    CGFloat bulge = Rw * 0.22f * blink;
    CGFloat L = c.x - Rw, Rt = c.x + Rw;

    if (blink <= 0.001f)
        return;

    /* upper lid */
    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, L, c.y + Rw);
    CGContextAddLineToPoint(ctx, Rt, c.y + Rw);
    CGContextAddLineToPoint(ctx, Rt, topEdge + bulge);
    CGContextAddQuadCurveToPoint(ctx, c.x, topEdge - bulge, L, topEdge + bulge);
    CGContextClosePath(ctx);
    CGContextClip(ctx);
    Axial(ctx, CGPointMake(c.x, c.y + Rw), CGPointMake(c.x, topEdge), &kLidRamp);
    CGContextRestoreGState(ctx);

    /* lower lid */
    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, L, c.y - Rw);
    CGContextAddLineToPoint(ctx, Rt, c.y - Rw);
    CGContextAddLineToPoint(ctx, Rt, botEdge - bulge * 0.5f);
    CGContextAddQuadCurveToPoint(ctx, c.x, botEdge + bulge * 0.5f, L, botEdge - bulge * 0.5f);
    CGContextClosePath(ctx);
    CGContextClip(ctx);
    Axial(ctx, CGPointMake(c.x, c.y - Rw), CGPointMake(c.x, botEdge), &kLidRamp);
    CGContextRestoreGState(ctx);

    /* thin sheen along the lid edges */
    CGContextSetLineWidth(ctx, lw);
    CGContextSetRGBStrokeColor(ctx, 0.45f, 0.46f, 0.50f, 0.8f);
    CGContextBeginPath(ctx);
    CGContextMoveToPoint(ctx, Rt, topEdge + bulge);
    CGContextAddQuadCurveToPoint(ctx, c.x, topEdge - bulge, L, topEdge + bulge);
    CGContextMoveToPoint(ctx, Rt, botEdge - bulge * 0.5f);
    CGContextAddQuadCurveToPoint(ctx, c.x, botEdge + bulge * 0.5f, L, botEdge - bulge * 0.5f);
    CGContextStrokePath(ctx);
}

/* ------------------------------------------------------------------ */
/* Public                                                             */
/* ------------------------------------------------------------------ */

static void Layout(CGRect b, CGPoint *c, CGFloat *R, CGFloat *Rw, CGFloat *lw)
{
    CGFloat m = (b.size.width < b.size.height) ? b.size.width : b.size.height;
    c->x = b.origin.x + b.size.width * 0.5f;
    c->y = b.origin.y + b.size.height * 0.5f;
    *R  = m * RIM_FRAC;
    *Rw = *R * WHITE_FRAC;
    *lw = m * 0.0022f;
    if (*lw < 1) *lw = 1;
}

CGRect EyeSocketBounds(CGRect bounds)
{
    CGPoint c; CGFloat R, Rw, lw;
    Layout(bounds, &c, &R, &Rw, &lw);
    return CGRectIntegral(CircleRect(c, Rw + 2 * lw));
}

void EyeDraw(CGContextRef ctx, CGRect bounds, const EyeState *s)
{
    CGPoint c; CGFloat R, Rw, lw, far;
    Layout(bounds, &c, &R, &Rw, &lw);
    far = (CGFloat)sqrt(bounds.size.width * bounds.size.width +
                        bounds.size.height * bounds.size.height) * 0.5f;

    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, bounds);

    /* glossy black robot body */
    CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
    CGContextFillRect(ctx, bounds);
    Radial(ctx, c, 0, c, far, &kBackgroundRamp);

    DrawRim(ctx, c, R, lw);

    /* everything inside the white eyeball */
    CGContextSaveGState(ctx);
    ClipToCircle(ctx, c, Rw);
    Radial(ctx, c, 0, c, Rw, &kWhiteRamp);
    Radial(ctx, c, Rw * 0.80f, c, Rw, &kBowlShadowRamp);
    DrawIris(ctx, c, Rw, lw, s);
    DrawLids(ctx, c, Rw, lw, s->blink);
    CGContextRestoreGState(ctx);

    StrokeCircle(ctx, c, Rw, lw, 0.25f, 0.26f, 0.30f, 0.9f);

    CGContextRestoreGState(ctx);
}
