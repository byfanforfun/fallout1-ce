#ifndef FALLOUT_PLIB_GNW_TIMER_H_
#define FALLOUT_PLIB_GNW_TIMER_H_

namespace fallout {

#define TIMERS_MAX 16

typedef int (*timer_call)(int a1, int a2, int a3);

struct Timer {
    timer_call func_ptr;
    int timeout;
    int is_fire;
    int a1;
    int a2;
    int a3;
};

void timer_tick();
void timer_init();
void timer_start();
void timer_stop();
void timer_reset();
int timer_create(int timer, timer_call func_ptr, int timeout, int a1, int a2, int a3);
int timer_delete(int timer);

}

#endif /* FALLOUT_PLIB_GNW_TIMER_H_ */