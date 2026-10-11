/*
 * eye_draw.c - Pure CoreGraphics renderer for the robot eye.
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

#define BEZEL_FRAC   0.44f   /* outer bezel radius / min(screen w,h)   */
#define SOCKET_FRAC  0.78f   /* socket radius / bezel radius           */
#define IRIS_FRAC    0.50f   /* iris radius / socket radius            */
#define TRAVEL_FRAC  0.46f   /* max iris travel / socket radius        */
#define RIVET_COUNT  8

/* ------------------------------------------------------------------ */
/* Colour ramps                                                       */
/* ------------------------------------------------------------------ */

#define MAX_STOPS 6

typedef struct { CGFloat pos, r, g, b, a; } Stop;
typedef struct { int n; Stop s[MAX_STOPS]; } Ramp;

/* Iris: hot core -> orange -> red -> dark red edge.  Change these to
   re-colour the eye (e.g. swap r and b channels for a blue eye). */
static const Ramp kIrisRamp = { 5, {
    { 0.00f, 1.00f, 0.95f, 0.80f, 1.0f },
    { 0.35f, 1.00f, 0.55f, 0.20f, 1.0f },
    { 0.55f, 0.95f, 0.15f, 0.05f, 1.0f },
    { 0.85f, 0.55f, 0.02f, 0.01f, 1.0f },
    { 1.00f, 0.22f, 0.00f, 0.00f, 1.0f } } };

static const Ramp kGlowRamp = { 3, {
    { 0.00f, 1.00f, 0.15f, 0.05f, 0.55f },
    { 0.55f, 1.00f, 0.12f, 0.04f, 0.30f },
    { 1.00f, 1.00f, 0.10f, 0.04f, 0.00f } } };

static const Ramp kPupilGlintRamp = { 2, {
    { 0.00f, 1.00f, 0.40f, 0.20f, 0.85f },
    { 1.00f, 1.00f, 0.10f, 0.05f, 0.00f } } };

static const Ramp kBackgroundRamp = { 2, {
    { 0.00f, 0.13f, 0.13f, 0.15f, 1.0f },
    { 1.00f, 0.00f, 0.00f, 0.00f, 1.0f } } };

static const Ramp kBezelLipRamp = { 3, {         /* top -> bottom */
    { 0.00f, 0.92f, 0.92f, 0.95f, 1.0f },
    { 0.50f, 0.55f, 0.55f, 0.58f, 1.0f },
    { 1.00f, 0.18f, 0.18f, 0.20f, 1.0f } } };

static const Ramp kBezelFaceRamp = { 4, {        /* top -> bottom */
    { 0.00f, 0.62f, 0.62f, 0.66f, 1.0f },
    { 0.45f, 0.42f, 0.42f, 0.46f, 1.0f },
    { 0.55f, 0.36f, 0.36f, 0.40f, 1.0f },
    { 1.00f, 0.26f, 0.26f, 0.29f, 1.0f } } };

static const Ramp kInnerLipRamp = { 2, {         /* top -> bottom (inset) */
    { 0.00f, 0.08f, 0.08f, 0.09f, 1.0f },
    { 1.00f, 0.70f, 0.70f, 0.74f, 1.0f } } };

static const Ramp kSocketRamp = { 2, {
    { 0.00f, 0.07f, 0.07f, 0.08f, 1.0f },
    { 1.00f, 0.01f, 0.01f, 0.01f, 1.0f } } };

static const Ramp kHousingRamp = { 3, {
    { 0.00f, 0.05f, 0.05f, 0.06f, 1.0f },
    { 0.85f, 0.16f, 0.16f, 0.18f, 1.0f },
    { 1.00f, 0.35f, 0.35f, 0.38f, 1.0f } } };

static const Ramp kRivetRamp = { 3, {
    { 0.00f, 0.98f, 0.98f, 1.00f, 1.0f },
    { 0.45f, 0.60f, 0.60f, 0.63f, 1.0f },
    { 1.00f, 0.15f, 0.15f, 0.17f, 1.0f } } };

static const Ramp kLidRamp = { 2, {              /* outer -> edge */
    { 0.00f, 0.40f, 0.40f, 0.43f, 1.0f },
    { 1.00f, 0.20f, 0.20f, 0.22f, 1.0f } } };

static const Ramp kLidShadowRamp = { 2, {
    { 0.00f, 0.0f, 0.0f, 0.0f, 0.65f },
    { 1.00f, 0.0f, 0.0f, 0.0f, 0.00f } } };

static const Ramp kInnerShadowRamp = { 3, {
    { 0.00f, 0.0f, 0.0f, 0.0f, 0.00f },
    { 0.60f, 0.0f, 0.0f, 0.0f, 0.35f },
    { 1.00f, 0.0f, 0.0f, 0.0f, 0.90f } } };

static const Ramp kGlassRamp = { 2, {
    { 0.00f, 1.0f, 1.0f, 1.0f, 0.20f },
    { 1.00f, 1.0f, 1.0f, 1.0f, 0.00f } } };

static const Ramp kSpecularRamp = { 2, {
    { 0.00f, 1.0f, 1.0f, 1.0f, 0.75f },
    { 1.00f, 1.0f, 1.0f, 1.0f, 0.00f } } };

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

/* Radial gradient between two circles (no extension: paints only the
   area swept between them). */
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

/* Disk filled with a vertical (top -> bottom) gradient. */
static void VerticalDisk(CGContextRef ctx, CGPoint c, CGFloat r, const Ramp *ramp)
{
    CGContextSaveGState(ctx);
    ClipToCircle(ctx, c, r);
    Axial(ctx, CGPointMake(c.x, c.y + r), CGPointMake(c.x, c.y - r), ramp);
    CGContextRestoreGState(ctx);
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

static void DrawRivet(CGContextRef ctx, CGPoint p, CGFloat r)
{
    /* contact shadow */
    CGContextSetRGBFillColor(ctx, 0, 0, 0, 0.55f);
    CGContextFillEllipseInRect(ctx, CircleRect(CGPointMake(p.x + r * 0.12f, p.y - r * 0.22f), r * 1.08f));
    /* dome with highlight toward upper-left */
    Radial(ctx, CGPointMake(p.x - r * 0.35f, p.y + r * 0.35f), 0, p, r, &kRivetRamp);
    /* screw slot */
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, p.x, p.y);
    CGContextRotateCTM(ctx, (CGFloat)(M_PI / 4));
    CGContextSetRGBFillColor(ctx, 0.08f, 0.08f, 0.09f, 0.85f);
    CGContextFillRect(ctx, CGRectMake(-r * 0.62f, -r * 0.09f, r * 1.24f, r * 0.18f));
    CGContextRestoreGState(ctx);
}

static void DrawBezel(CGContextRef ctx, CGPoint c, CGFloat R, CGFloat Rs, CGFloat lw)
{
    CGFloat faceR = R * 0.955f;
    CGFloat lipR  = Rs * 1.06f;
    CGFloat rivetOrbit = (faceR + lipR) * 0.5f;
    CGFloat rivetR = (faceR - lipR) * 0.17f;
    int i;

    /* soft drop shadow under the whole housing */
    Radial(ctx, CGPointMake(c.x, c.y - R * 0.04f), R * 0.96f,
           CGPointMake(c.x, c.y - R * 0.04f), R * 1.10f, &kLidShadowRamp);

    VerticalDisk(ctx, c, R, &kBezelLipRamp);          /* raised outer lip */
    VerticalDisk(ctx, c, faceR, &kBezelFaceRamp);     /* flat metal face  */
    StrokeCircle(ctx, c, faceR, lw * 1.5f, 0, 0, 0, 0.45f);

    /* fine machined grooves on the face */
    for (i = 1; i <= 3; i++) {
        CGFloat rr = lipR + (faceR - lipR) * (0.10f * i);
        StrokeCircle(ctx, c, rr, lw * 0.6f, 0, 0, 0, 0.18f);
        StrokeCircle(ctx, c, rr + lw * 0.6f, lw * 0.6f, 1, 1, 1, 0.08f);
    }

    for (i = 0; i < RIVET_COUNT; i++) {
        double a = (M_PI / RIVET_COUNT) + i * (2 * M_PI / RIVET_COUNT);
        DrawRivet(ctx, CGPointMake(c.x + rivetOrbit * (CGFloat)cos(a),
                                   c.y + rivetOrbit * (CGFloat)sin(a)), rivetR);
    }

    VerticalDisk(ctx, c, lipR, &kInnerLipRamp);       /* inset inner lip  */
    Radial(ctx, c, 0, c, Rs, &kSocketRamp);           /* dark socket      */
}

static void DrawIris(CGContextRef ctx, CGPoint c, CGFloat Rs, CGFloat lw,
                     const EyeState *s)
{
    CGFloat Ri = Rs * IRIS_FRAC;
    CGFloat travel = Rs * TRAVEL_FRAC;
    CGFloat lx = s->lookX, ly = s->lookY;
    CGFloat mag = (CGFloat)sqrt(lx * lx + ly * ly);
    CGFloat angle, pupilR, irisR = Ri * 0.90f;
    CGPoint o = CGPointMake(0, 0);
    int i;

    if (mag > 1) { lx /= mag; ly /= mag; mag = 1; }
    angle = (CGFloat)atan2(ly, lx);

    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, c.x + lx * travel, c.y + ly * travel);
    /* foreshorten along the gaze direction, as if the eyeball turned */
    CGContextRotateCTM(ctx, angle);
    CGContextScaleCTM(ctx, 1.0f - 0.24f * mag, 1.0f);
    CGContextRotateCTM(ctx, -angle);

    Radial(ctx, o, 0, o, Ri * 1.75f, &kGlowRamp);      /* light spill */
    Radial(ctx, o, 0, o, Ri, &kHousingRamp);           /* lens housing */
    StrokeCircle(ctx, o, Ri, lw, 0.55f, 0.55f, 0.60f, 0.9f);

    /* small indicator notches around the housing */
    CGContextSetLineCap(ctx, kCGLineCapRound);
    CGContextSetLineWidth(ctx, lw * 1.4f);
    CGContextSetRGBStrokeColor(ctx, 1.0f, 0.35f, 0.15f, 0.55f);
    for (i = 0; i < 12; i++) {
        double a = i * (2 * M_PI / 12);
        CGFloat ca = (CGFloat)cos(a), sa = (CGFloat)sin(a);
        CGContextMoveToPoint(ctx, ca * Ri * 0.925f, sa * Ri * 0.925f);
        CGContextAddLineToPoint(ctx, ca * Ri * 0.965f, sa * Ri * 0.965f);
    }
    CGContextStrokePath(ctx);

    Radial(ctx, o, 0, o, irisR, &kIrisRamp);           /* glowing iris */

    /* lens segments */
    CGContextSetLineWidth(ctx, lw * 0.8f);
    CGContextSetRGBStrokeColor(ctx, 0.15f, 0.0f, 0.0f, 0.55f);
    for (i = 0; i < 36; i++) {
        double a = i * (2 * M_PI / 36);
        CGFloat ca = (CGFloat)cos(a), sa = (CGFloat)sin(a);
        CGContextMoveToPoint(ctx, ca * irisR * 0.62f, sa * irisR * 0.62f);
        CGContextAddLineToPoint(ctx, ca * irisR * 0.97f, sa * irisR * 0.97f);
    }
    CGContextStrokePath(ctx);
    StrokeCircle(ctx, o, irisR * 0.78f, lw * 0.7f, 1.0f, 0.75f, 0.5f, 0.35f);
    StrokeCircle(ctx, o, irisR, lw * 1.2f, 0.10f, 0.0f, 0.0f, 0.8f);

    /* pupil */
    pupilR = irisR * (0.26f + 0.18f * s->pupil);
    Radial(ctx, o, pupilR, o, pupilR * 1.25f, &kLidShadowRamp);
    CGContextSetRGBFillColor(ctx, 0.0f, 0.0f, 0.0f, 1.0f);
    CGContextFillEllipseInRect(ctx, CircleRect(o, pupilR));
    StrokeCircle(ctx, o, pupilR, lw * 1.2f, 1.0f, 0.55f, 0.30f, 0.85f);
    Radial(ctx, o, 0, o, pupilR * 0.35f, &kPupilGlintRamp);

    CGContextRestoreGState(ctx);
}

static void DrawLids(CGContextRef ctx, CGPoint c, CGFloat Rs, CGFloat lw, CGFloat blink)
{
    CGFloat topEdge = c.y + Rs - blink * Rs;
    CGFloat botEdge = c.y - Rs + blink * Rs;
    CGFloat shadowH = Rs * 0.10f;
    CGRect top = CGRectMake(c.x - Rs, topEdge, 2 * Rs, c.y + Rs - topEdge);
    CGRect bot = CGRectMake(c.x - Rs, c.y - Rs, 2 * Rs, botEdge - (c.y - Rs));

    if (blink <= 0.001f)
        return;

    /* shadows cast onto the eye by the shutters */
    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, CGRectMake(c.x - Rs, topEdge - shadowH, 2 * Rs, shadowH));
    Axial(ctx, CGPointMake(c.x, topEdge), CGPointMake(c.x, topEdge - shadowH), &kLidShadowRamp);
    CGContextRestoreGState(ctx);
    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, CGRectMake(c.x - Rs, botEdge, 2 * Rs, shadowH));
    Axial(ctx, CGPointMake(c.x, botEdge), CGPointMake(c.x, botEdge + shadowH), &kLidShadowRamp);
    CGContextRestoreGState(ctx);

    /* shutter plates */
    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, top);
    Axial(ctx, CGPointMake(c.x, c.y + Rs), CGPointMake(c.x, topEdge), &kLidRamp);
    CGContextRestoreGState(ctx);
    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, bot);
    Axial(ctx, CGPointMake(c.x, c.y - Rs), CGPointMake(c.x, botEdge), &kLidRamp);
    CGContextRestoreGState(ctx);

    /* bright machined edges */
    CGContextSetLineWidth(ctx, lw * 1.5f);
    CGContextSetRGBStrokeColor(ctx, 0.80f, 0.80f, 0.85f, 0.9f);
    CGContextMoveToPoint(ctx, c.x - Rs, topEdge + lw);
    CGContextAddLineToPoint(ctx, c.x + Rs, topEdge + lw);
    CGContextMoveToPoint(ctx, c.x - Rs, botEdge - lw);
    CGContextAddLineToPoint(ctx, c.x + Rs, botEdge - lw);
    CGContextStrokePath(ctx);
}

static void DrawGlass(CGContextRef ctx, CGPoint c, CGFloat Rs)
{
    /* broad soft reflection, upper left */
    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, c.x - Rs * 0.32f, c.y + Rs * 0.45f);
    CGContextRotateCTM(ctx, (CGFloat)(M_PI / 6));
    CGContextScaleCTM(ctx, 1.0f, 0.45f);
    Radial(ctx, CGPointMake(0, 0), 0, CGPointMake(0, 0), Rs * 0.55f, &kGlassRamp);
    CGContextRestoreGState(ctx);

    /* small sharp specular */
    Radial(ctx, CGPointMake(c.x - Rs * 0.42f, c.y + Rs * 0.50f), 0,
           CGPointMake(c.x - Rs * 0.42f, c.y + Rs * 0.50f), Rs * 0.07f, &kSpecularRamp);
    /* faint secondary reflection, lower right */
    Radial(ctx, CGPointMake(c.x + Rs * 0.48f, c.y - Rs * 0.48f), 0,
           CGPointMake(c.x + Rs * 0.48f, c.y - Rs * 0.48f), Rs * 0.10f, &kGlassRamp);
}

/* ------------------------------------------------------------------ */
/* Public                                                             */
/* ------------------------------------------------------------------ */

static void Layout(CGRect b, CGPoint *c, CGFloat *R, CGFloat *Rs, CGFloat *lw)
{
    CGFloat m = (b.size.width < b.size.height) ? b.size.width : b.size.height;
    c->x = b.origin.x + b.size.width * 0.5f;
    c->y = b.origin.y + b.size.height * 0.5f;
    *R  = m * BEZEL_FRAC;
    *Rs = *R * SOCKET_FRAC;
    *lw = m * 0.0022f;
    if (*lw < 1) *lw = 1;
}

CGRect EyeSocketBounds(CGRect bounds)
{
    CGPoint c; CGFloat R, Rs, lw;
    Layout(bounds, &c, &R, &Rs, &lw);
    return CGRectIntegral(CircleRect(c, Rs + 2 * lw));
}

void EyeDraw(CGContextRef ctx, CGRect bounds, const EyeState *s)
{
    CGPoint c; CGFloat R, Rs, lw, far;
    Layout(bounds, &c, &R, &Rs, &lw);
    far = (CGFloat)sqrt(bounds.size.width * bounds.size.width +
                        bounds.size.height * bounds.size.height) * 0.5f;

    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, bounds);

    /* background: near-black with a faint vignette */
    CGContextSetRGBFillColor(ctx, 0, 0, 0, 1);
    CGContextFillRect(ctx, bounds);
    Radial(ctx, c, 0, c, far, &kBackgroundRamp);

    DrawBezel(ctx, c, R, Rs, lw);

    /* everything inside the socket */
    CGContextSaveGState(ctx);
    ClipToCircle(ctx, c, Rs);
    DrawIris(ctx, c, Rs, lw, s);
    DrawLids(ctx, c, Rs, lw, s->blink);
    Radial(ctx, c, Rs * 0.70f, c, Rs, &kInnerShadowRamp);
    DrawGlass(ctx, c, Rs);
    CGContextRestoreGState(ctx);

    StrokeCircle(ctx, c, Rs, lw * 1.5f, 0, 0, 0, 0.9f);

    CGContextRestoreGState(ctx);
}
