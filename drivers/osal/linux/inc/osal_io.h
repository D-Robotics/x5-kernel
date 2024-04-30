/*
 * Copyright 2022, Horizon Robotics
 */
#ifndef __OSAL_IO_H__
#define __OSAL_IO_H__

#include <asm-generic/io.h>

/**
 * @brief map the specified physical address to virtual
 *
 * @param[in] paddr physical address
 * @param[in] size memory size
 *
 * @return virutal address pointer.
 */
static inline void* osal_iomap(uint64_t paddr, size_t size)
{
	return ioremap(paddr, size);
}

/**
 * @brief unmap the specified virtual address
 *
 * @param[in] vaddr virtual address
 */
static inline void osal_iounmap(void* vaddr)
{
	iounmap(vaddr);
}



#endif
