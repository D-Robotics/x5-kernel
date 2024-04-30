#ifndef __OSAL_COMMON_TEST_H_
#define __OSAL_COMMON_TEST_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
typedef pthread_spinlock_t osal_spinlock_t;

#define osal_kfree   free
#define __user
#define true  1
#define false 0
#define pr_err printf
#define smp_wmb __sync_synchronize
typedef int32_t bool;

static inline void *osal_kmalloc(int32_t size, int32_t flags) { return malloc(size);}

static inline int
copy_from_user(void *to, const void __user *from, size_t n)
{
	memcpy(to, from, n);
    return 0;
}

static inline int
copy_to_user(void __user *to, const void *from, size_t n)
{
	memcpy(to, from, n);
    return 0;
}

#define osal_pr_err printf


#define CHECK_RESULT(id, cond, ret) \
	if (!(cond)) { \
		failed_num++; \
		printf("%s %d fail, ret:0x%lx\n", __func__, (id), (unsigned long)(ret)); \
	} else { \
		printf("%s %d pass, ret:0x%lx\n", __func__, (id), (unsigned long)(ret)); \
	}

#define CHECK_RESULT_OR_RETURN(cond, ret) \
	if (!(cond)) { \
		failed_num++; \
		printf("%s %d fail, ret:0x%lx\n", __func__, (id), (unsigned long)(ret)); \
		return ret;	\
	} else { \
		printf("%s %d pass, ret:0x%lx\n", __func__, (id), (unsigned long)(ret)); \
	}

#define  CHECK_FINAL_RESULT() \
	if (failed_num == 0) \
		printf("ALL PASS!\n"); \
	else \
		printf("%d Failed\n", failed_num);

#if 0
static inline void hex_dump(void *ptr, int32_t size) {
    unsigned char *data = (unsigned char *)ptr;
	return ;
	pthread_mutex_lock(&mutex);
    for (size_t i = 0; i < size; i += 16) {
        printf("%08zx: ", i);

        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02x ", data[i + j]);
            } else {
                printf("   ");
            }
        }
        putchar('\n');
    }
	pthread_mutex_unlock(&mutex);
}
#endif

static inline void osal_spin_lock_init(osal_spinlock_t *lock)
{
	int32_t ret;
	ret = pthread_spin_init((pthread_spinlock_t *)lock, 0);
	if (ret != 0)
		printf("%s failed, ret:%d\n", __func__, ret);
}

static inline void osal_spin_lock(osal_spinlock_t *lock)
{
	int32_t ret;
	ret = pthread_spin_lock((pthread_spinlock_t *)lock);
	if (ret != 0)
		printf("%s failed, ret:%d\n", __func__, ret);
}

static inline void osal_spin_unlock(osal_spinlock_t *lock)
{
	int32_t ret;
	pthread_spin_unlock((pthread_spinlock_t *)lock);
	if (ret != 0)
		printf("%s failed, ret:%d\n", __func__, ret);
}

static inline void osal_spin_lock_irqsave(osal_spinlock_t *lock, unsigned long *flags)
{
	int32_t ret;
	ret = pthread_spin_lock((pthread_spinlock_t *)lock);
	if (ret != 0)
		printf("%s failed, ret:%d\n", __func__, ret);
}

static inline void osal_spin_unlock_irqrestore(osal_spinlock_t *lock, unsigned long *flags)
{
	int32_t ret;
	ret = pthread_spin_unlock((pthread_spinlock_t *)lock);
	if (ret != 0)
		printf("%s failed, ret:%d\n", __func__, ret);
}

#endif
