/******************************************************************************
 *
 * Copyright (c) 2012 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/
#ifndef __SLSI_REGINFO_H__
#define __SLSI_REGINFO_H__

int slsi_read_regulatory(struct slsi_dev *sdev);
void slsi_regd_deinit(struct slsi_dev *sdev);
void slsi_regd_init(struct slsi_dev *sdev);

enum slsi_regdb_state {
	SLSI_REG_DB_NOT_SET,
	SLSI_REG_DB_ERROR,
	SLSI_REG_DB_SET,
};

struct reg_database {
	enum slsi_regdb_state regdb_state;
	uint32_t db_major_version;
	uint32_t db_minor_version;
	uint32_t num_countries;
	struct regdb_file_reg_rule *reg_rules;
	struct regdb_file_freq_range *freq_ranges;
	struct regdb_file_reg_rules_collection *rules_collection;
	struct regdb_file_reg_country *country;
};

struct regdb_file_freq_range {
	uint32_t start_freq; /* in MHz */
	uint32_t end_freq; /* in MHz */
	uint32_t max_bandwidth;	/* in MHz */
};

struct regdb_file_reg_rule {
	struct regdb_file_freq_range *freq_range;	/* ptr to regdb_file_freq_range array */
	uint32_t max_eirp;/* this is power in dBm */
	uint32_t flags;
};

struct regdb_file_reg_rules_collection {
	uint32_t reg_rule_num;
	/* pointers to struct regdb_file_reg_rule */
	struct regdb_file_reg_rule *reg_rule[8];
};

struct regdb_file_reg_country {
	uint8_t	alpha2[2];
	uint8_t pad_byte;
	uint8_t	dfs_region; /* first two bits define the DFS region */
	/* pointers to struct regdb_file_reg_rules_collection */
	struct regdb_file_reg_rules_collection *collection;
};

#endif

