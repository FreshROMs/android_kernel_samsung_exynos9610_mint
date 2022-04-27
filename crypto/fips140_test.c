#include <crypto/aead.h>
#include <crypto/hash.h>
#include <crypto/skcipher.h>
#include <crypto/drbg.h>
#include <crypto/kbkdf.h>
#include <linux/scatterlist.h>
#include <linux/err.h>

#include "fips140.h"
#include "fips140_test.h"

extern const cipher_test_suite_t aes_ecb_tv;
extern const cipher_test_suite_t aes_cbc_tv;
extern const aead_test_suite_t aes_gcm_tv;
extern const hash_test_suite_t sha1_tv;
extern const hash_test_suite_t sha224_tv;
extern const hash_test_suite_t sha256_tv;
extern const hash_test_suite_t sha384_tv;
extern const hash_test_suite_t sha512_tv;
extern const hash_test_suite_t hmac_sha1_tv;
extern const hash_test_suite_t hmac_sha224_tv;
extern const hash_test_suite_t hmac_sha256_tv;
extern const hash_test_suite_t hmac_sha384_tv;
extern const hash_test_suite_t hmac_sha512_tv;
extern const drbg_test_suite_t drbg_pr_hmac_sha256_tv;
extern const drbg_test_suite_t drbg_nopr_hmac_sha256_tv;
extern const kbkdf_test_suite_t kbkdf_hmac_sha512_ctr_nist_tv;

// TODO: to be removed
#pragma clang optimize off


#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
static char *fips_functest_mode;

static char *fips_functest_KAT_list[] = {
	"ecb(aes-generic)",
	"cbc(aes-generic)",
	"gcm_base(ctr(aes-generic),ghash-generic)",
	"ecb(aes-ce)",
	"cbc(aes-ce)",
	"gcm_base(ctr(aes-ce),ghash-generic)",
	"sha1-generic",
	"hmac(sha1-generic)",
	"sha1-ce",
	"hmac(sha1-ce)",
	"sha224-generic",
	"sha256-generic",
	"hmac(sha224-generic)",
	"hmac(sha256-generic)",
	"sha224-ce",
	"sha256-ce",
	"hmac(sha224-ce)",
	"hmac(sha256-ce)",
	"sha384-generic",
	"sha512-generic",
	"hmac(sha384-generic)",
	"hmac(sha512-generic)",
	"drbg_nopr_hmac_sha256",
	"drbg_pr_hmac_sha256",
	"kbkdf_hmac_sha512_ctr",
	"integrity"
};
static char *fips_functest_conditional_list[] = {
	"ndrng_crngt",
	"zeroization"
};

// This function is added to change fips_functest_KAT_num from tcrypt.c
void set_fips_functest_KAT_mode(const int num)
{
	if (num >= 0 && num < SKC_FUNCTEST_KAT_CASE_NUM)
		fips_functest_mode = fips_functest_KAT_list[num];
	else
		fips_functest_mode = SKC_FUNCTEST_NO_TEST;
}
EXPORT_SYMBOL_GPL(set_fips_functest_KAT_mode);

void set_fips_functest_conditional_mode(const int num)
{
	if (num >= 0 && num < SKC_FUNCTEST_CONDITIONAL_CASE_NUM)
		fips_functest_mode = fips_functest_conditional_list[num];
	else
		fips_functest_mode = SKC_FUNCTEST_NO_TEST;
}
EXPORT_SYMBOL_GPL(set_fips_functest_conditional_mode);

char *get_fips_functest_mode(void)
{
	if (fips_functest_mode)
		return fips_functest_mode;
	else
		return SKC_FUNCTEST_NO_TEST;
}
EXPORT_SYMBOL_GPL(get_fips_functest_mode);

#endif // CONFIG_CRYPTO_FIPS_FUNC_TEST

struct tcrypt_result {
	struct completion completion;
	int err;
};

static void crypt_complete(struct crypto_async_request *req, int err)
{
	struct tcrypt_result *res = req->data;

	if (err == -EINPROGRESS)
		return;

	res->err = err;
	complete(&res->completion);
}

static int __test_skcipher(struct crypto_skcipher *tfm,
							int enc,
							const cipher_testvec_t *tv)
{
#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
	const char *algo = crypto_tfm_alg_driver_name(crypto_skcipher_tfm(tfm));
#endif
	struct skcipher_request *req = NULL;
	struct tcrypt_result result;
	struct scatterlist sg_src;
	struct scatterlist sg_dst;
	int ret = -EINVAL;
	uint8_t* __out_buf = NULL;
	uint8_t* __in_buf = NULL;
	uint8_t* __iv = NULL;
	const uint8_t* __in = NULL;
	const uint8_t* __out = NULL;

	__out_buf = kmalloc(FIPS140_MAX_LEN_PCTEXT, GFP_KERNEL);
	__in_buf = kmalloc(FIPS140_MAX_LEN_PCTEXT, GFP_KERNEL);
	__iv = kmalloc(FIPS140_MAX_LEN_IV, GFP_KERNEL);

	if ((!__out_buf) ||
		(!__in_buf) ||
		(!__iv)) {
		ret = -ENOMEM;
		goto out;
	}

	__in = enc ? tv->ptext : tv->ctext;
	__out = enc ? tv->ctext : tv->ptext;

	memcpy(__in_buf, __in, tv->len);

	if (tv->iv_len) {
		memcpy(__iv, tv->iv, tv->iv_len);
	}
	else {
		memset(__iv, 0x00, FIPS140_MAX_LEN_IV);
	}

	init_completion(&result.completion);

	req = skcipher_request_alloc(tfm, GFP_KERNEL);
	if (!req) {
		goto out;
	}

	skcipher_request_set_callback(req,
									CRYPTO_TFM_REQ_MAY_BACKLOG,
									crypt_complete,
									&result);

#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
	if (!strcmp(algo, get_fips_functest_mode())) {
		unsigned char temp_key[512];

		memcpy(temp_key, tv->key, tv->klen);
		temp_key[0] += 1;
		ret = crypto_skcipher_setkey(tfm, temp_key, tv->klen);
	} else {
		ret = crypto_skcipher_setkey(tfm, tv->key, tv->klen);
	}
#else
	ret = crypto_skcipher_setkey(tfm, tv->key, tv->klen);
#endif

	if (ret) {
		goto out;
	}

	sg_init_one(&sg_src, __in_buf, tv->len);
	sg_init_one(&sg_dst, __out_buf, tv->len);

	skcipher_request_set_crypt( req,
								&sg_src,
								&sg_dst,
								tv->len,
								(void*)__iv);

	ret = enc ?	crypto_skcipher_encrypt(req) :
				crypto_skcipher_decrypt(req);
	switch (ret) {
	case 0:
		break;
	case -EINPROGRESS:
	case -EBUSY:
		wait_for_completion(&result.completion);
		reinit_completion(&result.completion);
		ret = result.err;
		if (!ret)
			break;

	default:
		goto out;
	}

	if(memcmp(__out_buf, __out, tv->len)) {
		ret = -EINVAL;
	}

out:
	skcipher_request_free(req);
	kzfree(__in_buf);
	kzfree(__out_buf);
	kzfree(__iv);

	return ret;
}

static int test_skcipher(const cipher_test_suite_t* tv,
			     	 	 const char *driver,
						 u32 type,
						 u32 mask)
{
	struct crypto_skcipher *tfm;
	int err = 0;
	int i = 0;
	const char *algo = NULL;

	tfm = crypto_alloc_skcipher(driver, type, mask);
	if (IS_ERR(tfm)) {
		pr_err("FIPS : skcipher allocation error");
		return PTR_ERR(tfm);
	}

	algo = crypto_tfm_alg_driver_name(crypto_skcipher_tfm(tfm));

	for (i = 0; i < tv->tv_count; i++) {
		err = __test_skcipher(tfm, FIPS140_TEST_ENCRYPT, &tv->vecs[i]);
		if (err) {
			pr_err("FIPS : %s, test %d encrypt failed, err=%d\n", algo, i, err);
			goto out;
		}
	}

	for (i = 0; i < tv->tv_count; i++) {
		err = __test_skcipher(tfm, FIPS140_TEST_DECRYPT, &tv->vecs[i]);
		if (err) {
			pr_err("FIPS : %s, test %d decrypt failed, err=%d\n", algo, i, err);
			goto out;
		}
	}

	pr_err("FIPS : self-tests for %s passed \n", algo);

out:
	crypto_free_skcipher(tfm);
	return err;
}

static int __test_aead(struct crypto_aead *tfm,
						int enc,
						const aead_testvec_t *tv)
{
    int ret = -EINVAL;
	uint8_t *__iv = NULL;
	uint8_t *__key = NULL;
	uint8_t* __out_buf = NULL;
	uint8_t* __in_buf = NULL;
	uint8_t* __assoc = NULL;
	uint8_t* __temp = NULL;
	const uint8_t* __out_ref = NULL;
	uint32_t __ilen = 0;
	uint32_t __out_ref_len = 0;
	uint32_t authsize = 0;
	uint32_t iv_len = 0;
	uint32_t sg_assoc_idx = 0;

	struct aead_request *req;
	struct scatterlist *sgin;
	struct scatterlist *sgout;
	struct crypto_wait wait;

#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
	const char *algo = crypto_tfm_alg_driver_name(crypto_aead_tfm(tfm));
#endif

	__iv = kzalloc(FIPS140_MAX_LEN_IV, GFP_KERNEL);
	__key = kzalloc(FIPS140_MAX_LEN_KEY, GFP_KERNEL);
	__in_buf = kzalloc(3*FIPS140_MAX_LEN_PCTEXT, GFP_KERNEL);
	__out_buf = kzalloc(3*FIPS140_MAX_LEN_PCTEXT, GFP_KERNEL);
	__assoc = kzalloc(FIPS140_MAX_LEN_PCTEXT, GFP_KERNEL);
	sgin = kzalloc(2*sizeof(*sgin), GFP_KERNEL);
	sgout = kzalloc(2*sizeof(*sgout), GFP_KERNEL);

	if (!__iv ||
		!__key ||
		!__in_buf ||
		!__out_buf ||
		!__assoc ||
		!sgin ||
		!sgout) {
		goto error_clean_resources;
	}

	iv_len = crypto_aead_ivsize(tfm);
	if (tv->iv_len)
		memcpy(__iv, tv->iv, iv_len);
	memcpy(__assoc, tv->assoc, tv->alen);
	memcpy(__key, tv->key, tv->klen);

	// Pass wrong key for functional tests
	// Test case : gcm(aes)
#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
	if (!strcmp(algo, get_fips_functest_mode())) {
		__key[0] += 1;
	}
#endif

	if (enc) {
		memcpy(__in_buf, tv->input, tv->ilen);
		__ilen = tv->ilen;
		__out_ref = tv->result;
		__out_ref_len = tv->rlen;
	} else {
		memcpy(__out_buf, tv->result, tv->rlen);
		__temp = __in_buf;
		__in_buf = __out_buf;
		__out_buf = __temp;
		__ilen = tv->rlen;
		__out_ref = tv->input;
 		__out_ref_len = tv->ilen;
	}

	crypto_init_wait(&wait);

	req = aead_request_alloc(tfm, GFP_KERNEL);
	if (!req)
		goto error_clean_resources;

	aead_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG,
					  crypto_req_done, &wait);

	crypto_aead_clear_flags(tfm, ~0);

	ret = crypto_aead_setkey(tfm, __key, tv->klen);
	if (ret) {
		ret = -EINVAL;
		goto error_clean_resources;
	}

	authsize = abs(__ilen - __out_ref_len);
	ret = crypto_aead_setauthsize(tfm, authsize);
	if (ret)
		goto error_clean_resources;

	sg_init_table(sgin, 2);
	sg_init_table(sgout, 2);

	sg_assoc_idx = !!tv->alen;
	sg_set_buf(&sgin[0], __assoc, tv->alen);
	sg_set_buf(&sgin[sg_assoc_idx], __in_buf, __ilen);

	sg_set_buf(&sgout[0], __assoc, tv->alen);
	sg_set_buf(&sgout[sg_assoc_idx], __out_buf, __out_ref_len);

	aead_request_set_crypt(req, sgin, sgout, __ilen, __iv);
	aead_request_set_ad(req, tv->alen);
	ret = crypto_wait_req(enc ? crypto_aead_encrypt(req)
					      : crypto_aead_decrypt(req), &wait);
	switch (ret) {
	case 0:
		break;
	default:
		goto error_clean_resources;
	}

	if (memcmp(__out_buf, __out_ref, __out_ref_len))
		ret = -EINVAL;

error_clean_resources:
	kzfree(sgin);
	kzfree(sgout);
	aead_request_free(req);
	kzfree(__iv);
	kzfree(__key);
	kzfree(__in_buf);
	kzfree(__out_buf);
	kzfree(__assoc);
	return ret;
}

static int test_aead(const aead_test_suite_t* tv,
			     	 	 const char *driver,
						 u32 type,
						 u32 mask)
{
	struct crypto_aead *tfm;
	int err = 0;
	int i = 0;
	const char *algo = NULL;

	tfm = crypto_alloc_aead(driver, type, mask);
	if (IS_ERR(tfm)) {
		pr_err("FIPS : aead allocation error");
		return PTR_ERR(tfm);
	}

	algo = crypto_tfm_alg_driver_name(crypto_aead_tfm(tfm));

	for (i = 0; i < tv->tv_count; i++) {
		err = __test_aead(tfm, FIPS140_TEST_ENCRYPT, &tv->vecs[i]);
		if (err) {
			pr_err("FIPS : %s, test encrypt %d failed, err=%d\n", algo, i, err);
			goto out;
		}
	}

	for (i = 0; i < tv->tv_count; i++) {
		err = __test_aead(tfm, FIPS140_TEST_DECRYPT, &tv->vecs[i]);
		if (err) {
			pr_err("FIPS : %s, test decrypt %d failed, err=%d\n", algo, i, err);
			goto out;
		}
	}

	pr_err("FIPS : self-tests for %s passed \n", algo);

out:
	crypto_free_aead(tfm);
	return err;
}

static int test_hash(const hash_test_suite_t* tv,
					const char *driver,
					u32 type,
					u32 mask)
{
	struct crypto_shash* tfm = NULL;
	struct shash_desc* shash_desc = NULL;
	int err = 0;
	int i = 0;
	int size = 0;
	uint8_t* __digest_buf = NULL;
	uint32_t __digest_len = 0;
	const char* __ptext = NULL;
	const char *algo = driver;

	__digest_buf = kmalloc(FIPS140_MAX_LEN_DIGEST, GFP_KERNEL);
	if(!__digest_buf) {
		return -ENOMEM;
	}

	tfm = crypto_alloc_shash(driver, 0, 0);
	if (IS_ERR(tfm)) {
		err = -EINVAL;
		tfm = NULL;
		pr_err("FIPS : shash allocation error");
		goto out;
	}

	algo = crypto_tfm_alg_driver_name(crypto_shash_tfm(tfm));

	size = sizeof(struct shash_desc) + crypto_shash_descsize(tfm);
	shash_desc = kmalloc(size, GFP_KERNEL);
	if (!shash_desc) {
		shash_desc = NULL;
		err = -ENOMEM;
		goto out;
	}

	shash_desc->tfm = tfm;
	shash_desc->flags = 0x0;
	__digest_len = crypto_shash_digestsize(tfm);

	for(i = 0; i < tv->tv_count; i++) {
		if(tv->vecs[i].klen) {
			err = crypto_shash_setkey(tfm, tv->vecs[i].key, tv->vecs[i].klen);
			if (err) {
				goto out;
			}
		}

		err = crypto_shash_init(shash_desc);
		if (err) {
			goto out;
		}

		__ptext = tv->vecs[i].ptext;
#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
		if (!strcmp(algo, get_fips_functest_mode())) {
			uint8_t func_buf[1024];
			uint32_t func_buf_len = sizeof(func_buf);

			if (sizeof(func_buf) < tv->vecs[i].plen)
				func_buf_len = sizeof(func_buf);
			else
				func_buf_len = tv->vecs[i].plen;


			memcpy(func_buf, tv->vecs[i].ptext, func_buf_len);
			func_buf[0] = func_buf[0] + 1;
			__ptext = func_buf;
		}
#endif
		err = crypto_shash_update(shash_desc, __ptext, tv->vecs[i].plen);
		if (err) {
			goto out;
		}

		err = crypto_shash_final(shash_desc, __digest_buf);
		if (err) {
			goto out;
		}

		if(memcmp(__digest_buf, tv->vecs[i].digest, __digest_len)) {
			err = -EINVAL;
			goto out;
		}
	}

	err = 0;

out:
	if (err) {
		pr_err("FIPS : %s, test %d failed, err=%d\n", algo, i, err);
	}
	else {
		pr_err("FIPS : self-tests for %s passed \n", algo);
	}

	if(tfm)
		crypto_free_shash(tfm);

	if(shash_desc)
		kzfree(shash_desc);

	if(__digest_buf)
		kzfree(__digest_buf);

	return err;
}

static int __test_drbg(const drbg_testvec_t *tv,
						int pr,
						const char *driver,
						u32 type,
						u32 mask)
{
	int ret = -EAGAIN;
	struct crypto_rng *drng;
	struct drbg_test_data test_data;
	struct drbg_string addtl, pers, testentropy;
	unsigned char *buf = kzalloc(tv->expectedlen, GFP_KERNEL);

	if (!buf)
		return -ENOMEM;

	drng = crypto_alloc_rng(driver, type, mask);
	if (IS_ERR(drng)) {
		kzfree(buf);
		return -ENOMEM;
	}

	test_data.testentropy = &testentropy;
	// Pass wrong entropy for functional tests
	// Test case : drbg
	#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
	if (!strcmp(driver, get_fips_functest_mode())) {
		unsigned char temp_buf[FIPS140_MAX_LEN_ENTROPY];

		memcpy(temp_buf, tv->entropy, tv->entropylen);
		temp_buf[0] += 1;
		drbg_string_fill(&testentropy, temp_buf, tv->entropylen);
	} else {
		drbg_string_fill(&testentropy, tv->entropy, tv->entropylen);
	}
	#else
	drbg_string_fill(&testentropy, tv->entropy, tv->entropylen);
	#endif

	drbg_string_fill(&pers, tv->pers, tv->perslen);
	ret = crypto_drbg_reset_test(drng, &pers, &test_data);
	if (ret) {
		goto outbuf;
	}

	drbg_string_fill(&addtl, tv->addtla, tv->addtllen);
	if (pr) {
		drbg_string_fill(&testentropy, tv->entpra, tv->entprlen);
		ret = crypto_drbg_get_bytes_addtl_test(drng,
			buf, tv->expectedlen, &addtl,	&test_data);
	} else {
		ret = crypto_drbg_get_bytes_addtl(drng,
			buf, tv->expectedlen, &addtl);
	}
	if (ret < 0) {
		goto outbuf;
	}

	drbg_string_fill(&addtl, tv->addtlb, tv->addtllen);
	if (pr) {
		drbg_string_fill(&testentropy, tv->entprb, tv->entprlen);
		ret = crypto_drbg_get_bytes_addtl_test(drng,
			buf, tv->expectedlen, &addtl, &test_data);
	} else {
		ret = crypto_drbg_get_bytes_addtl(drng,
			buf, tv->expectedlen, &addtl);
	}
	if (ret < 0) {
		goto outbuf;
	}

	ret = memcmp(tv->expected, buf, tv->expectedlen);

outbuf:
	crypto_free_rng(drng);
	kzfree(buf);
	return ret;
}

static int test_drbg(const drbg_test_suite_t *desc,
					const char *driver,
					u32 type,
					u32 mask)
{
	int err = 0;
	int pr = 0;
	int i = 0;
	const drbg_testvec_t *template = desc->vecs;
	unsigned int tcount = desc->tv_count;

	if (0 == memcmp(driver, "drbg_pr_", 8))
		pr = 1;

	for (i = 0; i < tcount; i++) {
		err = __test_drbg(&template[i], pr, driver, type, mask);
		if (err) {
			pr_err("FIPS : %s, test %d failed, err=%d\n", driver, i, err);
			err = -EINVAL;
			break;
		}
	}

	if(!err) {
		pr_err("FIPS : self-tests for %s passed \n", driver);
	}

	return err;
}

int test_kbkdf(const kbkdf_test_suite_t *tv)
{
	int ret = 0;
	uint32_t i = 0;
	uint32_t L_calc = 0;
	size_t ki_length = 0;
	uint8_t Ko_calc[MAX_KDF_OUTPUT_LENGTH_BYTES] = {0};

	for(i = 0; i < tv->tv_count; i++) {
		L_calc = tv->vecs[i].L;
		ki_length = tv->vecs[i].KiLength;

#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
		if (!strcmp("kbkdf_hmac_sha512_ctr", get_fips_functest_mode()))
			ki_length = 1;
#endif

		ret = crypto_calc_kdf_hmac_sha512_ctr(
				tv->vecs[i].mode,
				tv->vecs[i].rlen,
				tv->vecs[i].Ki,
				ki_length,
				Ko_calc,
				&L_calc,
				tv->vecs[i].Label,
				tv->vecs[i].LabelLength,
				tv->vecs[i].Context,
				tv->vecs[i].ContextLength);

		if (ret) {
			pr_err("FIPS : kbkdf_hmac_sha512_ctr, test %d internal error, err=%d\n", i, ret);
			return ret;
		}

		if (memcmp(tv->vecs[i].Ko,
					Ko_calc,
					tv->vecs[i].L)) {
			pr_err("FIPS : kbkdf_hmac_sha512_ctr, test %d failed \n", i);
			return -EINVAL;
		}
	}

	pr_err("FIPS : self-tests for kbkdf_hmac_sha512_ctr passed \n");
	return ret;
}

int fips140_kat(void)
{
	int ret = 0;

#ifdef CONFIG_CRYPTO_AES
	ret += test_skcipher(&aes_cbc_tv, "cbc(aes-generic)", 0, 0);
	ret += test_skcipher(&aes_ecb_tv, "ecb(aes-generic)", 0, 0);
	#ifdef CONFIG_CRYPTO_GCM
	ret += test_aead(&aes_gcm_tv, "gcm_base(ctr(aes-generic),ghash-generic)", 0, 0);
	#endif
#endif

#ifdef CONFIG_CRYPTO_AES_ARM64_CE
	ret += test_skcipher(&aes_ecb_tv, "ecb(aes-ce)", 0, 0);
	ret += test_skcipher(&aes_cbc_tv, "cbc(aes-ce)", 0, 0);
	#ifdef CONFIG_CRYPTO_GCM
	ret += test_aead(&aes_gcm_tv, "gcm_base(ctr(aes-ce),ghash-generic)", 0, 0);
	#endif
#endif

#ifdef CONFIG_CRYPTO_SHA1
	ret += test_hash(&sha1_tv, "sha1-generic", 0, 0);
	ret += test_hash(&hmac_sha1_tv, "hmac(sha1-generic)", 0, 0);
#endif

#ifdef CONFIG_CRYPTO_SHA1_ARM64_CE
	ret += test_hash(&sha1_tv, "sha1-ce", 0, 0);
	ret += test_hash(&hmac_sha1_tv, "hmac(sha1-ce)", 0, 0);
#endif

#ifdef CONFIG_CRYPTO_SHA256
	ret += test_hash(&sha224_tv, "sha224-generic", 0, 0);
	ret += test_hash(&sha256_tv, "sha256-generic", 0, 0);
	ret += test_hash(&hmac_sha224_tv, "hmac(sha224-generic)", 0, 0);
	ret += test_hash(&hmac_sha256_tv, "hmac(sha256-generic)", 0, 0);
#endif

#ifdef CONFIG_CRYPTO_SHA2_ARM64_CE
	ret += test_hash(&sha224_tv, "sha224-ce", 0, 0);
	ret += test_hash(&sha256_tv, "sha256-ce", 0, 0);
	ret += test_hash(&hmac_sha224_tv, "hmac(sha224-ce)", 0, 0);
	ret += test_hash(&hmac_sha256_tv, "hmac(sha256-ce)", 0, 0);
#endif

#ifdef CONFIG_CRYPTO_SHA512
	ret += test_hash(&sha384_tv, "sha384-generic", 0, 0);
	ret += test_hash(&sha512_tv, "sha512-generic", 0, 0);
	ret += test_hash(&hmac_sha384_tv, "hmac(sha384-generic)", 0, 0);
	ret += test_hash(&hmac_sha512_tv, "hmac(sha512-generic)", 0, 0);
#endif

#if defined CONFIG_CRYPTO_DRBG_HMAC
	ret += test_drbg(&drbg_nopr_hmac_sha256_tv, "drbg_nopr_hmac_sha256", 0, 0);
	ret += test_drbg(&drbg_pr_hmac_sha256_tv, "drbg_pr_hmac_sha256", 0, 0);
#endif

#ifdef CONFIG_CRYPTO_KBKDF_CTR_HMAC_SHA512
	ret += test_kbkdf(&kbkdf_hmac_sha512_ctr_nist_tv);
#endif

	return ret;
}

// TODO: to be removed
#pragma clang optimize on

