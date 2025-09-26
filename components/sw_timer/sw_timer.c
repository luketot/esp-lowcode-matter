#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>

#include <riscv/rv_utils.h>
#include <ulp_lp_core_print.h>

#include "sw_timer.h"

#ifdef CONFIG_MAX_SOFTWARE_TIMERS
#define SW_TIMER_MAX_ITEMS CONFIG_MAX_SOFTWARE_TIMERS
#else
#define  SW_TIMER_MAX_ITEMS 10
#endif /* CONFIG_MAX_SOFTWARE_TIMERS */

#define LP_CORE_FREQ_IN_KHZ 16000

static const char *TAG = "sw_timer";

typedef struct {
    bool active; /* false means a suspended/uninitialized timer */
    bool valid; /* whether this timer is valid or not */
    bool periodic; /* auto-reload the timer if it is periodic */
    uint32_t last_tick; /* last tick */
    int64_t remain_ticks; /* remain ticks to call the timer callback */
    int timeout_ms; /* timeout period */
    sw_timer_cb_t handler; /* callback */
    void *arg;
} sw_timer_t;

/* zerocode timer */
static sw_timer_t g_timers[SW_TIMER_MAX_ITEMS];

/**
 * @brief create zerocode timer
 *
 * @return handle if on available entry, return NULL
*/
sw_timer_handle_t sw_timer_create(sw_timer_config_t *config)
{
    if (config->handler == NULL) {
        printf("%s: %s Invalid handler\n", TAG, __func__);
        return NULL;
    }

    if (config->periodic == true && config->timeout_ms == 0) {
        printf("%s: %s Invalid periodic timer with timeout_ms=0\n", TAG, __func__);
        return NULL;
    }

    sw_timer_t *timer = NULL;

    for (int i=0; i<SW_TIMER_MAX_ITEMS; i++) {
        if (!g_timers[i].valid) {
            timer = &(g_timers[i]);
            break;
        }
    }

    if (timer) {
        timer->active = false; /* initial is inactive */
        timer->handler = config->handler;
        timer->arg = config->arg;
        timer->valid = true;
        timer->timeout_ms = config->timeout_ms;
        timer->periodic = config->periodic;
    }
    else {
        printf("%s: Lack of memory for sw_timer\n", TAG);
    }

    return (sw_timer_handle_t)timer;
}

/**
 * @brief delete a timer
*/
int sw_timer_delete(sw_timer_handle_t timer_handle)
{
    if (timer_handle == NULL){
        return -1;
    }

    sw_timer_t *timer = (sw_timer_t *)timer_handle;
    timer->active = false;
    timer->handler = NULL;
    timer->arg = NULL;
    timer->remain_ticks = -1;
    timer->valid = false;
    return 0;
}

/**
 * @brief start timer
 *
*/
int sw_timer_start(sw_timer_handle_t timer_handle)
{
    sw_timer_t *timer = (sw_timer_t *)timer_handle;
    if (timer == NULL || timer->valid == false){
        printf("%s: %s Invalid timer\n", TAG, __func__);
        return -1;
    }

    /* calculate remain_ticks */
    timer->remain_ticks = (int64_t)(timer->timeout_ms) * LP_CORE_FREQ_IN_KHZ;

    if (timer->remain_ticks == 0) {
        /* if remain_ticks=0, call handler immediately and stop timer */
        timer->handler(timer_handle, timer->arg);
        sw_timer_stop(timer);
    }
    else {
        /* update timer's last tick */
        timer->last_tick = RV_READ_CSR(mcycle);
        timer->active = 1;
    }

    return 0;
}

/**
 * @brief stop a timer
 *
 * update the last_tick & remain_ticks immediately
*/
int sw_timer_stop(sw_timer_handle_t timer_handle)
{
    sw_timer_t *timer = (sw_timer_t *)timer_handle;

    if (timer == NULL || timer->valid == false){
        printf("%s: %s Invalid timer\n", TAG, __func__);
        return -1;
    }

    if(timer->active == false) return 0;
    timer->active = false;
    return 0;
}


/**
 * @brief call from main function to update the timer list
*/
void sw_timer_run(void)
{
    for (int i=0; i<SW_TIMER_MAX_ITEMS; i++) {
        if (g_timers[i].valid && g_timers[i].active) {
            /* calculate escaped ticks & update last tick */
            uint32_t tick = RV_READ_CSR(mcycle); /* bugfix: time_gap overflow when timer started inside another timer cb */
            uint32_t time_gap = tick - g_timers[i].last_tick;
            g_timers[i].last_tick = tick; /* update last_tick */
            g_timers[i].remain_ticks -= (int64_t)time_gap; /* update remain_ticks */

            if (g_timers[i].remain_ticks <= 0) {
                /* handler may delete/stop timer. update timer status before executing handler */
                if (g_timers[i].periodic) {
                    /* periodic, reload timer. if not, stop timer */
                    sw_timer_start(&(g_timers[i]));
                } else {
                    sw_timer_stop(&(g_timers[i]));
                }

                /* call handler */
                g_timers[i].handler(&(g_timers[i]), g_timers[i].arg);
            }
        }
    }
}
