#include "osal_spinlock.h"

void osal_spin_init(osal_spinlock_t *lock) {
    lock->semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(lock->semaphore);
}

void osal_spin_lock(osal_spinlock_t *lock) {
    while (xSemaphoreTake(lock->semaphore, (TickType_t)0) != pdPASS) {
    }
}

void osal_spin_unlock(osal_spinlock_t *lock) {
    xSemaphoreGive(lock->semaphore);
}

void osal_spin_lock_irqsave(osal_spinlock_t *lock, uint64_t *flag) {
    portDISABLE_INTERRUPTS();

    lock->saved_priority = uxTaskPriorityGet(NULL);
    vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
    xSemaphoreTake(lock->semaphore, portMAX_DELAY);

    *flag = lock->saved_priority;
}

void osal_spin_unlock_irqrestore(osal_spinlock_t *lock, uint64_t *flag) {

    xSemaphoreGive(lock->semaphore);
    vTaskPrioritySet(NULL, *flag);

    portENABLE_INTERRUPTS();
}
