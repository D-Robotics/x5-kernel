/*
 * Copyright 2022, Horizon Robotics
 */

#include "osal.h"
#include "osal_linux_test.h"

#define TEST_KALLOC_SIZE 0x123
#define TEST_VALLOC_SIZE 0x12345

void *pkmem;
void *pvmem;

static void test_osal_kmalloc(void)
{
	pkmem = osal_kmalloc(TEST_KALLOC_SIZE, OSAL_KMALLOC_KERNEL);
	CHECK_RESULT(1, pkmem != NULL, pvmem);
	osal_kfree(pkmem);

	pkmem = osal_kmalloc(TEST_KALLOC_SIZE, OSAL_KMALLOC_ATOMIC);
	CHECK_RESULT(2, pkmem != NULL, pvmem);
	osal_kfree(pkmem);

	pkmem = osal_kmalloc(TEST_KALLOC_SIZE, 0);
	CHECK_RESULT(3, pkmem != NULL, pvmem);
	osal_kfree(pkmem);

	pkmem = osal_kzalloc(TEST_KALLOC_SIZE, OSAL_KMALLOC_KERNEL);
	CHECK_RESULT(4, pkmem != NULL, pvmem);
	osal_kfree(pkmem);

	pkmem = osal_kzalloc(TEST_KALLOC_SIZE, OSAL_KMALLOC_ATOMIC);
	CHECK_RESULT(5, pkmem != NULL, pvmem);
	osal_kfree(pkmem);

	pkmem = osal_kzalloc(TEST_KALLOC_SIZE, 0);
	CHECK_RESULT(6, pkmem != NULL, pvmem);
	osal_kfree(pkmem);
}

static void test_osal_malloc(void)
{
	pvmem = osal_malloc(TEST_VALLOC_SIZE);
	CHECK_RESULT(1, pvmem != NULL, pvmem);
	osal_free(pvmem);
	pvmem = NULL;
}

static void test_osal_copy_from_app(void)
{
	return;
}
static void test_osal_copy_to_app(void)
{
	return;
}
void test_osal_alloc(void)
{
	test_osal_kmalloc();
	test_osal_malloc();
	test_osal_copy_from_app();
	test_osal_copy_to_app();
}