/*
 * Register map access API - TEE support
 *
 * Copyright (c) 2020, Horizon CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/err.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/arm-smccc.h>

#include "internal.h"

typedef void(hobot_invoke_fn)(unsigned long, unsigned long, unsigned long,
			      unsigned long, unsigned long, unsigned long,
			      unsigned long, unsigned long,
			      struct arm_smccc_res *);

struct regmap_tee_context {
	void __iomem *regs;
	unsigned val_bytes;
	hobot_invoke_fn *invoke_fn;
	uint32_t fid;
	uint32_t r_cmd;
	uint32_t w_cmd;
	uint32_t u_cmd;

	void (*reg_write)(struct regmap_tee_context *ctx, unsigned int reg,
			  unsigned int val);
	unsigned int (*reg_read)(struct regmap_tee_context *ctx,
				 unsigned int reg);
	unsigned int (*update_bits)(struct regmap_tee_context *ctx, unsigned int reg,
				unsigned int mask, unsigned int val);
};

/**
 * hobot_smccc_smc() - Method to call firmware via SMC.
 * @a0: Argument passed to Secure EL3.
 * @a1: Argument passed to Secure EL3.
 * @a2: Argument passed to Secure EL3.
 * @a3: Argument passed to Secure EL3.
 * @a4: Argument passed to Secure EL3.
 * @a5: Argument passed to Secure EL3.
 * @a6: Argument passed to Secure EL3.
 * @a7: Argument passed to Secure EL3.
 * @res: return code stored in.
 *
 * This function call arm_smccc_smc directly.
 *
 * Return: return value stored in res argument.
 */
static void hobot_smccc_smc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

/**
 * hobot_smccc_hvc() - Method to call firmware via HVC.
 * @a0: Arguments passed to firmware.
 * @a1: Arguments passed to firmware.
 * @a2: Arguments passed to firmware.
 * @a3: Arguments passed to firmware.
 * @a4: Arguments passed to firmware.
 * @a5: Arguments passed to firmware.
 * @a6: Arguments passed to firmware.
 * @a7: Arguments passed to firmware.
 * @res: return code stored in.
 *
 * This function call arm_smccc_hvc directly.
 *
 * Return: return value stored in res argument.
 */
static void hobot_smccc_hvc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static hobot_invoke_fn *get_invoke_func(struct device_node *np)
{
	const char *method;

	if (of_property_read_string(np, "method", &method)) {
		pr_warn("missing \"method\" property\n");
		return ERR_PTR(-ENXIO);
	}

	if (!strcmp("hvc", method))
		return hobot_smccc_hvc;
	else if (!strcmp("smc", method))
		return hobot_smccc_smc;

	pr_warn("invalid \"method\" property: %s\n", method);
	return ERR_PTR(-EINVAL);
}

static int regmap_tee_regbits_check(size_t reg_bits)
{
	switch (reg_bits) {
	case 32:
		return 0;
	default:
		return -EINVAL;
	}
}

static int regmap_tee_get_min_stride(size_t val_bits)
{
	int min_stride;

	switch (val_bits) {
	case 32:
		min_stride = 4;
		break;
	default:
		return -EINVAL;
	}

	return min_stride;
}

static void regmap_tee_write32le(struct regmap_tee_context *ctx,
				 unsigned int reg, unsigned int val)
{
	struct arm_smccc_res res;

	ctx->invoke_fn(ctx->fid, ctx->w_cmd, (uintptr_t)ctx->regs + reg, val, 0,
		       0, 0, 0, &res);

	if (res.a0 != 0)
		panic("%s: return failed with status: %d\n", __func__,
		      (int32_t)res.a0);
}

static int regmap_tee_write(void *context, unsigned int reg, unsigned int val)
{
	struct regmap_tee_context *ctx = context;

	ctx->reg_write(ctx, reg, val);

	return 0;
}

static unsigned int regmap_tee_read32le(struct regmap_tee_context *ctx,
					unsigned int reg)
{
	struct arm_smccc_res res;

	ctx->invoke_fn(ctx->fid, ctx->r_cmd, (uintptr_t)ctx->regs + reg, 0, 0,
		       0, 0, 0, &res);

	if (res.a0 != 0)
		panic("%s: return failed with status: %d\n", __func__,
		      (int32_t)res.a0);

	return res.a1;
}

static int regmap_tee_read(void *context, unsigned int reg, unsigned int *val)
{
	struct regmap_tee_context *ctx = context;

	*val = ctx->reg_read(ctx, reg);

	return 0;
}

static unsigned int regmap_tee_update_bits32le(struct regmap_tee_context *ctx, unsigned int reg,
					unsigned int mask, unsigned int val)
{
	struct arm_smccc_res res;

	ctx->invoke_fn(ctx->fid, ctx->u_cmd, (uintptr_t)ctx->regs + reg, val, mask,
		0, 0, 0, &res);

	if (res.a0 != 0)
		panic("%s: return failed with status: %d\n", __func__,
			(int32_t)res.a0);

	return res.a1;
}

static int regmap_tee_update_bits(void *context, unsigned int reg, unsigned int mask, unsigned int val)
{
	struct regmap_tee_context *ctx = context;

	ctx->update_bits(ctx, reg, mask, val);

	return 0;
}

static void regmap_tee_free_context(void *context)
{
	kfree(context);
}

static const struct regmap_bus regmap_tee = {
	.fast_io = true,
	.reg_write = regmap_tee_write,
	.reg_read = regmap_tee_read,
	.reg_update_bits = regmap_tee_update_bits,
	.free_context = regmap_tee_free_context,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

static struct regmap_tee_context *
regmap_tee_gen_context(struct device *dev, struct device_node *np,
		       void __iomem *regs, const struct regmap_config *config)
{
	struct regmap_tee_context *ctx;
	hobot_invoke_fn *invoke_fn;
	int min_stride;
	int ret;
	struct device_node *s_np;

	if (!np) {
		return ERR_PTR(-EINVAL);
	}

	ret = regmap_tee_regbits_check(config->reg_bits);
	if (ret)
		return ERR_PTR(ret);

	if (config->pad_bits)
		return ERR_PTR(-EINVAL);

	min_stride = regmap_tee_get_min_stride(config->val_bits);
	if (min_stride < 0)
		return ERR_PTR(min_stride);

	if (config->reg_stride < min_stride)
		return ERR_PTR(-EINVAL);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->regs = regs;
	ctx->val_bytes = config->val_bits / 8;

	switch (regmap_get_val_endian(dev, &regmap_tee, config)) {
	case REGMAP_ENDIAN_DEFAULT:
	case REGMAP_ENDIAN_LITTLE:
#ifdef __LITTLE_ENDIAN
	case REGMAP_ENDIAN_NATIVE:
#endif
		switch (config->val_bits) {
		case 32:
			ctx->reg_read = regmap_tee_read32le;
			ctx->reg_write = regmap_tee_write32le;
			ctx->update_bits = regmap_tee_update_bits32le;
			break;
		default:
			ret = -EINVAL;
			goto err_free;
		}
		break;
	default:
		ret = -EINVAL;
		goto err_free;
	}

	s_np = of_parse_phandle(np, "services", 0);
	if (!s_np)
		s_np = np;

	invoke_fn = get_invoke_func(s_np);
	if (IS_ERR(invoke_fn)) {
		ret = PTR_ERR(invoke_fn);
		goto err_free;
	}
	ctx->invoke_fn = invoke_fn;

#define GET_OFNODE_U32(node, name, dst)                                        \
	do {                                                                   \
		ret = of_property_read_u32(node, name, dst);                   \
		if (ret) {                                                     \
			pr_err("%s: failed to get " name "\n", __func__);      \
			ret = -EINVAL;                                         \
			goto err_free;                                         \
		}                                                              \
	} while (0)

	GET_OFNODE_U32(s_np, "fid", &ctx->fid);
	GET_OFNODE_U32(s_np, "r_cmd", &ctx->r_cmd);
	GET_OFNODE_U32(s_np, "w_cmd", &ctx->w_cmd);
	GET_OFNODE_U32(s_np, "u_cmd", &ctx->u_cmd);

	return ctx;

err_free:
	kfree(ctx);

	return ERR_PTR(ret);
}

struct regmap *__regmap_init_tee(struct device *dev, struct device_node *np,
				 void __iomem *regs,
				 const struct regmap_config *config,
				 struct lock_class_key *lock_key,
				 const char *lock_name)
{
	struct regmap_tee_context *ctx;

	ctx = regmap_tee_gen_context(dev, np, regs, config);
	if (IS_ERR(ctx))
		return ERR_CAST(ctx);

	return __regmap_init(dev, &regmap_tee, ctx, config, lock_key,
			     lock_name);
}
EXPORT_SYMBOL_GPL(__regmap_init_tee);

struct regmap *
__devm_regmap_init_tee(struct device *dev, struct device_node *np,
		       void __iomem *regs, const struct regmap_config *config,
		       struct lock_class_key *lock_key, const char *lock_name)
{
	struct regmap_tee_context *ctx;

	ctx = regmap_tee_gen_context(dev, np, regs, config);
	if (IS_ERR(ctx))
		return ERR_CAST(ctx);

	return __devm_regmap_init(dev, &regmap_tee, ctx, config, lock_key,
				  lock_name);
}
EXPORT_SYMBOL_GPL(__devm_regmap_init_tee);

MODULE_LICENSE("GPL v2");
