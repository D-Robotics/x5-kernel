//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/* \file lca_te_ra.h
 * Arm China Trust Engine Linux Crypto Driver Request Agent
 */

#include "lca_te_driver.h"

#ifndef __LCA_TE_RA_H__
#define __LCA_TE_RA_H__

/**
 * Crypto async request complete helper for the protected requests.
 * (Aka requests submitted by lca_te_ra_submit(h, treq, prot=true)
 */
#define LCA_TE_RA_COMPLETE(h, comp, req, err) do {                             \
	lca_te_ra_complete_prepare((h), (struct te_request *)&(req)->__ctx);   \
	comp((req), (err));                                                    \
	lca_te_ra_complete_done((h), (struct te_request *)&(req)->__ctx);      \
} while(0)

struct te_ra_ctx;

/**
 * Request agent handle used by this module.
 */
typedef struct te_ra_ctx * lca_te_ra_handle;

#if defined(CONFIG_PM) || defined(CONFIG_PM_SLEEP)
/**
 * \brief           Suspend the request agent.
 * \retval          0 on success.
 * \retval          -EBUSY on error.
 */
int lca_te_ra_suspend(void);

/**
 * \brief           Resume the request agent.
 * \retval          Void.
 */
void lca_te_ra_resume(void);

#endif /* CONFIG_PM || CONFIG_PM_SLEEP */

/**
 * \brief           Poll till the request agent gets enabled. The calling
 *                  thread will sleep if the request agent is disabled.
 * \retval          Void.
 */
void lca_te_ra_poll(void);

/**
 * \brief           Allocate a request agent.
 * \param[out] ph   Buffer filled with a valid handle on success.
 * \param[in] name  Agent name.
 * \retval          0 on success.
 */
int lca_te_ra_alloc(lca_te_ra_handle *ph, const char *name);

/**
 * \brief           Free a request agent.
 * \param[in] h     Request agent handle to free.
 * \retval          Void.
 */
void lca_te_ra_free(lca_te_ra_handle h);

/**
 * \brief           Submit a request. The request is either submitted to the
 *                  the driver or handed to the agent depending on the calling
 *                  thread. The agent will submit the request in a separate
 *                  execution stream if receiving one.
 * \param[in] h     Request agent handle.
 * \param[in] treq  Request to submit.
 * \param[in] prot  Whether to protect the request from the calling thread.
 * \retval          0 if the request is well handled synchronously.
 * \retval          -EINPROGRESS if the request is correctly submitted.
 * \retval          Other minus value in case of error.
 */
int lca_te_ra_submit(lca_te_ra_handle h, struct te_request *treq, bool prot);

/**
 * \brief           Prepare completion for a request. This function shall be
 *                  called before calling the user's complete hook.
 * \param[in] h     Request agent handle.
 * \param[in] treq  Request to complete
 * \retval          0 on success.
 * \retval          -ENOMEM if out of memory.
 */
int lca_te_ra_complete_prepare(lca_te_ra_handle h, struct te_request *treq);

/**
 * \brief           Finish completion for a request. This function shall be
 *                  called after calling the user's complete hook.
 * \param[in] h     Request agent handle.
 * \param[in] treq  Request to complete
 * \retval          0 on success.
 * \retval          -ENOENT in case of internal error.
 */
int lca_te_ra_complete_done(lca_te_ra_handle h, struct te_request *treq);

/**
 * \brief           Check in an unprotected request to the request agent. This
 *                  is for requests that have no execution stream constraint
 *                  when issued to the driver.
 *                  The request is issued outside of the request agent.
 * \param[in] h     Request agent handle.
 * \param[in] treq  Request to check in.
 * \retval          Void.
 */
void lca_te_ra_request(lca_te_ra_handle h, struct te_request *treq);

/**
 * \brief           Complete a unprotected request for the request agent. The
 *                  request shall be recorded via lca_te_ra_request() or
 *                  submitted by lca_te_ra_submit(h, treq, prot=false) earlier.
 *                  This function shall be called before calling the user's
 *                  complete hook.
 * \param[in] h     Request agent handle.
 * \param[in] treq  Request to complete
 * \retval          Void.
 */
void lca_te_ra_complete(lca_te_ra_handle h, struct te_request *treq);

#endif /* __LCA_TE_RA_H__ */
