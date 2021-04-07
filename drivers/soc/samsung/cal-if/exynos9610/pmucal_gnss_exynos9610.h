/* individual sequence descriptor for GNSS control - init, reset, release, gnss_active_clear, gnss_reset_req_clear */
struct pmucal_seq gnss_init[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__MASK_PWR_REQ", 0x11860000, 0x0040, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_S__GNSS_START", 0x11860000, 0x0044, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_SEQUENCER_STATUS", 0x11860000, 0x0504, (0x7 << 4), (0x5 << 4), 0x11860000, 0x0504, (0x7 << 4), (0x5 << 4)),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "PMU_SHARED_PWR_REQ_GNSS_CONTROL__STATUS", 0x11860000, 0x800C, (0x1 << 0), (0x1 << 0), 0x11860000, 0x800C, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0040, (0x1 << 20), (0x0 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__MASK_PWR_REQ", 0x11860000, 0x0040, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "RESET_SEQUENCER_STATUS", 0x11860000, 0x0504, (0x7 << 4), 0, 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0040, (0x1 << 20), (0x1 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__GNSS_PWRON", 0x11860000, 0x0040, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "EXT_REGULATOR_CON_STATUS", 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0), 0x11860000, 0x3644, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CENTRAL_SEQ_GNSS_CONFIGURATION", 0x11860000, 0x02C0, (0x1 << 16), (0x0 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_AHEAD_GNSS_SYS_PWR_REG", 0x11860000, 0x1340, (0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLEANY_BUS_GNSS_SYS_PWR_REG", 0x11860000, 0x1344, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "LOGIC_RESET_GNSS_SYS_PWR_REG", 0x11860000, 0x1348, (0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TCXO_GATE_GNSS_SYS_PWR_REG", 0x11860000, 0x134c, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_DISABLE_ISO_SYS_PWR_REG", 0x11860000, 0x1350, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_RESET_ISO_SYS_PWR_REG", 0x11860000, 0x1354, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__GNSS_RESET_SET", 0x11860000, 0x0040, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CENTRAL_SEQ_GNSS_STATUS__STATES", 0x11860000, 0x02C4, (0xff << 16), (0x80 << 16), 0x11860000, 0x02C4, (0xff << 16), (0x80 << 16)),
};
struct pmucal_seq gnss_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__GNSS_PWRON", 0x11860000, 0x0040, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0040, (0x1 << 20), (0x1 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__SWEEPER_BYPASS_DATA_EN", 0x11860000, 0x0040, (0x1 << 16), (0x1 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__GNSS_RESET_SET", 0x11860000, 0x0040, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CENTRAL_SEQ_GNSS_STATUS__STATES", 0x11860000, 0x02C4, (0xff << 16), (0x0 << 16), 0x11860000, 0x02C4, (0xff << 16), (0x0 << 16)),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "PMU_SHARED_PWR_REQ_GNSS_CONTROL_STATUS", 0x11860000, 0x800C, (0x1 << 0), (0x1 << 0), 0x11860000, 0x800C, (0x1 << 0), (0x1 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLEANY_BUS_GNSS_CONFIGURATION", 0x11860000, 0x3A20, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CLEANY_BUS_GNSS_STATUS", 0x11860000, 0x3A24, (0x3 << 16), (0x0 << 16), 0x11860000, 0x3A24, (0x3 << 16), (0x0 << 16)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__SWEEPER_BYPASS_DATA_EN", 0x11860000, 0x0040, (0x1 << 16), (0x0 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0040, (0x1 << 20), (0x0 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__GNSS_PWRON", 0x11860000, 0x0040, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq gnss_gnss_reset_req_clear[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "GNSS_CTRL_NS__GNSS_RESET_REQ_CLR", 0x11860000, 0x0040, (0x1 << 8), (0x1 << 8), 0, 0, 0xffffffff, 0),
};
struct pmucal_gnss pmucal_gnss_list = {
		.init = gnss_init,
		.status = gnss_status,
		.reset_assert = gnss_reset_assert,
		.reset_release = gnss_reset_release,
		.gnss_reset_req_clear = gnss_gnss_reset_req_clear,
		.num_init = ARRAY_SIZE(gnss_init),
		.num_status = ARRAY_SIZE(gnss_status),
		.num_reset_assert = ARRAY_SIZE(gnss_reset_assert),
		.num_reset_release = ARRAY_SIZE(gnss_reset_release),
		.num_gnss_reset_req_clear = ARRAY_SIZE(gnss_gnss_reset_req_clear),
};
unsigned int pmucal_gnss_list_size = 1;
