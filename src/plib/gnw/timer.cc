#include <stdio.h>
#include <stdlib.h>

#include "plib/gnw/timer.h"
#include "platform_compat.h"

namespace fallout {

Timer timers[TIMERS_MAX];

int ingame_timer = 0;
int current_time = 0;
int timer_started = 0;

void timer_init()
{
    for(int i = 0; TIMERS_MAX > i; ++i)
        timers[i] = {};
}

void timer_start()
{
    timer_started = 1;
}

void timer_stop()
{
    timer_started = 0;
}

void timer_tick()
{
    if(1 > timer_started){return;}

    current_time = compat_timeGetTime();

    timer_call tc;
    for(int i = 0; TIMERS_MAX > i; ++i){
        if(timers[i].timeout != 0){
            if(1 > timers[i].is_fire && current_time >= ingame_timer+(timers[i].timeout*1000)) {
                timers[i].is_fire = 1;
                (*timers[i].func_ptr)(timers[i].a1);
            }
        }
    }
}

void timer_reset()
{
    ingame_timer = current_time;
}

int timer_create(int timer, timer_call func_ptr, int timeout, int a1)
{
    if(timers[timer].timeout != 0){return 1;}

    Timer t;
    t.func_ptr = func_ptr;
    t.timeout = timeout;
    t.a1 = a1;
    t.is_fire = 0;

    timers[timer] = t;

    return 0;
}

int timer_delete(int timer)
{
    if(timers[timer].timeout != 0){
        timers[timer] = {};
        return 0;
    }else{
        return 1;
    }
}

}