/* individual sequence descriptor for CP control - init, reset, release, cp_active_clear, cp_reset_req_clear */
struct pmucal_seq cp_init[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_PWR_REQ", 0x11860000, 0x0030, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_S__CP_START", 0x11860000, 0x0034, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_SEQUENCER_STATUS", 0x11860000, 0x0504, (0x7 << 0), (0x5 << 0), 0x11860000, 0x0504, (0x7 << 0), (0x5 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "PMU_SHARED_PWR_REQ_CP_CONTROL__STATUS", 0x11860000, 0x8004, (0x1 << 0), (0x1 << 0), 0x11860000, 0x8004, (0x1 << 0), (0x1 << 0)),
//	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "INTERRUPT_SET_PENDING", 0x12300000, 0x120C, (0x1 << 12), (0x1 << 12), 0x12300000, 0x120C, (0x1 << 12), (0x1 << 12)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0030, (0x1 << 20), (0x0 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_PWR_REQ", 0x11860000, 0x0030, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq cp_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "RESET_SEQUENCER_STATUS", 0x11860000, 0x0504, (0x7 << 0), 0, 0, 0, 0xffffffff, 0),
};
struct pmucal_seq cp_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CENTRAL_SEQ_CP_CONFIGURATION", 0x11860000, 0x0280, (0x1 << 16), (0x0 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_AHEAD_CP_SYS_PWR_REG", 0x11860000, 0x1320, (0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLEANY_BUS_CP_SYS_PWR_REG", 0x11860000, 0x1324, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "LOGIC_RESET_CP_SYS_PWR_REG", 0x11860000, 0x1328, (0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TCXO_GATE_CP_SYS_PWR_REG", 0x11860000, 0x132c, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_DISABLE_ISO_SYS_PWR_REG", 0x11860000, 0x1330, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_RESET_ISO_SYS_PWR_REG", 0x11860000, 0x1334, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_DUMP_PC_SYS_PWR_REG", 0x11860000, 0x1338, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__CP_RESET_SET", 0x11860000, 0x0030, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_DELAY, "DELAY", 0x11860000, 0, 0, 4000, 0, 0, 0, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CENTRAL_SEQ_CP_STATUS__STATES", 0x11860000, 0x0284, (0xff << 16), (0x80 << 16), 0x11860000, 0x0284, (0xff << 16), (0x80 << 16)),
};
struct pmucal_seq cp_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__CP_PWRON", 0x11860000, 0x0030, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0030, (0x1 << 20), (0x1 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__SWEEPER_BYPASS_DATA_EN", 0x11860000, 0x0030, (0x1 << 16), (0x1 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__CP_RESET_SET", 0x11860000, 0x0030, (0x1 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CENTRAL_SEQ_CP_STATUS__STATES", 0x11860000, 0x0284, (0xff << 16), (0x0 << 16), 0x11860000, 0x0284, (0xff << 16), (0x0 << 16)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CLEANY_BUS_CP_CONFIGURATION", 0x11860000, 0x3920, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "CLEANY_BUS_CP_STATUS", 0x11860000, 0x3924, (0x3 << 16), (0x0 << 16), 0x11860000, 0x3924, (0x3 << 16), (0x0 << 16)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__SWEEPER_BYPASS_DATA_EN", 0x11860000, 0x0030, (0x1 << 16), (0x0 << 16), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0030, (0x1 << 20), (0x0 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__CP_PWRON", 0x11860000, 0x0030, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq cp_enable_dump_pc_no_pg[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0030, (0x1 << 20), (0x1 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_PWR_REQ", 0x11860000, 0x0030, (0x1 << 18), (0x1 << 18), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS", 0x11860000, 0x0030, (0x1 << 17), (0x1 << 17), 0, 0, 0xffffffff, 0),
};
struct pmucal_seq cp_disable_dump_pc_no_pg[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS", 0x11860000, 0x0030, (0x1 << 17), (0x0 << 17), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_TCXO_REQ", 0x11860000, 0x0030, (0x1 << 20), (0x0 << 20), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "CP_CTRL_NS__MASK_PWR_REQ", 0x11860000, 0x0030, (0x1 << 18), (0x0 << 18), 0, 0, 0xffffffff, 0),
};
struct pmucal_cp pmucal_cp_list = {
		.init = cp_init,
		.status = cp_status,
		.reset_assert = cp_reset_assert,
		.reset_release = cp_reset_release,
		.cp_active_clear = 0,
		.cp_reset_req_clear = 0,
		.cp_enable_dump_pc_no_pg = cp_enable_dump_pc_no_pg,
		.cp_disable_dump_pc_no_pg = cp_disable_dump_pc_no_pg,
		.num_init = ARRAY_SIZE(cp_init),
		.num_status = ARRAY_SIZE(cp_status),
		.num_reset_assert = ARRAY_SIZE(cp_reset_assert),
		.num_reset_release = ARRAY_SIZE(cp_reset_release),
		.num_cp_active_clear = 0,
		.num_cp_reset_req_clear = 0,
		.num_cp_enable_dump_pc_no_pg = ARRAY_SIZE(cp_enable_dump_pc_no_pg),
		.num_cp_disable_dump_pc_no_pg = ARRAY_SIZE(cp_disable_dump_pc_no_pg),
};
unsigned int pmucal_cp_list_size = 1;
