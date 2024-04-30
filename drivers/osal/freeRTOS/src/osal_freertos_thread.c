#include "osal_thread.h"

#define MAX_THREAD_COUNT 10
static osal_thread_t osal_thread_map[MAX_THREAD_COUNT];

int osal_thread_init(osal_thread_t *res, osal_thread_attr_t *attr,
                     osal_thread_entry_t entryFunc, const void *param)
{
    // Create a new task
    BaseType_t result = xTaskCreate(entryFunc, attr->name, attr->stack_size, param, attr->priority, &res->handle);

    if (result == pdPASS) {
        // Store the thread attributes
        res->attr = *attr;

        // Find an empty slot in the thread map and store the mapping
        for (int i = 0; i < MAX_THREAD_COUNT; i++) {
            if (osal_thread_map[i].handle == NULL) {
                osal_thread_map[i] = *res;
                break;
            }
        }

        return 0;  // Success
    } else {
        return -1; // Failure
    }
}

int osal_thread_destory(osal_thread_t *p)
{
    // Delete the task
    vTaskDelete(p->handle);
    return 0;
}

uint64_t osal_thread_self(void)
{
    // Get the current task handle
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
    return (uint64_t)handle;
}

int osal_thread_start(osal_thread_t *p)
{
    // Start the task
    vTaskResume(p->handle);
    return 0;
}

int osal_thread_config_set(osal_thread_t *p, const osal_thread_attr_type_t type, const osal_thread_attr_t *pattr)
{
    switch (type) {
        case OSAL_ATTR_SCHED_POLICY:
            // Set the scheduling policy
            // Not supported in FreeRTOS, as it uses its own scheduling algorithm
            break;
        case OSAL_ATTR_SCHED_PRIORITY:
            // Set the task priority
            vTaskPrioritySet(p->handle, pattr->priority);
            break;
        case OSAL_ATTR_AFFINITY:
            // Set the task affinity
            // Not supported in FreeRTOS, as it uses cooperative multitasking
            break;
        case OSAL_ATTR_STACKSIZE:
            // Set the task stack size
            // Not supported in FreeRTOS, as it uses dynamic memory allocation for task stacks
            break;
        default:
            return -1; // Invalid attribute type
    }

    // Update the stored attributes
    p->attr = *pattr;
    return 0;
}

osal_thread_t* os_get_current(void)
{
    // Get the current task handle
    TaskHandle_t handle = xTaskGetCurrentTaskHandle();
 
    // Find the corresponding osal_thread_t structure in the thread map
    for (int i = 0; i < MAX_THREAD_COUNT; i++) {
        if (osal_thread_map[i].handle == handle) {
            return &osal_thread_map[i];
        }
    }
}