// SPDX-License-Identifier: GPL-2.0
/*
 * Synopsys MIPI D-PHY – debugfs helpers
 *
 * This file exports run-time switches via debugfs so that userspace
 * can enable / disable timing programming and TESTDOUT read-back
 * without rebuilding the driver.
 */

#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/phy/phy.h>
#include <linux/phy/phy-mipi-dphy.h>
#include <linux/debugfs.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "phy-snps-mipi-dphy.h"

#define DPHY_DBGFS_DIR	      "snps_mipi_dphy"
#define DPHY_DBGFS_DEF_ENABLE 1U

void snps_dphy_debugfs_init(struct snps_dphy *dphy)
{
    struct dentry *dir;

    dir = debugfs_create_dir(DPHY_DBGFS_DIR, NULL);
    if (!dir || IS_ERR(dir))
        return;

    dphy->dbgfs_dir    = dir;
    dphy->dbg_write_en = DPHY_DBGFS_DEF_ENABLE;
    dphy->dbg_read_en  = DPHY_DBGFS_DEF_ENABLE;

    debugfs_create_u8("write_timings_enable", 0644,
                      dphy->dbgfs_dir, &dphy->dbg_write_en);
    debugfs_create_u8("readback_enable", 0644,
                      dphy->dbgfs_dir, &dphy->dbg_read_en);
}

void snps_dphy_debugfs_exit(struct snps_dphy *dphy)
{
    if (dphy->dbgfs_dir) {
        debugfs_remove_recursive(dphy->dbgfs_dir);
        dphy->dbgfs_dir = NULL;
    }
}
