/******************************************************************************
 *
 * Copyright (c) 2012 - 2021 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/
#include "dev.h"
#include "reg_info.h"
#include "debug.h"
#include <linux/fs.h>
#include <linux/firmware.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif

void slsi_regd_init(struct slsi_dev *sdev)
{
	struct ieee80211_regdomain *slsi_world_regdom_custom = sdev->device_config.domain_info.regdomain;
	struct ieee80211_reg_rule  reg_rules[] = {
		/* Channel 1 - 11*/
		REG_RULE(2412 - 10, 2462 + 10, 40, 0, 20, 0),
		/* Channel 12 - 13 NO_IR*/
		REG_RULE(2467 - 10, 2472 + 10, 40, 0, 20, NL80211_RRF_NO_IR),
		/* Channel 36 - 48 */
		REG_RULE(5180 - 10, 5240 + 10, 80, 0, 20, 0),
		/* Channel 52 - 64 */
		REG_RULE(5260 - 10, 5320 + 10, 80, 0, 20, NL80211_RRF_DFS),
		/* Channel 100 - 144 */
		REG_RULE(5500 - 10, 5720 + 10, 80, 0, 20, NL80211_RRF_DFS),
		/* Channel 149 - 165 */
		REG_RULE(5745 - 10, 5825 + 10, 80, 0, 20, 0),
	};

	int                        i;

	SLSI_DBG1_NODEV(SLSI_INIT_DEINIT, "regulatory init\n");
	sdev->regdb.regdb_state = SLSI_REG_DB_NOT_SET;
	slsi_world_regdom_custom->n_reg_rules = (sizeof(reg_rules)) / sizeof(reg_rules[0]);
	for (i = 0; i < slsi_world_regdom_custom->n_reg_rules; i++)
		slsi_world_regdom_custom->reg_rules[i] = reg_rules[i];

	/* Country code '00' indicates world regulatory domain */
	slsi_world_regdom_custom->alpha2[0] = '0';
	slsi_world_regdom_custom->alpha2[1] = '0';

	wiphy_apply_custom_regulatory(sdev->wiphy, slsi_world_regdom_custom);
}

void  slsi_regd_deinit(struct slsi_dev *sdev)
{
	SLSI_DBG1(sdev, SLSI_INIT_DEINIT, "slsi_regd_deinit\n");

	kfree(sdev->device_config.domain_info.countrylist);
	sdev->device_config.domain_info.countrylist = NULL;
	kfree(sdev->regdb.freq_ranges);
	sdev->regdb.freq_ranges = NULL;
	kfree(sdev->regdb.reg_rules);
	sdev->regdb.reg_rules = NULL;
	kfree(sdev->regdb.rules_collection);
	sdev->regdb.rules_collection = NULL;
	kfree(sdev->regdb.country);
	sdev->regdb.country = NULL;
}

int firmware_read(const struct firmware *firm, void *dest, size_t size, int *offset)
{
	if (firm->size - *offset < size) {
		SLSI_INFO_NODEV("file size is too small to read data!\n");
		return -EINVAL;
	}
	memcpy(dest, firm->data + *offset, size);
	*offset = *offset + size;
	return size;
}

int slsi_read_regulatory(struct slsi_dev *sdev)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
	char *reg_file_t = "/vendor/etc/wifi/slsi_reg_database.bin";
#else
	char *reg_file_t = "../etc/wifi/slsi_reg_database.bin";
#endif

	int i = 0, j = 0, index = 0;
	uint32_t num_freqbands = 0, num_rules = 0, num_collections = 0;
	int script_version = 0;
	int r = 0, offset = 0;
	const struct firmware *firm;

	if (sdev->regdb.regdb_state == SLSI_REG_DB_SET) {
		SLSI_INFO(sdev, "Regulatory is already set\n");
		return 0;
	}

	r = mx140_request_file(sdev->maxwell_core, reg_file_t, &firm);
	if (r) {
		SLSI_INFO(sdev, "Error Loading %s file %d\n", reg_file_t, r);
		sdev->regdb.regdb_state = SLSI_REG_DB_ERROR;
		return -EINVAL;
	}

	if (firmware_read(firm, &script_version, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read script version\n");
		goto exit;
	}
	if (firmware_read(firm, &sdev->regdb.db_major_version, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read regdb major version %u\n", sdev->regdb.db_major_version);
		goto exit;
	}
	if (firmware_read(firm, &sdev->regdb.db_minor_version, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read regdb minor version %u\n", sdev->regdb.db_minor_version);
		goto exit;
	}
	if (firmware_read(firm, &num_freqbands, sizeof(uint32_t), &offset) < 0) {
		SLSI_INFO(sdev, "Failed to read Number of Frequency bands %u\n", num_freqbands);
		goto exit;
	}

	sdev->regdb.freq_ranges = kmalloc(sizeof(*sdev->regdb.freq_ranges) * num_freqbands, GFP_KERNEL);
	if (!sdev->regdb.freq_ranges) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->freq_ranges failed\n");
		goto exit;
	}
	for (i = 0; i < num_freqbands; i++) {
		if (firmware_read(firm, &sdev->regdb.freq_ranges[i], sizeof(struct regdb_file_freq_range), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb freq_ranges\n");
			goto exit_freq_ranges;
		}
	}

	if (firmware_read(firm, &num_rules, sizeof(uint32_t), &offset) < 0) {
		SLSI_ERR(sdev, "Failed to read num_rules\n");
		goto exit_freq_ranges;
	}

	sdev->regdb.reg_rules = kmalloc(sizeof(*sdev->regdb.reg_rules) * num_rules, GFP_KERNEL);
	if (!sdev->regdb.reg_rules) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->reg_rules failed\n");
		goto exit_freq_ranges;
	}
	for (i = 0; i < num_rules; i++) {
		if (firmware_read(firm, &index, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read index\n");
			goto exit_reg_rules;
		}
		sdev->regdb.reg_rules[i].freq_range = &sdev->regdb.freq_ranges[index];
		if (firmware_read(firm, &sdev->regdb.reg_rules[i].max_eirp, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb reg_rules\n");
			goto exit_reg_rules;
		}
		if (firmware_read(firm, &sdev->regdb.reg_rules[i].flags, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb flags\n");
			goto exit_reg_rules;
		}
	}

	if (firmware_read(firm, &num_collections, sizeof(uint32_t), &offset) < 0) {
		SLSI_ERR(sdev, "Failed to read num_collections\n");
		goto exit_reg_rules;
	}

	sdev->regdb.rules_collection = kmalloc(sizeof(*sdev->regdb.rules_collection) * num_collections, GFP_KERNEL);
	if (!sdev->regdb.rules_collection) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->rules_collection failed\n");
		goto exit_reg_rules;
	}
	for (i = 0; i < num_collections; i++) {
		if (firmware_read(firm, &sdev->regdb.rules_collection[i].reg_rule_num, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb rules_collection reg_rule_num\n");
			goto exit_rules_collection;
		}
		for (j = 0; j < sdev->regdb.rules_collection[i].reg_rule_num; j++) {
			if (firmware_read(firm, &index, sizeof(uint32_t), &offset) < 0) {
				SLSI_ERR(sdev, "Failed to read regdb rules collections index\n");
				goto exit_rules_collection;
			}
			sdev->regdb.rules_collection[i].reg_rule[j] = &sdev->regdb.reg_rules[index];
		}
	}

	if (firmware_read(firm, &sdev->regdb.num_countries, sizeof(uint32_t), &offset) < 0) {
		SLSI_ERR(sdev, "Failed to read regdb number of countries\n");
		goto exit_rules_collection;
	}
	SLSI_INFO(sdev, "Regulatory Version: %d.%d , Script Version: %d , Number of Countries: %d\n",
		  sdev->regdb.db_major_version, sdev->regdb.db_minor_version, script_version,
		  sdev->regdb.num_countries);

	sdev->regdb.country = kmalloc(sizeof(*sdev->regdb.country) * sdev->regdb.num_countries, GFP_KERNEL);
	if (!sdev->regdb.country) {
		SLSI_ERR(sdev, "kmalloc of sdev->regdb->country failed\n");
		goto exit_rules_collection;
	}
	for (i = 0; i < sdev->regdb.num_countries; i++) {
		if (firmware_read(firm, &sdev->regdb.country[i].alpha2, 2 * sizeof(uint8_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country alpha2\n");
			goto exit_country;
		}
		if (firmware_read(firm, &sdev->regdb.country[i].pad_byte, sizeof(uint8_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country pad_byte\n");
			goto exit_country;
		}
		if (firmware_read(firm, &sdev->regdb.country[i].dfs_region, sizeof(uint8_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country dfs_region\n");
			goto exit_country;
		}
		if (firmware_read(firm, &index, sizeof(uint32_t), &offset) < 0) {
			SLSI_ERR(sdev, "Failed to read regdb country index\n");
			goto exit_country;
		}
		sdev->regdb.country[i].collection = &sdev->regdb.rules_collection[index];
	}

	mx140_release_file(sdev->maxwell_core, firm);
	sdev->regdb.regdb_state = SLSI_REG_DB_SET;
	return 0;

exit_country:
	kfree(sdev->regdb.country);
	sdev->regdb.country = NULL;

exit_rules_collection:
	kfree(sdev->regdb.rules_collection);
	sdev->regdb.rules_collection = NULL;

exit_reg_rules:
	kfree(sdev->regdb.reg_rules);
	sdev->regdb.reg_rules = NULL;

exit_freq_ranges:
	kfree(sdev->regdb.freq_ranges);
	sdev->regdb.freq_ranges = NULL;
exit:
	sdev->regdb.regdb_state = SLSI_REG_DB_ERROR;
	mx140_release_file(sdev->maxwell_core, firm);
	return -EINVAL;
}
