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
    /* lookX, lookY, blink, smile, surprise */
    static const EyeState poses[6] = {
        {  0.00f,  0.00f, 0.00f,  1.00f, 0.00f },   /* straight ahead, smiling */
        { -0.90f,  0.30f, 0.00f,  0.70f, 0.00f },   /* glance left             */
        {  0.85f, -0.40f, 0.00f, -1.00f, 0.00f },   /* frown, looking down-right */
        {  0.00f,  0.60f, 0.00f,  0.00f, 1.00f },   /* surprised "O", looking up */
        {  1.00f,  0.00f, 0.00f,  0.90f, 0.00f },   /* full right, leaning     */
        {  0.20f,  0.00f, 0.85f,  1.00f, 0.00f }    /* mid-blink               */
    };
    const char *fullPath = (argc > 1) ? argv[1] : "cartoon-face.png";
    const char *posesPath = (argc > 2) ? argv[2] : "cartoon-face-poses.png";
    void *buf;
    CGContextRef ctx;
    int i, ok;

    ctx = MakeBitmap(&buf);
    EyeDraw(ctx, CGRectMake(0, 0, W, H), &poses[0]);
    ok = WritePNG(ctx, fullPath);
    CGContextRelease(ctx);
    free(buf);

    ctx = MakeBitmap(&buf);
    for (i = 0; i < 6; i++) {
        CGRect r = CGRectMake((i % 3) * (W / 3), (1 - i / 3) * (H / 2), W / 3, H / 2);
        EyeDraw(ctx, r, &poses[i]);
    }
    CGContextSetRGBStrokeColor(ctx, 0.4f, 0.4f, 0.4f, 1);
    CGContextSetLineWidth(ctx, 2);
    CGContextMoveToPoint(ctx, W / 3, 0);       CGContextAddLineToPoint(ctx, W / 3, H);
    CGContextMoveToPoint(ctx, 2 * W / 3, 0);   CGContextAddLineToPoint(ctx, 2 * W / 3, H);
    CGContextMoveToPoint(ctx, 0, H / 2);       CGContextAddLineToPoint(ctx, W, H / 2);
    CGContextStrokePath(ctx);
    ok = WritePNG(ctx, posesPath) && ok;
    CGContextRelease(ctx);
    free(buf);

    return ok ? 0 : 1;
}
