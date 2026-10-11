/*
 * EyeView.h - Full-screen view that animates the robot eye.
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

    /* pupil */
    BOOL   pupilChanging;
    double fromPupil, toPupil;
    double pupilStart, pupilDuration;

    /* blink */
    BOOL   blinking;
    double blinkStart, nextBlink;
}

- (void)startAnimating;
- (void)stopAnimating;

@end
