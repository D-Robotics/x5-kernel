#include "osal.h"
#include "osal_linux_test.h"

void test_osal_debug(void)
{
	osal_pr_debug("-------test osal_pr_debug---------\n");
	osal_pr_info ("-------test osal_pr_info---------\n");
	osal_pr_err  ("-------test osal_pr_err---------\n");
	osal_pr_warn ("-------test osal_pr_warn---------\n");
	osal_pr_emerg("-------test osal_pr_emerg---------\n");
	osal_print   ("-------test osal_print---------\n");
	OSAL_ASSERT(1 == 1);
}