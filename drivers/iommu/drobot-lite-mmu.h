/***************************************************************************
 *                      COPYRIGHT NOTICE
 *             Copyright 2024 Horizon Robotics, Inc.
 *                     All rights reserved.
 ***************************************************************************/
#ifndef DROBOT_LITE_MMU_H
#define DROBOT_LITE_MMU_H

#include <linux/dma-buf.h>
#include <linux/iommu.h>
#include <linux/ion.h>

/* register */
#define MMU_REG_CTRL	  0x00
#define MMU_REG_MAP_CTRL0 0x04
#define MMU_REG_MAP_CTRL1 0x08
#define MMU_REG_MAP_CTRL2 0x0C
#define MMU_REG_MAP_CTRL3 0x10

// MMU_REG_CTRL
#define BIT_CTRL_ENABLE BIT(0)

// MMU_REG_MAP_CTRLx
#define MAP_CTRL_SHIFT1 8
#define MAP_CTRL_SHIFT2 16
#define MAP_CTRL_SHIFT3 24

#define MAP_TABLE_SIZE	16
#define MAP_INDEX_SHIFT 28
#define MAP_INDEX_MASK	GENMASK(31, 28)
#define MAP_ADDR_SHIFT	28
#define MAP_ENTRY_SIZE	BIT(28)

/* 2B to 256 MB */
#define LITE_MMU_PGSIZE_BITMAP 0x1FFFF000

struct lite_mmu_domain {
	struct list_head iommus;
	/* lock for iommus add/remove */
	spinlock_t iommus_lock;
	/* lock for map table change */
	spinlock_t map_table_lock;
	struct iommu_domain domain;
	/* map setting of this domain */
	u8 map_table[MAP_TABLE_SIZE];
	u32 map_ref_cnt[MAP_TABLE_SIZE];
};

struct lite_mmu_iommu {
	struct list_head node; /* entry in lite_mmu_domain.iommus */

	struct device *dev;
	void __iomem *base;
	struct clk *clk;

	struct iommu_device iommu;
	struct iommu_domain *domain;
	struct iommu_group *group;
	struct device_link *link; /* runtime PM link from IOMMU to master */
};

static inline struct lite_mmu_domain *to_lite_mmu_domain(struct iommu_domain *domain)
{
	return container_of(domain, struct lite_mmu_domain, domain);
}

static inline u8 __get_index_from_va(dma_addr_t addr)
{
	return (u8)((addr & MAP_INDEX_MASK) >> MAP_INDEX_SHIFT);
}

static inline u8 __get_map_value(phys_addr_t addr)
{
	return (u8)(addr >> MAP_ADDR_SHIFT);
}


void lite_mmu_reset_mapping(struct device *dev);
void lite_mmu_restore_mapping(struct device *dev);

#endif /* DROBOT_LITE_MMU_H  */
