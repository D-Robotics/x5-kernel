//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_buf_mgr.h
 * Arm China Trust Engine Buffer Manager API
 */

#ifndef __LCA_TE_BUF_MGR_H__
#define __LCA_TE_BUF_MGR_H__

#include <linux/scatterlist.h>

#include "lca_te_driver.h"

/* Create memlist for the src sg list */
#define TE_BUF_MGR_GEN_MEMLIST_SRC(sg, nb, mlst)           \
	te_buf_mgr_gen_memlist((sg), (nb), (mlst), false)

/* Create memlist for the dst sg list */
#define TE_BUF_MGR_GEN_MEMLIST_DST(sg, nb, mlst)           \
	te_buf_mgr_gen_memlist((sg), (nb), (mlst), true)


int te_buf_mgr_gen_memlist(struct scatterlist *sg_list,
			   unsigned int nbytes,
			   te_memlist_t *list,
			   bool is_dst);

void te_buf_mgr_free_memlist(te_memlist_t *list, int err);

#endif /*__LCA_TE_BUF_MGR_H__*/

