/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <kunit/mock.h>
#include <linux/slab.h>
#include <linux/types.h>
#include "dsms_test.h"
#include "dsms_test_utils.h"

/* ------------------------------------------------------------------------- */
/* Module test functions                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* File: test_utils.c                                                        */
/* ------------------------------------------------------------------------- */

/* By default kmalloc is defined as kmalloc_mock in dsms_test.h*/
static void security_dsms_test_kmalloc_mock_test(struct test *test)
{
	void *p;

	security_dsms_test_request_kmalloc_fail_at(1);
	security_dsms_test_request_kmalloc_fail_at(3);
	/* kmalloc must call security_dsms_test_kmalloc_mock */
	EXPECT_EQ(test, p = kmalloc(1, GFP_KERNEL), NULL);
	kfree(p);
	EXPECT_NE(test, p = kmalloc(1, GFP_KERNEL), NULL);
	kfree(p);
	EXPECT_EQ(test, p = kmalloc(1, GFP_KERNEL), NULL);
	kfree(p);
	EXPECT_NE(test, p = kmalloc(1, GFP_KERNEL), NULL);
	kfree(p);
}

/* ------------------------------------------------------------------------- */
/* Module initialization and exit functions                                  */
/* ------------------------------------------------------------------------- */

static int security_dsms_test_utils_init(struct test *test)
{
	security_dsms_test_cancel_kmalloc_fail_requests();
	return 0;
}

static void security_dsms_test_utils_exit(struct test *test)
{
	security_dsms_test_cancel_kmalloc_fail_requests();
}

/* ------------------------------------------------------------------------- */
/* Module definition                                                         */
/* ------------------------------------------------------------------------- */

static struct test_case security_dsms_test_utils_test_cases[] = {
	TEST_CASE(security_dsms_test_kmalloc_mock_test),
	{},
};

static struct test_module security_dsms_test_utils_module = {
	.name = "security-dsms-test-utils-test",
	.init = security_dsms_test_utils_init,
	.exit = security_dsms_test_utils_exit,
	.test_cases = security_dsms_test_utils_test_cases,
};
module_test(security_dsms_test_utils_module);
