/*
 * Copyright (C) 2016 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/semaphore.h>
#include <linux/blkdev.h>
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <crypto/diskcipher.h>
#include <crypto/fmp.h>
#include <crypto/smu.h>

#include "dw_mmc.h"
#include "dw_mmc-exynos.h"
#include "../core/queue.h"

static inline void exynos_mmc_smu_entry0_init(struct dw_mci *host)
{
	mci_writel(host, MPSBEGIN0, 0);
	mci_writel(host, MPSEND0, 0xffffffff);
	mci_writel(host, MPSLUN0, 0xff);
	mci_writel(host, MPSCTRL0, DWMCI_MPSCTRL_BYPASS);
}

int exynos_mmc_smu_init(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (!priv || (priv->smu == SMU_ID_MAX)) {
		exynos_mmc_smu_entry0_init(host);
		return 0;
	}

	dev_info(host->dev, "%s with id:%d\n", __func__, priv->smu);
	return exynos_smu_init(priv->smu, SMU_INIT);
}

int exynos_mmc_smu_resume(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	int fmp_id;

	if (!priv)
		return 0;

	if (priv->smu < SMU_ID_MAX)
		fmp_id = priv->smu;
	else if (priv->fmp < SMU_ID_MAX)
		fmp_id = priv->fmp;
	else {
		exynos_mmc_smu_entry0_init(host);
		return 0;
	}

	dev_info(host->dev, "%s with id:%d\n", __func__, fmp_id);
	return exynos_smu_resume(fmp_id);
}

int exynos_mmc_smu_abort(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (!priv || (priv->smu == SMU_ID_MAX))
		return 0;

	dev_info(host->dev, "%s with id:%d\n", __func__, priv->smu);
	return exynos_smu_abort(priv->smu, SMU_ABORT);
}

#ifdef CONFIG_MMC_DW_EXYNOS_FMP
int exynos_mmc_fmp_sec_cfg(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	if (!priv || (priv->fmp == SMU_ID_MAX))
		return 0;

	if (priv->fmp != SMU_EMBEDDED)
		dev_err(host->dev, "%s is fails id:%d\n",
				__func__, priv->fmp);


	dev_info(host->dev, "%s with id:%d\n", __func__, priv->fmp);
	return exynos_fmp_sec_config(priv->fmp);
}

static struct bio *get_bio(struct dw_mci *host,
				struct mmc_data *data, bool cmdq_enabled)
{
	struct bio *bio = NULL;
	struct dw_mci_exynos_priv_data *priv;

	if (!host || !data) {
		pr_err("%s: Invalid MMC:%p data:%p\n", __func__, host, data);
		return NULL;
	}

	priv = host->priv;
	if (priv->fmp == SMU_ID_MAX)
		return NULL;

	if (cmdq_enabled) {
		pr_err("%s: no support cmdq\n", __func__, host, data);
	} else {
		struct mmc_queue_req *mq_rq;
		struct mmc_blk_request *brq = container_of(data, struct mmc_blk_request, data);
		struct request *req;

		if (!brq)
			return NULL;

		mq_rq = container_of(brq, struct mmc_queue_req, brq);

		if (!mq_rq || !virt_addr_valid(mq_rq))
				return NULL;

		req = mmc_queue_req_to_req(mq_rq);
		if (req && virt_addr_valid(req))
			bio = req->bio;
	}

	return bio;
}

int exynos_mmc_fmp_cfg(struct dw_mci *host,
		       void *desc,
		       struct mmc_data *mmc_data,
		       struct page *page, int sector_offset, bool cmdq_enabled)
{
	struct fmp_request req;
	struct bio *bio = get_bio(host, mmc_data, cmdq_enabled);
	struct crypto_diskcipher *dtfm;
	sector_t iv;

	if (!bio)
		goto no_crypto;

	/* fill fmp_data_setting */
	dtfm = crypto_diskcipher_get(bio);
	if (dtfm) {
		iv = bio->bi_iter.bi_sector + (sector_t)sector_offset;
		req.table = desc;
		req.cmdq_enabled = 0;
		req.iv = &iv;
		req.ivsize = sizeof(iv);

#ifdef CONFIG_EXYNOS_FMP_FIPS
		/* check fips flag. use fmp without diskcipher */
		if (!dtfm->algo) {
			if (exynos_fmp_crypt((void *)dtfm, &req))
				goto no_crypto;
			return 0;
		}
#endif
		if (crypto_diskcipher_set_crypt(dtfm, &req)) {
			pr_warn("%s: fails to set crypt\n", __func__);
			return -EINVAL;
		}
		return 0;
	}
no_crypto:
	exynos_fmp_bypass(desc, cmdq_enabled);
	return 0;
}

int exynos_mmc_fmp_clear(struct dw_mci *host, void *desc, bool cmdq_enabled)
{
	int ret = 0;
	struct bio *bio = get_bio(host, host->data, cmdq_enabled);
	struct fmp_request req;
	struct crypto_diskcipher *dtfm;
	struct fmp_crypto_info *ci;

	if (!bio)
		return 0;

	dtfm = crypto_diskcipher_get(bio);
	if (dtfm) {
		req.table = desc;
#ifdef CONFIG_EXYNOS_FMP_FIPS
		/* check fips flag. use fmp without diskcipher */
		if (!dtfm->algo) {
			ci = (struct fmp_crypto_info *)dtfm;

			if (ci && (ci->enc_mode == EXYNOS_FMP_FILE_ENC))
				ret = exynos_fmp_clear((void *)dtfm, &req);
			if (ret)
				pr_err("%s: Fail to clear desc for fips (%d)\n",
					__func__, ret);
			return ret;
		}
#endif
		/* clear key on descrptor */
		ci = crypto_tfm_ctx(crypto_diskcipher_tfm(dtfm));
		if (ci) {
			if (ci->enc_mode == EXYNOS_FMP_FILE_ENC)
				ret = crypto_diskcipher_clear_crypt(dtfm, &req);
		} else {
			pr_err("%s: Fail to get ci (%p)\n", __func__, ci);
			return -EINVAL;
		}
	}

	if (ret)
		pr_err("%s: Fail to clear desc (%d)\n", __func__, ret);
	return ret;
}
#endif
