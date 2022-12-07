#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/timer.h"
#include "extimer.h"

#define TIMER_DIVIDER         (80)  //  Hardware timer clock divider
#define TIMER_SCALE           (TIMER_BASE_CLK / TIMER_DIVIDER)  // convert counter value to seconds

typedef struct {
    int timer_group;
    int timer_idx;
    int alarm_interval;
    bool auto_reload;
} example_timer_info_t;

/**
 * @brief A sample structure to pass events from the timer ISR to task
 *
 */
typedef struct {
    example_timer_info_t info;
    uint64_t timer_counter_value;
} example_timer_event_t;

extern TaskHandle_t xHandle_stm32;


/*
 * A simple helper function to print the raw timer counter value
 * and the counter value converted to seconds
 */
static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\r\n", (uint32_t) (counter_value >> 32),
           (uint32_t) (counter_value));
    printf("Time   : %.8f s\r\n", (double) counter_value / TIMER_SCALE);
}

static void IRAM_ATTR timer_group_isr_callback(void *args)
{
    // BaseType_t high_task_awoken = pdFALSE;
    // example_timer_info_t *info = (example_timer_info_t *) args;

    // uint64_t timer_counter_value = timer_group_get_counter_value_in_isr(info->timer_group, info->timer_idx);

    // /* Prepare basic event data that will be then sent back to task */
    // example_timer_event_t evt = {
    //     .info.timer_group = info->timer_group,
    //     .info.timer_idx = info->timer_idx,
    //     .info.auto_reload = info->auto_reload,
    //     .info.alarm_interval = info->alarm_interval,
    //     .timer_counter_value = timer_counter_value
    // };

    // if (!info->auto_reload) {
    //     timer_counter_value += info->alarm_interval * TIMER_SCALE;
    //     timer_group_set_alarm_value_in_isr(info->timer_group, info->timer_idx, timer_counter_value);
    // }

    // /* Now just send the event data back to the main program task */
    // // xQueueSendFromISR(s_timer_queue, &evt, &high_task_awoken);

    // return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
    

    TIMERG0.int_clr_timers.t0_int_clr = 1;
    TIMERG0.hw_timer[0].config.tx_alarm_en =1;

         BaseType_t xYieldRequired = pdFALSE;
    
    vTaskNotifyGiveFromISR( xHandle_stm32,
    //                             //    xArrayIndex,
                                   &xYieldRequired );
    
     portYIELD_FROM_ISR(xYieldRequired);

     return;
}

/**
 * @brief Initialize selected timer of timer group
 *
 * @param group Timer Group number, index from 0
 * @param timer timer ID, index from 0
 * @param auto_reload whether auto-reload on alarm event
 * @param timer_interval_sec interval of alarm
 */
void extimer_init(uint32_t usec)
{
    uint8_t group = 0, timer=0;

    /* Select and initialize basic parameters of the timer */
    timer_config_t config = {
        .divider = TIMER_DIVIDER,
        .counter_dir = TIMER_COUNT_UP,
        .counter_en = TIMER_START,
        .intr_type = TIMER_INTR_LEVEL,
        .alarm_en = TIMER_ALARM_EN,
        .auto_reload = false,
    }; // default clock source is APB
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0);

    /* Configure the alarm value and the interrupt on alarm. */
    // time is 80 / 8 = 10 MHz, so count to 10 for 1 MHz clock
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, usec);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);

    // example_timer_info_t *timer_info = calloc(1, sizeof(example_timer_info_t));
    // timer_info->timer_group = group;
    // timer_info->timer_idx = timer;
    // timer_info->auto_reload = auto_reload;
    // timer_info->alarm_interval = timer_interval_sec;
    timer_isr_callback_add(TIMER_GROUP_0, TIMER_0, &timer_group_isr_callback, 0, 0 );

    timer_start(TIMER_GROUP_0, TIMER_0);
}