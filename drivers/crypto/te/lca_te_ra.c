//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2024 ARM Technology (China) Co., Ltd.
 */

/*#define DEBUG*/
#define pr_fmt(fmt) "te_crypt: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/freezer.h>
#include "lca_te_ra.h"

/**
 * Driver Request Agent
 *
 * #Introduction
 *
 * The request agent is designed to decouple the request submission from the
 * request complete path. A new request might be submitted in a request
 * complete hook in some special use cases. E.g., A ahash digest() request
 * will be submitted on dcrypto_authenc_encrypt_done().
 *
 * It is required for the driver to decouple a new request from the complete
 * hook of an early request unless the resource for a new request is unlimited.
 * Otherwise, the software might run into dead loop in the worst case. In the
 * LCA driver created based on the TE driver, the number of driver contexts for
 * a transformation is limited to LCA_TE_CTX_MAX for good concurrent performance
 * and resource consumption behavior. See lca_te_ctx.c for details.
 *
 * Besides, the request agent also supports power management.
 * - New requests will be pended after .suspend() until the driver is resumed.
 * - All outstanding requests shall complete before suspend the driver. The
 *   request agent will wait the completion for at most TE_RA_SUSP_WAIT_SEC
 *   seconds within the .suspend(). A -EBUSY error will be returned if time
 *   is out, which will cancel the entire suspend operation in turns.
 *
 * #Interfaces
 *
 * See lca_te_ra.h for details on the interfaces.
 *
 * #Functional Design
 *
 * The driver requests are classified into two groups according to their origin:
 * - Requests initiated by the user calling thread. Requests of this group are
 *   submitted to the driver directly.
 * - Requests initiated in the complete hook. A workqueue is used to submit the
 *   requests of this group to the driver on behave of the complete hook. The
 *   workqueue is created on driver basis.
 *
 * The below diagram outlines the request submission flow for:
 * 1. request initiated from the crypto user's calling thread.
 * 2. request (subreq) initiated in the complete hook.
 *
 *  +----------------------------------+
 *  |          crypto user             |
 *  +----------------------------------+
 *       ^                1.0:new |
 *       |                        v
 *  +----------+ 2.0:subreq  +---------+ 2.1:queue work +-----------+
 *  | complete |------------>| request |--------------->| workqueue |
 *  +----------+             +---------+                +-----------+
 *       ^                        |                           |
 *       |             1.1:submit |                2.2:submit |
 *       |                        v                           v
 *  +---------------------------------------------------------------+
 *  |                         TE driver                             |
 *  +---------------------------------------------------------------+
 *
 */

#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)

#define TE_RA_SUSP_WAIT_SEC                  10 /**< 10 seconds */

#define TE_RA_DRV_LOCK()                     do {                              \
	mutex_lock(&g_ra_drv.lock);                                            \
} while(0)

#define TE_RA_DRV_UNLOCK()                   do {                              \
	mutex_unlock(&g_ra_drv.lock);                                          \
} while(0)

#define TE_RA_DRV_POLL()                     do {                              \
	te_ra_poll();                                                          \
} while(0)

#define TE_RA_DRV_IS_ENABLE()                g_ra_drv.enable

#define TE_RA_INIT_REQ_LST(ra)               do {                              \
	INIT_LIST_HEAD(&(ra)->req_lst);                                        \
} while(0)

#define TE_RA_CHK_REQ_LST(ra)                do {                              \
	BUG_ON(!list_empty(&(ra)->req_lst));                                   \
} while(0)

#define TE_RA_ENQUEUE(ra)                    do {                              \
	list_add_tail(&(ra)->node, &g_ra_drv.ra_lst);                          \
} while(0)

#define TE_RA_DEQUEUE(ra)                    do {                              \
	list_del(&(ra)->node);                                                 \
} while(0)

#define TE_RA_REQ_ENQUEUE_UNSAFE(ra, treq)   do {                              \
	pr_debug("pid %d enqueue treq %p\n", current->pid, (treq));            \
	list_add_tail(&(treq)->node, &(ra)->req_lst);                          \
} while(0)

#define TE_RA_REQ_DEQUEUE_UNSAFE(treq)       do {                              \
	pr_debug("pid %d dequeue treq %p\n", current->pid, (treq));            \
	list_del(&(treq)->node);                                               \
} while(0)

#define TE_RA_REQ_ENQUEUE(ra, treq)          do {                              \
	mutex_lock(&(ra)->mut);                                                \
	TE_RA_REQ_ENQUEUE_UNSAFE((ra), (treq));                                \
	mutex_unlock(&(ra)->mut);                                              \
} while(0)

#define TE_RA_REQ_DEQUEUE(ra, treq)          do {                              \
	mutex_lock(&(ra)->mut);                                                \
	TE_RA_REQ_DEQUEUE_UNSAFE(treq);                                        \
	mutex_unlock(&(ra)->mut);                                              \
} while(0)

#else  /* CONFIG_PM || CONFIG_PM_SLEEP */

#define TE_RA_DRV_LOCK()                     do{} while(0)
#define TE_RA_DRV_UNLOCK()                   do{} while(0)
#define TE_RA_DRV_POLL()                     do{} while(0)
#define TE_RA_DRV_IS_ENABLE()                true

#define TE_RA_INIT_REQ_LST(ra)               do{} while(0)
#define TE_RA_CHK_REQ_LST(ra)                do{} while(0)

#define TE_RA_ENQUEUE(ra)                    do{} while(0)
#define TE_RA_DEQUEUE(ra)                    do{} while(0)

#define TE_RA_REQ_ENQUEUE_UNSAFE(ra, treq)   do{} while(0)
#define TE_RA_REQ_DEQUEUE_UNSAFE(treq)       do{} while(0)
#define TE_RA_REQ_ENQUEUE(ra, treq)          do{} while(0)
#define TE_RA_REQ_DEQUEUE(ra, treq)          do{} while(0)
#endif /* !CONFIG_PM && !CONFIG_PM_SLEEP */

/**
 * TE driver request context structure.
 */
struct te_ra_ctx {
	struct workqueue_struct *wq;
	struct list_head ccb_lst;    /**< Complete callback list */
#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)
	struct list_head req_lst;    /**< Outstanding request list */
	struct list_head node;
#endif
	struct mutex mut;
};

/**
 * Complete callback entry structure.
 */
struct te_ra_ccb_ent {
	pid_t pid;
	struct list_head node;
};

#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)
/**
 * Request agent driver instance
 */
struct te_ra_drv {
	bool enable;                 /**< Enable flag */
	wait_queue_head_t pm_wq;     /**< PM wait queue */
	struct list_head ra_lst;     /**< Agent list */
	struct mutex lock;           /**< Lock */
} g_ra_drv = {
	.enable = true,
	.pm_wq  = __WAIT_QUEUE_HEAD_INITIALIZER(g_ra_drv.pm_wq),
	.ra_lst = LIST_HEAD_INIT(g_ra_drv.ra_lst),
	.lock   = __MUTEX_INITIALIZER(g_ra_drv.lock),
};

static void te_ra_poll(void)
{
	/* Wait till the request agent gets enabled */
	wait_event_freezable(g_ra_drv.pm_wq, g_ra_drv.enable);
}
#endif /* CONFIG_PM || CONFIG_PM_SLEEP */

/**
 * Enqueue the request if the agent is enabled then return 0.
 * Otherwise return -EAGAIN.
 */
static int te_try_req_enqueue(struct te_ra_ctx *ctx, struct te_request *treq)
{
	int rc = -EAGAIN;

	/*
	 * Use lock to avoid race condition with the .suspend().
	 * Enqueue the request if the agent is enabled.
	 */
	TE_RA_DRV_LOCK();
	if (TE_RA_DRV_IS_ENABLE()) {
		TE_RA_REQ_ENQUEUE(ctx, treq);
		rc = 0;
	}
	TE_RA_DRV_UNLOCK();

	return rc;
}

static void kra_asubmit(struct work_struct *work)
{
	int rc;
	struct te_request *treq = container_of(work, struct te_request, work);

	pr_debug("kra asubmit req %p\n", treq->areq);
	BUG_ON(NULL == treq->fn);
	BUG_ON(NULL == treq->areq);
	/**
	 * Enqueue the incoming request. Unlike lca_te_ra_submit(), we need
	 * loop here till the request is successfully enqueued.
	 *
	 * The following points shall be considered here:
	 * - The request shall not be enqueued before polling the agent.
	 *   Otherwise, the .suspend() would suffer from -EBUSY error.
	 * - The asubmit() shall wait, or hold on submitting any request in
	 *   other words, till the agent is enabled. Otherwise, the TE driver
	 *   might suffer from command losing due to driver suspends.
	 * - The enqueue might fail even if poll() passes as the .suspend()
	 *   might disable the agent in between. So, we need to retry.
	 */
	do {
		TE_RA_DRV_POLL();
		rc = te_try_req_enqueue((struct te_ra_ctx *)treq->priv, treq);
	} while (-EAGAIN == rc);

	rc = treq->fn(treq);
	if (rc != -EINPROGRESS) {
		TE_RA_REQ_DEQUEUE((struct te_ra_ctx *)treq->priv, treq);
		/* Notify the caller of the result by xxx_request_complete() */
		treq->areq->complete(treq->areq, rc);
	}
}

#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)
int lca_te_ra_suspend(void)
{
	bool wait;
	struct te_ra_ctx *ra;
	unsigned long end_time = jiffies + TE_RA_SUSP_WAIT_SEC * HZ;

	/* Disable the request agent, use lock to sync with .submit() */
	mutex_lock(&g_ra_drv.lock);
	g_ra_drv.enable = false;
	mutex_unlock(&g_ra_drv.lock);

	/* Wait till all outstanding requests complete */
	while (1) {
		wait = false;
		list_for_each_entry(ra, &g_ra_drv.ra_lst, node) {
			mutex_lock(&ra->mut);
			wait |= !list_empty(&ra->req_lst);
			mutex_unlock(&ra->mut);
		}

		if (!wait || time_after(jiffies, end_time)) {
			break;
		}

		schedule();
	}

	if (wait) {
		/* Re-enable the request agent on error */
		g_ra_drv.enable = true;
		wake_up_all(&g_ra_drv.pm_wq);
		return -EBUSY;
	} else {
		return 0;
	}
}

void lca_te_ra_resume(void)
{
	/* Enable the request agent */
	g_ra_drv.enable = true;
	wake_up_all(&g_ra_drv.pm_wq);
}
#endif /* CONFIG_PM || CONFIG_PM_SLEEP */

void lca_te_ra_poll(void)
{
	TE_RA_DRV_POLL();
}

int lca_te_ra_alloc(lca_te_ra_handle *ph, const char *name)
{
	int rc = 0;
	struct te_ra_ctx *ctx;

	if (NULL == ph) {
		return -EINVAL;
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (NULL == ctx) {
		return -ENOMEM;
	}

	ctx->wq = alloc_workqueue("kra_%s",
				WQ_HIGHPRI | WQ_CPU_INTENSIVE | WQ_MEM_RECLAIM,
				1, name);
	if (NULL == ctx->wq) {
		pr_err("couldn't create ra wq: %s\n", name);
		rc = -ENOMEM;
		goto err_free_ctx;
	}

	/* Initialize the ctx obj */
	INIT_LIST_HEAD(&ctx->ccb_lst);
	mutex_init(&ctx->mut);
	TE_RA_INIT_REQ_LST(ctx);
	TE_RA_ENQUEUE(ctx);
	*ph = ctx;
	pr_debug("alloc ra name '%s' obj %p\n", name, ctx);
	goto out;

err_free_ctx:
	kfree(ctx);
out:
	return rc;
}

void lca_te_ra_free(lca_te_ra_handle h)
{
	struct te_ra_ccb_ent *ent, *tmp;
	struct te_ra_ctx *ctx = h;

	pr_debug("free ra obj %p\n", ctx);
	if (ctx->wq) {
		destroy_workqueue(ctx->wq);
		ctx->wq = NULL;
	}

	list_for_each_entry_safe(ent, tmp, &ctx->ccb_lst, node) {
		pr_debug("free ccb ent %p\n", ent);
		list_del(&ent->node);
		kfree(ent);
	}

	TE_RA_CHK_REQ_LST(ctx);
	mutex_destroy(&ctx->mut);
	TE_RA_DEQUEUE(ctx);
	memset(ctx, 0, sizeof(*ctx));
	kfree(ctx);
}

int lca_te_ra_submit(lca_te_ra_handle h, struct te_request *treq, bool prot)
{
	int rc = 0;
	bool found = false;
	pid_t pid = current->pid;
	struct te_ra_ctx *ctx = h;
	struct te_ra_ccb_ent *ent;

	/* Whether need to protect the request? */
	if (prot) {
		mutex_lock(&ctx->mut);
		list_for_each_entry(ent, &ctx->ccb_lst, node) {
			if (pid == ent->pid) {
				found = true;
				break;
			}
		}
		mutex_unlock(&ctx->mut);
	}

	/**
	 * Synchronously submit the request if all:
	 * - the agent is enabled.
	 * - no need to protect the request or not in the complete hook.
	 */
	if (!found) {
		/* Try enqueuing the incoming request */
		rc = te_try_req_enqueue(ctx, treq);
		if (rc != 0) {
			/* The agent is disabled. So, do async submit instead */
			goto do_async;
		}

		/*
		 * There is still a chance that the request agent is disabled by
		 * the .suspend() when reaching here. Considering the request
		 * has already been enqueued and the .suspend() will wait all
		 * enqueued requests complete before suspend the driver, it is
		 * still fine.
		 */
		BUG_ON(NULL == treq->fn);
		rc = treq->fn(treq);
		if (rc != -EINPROGRESS) {
			TE_RA_REQ_DEQUEUE(ctx, treq);
		}
		goto out;
	}

do_async:
	/* Asynchronously submit the request otherwise */
	pr_debug("pid %d kra submit req %p\n", pid, treq->areq);
	treq->priv = h;
	INIT_WORK(&treq->work, kra_asubmit);
	queue_work(ctx->wq, &treq->work);
	rc = -EINPROGRESS;

out:
	return rc;
}

int lca_te_ra_complete_prepare(lca_te_ra_handle h, struct te_request *treq)
{
	int rc = 0;
	pid_t pid = current->pid;
	struct te_ra_ctx *ctx = h;
	struct te_ra_ccb_ent *ent;

	/* Sanity check */
	mutex_lock(&ctx->mut);
	list_for_each_entry(ent, &ctx->ccb_lst, node) {
		BUG_ON(ent->pid == pid);
	}
	mutex_unlock(&ctx->mut);

	ent = kzalloc(sizeof(*ent), GFP_KERNEL);
	if (NULL == ent) {
		rc = -ENOMEM;
		goto out;
	}
	ent->pid = pid;

	mutex_lock(&ctx->mut);
	TE_RA_REQ_DEQUEUE_UNSAFE(treq);
	list_add_tail(&ent->node, &ctx->ccb_lst);
	pr_debug("pid %d insert ccb ent %p req %p\n", pid, ent, treq->areq);
	mutex_unlock(&ctx->mut);

out:
	return rc;
}

int lca_te_ra_complete_done(lca_te_ra_handle h, struct te_request *treq)
{
	int rc = -ENOENT;
	struct te_ra_ctx *ctx = h;
	struct te_ra_ccb_ent *ent, *tmp;

	mutex_lock(&ctx->mut);
	list_for_each_entry_safe(ent, tmp, &ctx->ccb_lst, node) {
		if (current->pid == ent->pid) {
			pr_debug("pid %d free ccb ent %p req %p\n",
				 current->pid, ent, treq->areq);
			list_del(&ent->node);
			kfree(ent);
			rc = 0;
			break;
		}
	}
	mutex_unlock(&ctx->mut);

	return rc;
}

void lca_te_ra_request(lca_te_ra_handle h, struct te_request *treq)
{
#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)
	struct te_ra_ctx *ctx = h;

	TE_RA_REQ_ENQUEUE(ctx, treq);
#else  /* !CONFIG_PM || CONFIG_PM_SLEEP */
	(void)h;
	(void)treq;
#endif /* !CONFIG_PM && !CONFIG_PM_SLEEP */
}

void lca_te_ra_complete(lca_te_ra_handle h, struct te_request *treq)
{
#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)
	struct te_ra_ctx *ctx = h;

	TE_RA_REQ_DEQUEUE(ctx, treq);
#else  /* CONFIG_PM || CONFIG_PM_SLEEP */
	(void)h;
	(void)treq;
#endif /* !CONFIG_PM && !CONFIG_PM_SLEEP */
}
