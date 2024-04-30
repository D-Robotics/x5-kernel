#include "osal.h"
#include "osal_linux_test.h"


void timer_fn(void *data)
{
	osal_pr_info("%s- %d\n", __func__, __LINE__);
}

void test_osal_time(void)
{
	osal_timer_t timer;
	osal_time_t time;

	uint64_t ts_ns;
	ts_ns = osal_time_get_ns();
	osal_usleep(1000);
	osal_msleep(12);
	osal_udelay(111);
	osal_mdelay(11);
	memset(&time, 0, sizeof(time));
	osal_time_get(&time);
	CHECK_RESULT(1, time.tv_sec > 0, 0);
	osal_pr_info("mono time.tv_sec:%lld, time.tv_nsec:%llu, ts_ns:%lld\n", time.tv_sec, time.tv_nsec, ts_ns);

	ts_ns = osal_time_get_real_ns();
	osal_mdelay(11);
	osal_time_get_real(&time);
	osal_pr_info("real time.tv_sec:%lld, time.tv_nsec:%llu, ts_ns:%lld\n", time.tv_sec, time.tv_nsec, ts_ns);

	osal_timer_init(&timer, timer_fn, &ts_ns);
	osal_timer_start(&timer, 100);
	osal_timer_stop(&timer);
}