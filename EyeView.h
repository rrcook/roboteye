/*
 * EyeView.h - Full-screen view that animates the robot face.
 */
#import <Cocoa/Cocoa.h>
#import "eye_draw.h"

@interface EyeView : NSView
{
    EyeState eye;
    NSTimer *timer;

    /* gaze movement */
    BOOL   moving;
    BOOL   slowMove;
    double fromX, fromY, toX, toY;
    double moveStart, moveDuration;
    double holdUntil;

    /* mouth expression */
    BOOL   exprChanging;
    double fromSmile, toSmile, fromSurprise, toSurprise;
    double exprStart, exprDuration;
    double nextExpr;

    /* blink */
    BOOL   blinking;
    double blinkStart, nextBlink;
}

- (void)startAnimating;
- (void)stopAnimating;

@end
