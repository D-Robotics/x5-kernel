#include "osal_semaphore.h"

void osal_sema_init(osal_sem_t *sem, int32_t val)
{
    /**
      * xSem API require two parameters(max, init) and max means max-count of sem,
      * max can not is 0 even if init(val) is 0, so set to val+2 here.
      */
    *sem = xSemaphoreCreateCounting(val + 2, val);
}

void osal_sema_up(osal_sem_t *sem)
{
    if (sem != NULL)
    {
        xSemaphoreGive(*sem);
    }
}

// in freeRTOS, this api will wait max delay, no user interrupt
int32_t osal_sema_down_interruptible(osal_sem_t *sem)
{
    if (pdTRUE == xSemaphoreTake(*sem, portMAX_DELAY)) {
        return 0;
    } else {
        return -1;
    }
}

int32_t osal_sema_down_timeout(osal_sem_t *sem, uint32_t time_ms)
{
    if (pdTRUE == xSemaphoreTake(*sem, pdMS_TO_TICKS(time_ms))) {
        return 0;
    } else {
        return -1;
    }
}