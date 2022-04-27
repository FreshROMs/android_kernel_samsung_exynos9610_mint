// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Exynos FMP crypt interface
 *
 * Copyright (C) 2018 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/bio.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <crypto/fmp.h>
#include <crypto/diskcipher.h>
#include "fmp/fmp_fips_fops.h"

int exynos_fmp_crypt_clear(struct bio *bio, void *table_addr)
{
	struct crypto_diskcipher *dtfm = crypto_diskcipher_get(bio);
	struct fmp_crypto_info *ci;
	struct fmp_request req;
	int ret = 0;

	if (unlikely(IS_ERR(dtfm))) {
		pr_warn("%s: fails to get crypt\n", __func__);
		return -EINVAL;
	} else if (dtfm) {
#ifdef CONFIG_EXYNOS_FMP_FIPS
	/* check fips flag. use fmp without diskcipher */
	if (!dtfm->algo) {
		req.table = table_addr;
		ret = exynos_fmp_clear((void *)dtfm, &req);
		if (ret) {
			pr_warn("%s: fails to clear fips\n",
			__func__);
			return ret;
		}
		return 0;
	}
#endif

		ci = crypto_tfm_ctx(crypto_diskcipher_tfm(dtfm));
		if (ci)
			if (ci->enc_mode == EXYNOS_FMP_FILE_ENC) {
				req.table = table_addr;
				ret = crypto_diskcipher_clear_crypt(dtfm, &req);
			}
	}
	if (ret)
		pr_err("%s: fail to config desc (bio, tfm, ci) ret:%d\n", __func__, ret);
	return ret;
}

int exynos_fmp_crypt_cfg(struct bio *bio, void *table_addr,
			u32 page_idx, u32 sector_unit)
{
	struct crypto_diskcipher *dtfm = crypto_diskcipher_get(bio);
	u64 iv;
	struct fmp_request req;
	int ret = 0;

	if (unlikely(IS_ERR(dtfm))) {
		pr_warn("%s: fails to get crypt\n", __func__);
		return -EINVAL;
	} else if (dtfm) {
		req.table = table_addr;
		req.cmdq_enabled = 0;
		req.iv = &iv;
		req.ivsize = sizeof(iv);
#ifdef CONFIG_EXYNOS_FMP_FIPS
		/* check fips flag. use fmp without diskcipher */
		if (!dtfm->algo) {
			if (exynos_fmp_crypt((void *)dtfm, &req))
				pr_warn("%s: fails to test fips\n", __func__);
			return 0;
		}
#endif
		iv = (dtfm->ivmode == IV_MODE_DUN) ? (bio_dun(bio) + page_idx) :
			(bio->bi_iter.bi_sector + (sector_t)sector_unit);
		ret = crypto_diskcipher_set_crypt(dtfm, &req);
		if (ret)
			pr_err("%s: fail to config desc (bio, tfm) ret:%d\n", __func__, ret);
		return ret;
	}

	exynos_fmp_bypass(table_addr, 0);
	return 0;
}

static int fmp_crypt(struct crypto_diskcipher *tfm, void *priv)
{
	struct fmp_crypto_info *ci = crypto_tfm_ctx(crypto_diskcipher_tfm(tfm));

	return exynos_fmp_crypt(ci, priv);
}

static int fmp_clear(struct crypto_diskcipher *tfm, void *priv)
{
	struct fmp_crypto_info *ci = crypto_tfm_ctx(crypto_diskcipher_tfm(tfm));

	return exynos_fmp_clear(ci, priv);
}

static int fmp_setkey(struct crypto_diskcipher *tfm, const char *in_key,
			u32 keylen, bool persistent)
{
	struct fmp_crypto_info *ci = crypto_tfm_ctx(crypto_diskcipher_tfm(tfm));

	return exynos_fmp_setkey(ci, (char *)in_key, keylen, persistent);
}

static int fmp_clearkey(struct crypto_diskcipher *tfm)
{
	struct fmp_crypto_info *ci = crypto_tfm_ctx(crypto_diskcipher_tfm(tfm));

	return exynos_fmp_clearkey(ci);
}

/* support crypto manager test without CRYPTO_MANAGER_DISABLE_TESTS */
static int fmp_do_test_crypt(struct crypto_diskcipher *tfm,
			  struct diskcipher_test_request *req)
{
	if (!req) {
		pr_err("%s: invalid parameter\n", __func__);
		return -EINVAL;
	}

	return exynos_fmp_test_crypt(crypto_tfm_ctx(crypto_diskcipher_tfm(tfm)),
		    req->iv, tfm->ivsize,
		    sg_virt(req->src), sg_virt(req->dst),
		    req->cryptlen, req->enc ? 1 : 0, tfm);
}

static inline void fmp_algo_init(struct crypto_tfm *tfm,
				 enum fmp_crypto_algo_mode algo)
{
	struct fmp_crypto_info *ci = crypto_tfm_ctx(tfm);
	struct crypto_diskcipher *diskc = __crypto_diskcipher_cast(tfm);
	struct diskcipher_alg *alg = crypto_diskcipher_alg(diskc);

	/* This field's stongly aligned 'fmp_crypto_info->use_diskc' */
	diskc->algo = (u32)algo;
	diskc->ivsize = FMP_IV_SIZE_16;
	ci->ctx = dev_get_drvdata(alg->dev);
	ci->algo_mode = algo;
}

static int fmp_aes_xts_init(struct crypto_tfm *tfm)
{
	fmp_algo_init(tfm, EXYNOS_FMP_ALGO_MODE_AES_XTS);
	return 0;
}

static int fmp_cbc_aes_init(struct crypto_tfm *tfm)
{
	fmp_algo_init(tfm, EXYNOS_FMP_ALGO_MODE_AES_CBC);
	return 0;
}

static struct diskcipher_alg fmp_algs[] = {{
	.base = {
		.cra_name = "xts(aes)-disk",
		.cra_driver_name = "xts(aes)-disk(fmp)",
		.cra_priority = 200,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct fmp_crypto_info),
		.cra_init = fmp_aes_xts_init,
	}
}, {
	.base = {
		.cra_name = "cbc(aes)-disk",
		.cra_driver_name = "cbc(aes)-disk(fmp)",
		.cra_priority = 200,
		.cra_module = THIS_MODULE,
		.cra_ctxsize = sizeof(struct fmp_crypto_info),
		.cra_init = fmp_cbc_aes_init,
	}
} };

#ifdef CONFIG_EXYNOS_FMP_FIPS
static const char pass[] = "passed";
static const char fail[] = "failed";

static ssize_t fmp_fips_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.overall ? pass : fail);
}

static ssize_t fmp_fips_aes_xts_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.aes_xts ? pass : fail);
}

static ssize_t fmp_fips_aes_cbc_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.aes_cbc ? pass : fail);
}

static ssize_t fmp_fips_sha256_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.sha256 ? pass : fail);
}

static ssize_t fmp_fips_hmac_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.hmac ? pass : fail);
}

static ssize_t fmp_fips_integrity_result_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct exynos_fmp *fmp = dev_get_drvdata(dev);

	return snprintf(buf, sizeof(pass), "%s\n", fmp->result.integrity ? pass : fail);
}

static DEVICE_ATTR(fmp_fips_status, 0444, fmp_fips_result_show, NULL);
static DEVICE_ATTR(aes_xts_status, 0444, fmp_fips_aes_xts_result_show, NULL);
static DEVICE_ATTR(aes_cbc_status, 0444, fmp_fips_aes_cbc_result_show, NULL);
static DEVICE_ATTR(sha256_status, 0444, fmp_fips_sha256_result_show, NULL);
static DEVICE_ATTR(hmac_status, 0444, fmp_fips_hmac_result_show, NULL);
static DEVICE_ATTR(integrity_status, 0444, fmp_fips_integrity_result_show, NULL);

static struct attribute *fmp_fips_attr[] = {
	&dev_attr_fmp_fips_status.attr,
	&dev_attr_aes_xts_status.attr,
	&dev_attr_aes_cbc_status.attr,
	&dev_attr_sha256_status.attr,
	&dev_attr_hmac_status.attr,
	&dev_attr_integrity_status.attr,
	NULL,
};

static struct attribute_group fmp_fips_attr_group = {
	.name	= "fmp-fips",
	.attrs	= fmp_fips_attr,
};

static int __nocfi fmp_fips_fops_open(struct inode *inode, struct file *file)
{
	return fmp_fips_open(inode, file);
}

static int __nocfi fmp_fips_fops_release(struct inode *inode, struct file *file)
{
	return fmp_fips_release(inode, file);
}

static long __nocfi fmp_fips_fops_ioctl(struct file *file, unsigned int cmd, unsigned long arg_)
{
	return fmp_fips_ioctl(file, cmd, arg_);
}

static long __nocfi fmp_fips_fops_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg_)
{
	return fmp_fips_compat_ioctl(file, cmd, arg_);
}

static const struct file_operations fmp_fips_fops = {
	.owner		= THIS_MODULE,
	.open		= fmp_fips_fops_open,
	.release	= fmp_fips_fops_release,
	.unlocked_ioctl = fmp_fips_fops_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= fmp_fips_fops_compat_ioctl,
#endif
};
#endif

static int exynos_fmp_probe(struct platform_device *pdev)
{
	struct diskcipher_alg *alg;
	struct exynos_fmp *fmp_ctx = exynos_fmp_init(pdev);
	int ret;
	int i;

	if (!fmp_ctx) {
		dev_err(&pdev->dev,
			"%s: Fail to register diskciphero\n", __func__);
		return -EINVAL;
	}
	dev_set_drvdata(&pdev->dev, fmp_ctx);

	for (i = 0; i < ARRAY_SIZE(fmp_algs); i++) {
		alg = &fmp_algs[i];
		alg->dev = &pdev->dev;
		alg->init = NULL;
		alg->setkey = fmp_setkey;
		alg->clearkey = fmp_clearkey;
		alg->crypt = fmp_crypt;
		alg->clear = fmp_clear;
		alg->do_crypt = fmp_do_test_crypt;
	}
	ret = crypto_register_diskciphers(fmp_algs, ARRAY_SIZE(fmp_algs));
	if (ret) {
		dev_err(&pdev->dev,
			"%s: Fail to register diskcipher. ret = %d\n",
			__func__, ret);
		exynos_fmp_exit(pdev);
		return -EINVAL;
	}

#ifdef CONFIG_EXYNOS_FMP_FIPS
	/* register FIPS ops */
	ret = sysfs_create_group(&pdev->dev.kobj, &fmp_fips_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "%s: Fail to create sysfs. ret(%d)\n",
				__func__, ret);
		crypto_unregister_diskciphers(fmp_algs, ARRAY_SIZE(fmp_algs));
		exynos_fmp_exit(pdev);
		return -EINVAL;
	}

	fmp_ctx->miscdev.fops = &fmp_fips_fops;
#endif

	dev_info(&pdev->dev, "Exynos FMP driver is registered to diskcipher\n");
	return 0;
}

static int exynos_fmp_remove(struct platform_device *pdev)
{
	void *drv_data = dev_get_drvdata(&pdev->dev);

	if (!drv_data) {
		pr_err("%s: Fail to get drvdata\n", __func__);
		return 0;
	}
	crypto_unregister_diskciphers(fmp_algs, ARRAY_SIZE(fmp_algs));
#ifdef CONFIG_EXYNOS_FMP_FIPS
	sysfs_remove_group(&pdev->dev.kobj, &fmp_fips_attr_group);
#endif
	exynos_fmp_exit(drv_data);
	return 0;
}

static const struct of_device_id exynos_fmp_match[] = {
	{ .compatible = "samsung,exynos-fmp" },
	{},
};

static struct platform_driver exynos_fmp_driver = {
	.driver = {
		   .name = "exynos-fmp",
		   .owner = THIS_MODULE,
		   .pm = NULL,
		   .of_match_table = exynos_fmp_match,
		   },
	.probe = exynos_fmp_probe,
	.remove = exynos_fmp_remove,
};

static int __init fmp_init(void)
{
	return platform_driver_register(&exynos_fmp_driver);
}
late_initcall(fmp_init);

static void __exit fmp_exit(void)
{
	platform_driver_unregister(&exynos_fmp_driver);
}
module_exit(fmp_exit);
MODULE_DESCRIPTION("Exynos Spedific crypto algo driver");
