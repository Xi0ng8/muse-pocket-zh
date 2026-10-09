// Muse Pocket: shared C button state machine, without GPIO/RTOS dependencies.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    POCKET_GESTURE_NONE, POCKET_GESTURE_SHORT, POCKET_GESTURE_DOUBLE, POCKET_GESTURE_LONG
} pocket_gesture_event_t;
typedef struct {
    bool pressed, fired;
    unsigned clicks;
    int64_t press_ms, release_ms, long_ms, short_max_ms;
} pocket_gesture_t;

static inline void pocket_gesture_cancel(pocket_gesture_t* state) {
    state->fired=true;
    state->clicks=0;
}

static inline pocket_gesture_event_t pocket_gesture_update(pocket_gesture_t* state, bool pressed,
                                                          int64_t now_ms, int64_t long_ms,
                                                          int64_t short_max_ms) {
    pocket_gesture_event_t event=POCKET_GESTURE_NONE;
    if(pressed && !state->pressed) {
        state->press_ms=now_ms;
        state->long_ms=long_ms;
        state->short_max_ms=short_max_ms;
        state->fired=false;
    }
    const int64_t held_ms=now_ms-state->press_ms;
    // Check both a held sample and its release: a poll must not miss a hold
    // whose threshold was crossed between the final two samples.
    if((pressed || state->pressed) && !state->fired && held_ms>=state->long_ms) {
        pocket_gesture_cancel(state);
        event=POCKET_GESTURE_LONG;
    } else if(!pressed && state->pressed && !state->fired && held_ms>=50 && held_ms<=state->short_max_ms) {
        if(state->clicks<2) ++state->clicks;
        state->release_ms=now_ms;
    }
    if(!pressed && state->clicks && now_ms-state->release_ms>=400) {
        event=state->clicks>=2?POCKET_GESTURE_DOUBLE:POCKET_GESTURE_SHORT;
        state->clicks=0;
    }
    state->pressed=pressed;
    return event;
}
