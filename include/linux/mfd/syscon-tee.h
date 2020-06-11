/*
 * TEE System Control Driver
 *
 * Copyright (C) 2020 Horizon.AI Ltd.
 *
 * Author: Zhaohui.Shi <zhaohui.shi@horizon.ai>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __LINUX_MFD_SYSCON_TEE_H__
#define __LINUX_MFD_SYSCON_TEE_H__

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/mfd/syscon.h>

#if defined(CONFIG_MFD_SYSCON_TEE)
extern struct regmap *syscon_tee_node_to_regmap(struct device_node *np);
extern struct regmap *syscon_tee_regmap_lookup_by_compatible(const char *s);
extern struct regmap *syscon_tee_regmap_lookup_by_pdevname(const char *s);
extern struct regmap *
syscon_tee_regmap_lookup_by_phandle(struct device_node *np,
				    const char *property);
#elif defined(CONFIG_MFD_SYSCON)
#define syscon_tee_node_to_regmap(np) syscon_node_to_regmap(np)
#define syscon_tee_regmap_lookup_by_compatible(s)                              \
	syscon_regmap_lookup_by_compatible(s)
#define syscon_tee_regmap_lookup_by_pdevname(s)                                \
	syscon_regmap_lookup_by_pdevname(s)
#define syscon_tee_regmap_lookup_by_phandle(np, property)                      \
	syscon_regmap_lookup_by_phandle(np, property)
#else
static inline struct regmap *syscon_tee_node_to_regmap(struct device_node *np)
{
	return ERR_PTR(-ENOTSUPP);
}

static inline struct regmap *
syscon_tee_regmap_lookup_by_compatible(const char *s)
{
	return ERR_PTR(-ENOTSUPP);
}

static inline struct regmap *syscon_tee_regmap_lookup_by_pdevname(const char *s)
{
	return ERR_PTR(-ENOTSUPP);
}

static inline struct regmap *
syscon_tee_regmap_lookup_by_phandle(struct device_node *np,
				    const char *property)
{
	return ERR_PTR(-ENOTSUPP);
}
#endif

#endif /* __LINUX_MFD_SYSCON_TEE_H__ */
