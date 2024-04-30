//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

/**
 * The OSAL deployment config
 * Currently we only support:
 * 1. Linux kernel
 * 2. Linux user space
 * 3. OPTEE TA space
 * 4. OPTEE OS
 * 5. UBL baremetal environment
 */
#include "osal_common.h"

#if !(defined(OSAL_ENV_LINUX_KERNEL) || defined(OSAL_ENV_LINUX_USER) ||        \
      defined(OSAL_ENV_OPTEE_TA) || defined(OSAL_ENV_OPTEE_OS) ||              \
      defined(OSAL_ENV_UBL) || defined(OSAL_ENV_UBOOT) || defined(OSAL_ENV_LK))
#error "OSAL_ENV not defined!"
#endif
