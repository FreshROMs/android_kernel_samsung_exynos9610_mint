#include "pmucal_shub.h"
#include "pmucal_rae.h"

/**
 *  pmucal_shub_init - init shub.
 *		        exposed to PWRCAL interface.

 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_shub_init(void)
{
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	return 0;
}

/**
 *  pmucal_shub_standbywfi_status - get shub standbywfi status.
 *		        exposed to PWRCAL interface.

 *  Returns 1 when the shub is in wfi, 0 when not.
 *  Otherwise, negative error code.
 */
int pmucal_shub_standbywfi_status(void)
{
	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_shub_list.status) {
		pr_err("%s there is no sequence element for shub-status.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	pmucal_rae_handle_shub_seq(pmucal_shub_list.status,
				pmucal_shub_list.num_status);

	pr_err("shub_status: 0x%X\n", pmucal_shub_list.status->value);

	if (pmucal_shub_list.status->value == 0x1)
		return 1;
	else
		return 0;
}

/**
 *  pmucal_shub_reset_assert - reset assert shub.
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_shub_reset_assert(void)
{
	int ret;

	if (!pmucal_shub_list.reset_assert) {
		pr_err("%s there is no sequence element for shub-reset_assert.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_shub_seq(pmucal_shub_list.reset_assert,
				pmucal_shub_list.num_reset_assert);
	if (ret) {
		pr_err("%s %s: error on handling shub-reset_assert sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	pr_info("%s done\n", __func__);
	return 0;
}

/**
 *  pmucal_shub_reset_release_config - reset_release_config shub except SHUB CPU reset
 *		        exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int pmucal_shub_reset_release_config(void)
{
	int ret;

	if (!pmucal_shub_list.reset_release_config) {
		pr_err("%s there is no sequence element for shub-reset_release_config.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_shub_seq(pmucal_shub_list.reset_release_config,
				pmucal_shub_list.num_reset_release_config);
	if (ret) {
		pr_err("%s %s: error on handling shub-reset_release_config sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	pr_info("%s doing\n", __func__);
	return 0;
}
int pmucal_shub_reset_release(void)
{
	int ret;

	if (!pmucal_shub_list.reset_release) {
		pr_err("%s there is no sequence element for shub-reset_release.\n",
				PMUCAL_PREFIX);
		return -ENOENT;
	}

	ret = pmucal_rae_handle_shub_seq(pmucal_shub_list.reset_release,
				pmucal_shub_list.num_reset_release);
	if (ret) {
		pr_err("%s %s: error on handling shub-reset_release sequence.\n",
				PMUCAL_PREFIX, __func__);
		return ret;
	}

	pr_info("%s done\n", __func__);
	return 0;

}
int pmucal_is_shub_regs(int reg)
{
	int i;
	int is_shub_regs = 0;

	for (i = 0; i < pmucal_shub_list.num_init; i++) {
		if (reg == pmucal_shub_list.init[i].base_pa + pmucal_shub_list.init[i].offset) {
			is_shub_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_shub_list.num_reset_assert; i++) {
		if (reg == pmucal_shub_list.reset_assert[i].base_pa + pmucal_shub_list.reset_assert[i].offset) {
			is_shub_regs = 1;
			goto out;
		}
	}

	for (i = 0; i < pmucal_shub_list.num_reset_release; i++) {
		if (reg == pmucal_shub_list.reset_release[i].base_pa + pmucal_shub_list.reset_release[i].offset) {
			is_shub_regs = 1;
			goto out;
		}
	}

out:
	return is_shub_regs;
}

/**
 *  pmucal_cp_initialize - Initialize function of PMUCAL SHUB common logic.
 *		            exposed to PWRCAL interface.
 *
 *  Returns 0 on success. Otherwise, negative error code.
 */
int __init pmucal_shub_initialize(void)
{
	int ret = 0;

	pr_info("%s%s()\n", PMUCAL_PREFIX, __func__);

	if (!pmucal_shub_list_size) {
		pr_err("%s %s: there is no shub list. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		return -ENOENT;
	}

	/* convert physical base address to virtual addr */
	ret = pmucal_rae_phy2virt(pmucal_shub_list.init,
				pmucal_shub_list.num_init);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting init...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_shub_list.status,
				pmucal_shub_list.num_status);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting status...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_shub_list.reset_assert,
				pmucal_shub_list.num_reset_assert);

//investigating virtual address of assrtion

	pr_info("%s %p\n", PMUCAL_PREFIX, pmucal_shub_list.reset_assert[0].base_va);

	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_assert...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_shub_list.reset_release_config,
				pmucal_shub_list.num_reset_release_config);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_release_config...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}

	ret = pmucal_rae_phy2virt(pmucal_shub_list.reset_release,
				pmucal_shub_list.num_reset_release);
	if (ret) {
		pr_err("%s %s: error on PA2VA conversion. aborting reset_release...\n",
				PMUCAL_PREFIX, __func__);
		goto out;
	}
out:
	return ret;
}
