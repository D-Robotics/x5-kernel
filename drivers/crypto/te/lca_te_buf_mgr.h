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

int te_buf_mgr_gen_memlist_ex(struct scatterlist *sg_list,
				unsigned int nbytes, te_memlist_t *list,
				unsigned char * ext_data, unsigned int elen);
int te_buf_mgr_gen_memlist(struct scatterlist *sg_list,
			   unsigned int nbytes, te_memlist_t *list);
void te_buf_mgr_free_memlist(te_memlist_t *list);
int te_buf_mgr_init(struct te_drvdata *drvdata);
int te_buf_mgr_fini(struct te_drvdata *drvdata);


#endif /*__LCA_TE_BUF_MGR_H__*/

