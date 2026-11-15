// MKW_FPS=auto: the present interval (1-4: 60, 30, 20, 15 fps) from the time the game measured for each frame (its
// frame object's [+0x128], game.c). The game's own choice stops at 2: below 30 fps the race ran in slow motion
// (0.75-0.83x on the Radeon 660M at 36 ms a frame). Its rule (acc += measured - interval * 16.7 ms, capped at
// +-66.7 ms, interval = acc / 16.7 + 1) taken to 4 would cycle 2, 3, 4 on a steady 36 ms frame, each step undoing the
// last; so this one has hysteresis: up as soon as the median of the last 8 frames is 8 % past the interval, to the
// one it fits that way; down only when the median of the last two seconds fits a lower interval with 15 % to spare.
#include <string.h>

#include "host.h"

#define PERIOD_MS (1000.0 / 60.0)
#define UP_FRAMES 8
#define DOWN_MARGIN 0.85   // lat4: race frames of 25-40 ms moved it 2 <-> 3 every few seconds with 0.92 over 1 s
#define DOWN_SECONDS 2
#define UP_MARGIN 1.08   // gp15b: 50-54 ms spikes at interval 3 moved it to 4 one or two times every 5 s

static double median(const FpsAuto *a, unsigned count) {
    double v[FPS_AUTO_HIST];
    unsigned n = count < a->n ? count : a->n;
    for (unsigned i = 0; i < n; i++) v[i] = a->hist[(a->head + FPS_AUTO_HIST - 1 - i) % FPS_AUTO_HIST];
    for (unsigned i = 1; i < n; i++)   // insertion sort: at most 60 values
        for (unsigned j = i; j > 0 && v[j - 1] > v[j]; j--) {
            double t = v[j];
            v[j] = v[j - 1], v[j - 1] = t;
        }
    return n ? v[n / 2] : 0.0;
}

static int fitting(double ms, double margin) {   // the smallest interval a frame of ms fits in
    for (int iv = 1; iv < FPS_AUTO_MAX; iv++)
        if (ms <= iv * PERIOD_MS * margin) return iv;
    return FPS_AUTO_MAX;
}

void fps_auto_reset(FpsAuto *a) {
    memset(a, 0, sizeof *a);
    a->iv = 1;
}

int fps_auto_step(FpsAuto *a, double measured_ms) {
    a->hist[a->head] = measured_ms;
    a->head = (a->head + 1) % FPS_AUTO_HIST;
    if (a->n < FPS_AUTO_HIST) a->n++;
    a->since++;
    if (a->since >= UP_FRAMES && a->n >= UP_FRAMES) {
        int up = fitting(median(a, UP_FRAMES), UP_MARGIN);
        if (up > a->iv) {
            a->iv = up, a->since = 0;
            return a->iv;
        }
    }
    unsigned second = DOWN_SECONDS * 60 / (unsigned)a->iv;   // two seconds of frames at this interval
    if (a->iv > 1 && a->since >= second && a->n >= second) {
        int down = fitting(median(a, second), DOWN_MARGIN);
        if (down < a->iv) a->iv = down, a->since = 0;
    }
    return a->iv;
}
