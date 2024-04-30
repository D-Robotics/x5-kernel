#include "osal.h"
#include "osal_linux_test.h"

osal_mutex_t mtx;
osal_spinlock_t slock;
osal_sem_t sem;

void test_osal_mutex(void)
{
	int32_t ret;
	osal_mutex_init(&mtx);

	osal_mutex_lock(&mtx);
	osal_mutex_unlock(&mtx);

	ret = osal_mutex_trylock(&mtx);
	CHECK_RESULT(1, ret == 1, ret);
	osal_mutex_unlock(&mtx);

}

void test_osal_spinlock(void)
{
	osal_spin_init(&slock);

	osal_spin_lock(&slock);
	osal_spin_unlock(&slock);

	osal_spin_trylock(&slock);
	osal_spin_unlock(&slock);
}

//TODO: test multiple thread
void test_osal_semaphore(void)
{
	osal_sema_init(&sem, 1);
	osal_sema_down_timeout(&sem, 1000);
	osal_sema_up(&sem);
}

void test_osal_lock(void)
{
	test_osal_mutex();
	test_osal_spinlock();
	test_osal_semaphore();
}
