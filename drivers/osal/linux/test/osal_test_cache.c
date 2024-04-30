#include "osal.h"
#include "osal_linux_test.h"

static void *pkmem;
#define TEST_CACHE_MEM_SIZE 0x1234
void test_osal_dma_cache_clean(void)
{
	osal_dma_cache_clean(pkmem, TEST_CACHE_MEM_SIZE);
}

void test_osal_dma_cache_invalidate(void)
{
	osal_dma_cache_invalidate(pkmem, TEST_CACHE_MEM_SIZE);
}

void test_osal_dma_cache_flush(void)
{
	osal_dma_cache_invalidate(pkmem, TEST_CACHE_MEM_SIZE);
}

void test_osal_cache(void)
{
	pkmem = osal_kmalloc(TEST_CACHE_MEM_SIZE, 0);
	test_osal_dma_cache_clean();
	test_osal_dma_cache_invalidate();
	test_osal_dma_cache_flush();
	osal_kfree(pkmem);
	pkmem = NULL;
}
