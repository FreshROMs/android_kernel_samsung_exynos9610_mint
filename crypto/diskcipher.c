/*
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/blkdev.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/crypto.h>
#include <crypto/algapi.h>
#include <crypto/diskcipher.h>
#include <linux/delay.h>
#include <linux/mm_types.h>
#include <linux/fs.h>
#include <linux/fscrypt.h>

#include "internal.h"

#ifdef CONFIG_CRYPTO_DISKCIPHER_DEBUG
#include <crypto/fmp.h>

#define DUMP_MAX 5

enum diskcipher_dbg { /* for DISKCIPHER_DEBUG */
	DISKC_API_ALLOC, DISKC_API_FREE, DISKC_API_FREEREQ,
	DISKC_API_SETKEY, DISKC_API_CLEARKEY,
	DISKC_API_SET, DISKC_API_GET, DISKC_API_GET_DISK,
	DISKC_API_CRYPT, DISKC_API_CLEAR,
	DISKC_INODE_SYNC_ERR, DISKC_NO_SYNC_ERR,
	DISKC_NO_DISKC_ERR, DISKC_NO_KEY_ERR,
	DISKC_MERGE_NO, DISKC_MERGE_DM, DISKC_MERGE, DISKC_MERGE_DUN,
	DISKC_USER_MAX
};

struct diskc_debug_info {
	int cnt[DISKC_USER_MAX][2];
};

static struct diskc_debug_info diskc_dbg;

static void crypto_diskcipher_debug(enum diskcipher_dbg api, bool err)
{
	struct diskc_debug_info *dbg = &diskc_dbg;

	dbg->cnt[api][err]++;
}

static void disckipher_log_show(struct seq_file *m)
{
	int i;
	struct diskc_debug_info *dbg = &diskc_dbg;
	char name[DISKC_USER_MAX][32] = {
		"ALLOC", "FREE", "FREEREQ",
		"SETKEY", "CLEARKEY",
		"SET", "GET", "GET-dm",
		"CRYPT", "CLEAR",
		"err_inode", "err_sync", "err_diskc", "err_no_key",
		"no-merge", "merge-dm", "merge", "merge-dun"};

	for (i = 0; i < DISKC_USER_MAX; i++)
		if (dbg->cnt[i][0] || dbg->cnt[i][1])
			seq_printf(m, "%s\t: %6u(err:%u)\n",
				name[i], dbg->cnt[i][0], dbg->cnt[i][1]);
}
#else
#define disckipher_log_show(a)			do { } while (0)
#define crypto_diskcipher_debug(a, b)		do { } while (0)
#endif

static int crypto_diskcipher_check(struct bio *bio)
{
	struct crypto_diskcipher *ci = NULL;
	struct inode *inode = NULL;
	struct page *page = NULL;
	struct address_space *mapping;

	if (!bio) {
		pr_err("%s: doesn't exist bio\n", __func__);
		return 0;
	}

	/* enc without fscrypt */
	ci = bio->bi_cryptd;
	if (!ci->inode)
		return 0;
	if (ci->algo == 0)
		return 0;

	page = bio->bi_io_vec[0].bv_page;
	if (!page || PageAnon(page))
		return 0;

        if (unlikely(PageSwapCache(page)))
               return 0;

	mapping = page_mapping(page);
	if(!mapping)
		return 0;

	if (!mapping->host || atomic_read(&ci->inode->i_dio_count))
		return 0;

	if (!mapping->host->i_crypt_info)
                return 0;

	inode = mapping->host;
	if (ci->inode != inode) {
		pr_err("%s: fails to invalid inode\n", __func__);
		return -EINVAL;
	}

	if (!fscrypt_has_encryption_key(inode)) {
		pr_err("%s: fails to invalid key\n", __func__);
		return -EINVAL;
	}

	ci = fscrypt_get_bio_cryptd(inode);
	if (!ci) {
		pr_err("%s: fails to invalid crypto info\n", __func__);
		return -EINVAL;

	} else if ((bio->bi_cryptd != ci) &&
			!(bio->bi_flags & REQ_OP_DISCARD)) {
		pr_err("%s: fails to async crypto info\n", __func__);
		return -EINVAL;
	}
	return 0;
}

struct crypto_diskcipher *crypto_diskcipher_get(struct bio *bio)
{
	struct crypto_diskcipher *diskc = NULL;

	if (!bio || !virt_addr_valid(bio)) {
		pr_err("%s: Invalid bio:%pK\n", __func__, bio);
		return NULL;
	}
	if (bio->bi_opf & REQ_CRYPT) {
		if (bio->bi_cryptd) {
			if (!crypto_diskcipher_check(bio)) {
				diskc = bio->bi_cryptd;
			} else {
				pr_err("%s: fail to check diskcipher bio:%pK\n",
						__func__, bio);
				diskc = ERR_PTR(-EINVAL);
			}
		} else {
			pr_err("%s: no diskcipher on bio:%pK\n",
					__func__, bio);
			diskc = ERR_PTR(-EINVAL);
		}
	}
	return diskc;
}

static inline void *bio_has_crypt(struct bio *bio)
{
	if (bio && (bio->bi_opf & REQ_CRYPT))
		return bio->bi_cryptd;

	return NULL;
}

bool crypto_diskcipher_blk_mergeble(struct bio *bio1, struct bio *bio2)
{
	if (!bio_has_crypt(bio1) && !bio_has_crypt(bio2))
		return true;

	if (bio_has_crypt(bio1) == bio_has_crypt(bio2)) {
		struct crypto_diskcipher *tfm1 = bio1->bi_cryptd;
		struct crypto_diskcipher *tfm2 = bio2->bi_cryptd;
#ifdef CONFIG_CRYPTO_DISKCIPHER_DEBUG
		struct inode *inode1 = tfm1->inode;
		struct inode *inode2 = tfm2->inode;

		if (inode1 != inode2)
			panic("%s: no same inode:%pK, %pK, tfm:%pK,%pK, bi_opf:%x,%x\n",
				__func__, inode1, inode2, tfm1, tfm2, bio1->bi_opf, bio2->bi_opf);

		if ((!inode1 && bio_dun(bio1)) || (!inode2 && bio_dun(bio2)))
			panic("%s: inval inode:%pK,%pK, tfm:%pK,%pK, bio:%pK,%pK, bi_opf:%x,%x\n",
				__func__, inode1, inode2, tfm1, tfm2, bio1, bio2, bio1->bi_opf, bio2->bi_opf);
#endif

		/* no inode for DM-crypt and DM-default-key */
		if (!tfm1->inode) {
			crypto_diskcipher_debug(DISKC_MERGE_DM, false);
			return true;
		}

		if ((tfm1->ivmode == IV_MODE_DUN) &&
			(tfm2->ivmode == IV_MODE_DUN)) {
			if (bio_dun(bio1) && bio_dun(bio2) &&
				(bio_end_dun(bio1) == bio_dun(bio2))) {
				crypto_diskcipher_debug(DISKC_MERGE_DUN, false);
				return true;
			}
		} else if ((tfm1->ivmode == IV_MODE_LBA) &&
			(tfm2->ivmode == IV_MODE_LBA)) {
			crypto_diskcipher_debug(DISKC_MERGE, false);
			return true;
		}
		crypto_diskcipher_debug(DISKC_MERGE_NO, false);
	}
	return false;
}

void crypto_diskcipher_set(struct bio *bio, struct crypto_diskcipher *tfm, u64 dun)
{
	if (bio && tfm) {
		bio->bi_opf |= REQ_CRYPT;
		bio->bi_cryptd = tfm;
		if (dun)
			bio->bi_iter.bi_dun = dun;
	}
	crypto_diskcipher_debug(DISKC_API_SET, false);
}

int crypto_diskcipher_setkey(struct crypto_diskcipher *tfm, const char *in_key,
				unsigned int key_len, bool persistent,
				const struct inode *inode)
{
	struct crypto_tfm *base = crypto_diskcipher_tfm(tfm);
	struct diskcipher_alg *cra = __crypto_diskcipher_alg(base->__crt_alg);
	int ret = -EINVAL;

	if (cra)
		ret = cra->setkey(tfm, in_key, key_len, persistent);
	else
		pr_err("%s: doesn't exist cra. base:%pK", __func__, base);

	tfm->inode = (struct inode *)inode;
	tfm->ivmode = IV_MODE_LBA;
	if (inode) {
		/* check the filesystem for fscrypt */
		if (inode->i_sb) {
			if (inode->i_sb->s_type) {
				if (!strcmp(inode->i_sb->s_type->name, "f2fs"))
					tfm->ivmode = IV_MODE_DUN;
			}
		}
	}
	crypto_diskcipher_debug(DISKC_API_SETKEY, ret ? true : false);

	return ret;
}

int crypto_diskcipher_clearkey(struct crypto_diskcipher *tfm)
{
	struct crypto_tfm *base = crypto_diskcipher_tfm(tfm);
	struct diskcipher_alg *cra = __crypto_diskcipher_alg(base->__crt_alg);
	int ret = -EINVAL;

	if (cra)
		ret = cra->clearkey(tfm);
	else
		pr_err("%s: doesn't exist cra. base:%pK", __func__, base);

	crypto_diskcipher_debug(DISKC_API_CLEARKEY, ret ? true : false);
	return ret;
}

int crypto_diskcipher_set_crypt(struct crypto_diskcipher *tfm, void *req)
{
	struct crypto_tfm *base = crypto_diskcipher_tfm(tfm);
	struct diskcipher_alg *cra = NULL;
	int ret = -EINVAL;

	if (!base) {
		pr_err("%s: doesn't exist base. tfm:%pK", __func__, tfm);
		goto out;
	}

	cra = __crypto_diskcipher_alg(base->__crt_alg);
	if (!cra) {
		pr_err("%s: doesn't exist cra. base:%pK\n", __func__, base);
		goto out;
	}

	ret = cra->crypt(tfm, req);
	if (ret)
		pr_err("%s fails ret:%d, cra:%pK\n", __func__, ret, cra);
out:
	crypto_diskcipher_debug(DISKC_API_CRYPT, ret ? true : false);
	return ret;
}

int crypto_diskcipher_clear_crypt(struct crypto_diskcipher *tfm, void *req)
{
	struct crypto_tfm *base = crypto_diskcipher_tfm(tfm);
	struct diskcipher_alg *cra = NULL;
	int ret = -EINVAL;

	if (!base) {
		pr_err("%s: doesn't exist base, tfm:%pK\n", __func__, tfm);
		goto out;
	}

	cra = __crypto_diskcipher_alg(base->__crt_alg);
	if (!cra) {
		pr_err("%s: doesn't exist cra. base:%pK\n", __func__, base);
		goto out;
	}

	ret = cra->clear(tfm, req);
	if (ret)
		pr_err("%s fails ret:%d, cra:%pK\n", __func__, ret, cra);
out:
	crypto_diskcipher_debug(DISKC_API_CLEAR, ret ? true : false);
	return ret;
}

int diskcipher_do_crypt(struct crypto_diskcipher *tfm,
			struct diskcipher_test_request *req)
{
	struct crypto_tfm *base = crypto_diskcipher_tfm(tfm);
	struct diskcipher_alg *cra = __crypto_diskcipher_alg(base->__crt_alg);
	int ret = -EINVAL;

	if (!cra) {
		pr_err("%s: doesn't exist cra. base:%pK\n", __func__, base);
		return ret;
	}

	if (cra->do_crypt)
		ret = cra->do_crypt(tfm, req);
	if (ret)
		pr_err("%s fails ret:%d", __func__, ret);
	return ret;
}

static int crypto_diskcipher_init_tfm(struct crypto_tfm *base)
{
	struct crypto_diskcipher *tfm = __crypto_diskcipher_cast(base);
	struct diskcipher_alg *alg = crypto_diskcipher_alg(tfm);

	if (alg->init)
		alg->init(tfm);
	return 0;
}

unsigned int crypto_diskcipher_extsize(struct crypto_alg *alg)
{
	return alg->cra_ctxsize +
		(alg->cra_alignmask & ~(crypto_tfm_ctx_alignment() - 1));
}

static void crypto_diskcipher_show(struct seq_file *m, struct crypto_alg *alg)
{
	seq_printf(m, "type         : diskcipher\n");
	disckipher_log_show(m);
}

static const struct crypto_type crypto_diskcipher_type = {
	.extsize = crypto_diskcipher_extsize,
	.init_tfm = crypto_diskcipher_init_tfm,
#ifdef CONFIG_PROC_FS
	.show = crypto_diskcipher_show,
#endif
	.maskclear = ~CRYPTO_ALG_TYPE_MASK,
	.maskset = CRYPTO_ALG_TYPE_MASK,
	.type = CRYPTO_ALG_TYPE_DISKCIPHER,
	.tfmsize = offsetof(struct crypto_diskcipher, base),
};

#define DISKC_NAME		"-disk"
#define DISKC_NAME_SIZE		(5)
#define DISKCIPHER_MAX_IO_MS	(1000)
struct crypto_diskcipher *crypto_alloc_diskcipher(const char *alg_name,
					u32 type, u32 mask, bool force)
{
	int alg_name_len;

	if (!force) {
		crypto_diskcipher_debug(DISKC_API_ALLOC, false);
		return crypto_alloc_tfm(alg_name,
				&crypto_diskcipher_type, type, mask);
	}

	alg_name_len = strlen(alg_name);
	if (alg_name_len + DISKC_NAME_SIZE < CRYPTO_MAX_ALG_NAME) {
		char diskc_name[CRYPTO_MAX_ALG_NAME];

		strcpy(diskc_name, alg_name);
		strcat(diskc_name, DISKC_NAME);
		crypto_diskcipher_debug(DISKC_API_ALLOC, false);
		return crypto_alloc_tfm(diskc_name,
				&crypto_diskcipher_type, type, mask);
	}
	crypto_diskcipher_debug(DISKC_API_ALLOC, true);
	return NULL;
}

void crypto_free_diskcipher(struct crypto_diskcipher *tfm)
{
	crypto_diskcipher_debug(DISKC_API_FREE, false);
	crypto_destroy_tfm(tfm, crypto_diskcipher_tfm(tfm));
}

int crypto_register_diskcipher(struct diskcipher_alg *alg)
{
	struct crypto_alg *base = &alg->base;

	base->cra_type = &crypto_diskcipher_type;
	base->cra_flags = CRYPTO_ALG_TYPE_DISKCIPHER;
	return crypto_register_alg(base);
}

void crypto_unregister_diskcipher(struct diskcipher_alg *alg)
{
	crypto_unregister_alg(&alg->base);
}

int crypto_register_diskciphers(struct diskcipher_alg *algs, int count)
{
	int i, ret;

	for (i = 0; i < count; i++) {
		ret = crypto_register_diskcipher(algs + i);
		if (ret)
			goto err;
	}
	return 0;

err:
	for (--i; i >= 0; --i)
		crypto_unregister_diskcipher(algs + i);
	return ret;
}

void crypto_unregister_diskciphers(struct diskcipher_alg *algs, int count)
{
	int i;

	for (i = count - 1; i >= 0; --i)
		crypto_unregister_diskcipher(algs + i);
}
