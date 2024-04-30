#include "osal_mutex.h"

void osal_mutex_init(osal_mutex_t *lock)
{
    *lock = xSemaphoreCreateBinary();
    xSemaphoreGive(*lock);
}

void osal_mutex_lock(osal_mutex_t *lock)
{
    xSemaphoreTake(*lock, portMAX_DELAY);
}

void osal_mutex_unlock(osal_mutex_t *lock)
{
    xSemaphoreGive(*lock);
}

int32_t osal_mutex_trylock(osal_mutex_t *lock)
{
    if (pdTRUE == xSemaphoreTake(*lock,  (TickType_t) 0x5UL)) {
        return 0;
    } else {
        return 1;
    }
}
