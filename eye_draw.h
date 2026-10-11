/*
 * eye_draw.h - Pure CoreGraphics renderer for the robot eye.
 *
 * Kept free of Cocoa so the exact same drawing code is used by the
 * application and by the command-line preview tool.
 *
 * Targets Mac OS X 10.4 (CoreGraphics only, no 10.5+ APIs).
 */
#ifndef EYE_DRAW_H
#define EYE_DRAW_H

#include <ApplicationServices/ApplicationServices.h>

/* CGFloat appeared in the 10.5 SDK; on 10.4 everything is float. */
#ifndef CGFLOAT_DEFINED
typedef float CGFloat;
#define CGFLOAT_DEFINED 1
#endif

typedef struct {
    CGFloat lookX;   /* -1..1, gaze direction (unit disk), +x = right   */
    CGFloat lookY;   /* -1..1, +y = up                                   */
    CGFloat pupil;   /* 0 = constricted, 1 = dilated                     */
    CGFloat blink;   /* 0 = fully open, 1 = shutters fully closed        */
} EyeState;

/* Rectangle (in bounds coordinates) that changes when the eye animates. */
CGRect EyeSocketBounds(CGRect bounds);

/* Draw the complete scene (background, bezel, eye) into bounds.
   Coordinates are Quartz-style: origin bottom-left, y up. */
void EyeDraw(CGContextRef ctx, CGRect bounds, const EyeState *state);

#endif
