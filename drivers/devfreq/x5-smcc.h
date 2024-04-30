// SPDX-License-Identifier: GPL-2.0-only

#ifndef __X5_SMCC_H
#define __X5_SMCC_H

/* SMC function IDs for SiP Service queries */
#define HORIZON_SIP_BASE         0xc2000000
#define HORIZON_SIP_DDR_DVFS_SET (HORIZON_SIP_BASE + 0x0)
#define HORIZON_SIP_DDR_DVFS_GET (HORIZON_SIP_BASE + 0x1)

/* devfreq driver */
#define HORIZON_SIP_DVFS_GET_FREQ_COUNT (HORIZON_SIP_BASE + 0x10)
#define HORIZON_SIP_DVFS_GET_FREQ_INFO  (HORIZON_SIP_BASE + 0x11)

#endif /* __X5_SMCC_H */
