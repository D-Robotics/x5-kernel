//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 ARM Technology (China) Co., Ltd.
 */

#include <linux/sysfs.h>
#include <linux/device.h>
#include "lca_te_otp.h"
#include "driver/te_drv_otp.h"

#define MAX(a,b,c) ((a)>(b)?((a)>(c)?(a):(c)):((b)>(c)?(b):(c)))
#define MAX_BASIC_OTP_PART_SIZE (32)
#define BASIC_OTP_PART_NUM (7)
#define EXT_OTP_PART_NUM (3)

struct otp_info {
	const char *name;
	size_t len;
	bool bext;
};

static const struct otp_info basic_otp[BASIC_OTP_PART_NUM] = {
	/* basic otp info, the len is constant */
	{
		.name = "model_id",
		.len = 4,
	},
	{
		.name = "model_key",
		.len = 16,
		.bext = true,
	},
	{
		.name = "device_id",
		.len = 4,
	},
	{
		.name = "dev_root_key",
		.len = 16,
		.bext = true,
	},
	{
		.name = "secboot_pubkey_hash",
		.len = 32,
	},
	{
		.name = "life_cycle",
		.len = 4,
	},
	{
		.name = "lock_control",
		.len = 4,
	}
};

static struct otp_info ext_otp[EXT_OTP_PART_NUM] = {
	/* ext otp info, the len will be updated */
	{
		.name = "usr_non_sec_region",
		.len = 0,
	},
	{
		.name = "usr_sec_region",
		.len = 0,
	},
	{
		.name = "test_region",
		.len = 0,
	}
};


static ssize_t otp_show(struct device *, struct device_attribute *, char *);

static struct device_attribute dev_attr_otp = __ATTR(otp, 0444, otp_show, NULL);

static ssize_t otp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int rc = 0;
	char *p = buf;
	struct te_drvdata *drvdata = (struct te_drvdata *)dev->driver_data;
	te_crypt_drv_t *te_drv;
	struct te_otp_conf otp_conf;
	int bytes_remain = PAGE_SIZE;
	int bytes = 0;
	int i, j;
	unsigned char *q;
	size_t q_size, offset;
	size_t otp_ext_size;
	int flen = 0;

	te_drv = te_drv_get(drvdata->h, TE_DRV_TYPE_OTP);
	if(NULL == te_drv) {
		return 0;
	}

	pm_runtime_get_sync(dev);

	rc = te_otp_get_conf((te_otp_drv_t *)te_drv, &otp_conf);
	if(rc != TE_SUCCESS || !otp_conf.otp_exist) {
		goto fail0;
	}
	otp_ext_size = MAX(otp_conf.otp_ns_sz,
			   otp_conf.otp_s_sz, otp_conf.otp_tst_sz);
	q_size = ((MAX_BASIC_OTP_PART_SIZE > otp_ext_size) ?
			MAX_BASIC_OTP_PART_SIZE : otp_ext_size);

	q = kmalloc(q_size, GFP_KERNEL);
	if(NULL == q) {
		goto fail0;
	}
	/* get basic otp info */
	bytes = 0;
	offset = 0;
	for(i = 0; i < BASIC_OTP_PART_NUM; i++) {
		flen = basic_otp[i].len;
		bytes += scnprintf(p + bytes, bytes_remain - bytes, "%s:",
			basic_otp[i].name);
		/* update size of model key and device root key */
		if (((strcasecmp("model_key", basic_otp[i].name) == 0) ||
		     (strcasecmp("dev_root_key", basic_otp[i].name) == 0)) &&
		     basic_otp[i].bext) {
			flen = otp_conf.otp_skey_sz;
		}

		rc = te_otp_read((te_otp_drv_t *)te_drv, offset, q, flen);
		offset += flen;
		if(TE_SUCCESS == rc) {
			for(j = 0; j < flen; j++) {
				if(j == flen - 1) {
					bytes += scnprintf(p + bytes,
						bytes_remain - bytes,
						"%02x\r\n", q[j]);
				} else {
					bytes += scnprintf(p + bytes,
						bytes_remain - bytes,
						"%02x", q[j]);
				}
			}
		} else {
			bytes += scnprintf(p + bytes, bytes_remain - bytes,
				 "%s\r\n", "N/A");
		}
	}

	/* get ext otp info */
	ext_otp[0].len = otp_conf.otp_ns_sz;
	ext_otp[1].len = otp_conf.otp_s_sz;
	ext_otp[2].len = otp_conf.otp_tst_sz;
	for(i = 0; i < EXT_OTP_PART_NUM; i++) {
		bytes += scnprintf(p + bytes, bytes_remain - bytes, "%s:",
				ext_otp[i].name);
		rc = te_otp_read((te_otp_drv_t *)te_drv, offset, q,
				 ext_otp[i].len);

		offset += ext_otp[i].len;
		if(TE_SUCCESS == rc && ext_otp[i].len > 0) {
			for(j = 0; j < ext_otp[i].len; j++) {
				if(j == ext_otp[i].len - 1)
					bytes += scnprintf(p + bytes,
						bytes_remain - bytes,
						"%02x\r\n", q[j]);
				else
					bytes += scnprintf(p + bytes,
						bytes_remain - bytes,
						"%02x", q[j]);
			}
		} else {
			bytes += scnprintf(p + bytes, bytes_remain - bytes,
					"%s\r\n", "N/A");
		}
	}

	kfree(q);
fail0:
	pm_runtime_put_autosuspend(dev);
	te_drv_put(drvdata->h, TE_DRV_TYPE_OTP);
	return bytes;
}

static struct attribute *lca_te_sysfs_entries[] = {
	&dev_attr_otp.attr,
	NULL,
};

/* /sys/devices/platform/amba/{addr}.{name}/otp/otp */
static struct attribute_group otp_attribute_group = {
	.name = "otp",		/* put in device directory */
	.attrs = lca_te_sysfs_entries,
};

static struct device *__dev = NULL;
int lca_te_otp_alloc(struct te_drvdata *drvdata)
{
	int rc = 0;

	__dev = drvdata_to_dev(drvdata);

	rc = sysfs_create_group(&__dev->kobj, &otp_attribute_group);
	if (rc) {
		dev_err(__dev, "could not create sysfs device attributes\n");
	}

	return rc;
}

int lca_te_otp_free(struct te_drvdata *drvdata)
{
	int rc = 0;
	struct device *dev = drvdata_to_dev(drvdata);

	sysfs_remove_group(&dev->kobj, &otp_attribute_group);
	__dev = NULL;
	return rc;
}

int lca_te_otp_read(size_t offset, uint8_t *buf, size_t len)
{
	int rc = 0;
	struct te_drvdata *drvdata;
	te_crypt_drv_t *te_drv;

	if(NULL == __dev) {
		return -ENXIO;
	}
	drvdata = (struct te_drvdata *)__dev->driver_data;
	te_drv = te_drv_get(drvdata->h, TE_DRV_TYPE_OTP);
	if(NULL == te_drv) {
		return -ENXIO;
	}

	pm_runtime_get_sync(__dev);
	rc = te_otp_read((te_otp_drv_t *)te_drv, offset, buf, len);
	pm_runtime_put_autosuspend(__dev);

	te_drv_put(drvdata->h, TE_DRV_TYPE_OTP);
	return TE2ERRNO(rc);
}

EXPORT_SYMBOL_GPL(lca_te_otp_read);

