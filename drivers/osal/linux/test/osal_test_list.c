#include "osal.h"
#include "osal_linux_test.h"

struct list_test_data {
	osal_list_head_t list;
	int32_t data;
};



void test_osal_list(void)
{
	int32_t i;
	int32_t ret;
	struct list_test_data *p, *cur, *next;
	osal_list_head_t list_head;
	memset(&list_head, 0, sizeof(list_head));

	osal_list_head_init(&list_head);
	ret = osal_list_empty(&list_head);
	CHECK_RESULT(1, ret == 1, ret);
	for (i = 0; i < 10; i++) {
		p = osal_kmalloc(sizeof(*p), 0);
		p->data = i;
		osal_list_add(&p->list, &list_head);
	}

	osal_list_for_each_entry_safe(cur, next, &list_head, list) {
		osal_pr_info("test osal_list_for_each_entry_safe: cur->data:%d\n", cur->data);
	}

	osal_list_for_each_entry_safe_reverse(cur, next, &list_head, list) {
		osal_pr_info("test osal_list_for_each_entry_safe_reverse: cur->data:%d\n", cur->data);
	}

	osal_list_for_each_entry(cur, &list_head, list) {
		osal_pr_info("test osal_list_for_each_entry : cur->data:%d\n", cur->data);
		//osal_list_del(&cur->list);
	}

	ret = osal_list_empty(&list_head);
	CHECK_RESULT(1, ret == 0, ret);

}
