//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#ifndef __TRUSTENGINE_ENDIAN_H__
#define __TRUSTENGINE_ENDIAN_H__

#ifndef __ASSEMBLY__

#define ___te_swap16(x) ((uint16_t)(                                \
    (((uint16_t)(x) & (uint16_t)0x00ffU) << 8) |                    \
    (((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))

#define ___te_swap32(x) ((uint32_t)(                                \
    (((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) |              \
    (((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) |              \
    (((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) |              \
    (((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24)))

#define ___te_swap64(x) ((uint64_t)(                                \
    (((uint64_t)(x) & (uint64_t)0x00000000000000ffULL) << 56) |     \
    (((uint64_t)(x) & (uint64_t)0x000000000000ff00ULL) << 40) |     \
    (((uint64_t)(x) & (uint64_t)0x0000000000ff0000ULL) << 24) |     \
    (((uint64_t)(x) & (uint64_t)0x00000000ff000000ULL) <<  8) |     \
    (((uint64_t)(x) & (uint64_t)0x000000ff00000000ULL) >>  8) |     \
    (((uint64_t)(x) & (uint64_t)0x0000ff0000000000ULL) >> 24) |     \
    (((uint64_t)(x) & (uint64_t)0x00ff000000000000ULL) >> 40) |     \
    (((uint64_t)(x) & (uint64_t)0xff00000000000000ULL) >> 56)))

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define HTOLE16(x) ((uint16_t)(x))
#define HTOLE32(x) ((uint32_t)(x))
#define HTOLE64(x) ((uint64_t)(x))
#define LE16TOH(x) ((uint16_t)(x))
#define LE32TOH(x) ((uint32_t)(x))
#define LE64TOH(x) ((uint64_t)(x))

#define HTOBE16(x) (___te_swap16(x))
#define HTOBE32(x) (___te_swap32(x))
#define HTOBE64(x) (___te_swap64(x))
#define BE16TOH(x) (___te_swap16(x))
#define BE32TOH(x) (___te_swap32(x))
#define BE64TOH(x) (___te_swap64(x))

#else  /* __ORDER_LITTLE_ENDIAN__ */

#define HTOLE16(x) (___te_swap16(x))
#define HTOLE32(x) (___te_swap32(x))
#define HTOLE64(x) (___te_swap64(x))
#define LE16TOH(x) (___te_swap16(x))
#define LE32TOH(x) (___te_swap32(x))
#define LE64TOH(x) (___te_swap64(x))

#define HTOBE16(x) ((uint16_t)(x))
#define HTOBE32(x) ((uint32_t)(x))
#define HTOBE64(x) ((uint64_t)(x))
#define BE16TOH(x) ((uint16_t)(x))
#define BE32TOH(x) ((uint32_t)(x))
#define BE64TOH(x) ((uint64_t)(x))

#endif /* !__ORDER_LITTLE_ENDIAN__ */

#endif /* !__ASSEMBLY__ */
#endif /* __TRUSTENGINE_ENDIAN_H__ */
