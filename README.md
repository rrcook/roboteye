# RobotEye

A full-screen cute robot face for Mac OS X 10.4.11 on PowerPC. When it launches
it takes over the screen. It hides the menu bar, Dock and cursor and shows a
cartoon robot head: a white helmet with a dark visor, two glowing blue oval eyes
and a blue mouth. The eyes hold a gaze for a random time, then dart to a new
direction or back to straight ahead. Now and then they do a slow scan, and they
blink. The mouth mostly smiles, with an occasional frown or a surprised "O".
**Press any key to quit.**

![preview](robot_eye_preview.png)

## Building (Xcode 2.5 command-line tools)

    make          # builds RobotEye.app
    make run      # builds and launches it
    make preview  # re-renders the preview PNGs from the real drawing code
    make clean

You can also double-click `RobotEye.app` in the Finder.

## Files

| File | Purpose |
|------|---------|
| `main.m` | App startup, borderless full-screen window, hides the menu bar, Dock and cursor |
| `EyeView.m/.h` | Animation: random gaze targets, hold times, darts and scans, blinks, mouth expressions; quits on any key |
| `eye_draw.c/.h` | All drawing, in plain CoreGraphics (10.4-safe, gradients via CGShading) |
| `tools/preview.c` | Renders still PNGs with `eye_draw.c` |

## Tuning

- Timing and behavior: the `#define`s at the top of `EyeView.m`.
- Size and proportions: the `*_FRAC`, `EYE_*` and `MOUTH_Y` values at the top of `eye_draw.c`.
- How often it frowns or looks surprised: `FROWN_CHANCE` and `SURPRISE_CHANCE` in `EyeView.m`.
- Colors: `GLOW_*` (eyes and mouth) and `OUTLINE_*` in `eye_draw.c`, plus the `Ramp` tables.
