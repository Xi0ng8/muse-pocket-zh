#include "pocket_gesture.h"
#include <assert.h>

int main(void) {
    pocket_gesture_t state={0};
    assert(pocket_gesture_update(&state,true,0,2000,1999)==POCKET_GESTURE_NONE);
    assert(pocket_gesture_update(&state,true,1990,2000,1999)==POCKET_GESTURE_NONE);
    // The button can be released between the threshold and the next poll.
    assert(pocket_gesture_update(&state,false,2010,2000,1999)==POCKET_GESTURE_LONG);
    assert(pocket_gesture_update(&state,false,2000,2000,1999)==POCKET_GESTURE_NONE);
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,10,2000,1999);
    assert(pocket_gesture_update(&state,true,2010,2000,1999)==POCKET_GESTURE_LONG);
    assert(pocket_gesture_update(&state,true,6000,2000,1999)==POCKET_GESTURE_NONE);
    assert(pocket_gesture_update(&state,false,7000,2000,1999)==POCKET_GESTURE_NONE);
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,0,2000,1999);
    assert(pocket_gesture_update(&state,false,1999,2000,1999)==POCKET_GESTURE_NONE);
    assert(pocket_gesture_update(&state,false,2399,2000,1999)==POCKET_GESTURE_SHORT);
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,0,2000,1999);
    pocket_gesture_update(&state,false,100,2000,1999);
    pocket_gesture_update(&state,true,200,2000,1999);
    pocket_gesture_update(&state,false,300,2000,1999);
    assert(pocket_gesture_update(&state,false,700,2000,1999)==POCKET_GESTURE_DOUBLE);
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,0,2000,1999);
    pocket_gesture_update(&state,false,100,2000,1999);
    pocket_gesture_update(&state,true,200,2000,1999);
    assert(pocket_gesture_update(&state,false,2700,2000,1999)==POCKET_GESTURE_LONG);
    assert(pocket_gesture_update(&state,false,2700,2000,1999)==POCKET_GESTURE_NONE);
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,0,5000,1000);
    pocket_gesture_update(&state,false,1500,5000,1000);
    assert(pocket_gesture_update(&state,false,2000,5000,1000)==POCKET_GESTURE_NONE);
    pocket_gesture_update(&state,true,3000,5000,1000);
    assert(pocket_gesture_update(&state,false,8000,5000,1000)==POCKET_GESTURE_LONG);
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,0,2000,1999);
    pocket_gesture_cancel(&state);
    assert(pocket_gesture_update(&state,false,500,2000,1999)==POCKET_GESTURE_NONE);
    assert(pocket_gesture_update(&state,false,1000,2000,1999)==POCKET_GESTURE_NONE);
    // Configuration changes during a hold cannot lower the sampled threshold.
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,0,5000,1000);
    assert(pocket_gesture_update(&state,true,2000,2000,1999)==POCKET_GESTURE_NONE);
    assert(pocket_gesture_update(&state,false,5000,2000,1999)==POCKET_GESTURE_LONG);
    state=(pocket_gesture_t){0};
    pocket_gesture_update(&state,true,0,2000,1999);
    assert(pocket_gesture_update(&state,false,2000,5000,1000)==POCKET_GESTURE_LONG);
    return 0;
}
