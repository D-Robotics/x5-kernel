#include "osal_time.h"

uint64_t osal_time_get_ns(void)
{
    TickType_t tick = xTaskGetTickCount();
    TickType_t tick_interval_ns = (TickType_t)((1 / (double)configTICK_RATE_HZ) * 1e9);
    uint64_t time_ns = (uint64_t)tick * tick_interval_ns;
    return time_ns;
}

#define TICK_INTERVAL_US (1000000 / configTICK_RATE_HZ)
void osal_usleep(uint64_t us)
{
    TickType_t tick_delay = (TickType_t)((us + (TickType_t)((TICK_INTERVAL_US - 1)) / TICK_INTERVAL_US));
    vTaskDelay(tick_delay);
}

void osal_msleep(uint64_t ms)
{
    TickType_t tick_delay = (TickType_t)pdMS_TO_TICKS(ms);
    vTaskDelay(tick_delay);
}

static void __delay_cnt(uint64_t cnt)
{
    uint64_t current, end;
    uint32_t rd1, rd2;

    __asm(" mrrc p15, 0, %0, %1, c14 ": "=r"(rd1), "=r"(rd2): );
    end = cnt + (((uint64_t)rd2) << 32) | rd1;
    while (1) {
        __asm(" mrrc p15, 0, %0, %1, c14 ": "=r"(rd1), "=r"(rd2): );
        current = (((uint64_t)rd2) << 32) | rd1;
        if (current >= end) {
            break;
        }
    }
}

void osal_udelay(uint64_t us)
{
    uint64_t cnt;

    cnt = us * SYSCNT_SYSCNT_FID0_FREQUENCY / 1000000;
    __delay_cnt(cnt);
}

void osal_mdelay(uint64_t ms)
{
    uint64_t cnt;

    cnt = ms * SYSCNT_SYSCNT_FID0_FREQUENCY / 1000;
    __delay_cnt(cnt);
}

static void timer_callback(TimerHandle_t xTimer) {
    osal_timer_t *timer = (osal_timer_t *)pvTimerGetTimerID(xTimer);
    if (timer != NULL && timer->cb_func != NULL) {
        timer->cb_func(timer->data);
    }
}

int osal_timer_init(osal_timer_t *timer, void (*func)(void *), void *data) {
    if (timer == NULL || func == NULL) {
        return -1;
    }

    timer->handle = xTimerCreate(NULL, pdMS_TO_TICKS(timer->interval_ms), pdTRUE, &timer->timer_id, timer_callback);

    if (timer->handle == NULL) {
        return -1;
    }

    timer->cb_func = func;
    timer->data = data;

    return 0;
}

void osal_timer_start(osal_timer_t *timer, uint64_t interval_ms) {
    if (xTimerIsTimerActive(timer->handle)) {
        xTimerStop(timer->handle, 0);
    }

    timer->interval_ms = interval_ms;

    xTimerChangePeriod(timer->handle, pdMS_TO_TICKS(timer->interval_ms), 0);
    xTimerStart(timer->handle, 0);
}

int osal_timer_stop(osal_timer_t *timer) {
    if (xTimerStop(timer->handle, 0) == pdPASS) {
        return 0;
    } else {
        return -1;
    }
}
