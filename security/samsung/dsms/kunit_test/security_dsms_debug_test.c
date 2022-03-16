/*
 * Copyright (c) 2020 Samsung Electronics Co., Ltd. All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 */

#include <kunit/test.h>
#include <linux/dsms.h>
#include "dsms_kernel_api.h"
#include "dsms_test.h"

/* ------------------------------------------------------------------------- */
/* Module test functions                                                     */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* File: dsms_kernel_api.h                                                   */
/* ------------------------------------------------------------------------- */

/* ------------------------------------------------------------------------- */
/* Function to be tested: DSMS_LOG_INFO                                      */
/* ------------------------------------------------------------------------- */

static void security_dsms_debug_success_test(struct test *test)
{
	DSMS_LOG_INFO("DSMS Debug unit test %x\n", 0xdeadbeef);
	SUCCEED(test);
}

/* ------------------------------------------------------------------------- */
/* Module definition                                                         */
/* ------------------------------------------------------------------------- */

static struct test_case security_dsms_debug_test_cases[] = {
	TEST_CASE(security_dsms_debug_success_test),
	{},
};

static struct test_module security_dsms_debug_test_module = {
	.name = "security-dsms-debug-test",
	.test_cases = security_dsms_debug_test_cases,
};
module_test(security_dsms_debug_test_module);
