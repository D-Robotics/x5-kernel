// SPDX-License-Identifier: GPL-2.0
/*
 * Synopsys MIPI D-PHY – debugfs interface header
 */

#ifndef _PHY_DEBUGFS_H_
#define _PHY_DEBUGFS_H_

#include <linux/types.h>

/* Create / remove debugfs entries */
void snps_dphy_debugfs_init(struct snps_dphy *dphy);
void snps_dphy_debugfs_exit(struct snps_dphy *dphy);

#endif /* _PHY_DEBUGFS_H_ */
