/*
 * FIPS 200 support.
 *
 * Copyright (c) 2008 Neil Horman <nhorman@tuxdriver.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 */

#include <linux/export.h>
#include <linux/fips.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysctl.h>
#include "fips140.h"

/*
	Notes:
	1) The revision provides FIPS140ed kernel only
	2) CONFIG_CRYPTO_FIPS meaning is different comparing to vanilla kernel, "FIPS 200" -> "FIPS 140-2"
	3) provide fips_enable always ON 
*/

int fips_enabled = 1;
EXPORT_SYMBOL_GPL(fips_enabled);

static int IN_FIPS140_ERROR = FIPS140_NO_ERR;

bool in_fips_err(void)
{
	return (IN_FIPS140_ERROR == FIPS140_ERR);
}
EXPORT_SYMBOL_GPL(in_fips_err);

void set_in_fips_err(void)
{
	IN_FIPS140_ERROR = FIPS140_ERR;
}
EXPORT_SYMBOL_GPL(set_in_fips_err);

#ifdef CONFIG_CRYPTO_FIPS_FUNC_TEST
void reset_in_fips_err(void)
{
	IN_FIPS140_ERROR = FIPS140_NO_ERR;
}
EXPORT_SYMBOL_GPL(reset_in_fips_err);
#endif /* CONFIG_CRYPTO_FIPS_FUNC_TEST */

static struct ctl_table crypto_sysctl_table[] = {
	{
		.procname       = "fips_status",
		.data           = &IN_FIPS140_ERROR,
		.maxlen         = sizeof(int),
		.mode           = 0444,
		.proc_handler   = proc_dointvec
	},
	{}
};

static struct ctl_table crypto_dir_table[] = {
	{
		.procname       = "crypto",
		.mode           = 0555,
		.child          = crypto_sysctl_table
	},
	{}
};

static struct ctl_table_header *crypto_sysctls;

static void crypto_proc_fips_init(void)
{
	crypto_sysctls = register_sysctl_table(crypto_dir_table);
}

static void crypto_proc_fips_exit(void)
{
	unregister_sysctl_table(crypto_sysctls);
}

static int __init fips_init(void)
{
	crypto_proc_fips_init();
	return 0;
}

static void __exit fips_exit(void)
{
	crypto_proc_fips_exit();
}

module_init(fips_init);
module_exit(fips_exit);
