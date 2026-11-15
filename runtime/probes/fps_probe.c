// --probe-fps: MKW_FPS=auto's controller (core/boot/fps_auto.c) on synthetic frame times. Each case feeds frames of
// a fixed length and checks where the interval settles and how often it changed: a steady frame must give one
// interval and no back and forth (the game's own rule taken to 4 would cycle 2, 3, 4 on 36 ms frames). Needs no game.
#include <stdio.h>

#include "host.h"

static int fails;

// frames of ms each (plus jitter of +-j alternating); returns the interval after them, counting the changes
static int feed(FpsAuto *a, double ms, double j, int frames, int *changes) {
    for (int i = 0; i < frames; i++) {
        int prev = a->iv;
        if (fps_auto_step(a, ms + (i & 1 ? j : -j)) != prev) (*changes)++;
    }
    return a->iv;
}

static void expect(const char *what, int iv, int want, int changes, int max_changes) {
    int ok = iv == want && changes <= max_changes;
    if (!ok) fails++;
    printf("%s fps: %-38s interval %d (want %d), %d changes (at most %d)\n", ok ? "ok  " : "FAIL", what, iv, want,
           changes, max_changes);
}

int probe_fps(void) {
    static const struct { const char *what; double ms, jitter; int want; } steady[] = {
        {"15 ms frames (fits 60)", 15.0, 1.0, 1},       {"16 ms frames (just fits 60)", 16.0, 0.3, 1},
        {"20 ms frames", 20.0, 2.0, 2},                 {"36 ms frames (Radeon 660M race)", 36.0, 3.0, 3},
        {"45 ms frames", 45.0, 4.0, 3},                 {"55 ms frames", 55.0, 4.0, 4},
        {"120 ms frames (slower than 15)", 120.0, 10.0, 4},
    };
    for (unsigned i = 0; i < sizeof steady / sizeof steady[0]; i++) {
        FpsAuto a;
        fps_auto_reset(&a);
        int changes = 0;
        feed(&a, steady[i].ms, steady[i].jitter, 120, &changes);   // settle
        int settled = changes;
        changes = 0;
        int iv = feed(&a, steady[i].ms, steady[i].jitter, 1200, &changes);   // 20 s at 60 Hz: no more changes
        expect(steady[i].what, iv, steady[i].want, changes, 0);
        if (settled > 2) fails++, printf("FAIL fps: %s took %d changes to settle\n", steady[i].what, settled);
    }

    FpsAuto a;   // a loading hitch of 4 frames at 60 fps: the median of 8 does not move
    fps_auto_reset(&a);
    int changes = 0;
    feed(&a, 15.0, 1.0, 120, &changes);
    feed(&a, 200.0, 0.0, 3, &changes);
    int iv = feed(&a, 15.0, 1.0, 120, &changes);
    expect("15 ms with a 3-frame 200 ms hitch", iv, 1, changes, 0);

    fps_auto_reset(&a);   // a load (36 ms race, then 12 ms menu): back to 60 within about two seconds
    changes = 0;
    feed(&a, 36.0, 3.0, 300, &changes);
    iv = feed(&a, 12.0, 1.0, 50, &changes);   // 15 +- 1 would not: its median, 16, is in the hysteresis gap
    expect("36 ms then 12 ms, 50 frames later", iv, 1, changes, 3);

    fps_auto_reset(&a);   // lat4: race frames spread over 25-40 ms, around 30 fps: one interval, no back and forth
    changes = 0;
    static const double spread[] = {25, 31, 38, 27, 40, 29, 33, 26, 36, 30, 28, 39, 32, 34, 27, 31};
    for (int i = 0; i < 300; i++) {
        int prev = a.iv;
        if (fps_auto_step(&a, spread[i % 16]) != prev) changes++;
    }
    expect("frames spread over 25-40 ms", a.iv, a.iv == 2 ? 2 : 3, changes, 1);

    printf("fps: %s\n", fails ? "FAILED" : "all passed");
    return fails ? 1 : 0;
}
