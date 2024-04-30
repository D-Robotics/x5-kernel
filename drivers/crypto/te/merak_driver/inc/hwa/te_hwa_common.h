//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_HWA_COMMON_H__
#define __TRUSTENGINE_HWA_COMMON_H__

#include <te_common.h>
#include <te_endian.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ASSEMBLY__

struct te_hwa_host;

/**
 * Load the value of the register \p nm from hwa module \p mod.
 * Log the register value if WITH_BITFIELD_LOG is defined.
 *
 * \regs            register file.
 * \mod             module.
 * \nm              register name.
 */


#define HWA_REG_GET(regs, mod, nm) __extension__({         \
    unsigned int __val = LE32TOH((regs) -> nm .val);       \
    log_##mod##_##nm (__val);                              \
    __val;                                                 \
})

/**
 * Set the register \p nm of hwa module \p mod to value \p nv.
 * Log the register value if WITH_BITFIELD_LOG is defined.
 *
 * \regs            register file.
 * \mod             module.
 * \nm              register name.
 * \nv              new value.
 */


#define HWA_REG_SET(regs, mod, nm, nv) do {                \
    (regs) -> nm .val = HTOLE32((nv));                     \
    log_##mod##_##nm (nv);                                 \
} while(0)

/**
 * Get the field \p fn value from the specified register \p rn
 * value \p val.
 *
 * \val             Register value, 32-bit.
 * \rn              Register name.
 * \fn              Register field name.
 */
#define HWA_FIELD_GET(val, rn, fn) __extension__({         \
     GET_BITS((val),rn ##_## fn ##_SHIFT,                  \
              rn ##_## fn ##_WIDTH);                       \
})

/**
 * Set the field \p fn value \p fv in the specified register \p rn
 * value \p val.
 *
 * \val             Register value, 32-bit.
 * \rn              Register name.
 * \fn              Register field name.
 * \fv              Register field value.
 */
#define HWA_FIELD_SET(val, rn, fn, fv) __extension__({    \
     SET_BITS((val),rn ##_## fn ##_SHIFT,                 \
              rn ##_## fn ##_WIDTH,(fv));                 \
})

/**
 * Trust engine hwa crypto structure
 */
typedef struct te_hwa_crypt {
   struct te_hwa_host *host;
   void *__ctx;                  /**< private context pointer */
} te_hwa_crypt_t;

/**
 * \brief           This function initializes the crypto context.
 * \param[in] crypt The hwa crypto context.
 * \param[in] host  The hwa host pointer.
 * \param[in] priv  The private context pointer.
 * \return          void.
 */
static inline void hwa_crypt_init(te_hwa_crypt_t *crypt,
                                  struct te_hwa_host *host,
                                  void *priv)
{
    crypt->host  = host;
    crypt->__ctx = priv;
}

/**
 * \brief           This function cleans up the crypto context.
 * \param[in] crypt The hwa crypto context.
 * \return          void.
 */
static inline void hwa_crypt_exit(te_hwa_crypt_t *crypt)
{
    osal_memset(crypt, 0, sizeof(*crypt));
}

/**
 * \brief           This function gets the host ptr of a hwa crypto context.
 * \param[in] crypt The hwa crypto context.
 * \return          The hwa host pointer.
 */
static inline struct te_hwa_host* hwa_crypt_host(te_hwa_crypt_t *crypt)
{
    return crypt->host;
}

/**
 * \brief           This function gets the private ctx of a hwa crypto context.
 * \param[in] crypt The hwa crypto context.
 * \return          The private context pointer.
 */
static inline void* hwa_crypt_ctx(te_hwa_crypt_t *crypt)
{
    return crypt->__ctx;
}

#endif /* !__ASSEMBLY__ */

#ifdef __cplusplus
}
#endif

#endif /* __TRUSTENGINE_HWA_COMMON_H__ */
