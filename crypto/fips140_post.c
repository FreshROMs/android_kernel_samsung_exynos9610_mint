
#include <crypto/hash.h>
#include <crypto/rng.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/module.h>
#include "fips140.h"
#include "fips140_test.h"

// TODO: to be removed
#pragma clang optimize off

static int __init fips140_post(void)
{
	int err = -ENOMEM;

	pr_info("FIPS : POST (%s)\n", SKC_VERSION_TEXT);
	err = fips140_kat();

	if (err) {
		pr_err("FIPS : POST - one or more algorithm tests failed\n");
		set_in_fips_err();
	} else {
		pr_info("FIPS : POST - Algorithm Tests Passed\n");
		if (do_integrity_check() != 0) {
#ifndef CONFIG_FUNCTION_TRACER
			pr_err("FIPS : POST - Integrity Check Failed\n");
			set_in_fips_err();
#else
			pr_err("FIPS : POST - Integrity Check bypassed due to ftrace debug mode\n");
#endif
		} else {
			pr_info("FIPS : POST - Integrity Check Passed\n");
		}
		if (in_fips_err())
			pr_err("FIPS : POST - CRYPTO API in FIPS Error\n");
		else
			pr_info("FIPS : POST - CRYPTO API started in FIPS approved mode\n");
	}

	return err;
}

// When SKC_FUNC_TEST is defined, this function will be called instead of tcrypt_mode_init
// tcyprt_mode_init will be called as test case number
// after all tests are done, the normal POST test will start
#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
static int __init fips140_post_func_test(void)
{
	int i;
	struct crypto_ahash *tfm;
	struct crypto_rng *rng;

	pr_info("FIPS FUNC : Functional test start\n");

	for (i = 0; i < SKC_FUNCTEST_KAT_CASE_NUM; i++) {
		set_fips_functest_KAT_mode(i);
		pr_info("FIPS FUNC : --------------------------------------------------\n");
		pr_info("FIPS FUNC : Failure inducement case %d - [%s]\n", i + 1, get_fips_functest_mode());
		pr_info("FIPS FUNC : --------------------------------------------------\n");

		fips140_post();

		pr_info("FIPS FUNC : (%d-1) POST done. SKC module FIPS status : %s\n",
			i+1, in_fips_err()?"failed":"passed");
		pr_info("FIPS FUNC : (%d-2) Try to use crypto\n", i + 1);
		// Check the module is not working in FIPS failure
		tfm = crypto_alloc_ahash("sha256", 0, 0);
		if (IS_ERR(tfm))
			pr_info("FIPS FUNC : (%d-3) alloc hash is failed as expected\n", i + 1);
		else {
			pr_info("FIPS FUNC : (%d-3) crypto allocation is success\n", i + 1);
			crypto_free_ahash(tfm);
		}

// reset the fips err flag to prepare the next test
		pr_err("FIPS FUNC : (%d-4) revert FIPS status to no error\n", i + 1);
		reset_in_fips_err();
	}

	for (i = 0; i < SKC_FUNCTEST_CONDITIONAL_CASE_NUM; i++) {
		set_fips_functest_conditional_mode(i);
		pr_info("FIPS FUNC : --------------------------------------------------\n");
		pr_info("FIPS FUNC : conditional test case %d - [%s]\n", i + 1, get_fips_functest_mode());
		pr_info("FIPS FUNC : --------------------------------------------------\n");
		rng = crypto_alloc_rng("drbg_pr_hmac_sha256", 0, 0);
		if (IS_ERR(rng)) {
			pr_err("FIPS FUNC : rng alloc was failed\n");
			continue;
		}
		if (crypto_rng_reset(rng, NULL, 0))
			pr_err("FIPS FUNC : DRBG instantiate failed as expected\n");
		crypto_free_rng(rng);
	}
	set_fips_functest_conditional_mode(-1);

	pr_info("FIPS FUNC : Functional test end\n");
	pr_info("FIPS FUNC : Normal POST start\n");
	return fips140_post();
}
#endif

/*
 * If an init function is provided, an exit function must also be provided
 * to allow module unload.
 */
static void __exit fips140_fini(void) { }

#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
	late_initcall(fips140_post_func_test);
#else
	late_initcall(fips140_post);
#endif
module_exit(fips140_fini);

// TODO: to be removed
#pragma clang optimize on

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("FIPS140 POST");
