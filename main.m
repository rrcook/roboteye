/*
 * main.m - RobotEye: a full-screen robot eye for Mac OS X 10.4 (PowerPC).
 *
 * No nib: the application, window and view are created in code.
 */
#import <Cocoa/Cocoa.h>
#import <Carbon/Carbon.h>   /* SetSystemUIMode: hides menu bar and Dock */
#import "EyeView.h"

/* Borderless windows refuse key status by default; we need it for keyDown. */
@interface FullScreenWindow : NSWindow
@end

@implementation FullScreenWindow

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (BOOL)canBecomeMainWindow
{
    return YES;
}

/* Don't let AppKit push the window down below the menu bar area. */
- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen
{
    return frameRect;
}

@end

@interface AppController : NSObject
{
    FullScreenWindow *window;
    EyeView *eyeView;
}
@end

@implementation AppController

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    NSRect frame = [[NSScreen mainScreen] frame];

    SetSystemUIMode(kUIModeAllHidden, 0);

    window = [[FullScreenWindow alloc] initWithContentRect:frame
                                                 styleMask:NSBorderlessWindowMask
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
    [window setBackgroundColor:[NSColor blackColor]];
    [window setOpaque:YES];
    [window setHasShadow:NO];
    [window setFrame:frame display:NO];

    eyeView = [[EyeView alloc] initWithFrame:NSMakeRect(0, 0, frame.size.width, frame.size.height)];
    [window setContentView:eyeView];

    [NSApp activateIgnoringOtherApps:YES];
    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:eyeView];
    [NSCursor hide];

    [eyeView startAnimating];
}

- (void)applicationWillTerminate:(NSNotification *)note
{
    [eyeView stopAnimating];
    [NSCursor unhide];
    SetSystemUIMode(kUIModeNormal, 0);
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    AppController *controller;

    [NSApplication sharedApplication];
    controller = [[AppController alloc] init];
    [NSApp setDelegate:controller];
    [NSApp run];

    [controller release];
    [pool release];
    return 0;
}
