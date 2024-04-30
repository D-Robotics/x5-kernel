//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include "lca_te_buf_mgr.h"

int te_buf_mgr_gen_memlist_ex(struct scatterlist *sg_list,
			   unsigned int nbytes, te_memlist_t *list,
			   unsigned char * ext_data, unsigned int elen)
{
	unsigned int i = 0, seg_len;
	int nents = 0;

	/* 0 size is also a valid parameter, so we just set the
	 * list to NULL and return success(0)
	 * */
	if (nbytes == 0 && ext_data == NULL) {
		list->nent = 0;
		list->ents = NULL;
		return 0;
	}

	if (nbytes) {
		nents = sg_nents_for_len(sg_list, nbytes);
			if (nents < 0)
				return nents;
	}

	if (ext_data && elen) {
		nents = nents + 1;
	}

	list->ents =
		(te_mement_t *)kmalloc(nents*sizeof(te_mement_t),
				GFP_KERNEL);
	if (list->ents == NULL) {
		return -ENOMEM;
	}

	while (i < nents && nbytes) {
		if (sg_list->length != 0) {
			list->ents[i].buf = sg_virt(sg_list);
			seg_len = (sg_list->length > nbytes)?nbytes:sg_list->length;
			list->ents[i].len = (size_t)seg_len;
			nbytes -= seg_len;
			i++;
			sg_list = sg_next(sg_list);
		} else {
			sg_list = (struct scatterlist *)sg_page(sg_list);
		}
	}

	if (ext_data && elen) {
		list->ents[i].buf = ext_data;
		list->ents[i].len = (size_t)elen;
	}
	list->nent = (uint32_t)nents;
	return 0;

}

int te_buf_mgr_gen_memlist(struct scatterlist *sg_list,
			   unsigned int nbytes, te_memlist_t *list)
{
	return te_buf_mgr_gen_memlist_ex(sg_list, nbytes, list, NULL, 0);
}
void te_buf_mgr_free_memlist(te_memlist_t *list)
{
	if (list->ents != NULL) {
		kfree(list->ents);
		list->ents = NULL;
	}

}



int te_buf_mgr_init(struct te_drvdata *drvdata)
{

	return 0;
}

int te_buf_mgr_fini(struct te_drvdata *drvdata)
{

	return 0;
}

