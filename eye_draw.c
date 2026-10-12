/*
 * eye_draw.c - Pure CoreGraphics renderer for the robot face.
 *
 * Style: cute cartoon robot head - white helmet with a dark outline,
 * dark visor, two glowing blue oval eyes and a blue mouth that morphs
 * between smile, frown and a surprised "O".
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

#define HEAD_H_FRAC   0.74f   /* head height / min(screen w,h)          */
#define HEAD_ASPECT   1.20f   /* head width / head height               */
#define VISOR_W_FRAC  0.80f   /* visor width / head width               */
#define VISOR_H_FRAC  0.64f   /* visor height / head height             */

#define EYE_X         0.25f   /* eye centre offset / visor width        */
#define EYE_Y         0.08f   /* eye centre offset / visor height       */
#define EYE_W         0.135f  /* eye width / visor width                */
#define EYE_H         0.40f   /* eye height / visor height              */
#define GAZE_X        0.09f   /* max eye travel / visor width           */
#define GAZE_Y        0.09f   /* max eye travel / visor height          */

#define MOUTH_Y      -0.24f   /* mouth centre offset / visor height     */

/* ------------------------------------------------------------------ */
/* Colours                                                            */
/* ------------------------------------------------------------------ */

#define OUTLINE_R 0.09f       /* dark navy cartoon outline */
#define OUTLINE_G 0.11f
#define OUTLINE_B 0.22f

#define GLOW_R    0.30f       /* eye / mouth blue */
#define GLOW_G    0.72f
#define GLOW_B    1.00f

#define MAX_STOPS 5

typedef struct { CGFloat pos, r, g, b, a; } Stop;
typedef struct { int n; Stop s[MAX_STOPS]; } Ramp;

static const Ramp kBackgroundRamp = { 2, {
    { 0.00f, 0.13f, 0.16f, 0.21f, 1.0f },
    { 1.00f, 0.02f, 0.03f, 0.05f, 1.0f } } };

static const Ramp kHelmetRamp = { 3, {           /* top -> bottom */
    { 0.00f, 1.00f, 1.00f, 1.00f, 1.0f },
    { 0.55f, 0.96f, 0.97f, 0.99f, 1.0f },
    { 1.00f, 0.76f, 0.82f, 0.90f, 1.0f } } };

static const Ramp kVisorRamp = { 2, {            /* centre -> edge */
    { 0.00f, 0.11f, 0.13f, 0.27f, 1.0f },
    { 1.00f, 0.04f, 0.05f, 0.12f, 1.0f } } };

static const Ramp kGlowRamp = { 3, {
    { 0.00f, GLOW_R, GLOW_G, GLOW_B, 0.45f },
    { 0.45f, GLOW_R, GLOW_G, GLOW_B, 0.18f },
    { 1.00f, GLOW_R, GLOW_G, GLOW_B, 0.00f } } };

static const Ramp kEyeRamp = { 3, {              /* centre -> edge */
    { 0.00f, 0.62f, 0.90f, 1.00f, 1.0f },
    { 0.60f, GLOW_R, GLOW_G, GLOW_B, 1.0f },
    { 1.00f, 0.20f, 0.58f, 0.95f, 1.0f } } };

static const Ramp kGlossRamp = { 2, {
    { 0.00f, 1.0f, 1.0f, 1.0f, 0.16f },
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

/* CGPathAddRoundedRect is 10.5+, so build it from arcs. */
static void AddRoundRect(CGContextRef ctx, CGRect r, CGFloat rad)
{
    CGFloat minX = CGRectGetMinX(r), maxX = CGRectGetMaxX(r);
    CGFloat minY = CGRectGetMinY(r), maxY = CGRectGetMaxY(r);
    CGFloat lim = ((r.size.width < r.size.height) ? r.size.width : r.size.height) * 0.5f;

    if (rad > lim) rad = lim;
    CGContextMoveToPoint(ctx, minX + rad, minY);
    CGContextAddArcToPoint(ctx, maxX, minY, maxX, maxY, rad);
    CGContextAddArcToPoint(ctx, maxX, maxY, minX, maxY, rad);
    CGContextAddArcToPoint(ctx, minX, maxY, minX, minY, rad);
    CGContextAddArcToPoint(ctx, minX, minY, maxX, minY, rad);
    CGContextClosePath(ctx);
}

static void SetOutline(CGContextRef ctx, CGFloat width)
{
    CGContextSetRGBStrokeColor(ctx, OUTLINE_R, OUTLINE_G, OUTLINE_B, 1);
    CGContextSetLineWidth(ctx, width);
}

/* White part with vertical shading and a dark outline. */
static void HelmetEllipse(CGContextRef ctx, CGRect r, CGFloat lw)
{
    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    CGContextAddEllipseInRect(ctx, r);
    CGContextClip(ctx);
    Axial(ctx, CGPointMake(0, CGRectGetMaxY(r)), CGPointMake(0, CGRectGetMinY(r)), &kHelmetRamp);
    CGContextRestoreGState(ctx);
    SetOutline(ctx, lw);
    CGContextStrokeEllipseInRect(ctx, r);
}

/* ------------------------------------------------------------------ */
/* Layout                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    CGPoint c;          /* head centre      */
    CGFloat hw, hh;     /* head size        */
    CGPoint vc;         /* visor centre     */
    CGFloat vw, vh;     /* visor size       */
    CGFloat lw;         /* outline width    */
} Layout;

static void DoLayout(CGRect b, Layout *L)
{
    CGFloat m = (b.size.width < b.size.height) ? b.size.width : b.size.height;

    L->hh = m * HEAD_H_FRAC;
    L->hw = L->hh * HEAD_ASPECT;
    L->c = CGPointMake(b.origin.x + b.size.width * 0.5f,
                       b.origin.y + b.size.height * 0.5f - m * 0.02f);
    L->vw = L->hw * VISOR_W_FRAC;
    L->vh = L->hh * VISOR_H_FRAC;
    L->vc = CGPointMake(L->c.x, L->c.y - L->hh * 0.03f);
    L->lw = m * 0.012f;
    if (L->lw < 2) L->lw = 2;
}

/* ------------------------------------------------------------------ */
/* Scene parts                                                        */
/* ------------------------------------------------------------------ */

static void DrawHead(CGContextRef ctx, const Layout *L)
{
    CGFloat earR = L->hh * 0.15f;
    CGRect head = CenteredRect(L->c, L->hw, L->hh);
    CGRect ear;
    int side;

    /* antenna dome peeking over the top */
    HelmetEllipse(ctx, CenteredRect(CGPointMake(L->c.x, CGRectGetMaxY(head) + L->hh * 0.015f),
                                    L->hh * 0.24f, L->hh * 0.15f), L->lw);
    CGContextSetRGBFillColor(ctx, OUTLINE_R, OUTLINE_G, OUTLINE_B, 1);
    CGContextFillEllipseInRect(ctx, CenteredRect(CGPointMake(L->c.x, CGRectGetMaxY(head) + L->hh * 0.04f),
                                                 L->hh * 0.09f, L->hh * 0.05f));

    /* ear pods */
    for (side = -1; side <= 1; side += 2) {
        CGPoint ec = CGPointMake(L->c.x + side * L->hw * 0.50f, L->c.y + L->hh * 0.02f);
        ear = CenteredRect(ec, earR * 1.15f, earR * 2.0f);
        HelmetEllipse(ctx, ear, L->lw);
        SetOutline(ctx, L->lw * 0.6f);
        CGContextStrokeEllipseInRect(ctx, CGRectInset(ear, earR * 0.18f, earR * 0.30f));
    }

    /* helmet */
    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    AddRoundRect(ctx, head, L->hh * 0.46f);
    CGContextClip(ctx);
    Axial(ctx, CGPointMake(0, CGRectGetMaxY(head)), CGPointMake(0, CGRectGetMinY(head)), &kHelmetRamp);
    CGContextRestoreGState(ctx);
    CGContextBeginPath(ctx);
    AddRoundRect(ctx, head, L->hh * 0.46f);
    SetOutline(ctx, L->lw);
    CGContextStrokePath(ctx);
}

static void DrawVisor(CGContextRef ctx, const Layout *L)
{
    CGRect visor = CenteredRect(L->vc, L->vw, L->vh);

    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    AddRoundRect(ctx, visor, L->vh * 0.45f);
    CGContextClip(ctx);
    CGContextSetRGBFillColor(ctx, 0.04f, 0.05f, 0.12f, 1);
    CGContextFillRect(ctx, visor);
    EllipticalRadial(ctx, L->vc, L->vw * 0.55f, L->vh * 0.60f, &kVisorRamp);
    CGContextRestoreGState(ctx);
}

/* Soft glass sheen, drawn last so it sits over the eyes. */
static void DrawVisorGloss(CGContextRef ctx, const Layout *L)
{
    CGRect visor = CenteredRect(L->vc, L->vw, L->vh);

    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    AddRoundRect(ctx, visor, L->vh * 0.45f);
    CGContextClip(ctx);
    EllipticalRadial(ctx, CGPointMake(L->vc.x - L->vw * 0.22f, L->vc.y + L->vh * 0.36f),
                     L->vw * 0.30f, L->vh * 0.14f, &kGlossRamp);
    CGContextRestoreGState(ctx);

    CGContextBeginPath(ctx);
    AddRoundRect(ctx, visor, L->vh * 0.45f);
    SetOutline(ctx, L->lw);
    CGContextStrokePath(ctx);
}

static void DrawEyes(CGContextRef ctx, const Layout *L, const EyeState *s,
                     CGFloat gx, CGFloat gy)
{
    CGFloat grow = 1.0f + 0.15f * s->surprise;
    CGFloat ew = L->vw * EYE_W * grow;
    CGFloat eh = L->vh * EYE_H * grow;
    CGFloat open = 1.0f - s->blink;
    CGFloat h;
    int side;

    /* a blink squashes the oval down to a thin bar */
    h = eh * open;
    if (h < ew * 0.22f)
        h = ew * 0.22f;

    for (side = -1; side <= 1; side += 2) {
        CGPoint ec = CGPointMake(L->vc.x + side * L->vw * EYE_X + gx,
                                 L->vc.y + L->vh * EYE_Y + gy);
        CGRect eye = CenteredRect(ec, ew, h);

        EllipticalRadial(ctx, ec, ew * 1.25f, h * 0.5f + ew * 0.75f, &kGlowRamp);

        CGContextSaveGState(ctx);
        CGContextBeginPath(ctx);
        CGContextAddEllipseInRect(ctx, eye);
        CGContextClip(ctx);
        CGContextSetRGBFillColor(ctx, GLOW_R, GLOW_G, GLOW_B, 1);
        CGContextFillRect(ctx, eye);
        EllipticalRadial(ctx, ec, ew * 0.5f, h * 0.5f, &kEyeRamp);
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
    return CGRectIntegral(CGRectInset(CenteredRect(L.vc, L.vw, L.vh), -L.lw, -L.lw));
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
    Radial(ctx, L.c, 0, L.c, far, &kBackgroundRamp);

    DrawHead(ctx, &L);
    DrawVisor(ctx, &L);

    CGContextSaveGState(ctx);
    CGContextBeginPath(ctx);
    AddRoundRect(ctx, CenteredRect(L.vc, L.vw, L.vh), L.vh * 0.45f);
    CGContextClip(ctx);
    DrawEyes(ctx, &L, s, gx, gy);
    DrawMouth(ctx, &L, s, gx, gy);
    CGContextRestoreGState(ctx);

    DrawVisorGloss(ctx, &L);

    CGContextRestoreGState(ctx);
}
