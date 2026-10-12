/*
 * EyeView.m - Full-screen view that animates the robot face.
 *
 * The eyes hold a gaze for a random interval, then dart (or, sometimes,
 * slowly scan) to a new direction or back to straight ahead. They blink
 * now and then. The mouth mostly smiles, with the occasional frown or
 * surprised "O". Any key quits.
 */
#import "EyeView.h"

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define FRAME_INTERVAL   (1.0 / 30.0)
#define BLINK_DURATION   0.28
#define CENTER_CHANCE    0.30   /* chance the next look is straight ahead */
#define SCAN_CHANCE      0.15   /* chance a move is a slow scan            */
#define HOLD_MIN         0.7    /* seconds to hold a gaze                  */
#define HOLD_MAX         4.0
#define BLINK_GAP_MIN    3.0    /* seconds between blinks                  */
#define BLINK_GAP_MAX    9.0
#define FROWN_CHANCE     0.15   /* per mood change                         */
#define SURPRISE_CHANCE  0.15
#define SMILE_HOLD_MIN   3.0    /* seconds between mood changes            */
#define SMILE_HOLD_MAX   8.0

static double Now(void)
{
    return CFAbsoluteTimeGetCurrent();
}

static double RandUnit(void)
{
    return (double)random() / 2147483647.0;
}

static double RandRange(double lo, double hi)
{
    return lo + (hi - lo) * RandUnit();
}

/* Fast start, soft landing - like a saccade. */
static double EaseOutCubic(double t)
{
    double u = 1.0 - t;
    return 1.0 - u * u * u;
}

static double EaseInOut(double t)
{
    return t * t * (3.0 - 2.0 * t);
}

@implementation EyeView

- (id)initWithFrame:(NSRect)frame
{
    self = [super initWithFrame:frame];
    if (self != nil) {
        double now = Now();

        srandom((unsigned)time(NULL) ^ ((unsigned)getpid() << 16));
        eye.lookX = 0;
        eye.lookY = 0;
        eye.blink = 0;
        eye.smile = 1;
        eye.surprise = 0;
        toSmile = 1;
        toSurprise = 0;
        holdUntil = now + 1.5;
        nextExpr = now + RandRange(SMILE_HOLD_MIN, SMILE_HOLD_MAX);
        nextBlink = now + RandRange(2.0, 5.0);
    }
    return self;
}

- (void)dealloc
{
    [self stopAnimating];
    [super dealloc];
}

- (void)startAnimating
{
    if (timer == nil) {
        timer = [[NSTimer scheduledTimerWithTimeInterval:FRAME_INTERVAL
                                                  target:self
                                                selector:@selector(tick:)
                                                userInfo:nil
                                                 repeats:YES] retain];
    }
}

- (void)stopAnimating
{
    [timer invalidate];
    [timer release];
    timer = nil;
}

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)keyDown:(NSEvent *)event
{
    [NSApp terminate:self];
}

- (void)drawRect:(NSRect)rect
{
    CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    NSRect b = [self bounds];

    EyeDraw(ctx, CGRectMake(b.origin.x, b.origin.y, b.size.width, b.size.height), &eye);
}

/* Only the visor changes while animating; redraw just that area. */
- (NSRect)socketRect
{
    NSRect b = [self bounds];
    CGRect r = EyeSocketBounds(CGRectMake(b.origin.x, b.origin.y,
                                          b.size.width, b.size.height));
    return NSMakeRect(r.origin.x, r.origin.y, r.size.width, r.size.height);
}

- (void)chooseNextLook:(double)now
{
    double tx = 0, ty = 0, dx, dy, dist = 0;
    int tries;

    /* Pick somewhere noticeably different from where we are now. */
    for (tries = 0; tries < 8; tries++) {
        if (RandUnit() < CENTER_CHANCE) {
            tx = 0;
            ty = 0;
        } else {
            double a = RandRange(0, 2 * M_PI);
            double r = RandRange(0.5, 1.0);
            tx = r * cos(a);
            ty = r * sin(a);
        }
        dx = tx - eye.lookX;
        dy = ty - eye.lookY;
        dist = sqrt(dx * dx + dy * dy);
        if (dist > 0.35)
            break;
    }

    fromX = eye.lookX;
    fromY = eye.lookY;
    toX = tx;
    toY = ty;
    slowMove = (RandUnit() < SCAN_CHANCE);
    moveDuration = slowMove ? RandRange(1.0, 2.0) : 0.10 + 0.12 * dist;
    moveStart = now;
    moving = YES;
}

- (void)setExpressionSmile:(double)smile surprise:(double)surprise
                  duration:(double)duration now:(double)now
{
    fromSmile = eye.smile;
    fromSurprise = eye.surprise;
    toSmile = smile;
    toSurprise = surprise;
    exprStart = now;
    exprDuration = duration;
    exprChanging = YES;
}

- (void)chooseNextExpression:(double)now
{
    double r;

    /* After a frown or a surprise, always go back to smiling. */
    if (toSmile < 0.3 || toSurprise > 0.5) {
        [self setExpressionSmile:RandRange(0.6, 1.0) surprise:0 duration:0.4 now:now];
        nextExpr = now + RandRange(SMILE_HOLD_MIN, SMILE_HOLD_MAX);
        return;
    }

    r = RandUnit();
    if (r < FROWN_CHANCE) {
        [self setExpressionSmile:-1.0 surprise:0 duration:0.5 now:now];
        nextExpr = now + RandRange(1.2, 2.5);
    } else if (r < FROWN_CHANCE + SURPRISE_CHANCE) {
        /* snap into the "O", and glance at whatever startled us */
        [self setExpressionSmile:0 surprise:1.0 duration:0.12 now:now];
        nextExpr = now + RandRange(0.9, 1.8);
        [self chooseNextLook:now];
        slowMove = NO;
        moveDuration = 0.12;
    } else {
        /* a slightly different smile */
        [self setExpressionSmile:RandRange(0.4, 1.0) surprise:0 duration:0.5 now:now];
        nextExpr = now + RandRange(SMILE_HOLD_MIN, SMILE_HOLD_MAX);
    }
}

- (void)tick:(NSTimer *)t
{
    double now = Now();
    double p, e;
    BOOL dirty = NO;

    if (moving) {
        p = (now - moveStart) / moveDuration;
        if (p >= 1.0) {
            p = 1.0;
            moving = NO;
            holdUntil = now + RandRange(HOLD_MIN, HOLD_MAX);
        }
        e = slowMove ? EaseInOut(p) : EaseOutCubic(p);
        eye.lookX = fromX + (toX - fromX) * e;
        eye.lookY = fromY + (toY - fromY) * e;
        dirty = YES;
    } else if (now >= holdUntil) {
        [self chooseNextLook:now];
    }

    if (!exprChanging && now >= nextExpr)
        [self chooseNextExpression:now];
    if (exprChanging) {
        p = (now - exprStart) / exprDuration;
        if (p >= 1.0) {
            p = 1.0;
            exprChanging = NO;
        }
        e = EaseInOut(p);
        eye.smile = fromSmile + (toSmile - fromSmile) * e;
        eye.surprise = fromSurprise + (toSurprise - fromSurprise) * e;
        dirty = YES;
    }

    if (!blinking && now >= nextBlink) {
        blinking = YES;
        blinkStart = now;
    }
    if (blinking) {
        p = (now - blinkStart) / BLINK_DURATION;
        if (p >= 1.0) {
            eye.blink = 0;
            blinking = NO;
            /* occasionally a quick double blink */
            nextBlink = now + ((RandUnit() < 0.15) ? 0.15
                                                   : RandRange(BLINK_GAP_MIN, BLINK_GAP_MAX));
        } else {
            eye.blink = (p < 0.45) ? p / 0.45 : (1.0 - p) / 0.55;
        }
        dirty = YES;
    }

    if (dirty)
        [self setNeedsDisplayInRect:[self socketRect]];
}

@end
