/*
 * preview.c - Render still images of the robot eye to PNG files using
 * the same eye_draw.c code the application uses.
 *
 *   preview <full.png> <poses.png>
 *
 * Builds on 10.4 or any later Mac OS X (uses ImageIO, 10.4+).
 */
#include "../eye_draw.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 1440
#define H 900

static CGContextRef MakeBitmap(void **buf)
{
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx;
    *buf = calloc((size_t)W * H, 4);
    ctx = CGBitmapContextCreate(*buf, W, H, 8, W * 4, cs, kCGImageAlphaPremultipliedLast);
    CGColorSpaceRelease(cs);
    return ctx;
}

static int WritePNG(CGContextRef ctx, const char *path)
{
    CGImageRef img = CGBitmapContextCreateImage(ctx);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path,
                                                           (CFIndex)strlen(path), false);
    CGImageDestinationRef dst = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, NULL);
    int ok = 0;

    if (dst != NULL) {
        CGImageDestinationAddImage(dst, img, NULL);
        ok = CGImageDestinationFinalize(dst);
        CFRelease(dst);
    }
    CFRelease(url);
    CGImageRelease(img);
    if (!ok)
        fprintf(stderr, "preview: could not write %s\n", path);
    return ok;
}

int main(int argc, char **argv)
{
    static const EyeState poses[4] = {
        {  0.00f,  0.00f, 0.50f, 0.00f },   /* straight ahead          */
        { -0.75f,  0.62f, 0.10f, 0.00f },   /* up-left, pupil small    */
        {  1.00f, -0.05f, 1.00f, 0.00f },   /* right, pupil dilated    */
        {  0.10f, -0.70f, 0.50f, 0.55f }    /* down, mid-blink         */
    };
    const char *fullPath = (argc > 1) ? argv[1] : "robot_eye_preview.png";
    const char *posesPath = (argc > 2) ? argv[2] : "robot_eye_poses.png";
    void *buf;
    CGContextRef ctx;
    int i, ok;

    ctx = MakeBitmap(&buf);
    EyeDraw(ctx, CGRectMake(0, 0, W, H), &poses[0]);
    ok = WritePNG(ctx, fullPath);
    CGContextRelease(ctx);
    free(buf);

    ctx = MakeBitmap(&buf);
    for (i = 0; i < 4; i++) {
        CGRect r = CGRectMake((i % 2) * (W / 2), (1 - i / 2) * (H / 2), W / 2, H / 2);
        EyeDraw(ctx, r, &poses[i]);
    }
    CGContextSetRGBStrokeColor(ctx, 0.3f, 0.3f, 0.3f, 1);
    CGContextSetLineWidth(ctx, 2);
    CGContextMoveToPoint(ctx, W / 2, 0);   CGContextAddLineToPoint(ctx, W / 2, H);
    CGContextMoveToPoint(ctx, 0, H / 2);   CGContextAddLineToPoint(ctx, W, H / 2);
    CGContextStrokePath(ctx);
    ok = WritePNG(ctx, posesPath) && ok;
    CGContextRelease(ctx);
    free(buf);

    return ok ? 0 : 1;
}
