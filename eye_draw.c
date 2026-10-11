/*
 * eye_draw.c - Pure CoreGraphics renderer for the robot eye.
 *
 * Cartoon line-art style: white outlines on a black background.
 * Only uses APIs available on Mac OS X 10.4.
 */
#include "eye_draw.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ------------------------------------------------------------------ */
/* Look                                                               */
/* ------------------------------------------------------------------ */

#define BEZEL_FRAC    0.44f   /* outer ring radius / min(screen w,h)     */
#define SOCKET_FRAC   0.80f   /* socket radius / outer ring radius       */
#define IRIS_FRAC     0.52f   /* iris radius / socket radius             */
#define LINE_FRAC     0.016f  /* main line width / min(screen w,h)       */
#define SQUASH        0.20f   /* iris foreshortening at full deflection  */
#define BOLT_COUNT    8

/* Line colour (white) and background (black). */
#define INK_R 1.0f
#define INK_G 1.0f
#define INK_B 1.0f
#define BG_R  0.0f
#define BG_G  0.0f
#define BG_B  0.0f

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static CGRect CircleRect(CGPoint c, CGFloat r)
{
    return CGRectMake(c.x - r, c.y - r, 2 * r, 2 * r);
}

static void Ink(CGContextRef ctx)
{
    CGContextSetRGBStrokeColor(ctx, INK_R, INK_G, INK_B, 1);
    CGContextSetRGBFillColor(ctx, INK_R, INK_G, INK_B, 1);
}

static void Paper(CGContextRef ctx)
{
    CGContextSetRGBFillColor(ctx, BG_R, BG_G, BG_B, 1);
}

static void StrokeCircle(CGContextRef ctx, CGPoint c, CGFloat r, CGFloat width)
{
    CGContextSetLineWidth(ctx, width);
    CGContextStrokeEllipseInRect(ctx, CircleRect(c, r));
}

static void ClipToCircle(CGContextRef ctx, CGPoint c, CGFloat r)
{
    CGContextBeginPath(ctx);
    CGContextAddEllipseInRect(ctx, CircleRect(c, r));
    CGContextClip(ctx);
}

/* ------------------------------------------------------------------ */
/* Scene parts                                                        */
/* ------------------------------------------------------------------ */

static void DrawHousing(CGContextRef ctx, CGPoint c, CGFloat R, CGFloat Rs, CGFloat T)
{
    CGFloat orbit = (R + Rs) * 0.5f;
    CGFloat boltR = (R - Rs) * 0.16f;
    int i;

    Ink(ctx);
    StrokeCircle(ctx, c, R, T * 1.2f);

    /* little outlined bolts between the rings */
    for (i = 0; i < BOLT_COUNT; i++) {
        double a = (M_PI / BOLT_COUNT) + i * (2 * M_PI / BOLT_COUNT);
        StrokeCircle(ctx, CGPointMake(c.x + orbit * (CGFloat)cos(a),
                                      c.y + orbit * (CGFloat)sin(a)),
                     boltR, T * 0.45f);
    }
}

static void DrawIris(CGContextRef ctx, CGPoint c, CGFloat Rs, CGFloat T,
                     const EyeState *s)
{
    CGFloat Ri = Rs * IRIS_FRAC;
    CGFloat travel = Rs - Ri - T * 1.5f;
    CGFloat lx = s->lookX, ly = s->lookY;
    CGFloat mag = (CGFloat)sqrt(lx * lx + ly * ly);
    CGFloat angle, pupilR, dash[2];
    CGPoint o = CGPointMake(0, 0);

    if (mag > 1) { lx /= mag; ly /= mag; mag = 1; }
    angle = (CGFloat)atan2(ly, lx);

    CGContextSaveGState(ctx);
    CGContextTranslateCTM(ctx, c.x + lx * travel, c.y + ly * travel);
    /* cartoon squash along the gaze direction */
    CGContextRotateCTM(ctx, angle);
    CGContextScaleCTM(ctx, 1.0f - SQUASH * mag, 1.0f);
    CGContextRotateCTM(ctx, -angle);

    /* solid black lens body hides the glass glint behind it */
    Paper(ctx);
    CGContextFillEllipseInRect(ctx, CircleRect(o, Ri));
    Ink(ctx);
    StrokeCircle(ctx, o, Ri, T);

    /* dashed inner lens ring */
    dash[0] = T * 0.9f;
    dash[1] = T * 0.9f;
    CGContextSetLineDash(ctx, 0, dash, 2);
    StrokeCircle(ctx, o, Ri * 0.80f, T * 0.4f);
    CGContextSetLineDash(ctx, 0, NULL, 0);

    /* solid pupil with cartoon glints */
    pupilR = Ri * (0.30f + 0.20f * s->pupil);
    CGContextFillEllipseInRect(ctx, CircleRect(o, pupilR));
    Paper(ctx);
    CGContextFillEllipseInRect(ctx, CircleRect(CGPointMake(-pupilR * 0.38f, pupilR * 0.38f),
                                               pupilR * 0.26f));
    CGContextFillEllipseInRect(ctx, CircleRect(CGPointMake(pupilR * 0.30f, -pupilR * 0.34f),
                                               pupilR * 0.11f));

    CGContextRestoreGState(ctx);
}

static void DrawLids(CGContextRef ctx, CGPoint c, CGFloat Rs, CGFloat T, CGFloat blink)
{
    CGFloat topEdge = c.y + Rs - blink * Rs;
    CGFloat botEdge = c.y - Rs + blink * Rs;

    if (blink <= 0.001f)
        return;

    Paper(ctx);
    CGContextFillRect(ctx, CGRectMake(c.x - Rs, topEdge, 2 * Rs, c.y + Rs - topEdge));
    CGContextFillRect(ctx, CGRectMake(c.x - Rs, c.y - Rs, 2 * Rs, botEdge - (c.y - Rs)));

    Ink(ctx);
    CGContextSetLineWidth(ctx, T * 0.8f);
    CGContextMoveToPoint(ctx, c.x - Rs, topEdge);
    CGContextAddLineToPoint(ctx, c.x + Rs, topEdge);
    CGContextMoveToPoint(ctx, c.x - Rs, botEdge);
    CGContextAddLineToPoint(ctx, c.x + Rs, botEdge);
    CGContextStrokePath(ctx);
}

/* Cartoon reflection on the glass: an arc and a dot, upper left. */
static void DrawGlint(CGContextRef ctx, CGPoint c, CGFloat Rs, CGFloat T)
{
    double a;
    CGFloat r = Rs * 0.84f;

    Ink(ctx);
    CGContextSetLineCap(ctx, kCGLineCapRound);
    CGContextSetLineWidth(ctx, T * 0.6f);
    CGContextBeginPath(ctx);
    CGContextAddArc(ctx, c.x, c.y, r, (CGFloat)(M_PI * 0.58), (CGFloat)(M_PI * 0.80), 0);
    CGContextStrokePath(ctx);

    a = M_PI * 0.87;
    CGContextFillEllipseInRect(ctx, CircleRect(CGPointMake(c.x + r * (CGFloat)cos(a),
                                                           c.y + r * (CGFloat)sin(a)),
                                               T * 0.38f));
    CGContextSetLineCap(ctx, kCGLineCapButt);
}

/* ------------------------------------------------------------------ */
/* Public                                                             */
/* ------------------------------------------------------------------ */

static void Layout(CGRect b, CGPoint *c, CGFloat *R, CGFloat *Rs, CGFloat *T)
{
    CGFloat m = (b.size.width < b.size.height) ? b.size.width : b.size.height;
    c->x = b.origin.x + b.size.width * 0.5f;
    c->y = b.origin.y + b.size.height * 0.5f;
    *R  = m * BEZEL_FRAC;
    *Rs = *R * SOCKET_FRAC;
    *T  = m * LINE_FRAC;
    if (*T < 2) *T = 2;
}

CGRect EyeSocketBounds(CGRect bounds)
{
    CGPoint c; CGFloat R, Rs, T;
    Layout(bounds, &c, &R, &Rs, &T);
    return CGRectIntegral(CircleRect(c, Rs + T));
}

void EyeDraw(CGContextRef ctx, CGRect bounds, const EyeState *s)
{
    CGPoint c; CGFloat R, Rs, T;
    Layout(bounds, &c, &R, &Rs, &T);

    CGContextSaveGState(ctx);
    CGContextClipToRect(ctx, bounds);

    Paper(ctx);
    CGContextFillRect(ctx, bounds);

    DrawHousing(ctx, c, R, Rs, T);

    /* everything inside the socket */
    CGContextSaveGState(ctx);
    ClipToCircle(ctx, c, Rs);
    DrawGlint(ctx, c, Rs, T);
    DrawIris(ctx, c, Rs, T, s);
    DrawLids(ctx, c, Rs, T, s->blink);
    CGContextRestoreGState(ctx);

    Ink(ctx);
    StrokeCircle(ctx, c, Rs, T);

    CGContextRestoreGState(ctx);
}
