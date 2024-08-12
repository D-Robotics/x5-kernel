//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/*#define DEBUG*/
#define pr_fmt(fmt) "te_crypt: " fmt

#include "lca_te_buf_mgr.h"

/**
 * The CE driver holds an assumption on the output linklist (memlist) entries.
 * That is all linklist entries shall start and end at a cacheline aligned
 * address except the start of the first entry and the end of the last one.
 * This assumption stands if the request memory is generated using
 * __get_free_page(). Otherwise, the assumption is doubtful.
 *
 * To prevent potential data coherency problem in case of misaligned output
 * linklist, the LCA buffer manager has been improved.
 *
 * The below figure outlines the general processing for a misaligned output
 * scatterlist entry. A misaligned scatterlist entry can be mapped to three
 * linklist entries at the most before passed to the driver, with full cacheline
 * aligned memory assigned for each entry. It is to save the alignment fixing
 * in the driver to
 *  - have the data reside in the low address of the 1st entry.
 *  - have the data reside in the high address of the 3rd entry.
 *
 *   Address      low ------------------------------------------> high
 *                    .             .             .             .
 *                    .< cacheline >.     ...     .< cacheline >.
 *                    .             .             .             .
 *                    .     start   .             .        end  .
 *                            +-----+-------------+---------+
 *   Scatterlist              |    scatterlist entry#i      |
 *                            +-----+-------------+---------+
 *                     .------'    /|             |\         \
 *                    /           / |             | \         \
 *                   /     .-----'  |             |  `-.  ofs  `-.
 *                  /     /         |             |     \ /       \
 *                 +-----+-------+  +-------------+  +---+---------+
 *   Linklist      |  0  |  N/A  |  |      1      |  |N/A|    2    |
 *                 +-----+-------+  +-------------+  +---+---------+
 *                        ^                                 ^
 *                        |                                 |
 *                 +-------------+                   +-------------+
 *   Fixuplist  ...|    fent#m   |------------------>|   fent#m+1  |...
 *                 +-------------+                   +-------------+
 *
 * Note different memory is used for the cache line incomplete head and(or) tail
 * part(s) of a dst scatterlist entry, if applicable, to load the data outputted
 * from the driver. And that output data is copied to the dst scatterlist by CPU
 * on success by the te_buf_mgr_free_memlist().
 *
 */

/**
 * Linklist fixup entry structure.
 */
struct te_buf_fixup {
	struct list_head node;
	void *dst;             /**< dst address in the scatterlist */
	unsigned int ofs;      /**< offset into the buf */
	unsigned int len;      /**< data length in bytes */
	u8 buf[TE_DMA_ALIGNED] __te_dma_aligned;
};

/**
 * Buffer manager structure.
 */
struct te_buf_mgr {
	u32 nent;              /**< total memlist entries */
	u32 nused;             /**< number of used memlist entries */
	struct list_head lst;  /**< fixup list (optional) */
	te_mement_t ents[];    /**< memlist array */
};

static struct te_buf_fixup* buf_mgr_fix_node_alloc(void *adr, unsigned int len)
{
	struct te_buf_fixup *fent;

	fent = kzalloc(sizeof(*fent), GFP_KERNEL);
	if (NULL == fent) {
		return ERR_PTR(-ENOMEM);
	}

	BUG_ON(!UTILS_IS_ALIGNED(fent->buf, TE_DMA_ALIGNED));
	BUG_ON((len >= TE_DMA_ALIGNED) || ((uintptr_t)adr + len >
	UTILS_ROUND_DOWN((uintptr_t)adr + TE_DMA_ALIGNED, TE_DMA_ALIGNED)));
	fent->dst = adr;
	fent->len = len;
	INIT_LIST_HEAD(&fent->node);

	return fent;
}

static void buf_mgr_dump_sglist(struct scatterlist *sg, unsigned int nbytes)
{
#if defined(DEBUG)
	unsigned int slen;

	pr_debug("=======dump sglist %p nbytes %d:\n", sg, nbytes);
	/* Traverse the sg_list */
	while (nbytes && sg != NULL) {
		if (sg->length != 0) {
			pr_debug("\tent %p, %p\n",
				 sg_virt(sg), sg_virt(sg) + sg->length);

			slen = (sg->length > nbytes) ? nbytes : sg->length;
			nbytes -= slen;
			sg = sg_next(sg);
		} else {
			sg = (struct scatterlist *)sg_page(sg);
		}
	}
#else
	(void)sg;
	(void)nbytes;
#endif
}

static void buf_mgr_dump_memlist(struct te_buf_mgr *mgr)
{
#if defined(DEBUG)
	int i;
	struct te_buf_fixup *fent;

	pr_debug("-------dump memlist %p nent %d nused %d:\n",
		 mgr->ents, mgr->nent, mgr->nused);
	for (i = 0; i < mgr->nused; i++) {
		pr_debug("\tent[%d]: %p, %p\n", i, mgr->ents[i].buf,
			 mgr->ents[i].buf + mgr->ents[i].len);
	}

	list_for_each_entry(fent, &mgr->lst, node) {
		pr_debug("\tfent: buf %p dst %p ofs %d len %d\n",
			 fent->buf, fent->dst, fent->ofs, fent->len);
	}
#else
	(void)mgr;
#endif
}

/*
 * Proceed one entry for the src sg_list.
 * Return number of memlist entries created.
 */
static int buf_mgr_proc_sg_src(struct te_buf_mgr *mgr, int idx,
			       void *sga, unsigned int sglen)
{
	int i = idx;

	if (sglen > 0) {
		BUG_ON(i >= mgr->nent);
		mgr->ents[i].buf = sga;
		mgr->ents[i].len = sglen;
		i++;
	}

	return (i - idx);
}

/**
 * Proceed one entry for the dst sg_list.
 * Return values:
 * >=0 - number of memlist entries created.
 * <0  - error
 */
static int buf_mgr_proc_sg_dst(struct te_buf_mgr *mgr, int idx,
			       void *sga, unsigned int sglen)
{
	uintptr_t adr;
	int rc = 0, i = idx;
	unsigned int len, elen;
	struct te_buf_fixup *fent;

	adr = (uintptr_t)sga;
	len = sglen;

	/* Fixup the head incomplete cache line if applicable */
	if ((len > 0) && !UTILS_IS_ALIGNED(adr, TE_DMA_ALIGNED)) {
		BUG_ON(i >= mgr->nent);
		elen = UTILS_MIN(len,
				 (UTILS_ROUND_UP(adr, TE_DMA_ALIGNED) - adr));
		fent = buf_mgr_fix_node_alloc((void *)adr, elen);
		if (IS_ERR(fent)) {
			rc = PTR_ERR(fent);
			goto err;
		}

		/**
		 * Have the head start at cache line aligned address to save the
		 * head entry alignment handling in the driver.
		 */
		mgr->ents[i].buf = fent->buf;
		mgr->ents[i].len = elen;
		list_add_tail(&fent->node, &mgr->lst);

		adr += elen;
		len -= elen;
		i++;
	}

	/* Memory of complete cache lines in the middle if applicable */
	if (len >= TE_DMA_ALIGNED) {
		BUG_ON(i >= mgr->nent);
		elen = UTILS_ROUND_DOWN(len, TE_DMA_ALIGNED);
		mgr->ents[i].buf = (void *)adr;
		mgr->ents[i].len = elen;
		adr += elen;
		len -= elen;
		i++;
	}

	/* Fixup the tail incomplete cache line if applicable */
	if (len > 0) {
		BUG_ON(len >= TE_DMA_ALIGNED);
		BUG_ON(i >= mgr->nent);

		elen = len;
		fent = buf_mgr_fix_node_alloc((void *)adr, elen);
		if (IS_ERR(fent)) {
			rc = PTR_ERR(fent);
			goto err;
		}

		/**
		 * Set ofs to have the last entry end at cache line aligned
		 * address so as to save the tail entry alignment handling in
		 * the driver.
		 */
		fent->ofs = TE_DMA_ALIGNED - elen;
		mgr->ents[i].buf = fent->buf + fent->ofs;
		mgr->ents[i].len = elen;
		list_add_tail(&fent->node, &mgr->lst);

		adr += elen;
		len -= elen;
		i++;
	}

	return (i - idx); /* Number of created memlist entries */

err:
	return rc;
}

static void buf_mgr_free(struct te_buf_mgr *mgr, int err)
{
	struct te_buf_fixup *fent, *tmp;

	list_for_each_entry_safe(fent, tmp, &mgr->lst, node) {
		/* Copy the output to dst on success */
		if (0 == err) {
			BUG_ON(fent->len >= TE_DMA_ALIGNED);
			BUG_ON(fent->ofs + fent->len > TE_DMA_ALIGNED);
			memcpy(fent->dst, fent->buf + fent->ofs , fent->len);
		}

		list_del(&fent->node);
		kfree(fent);
	}

	memset(mgr, 0, sizeof(*mgr));
	kfree(mgr);
}

int te_buf_mgr_gen_memlist(struct scatterlist *sg_list,
				unsigned int nbytes,
				te_memlist_t *list,
				bool is_dst)
{
	struct te_buf_mgr *mgr = NULL;
	unsigned int i = 0, slen;
	void *sadr;
	int nents = 0;
	int rc = 0;

	if (NULL == list) {
		return -EINVAL;
	}

	/* 0 size is also a valid parameter, so we just set the
	* list to NULL and return success(0)
	* */
	if (0 == nbytes) {
		list->nent = 0;
		list->ents = NULL;
		return 0;
	}

	if (NULL == sg_list) {
		return -EINVAL;
	}

	if (nbytes) {
		nents = sg_nents_for_len(sg_list, nbytes);
		if (nents < 0) {
			return nents;
		}
	}

	/**
	 * A src scatterlist entry maps to one memlist entry.
	 *
	 * A dst scatterlist entry can be mapped to at most 3 memlist entries
	 * after fixup. So, we are likely to produce more te_mement_t entries
	 * than needed. It's fine for sizeof(te_mement_t) is small. The reward
	 * is we can save another scatterlist tranverse then.
	 */
	if (is_dst) {
		nents *= 3;
	}
	mgr = kzalloc(sizeof(*mgr) + nents * sizeof(te_mement_t), GFP_KERNEL);
	if (NULL == mgr) {
		return -ENOMEM;
	}
	mgr->nent = nents;
	INIT_LIST_HEAD(&mgr->lst);

	buf_mgr_dump_sglist(sg_list, nbytes);
	/* Traverse the sg_list */
	while (i < nents && nbytes) {
		if (sg_list->length != 0) {
			sadr = sg_virt(sg_list);
			slen = (sg_list->length > nbytes) ?
				  nbytes : sg_list->length;
			if (is_dst) {
				rc = buf_mgr_proc_sg_dst(mgr, i, sadr, slen);
			} else {
				rc = buf_mgr_proc_sg_src(mgr, i, sadr, slen);
			}
			if (rc < 0) {
				pr_err("buf_mgr_proc_sg_%s ret %d\n",
					(is_dst) ? "dst" : "src", rc);
				goto err;
			}

			nbytes -= slen;
			i += rc;
			sg_list = sg_next(sg_list);
		} else {
			sg_list = (struct scatterlist *)sg_page(sg_list);
		}
	}

	mgr->nused = i;
	buf_mgr_dump_memlist(mgr);

	list->ents = mgr->ents;
	list->nent = i;
	return 0;

err:
	buf_mgr_free(mgr, rc);
	return rc;
}

void te_buf_mgr_free_memlist(te_memlist_t *list, int err)
{
	struct te_buf_mgr *mgr;

	if (NULL == list->ents) {
		list->nent = 0;
		return;
	}

	mgr = container_of(list->ents, struct te_buf_mgr, ents[0]);
	buf_mgr_free(mgr, err);
	list->ents = NULL;
	list->nent = 0;
}

