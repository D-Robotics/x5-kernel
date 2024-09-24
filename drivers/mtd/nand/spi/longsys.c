// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2024 D-Robotics, Inc.
 * All rights reserved.
 *
 * Author:
 *› jinbao.he <jinbao.he@d-robotics.cc>
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/mtd/spinand.h>

#define SPINAND_MFR_LONGSYS		0xCD
#define SPINAND_DID_LONGSYS		0x60

static SPINAND_OP_VARIANTS(read_cache_variants,
		SPINAND_PAGE_READ_FROM_CACHE_X4_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_X2_OP(0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(true, 0, 1, NULL, 0),
		SPINAND_PAGE_READ_FROM_CACHE_OP(false, 0, 1, NULL, 0));

static SPINAND_OP_VARIANTS(write_cache_variants,
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static SPINAND_OP_VARIANTS(update_cache_variants,
		SPINAND_PROG_LOAD(true, 0, NULL, 0));

static int f35uqa512m_ooblayout_ecc(struct mtd_info *mtd, int section,
					struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	/* ECC is not user accessible */
	region->offset = 0;
	region->length = 0;

	return 0;
}

static int f35uqa512m_ooblayout_free(struct mtd_info *mtd, int section,
				    struct mtd_oob_region *region)
{
	if (section > 3)
		return -ERANGE;

	/*
	 * No ECC data is stored in the accessible OOB so the full 16 bytes
	 * of each spare region is available to the user. Apparently also
	 * covered by the internal ECC.
	 */
	if (section) {
		region->offset = 16 * section;
		region->length = 16;
	} else {
		/* First byte in spare0 area is used for bad block marker */
		region->offset = 1;
		region->length = 15;
	}

	return 0;
}

static const struct mtd_ooblayout_ops f35uqa512m_ooblayout = {
	.ecc = f35uqa512m_ooblayout_ecc,
	.free = f35uqa512m_ooblayout_free,
};

static const struct spinand_info longsys_spinand_table[] = {
	SPINAND_INFO("F35UQA512M",
			SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x60),
			NAND_MEMORG(1, 2048, 64, 64, 512, 20, 1, 1, 1),
			NAND_ECCREQ(1, 512),
			SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
						&write_cache_variants,
						&update_cache_variants),
			SPINAND_HAS_QE_BIT,
			SPINAND_ECCINFO(&f35uqa512m_ooblayout,
					NULL)),
	SPINAND_INFO("F35UQA001G",
			SPINAND_ID(SPINAND_READID_METHOD_OPCODE_DUMMY, 0x61),
			NAND_MEMORG(1, 2048, 64, 64, 1024, 20, 1, 1, 1),
			NAND_ECCREQ(1, 512),
			SPINAND_INFO_OP_VARIANTS(&read_cache_variants,
						&write_cache_variants,
						&update_cache_variants),
			SPINAND_HAS_QE_BIT,
			SPINAND_ECCINFO(&f35uqa512m_ooblayout,
					NULL)),
};

static int longsysdevice_spinand_detect(struct spinand_device *spinand)
{
	u8 *id = spinand->id.data;
	int ret;

	if (id[1] != SPINAND_MFR_LONGSYS)
		return 0;

	ret = spinand_match_and_init(spinand, longsys_spinand_table,
				     ARRAY_SIZE(longsys_spinand_table),
				     id[2]);
	if (ret)
		return ret;

	return 1;
}

static const struct spinand_manufacturer_ops longsys_spinand_manuf_ops = {
	.init = longsysdevice_spinand_detect,
};

const struct spinand_manufacturer longsys_spinand_manufacturer = {
	.id = SPINAND_MFR_LONGSYS,
	.name = "Longsys",
	.chips = longsys_spinand_table,
	.nchips = ARRAY_SIZE(longsys_spinand_table),
	.ops = &longsys_spinand_manuf_ops,
};
