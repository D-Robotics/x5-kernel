#include <linux/stddef.h>
#include "osal.h"
#include "osal_linux_test.h"

void test_osal_io(void)
{
	void *p = NULL;
	p = osal_iomap(0x4090000, 0x1000);
	CHECK_RESULT(1, p != NULL, p);
	osal_iounmap(p);
}