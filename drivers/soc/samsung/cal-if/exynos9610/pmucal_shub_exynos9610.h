/* individual sequence descriptor for SHUB control - init, reset, release */
struct pmucal_seq shub_init[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "QCH_CON_CM4_SHUB_QCH", 0x11000000,
			0x3018, (0x7 << 0), (0x4 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_SUBCPU_SHUB_OPTION", 0x11860000,
			0x3D28, (0x1 << 15), (0x1 << 15), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_SU:BCPU_SHUB_STATUS", 0x11860000,
			0x3D24, (0x1 << 0), (0x0 << 0), 0x11860000, 0x3D24,
			(0x1 << 0), (0x0 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "QCH_CON_CM4_SHUB_QCH", 0x11000000,
			0x3018, (0x7 << 0), (0x6 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "QCH_CON_CM4_SHUB_QCH", 0x11000000,
			0x3018, (0x7 << 0), (0x2 << 0), 0, 0, 0xffffffff, 0),
};

struct pmucal_seq shub_standbywfi_status[] = {
	PMUCAL_SEQ_DESC(PMUCAL_READ, "RESET_SUBCPU_SHUB_STATUS", 0x11860000,
			0x3D24, (0x1 << 28), 0, 0, 0, 0xffffffff, 0),
};

struct pmucal_seq shub_reset_assert[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_SUBCPU_SHUB_CONFIGURATION",
			0x11860000, 0x3D20, (0x1 << 0), (0x0 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_SUBCPU_SHUB_STATUS", 0x11860000,
			0x3D24, (0x1 << 0), (0x0 << 0), 0x11860000, 0x3D24,
			(0x1 << 0), (0x0 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_BUS_SHUB_OPTION", 0x11860000, 0x2C68,
			(0x3 << 0), (0x3 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_BUS_SHUB_CONFIGURATION", 0x11860000,
			0x2C60, (0x7 << 0), (0x6 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "TOP_BUS_SHUB_STATUS", 0x11860000, 0x2C64,
			(0x7 << 0), (0x6 << 0), 0x11860000, 0x2C64, (0x7 << 0),
			(0x6 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_PWR_SHUB_CONFIGURATION", 0x11860000,
			0x2CE0, (0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "TOP_PWR_SHUB_STATUS", 0x11860000, 0x2CE4,
			(0x3 << 0), (0x0 << 0), 0x11860000, 0x2CE4, (0x3 << 0),
			(0x0 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "LOGIC_RESET_SHUB_CONFIGURATION",
			0x11860000, 0x2D80, (0x3 << 0), (0x0 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "LOGIC_RESET_SHUB_STATUS", 0x11860000,
			0x2D84, (0x3 << 0), (0x0 << 0), 0x11860000, 0x2D64,
			(0x3 << 0), (0x0 << 0)),

	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_OTP_SHUB_CONFIGURATION",
			0x11860000, 0x2B80, (0x3 << 0), (0x0 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_OTP_SHUB_STATUS", 0x11860000,
			0x2B84, (0x3 << 0), (0x0 << 0), 0x11860000, 0x2B64,
			(0x3 << 0), (0x0 << 0)),

	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_CMU_SHUB_CONFIGURATION",
			0x11860000, 0x2A60, (0x3 << 0), (0x0 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_CMU_SHUB_STATUS", 0x11860000,
			0x2A64, (0x3 << 0), (0x0 << 0), 0x11860000, 0x2A64,
			(0x3 << 0), (0x0 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_BUS_SHUB_CONFIGURATION", 0x11860000,
			0x2C60, (0x7 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "TOP_BUS_SHUB_STATUS", 0x11860000, 0x2C64,
			(0x7 << 0), (0x0 << 0), 0x11860000, 0x2C64, (0x7 << 0),
			(0x0 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_BUS_SHUB_OPTION", 0x11860000, 0x2C68,
			(0x3 << 0), (0x0 << 0), 0, 0, 0xffffffff, 0),
};

struct pmucal_seq shub_reset_release_config[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_OTP_SHUB_CONFIGURATION",
			0x11860000, 0x2ce0, (0x3 << 0), (0x3 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_OTP_SHUB_STATUS", 0x11860000,
			0x2ce4, (0x3 << 0), (0x3 << 0), 0x11860000, 0x2ce4,
			(0x3 << 0), (0x3 << 0)),

	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_CMU_SHUB_CONFIGURATION",
			0x11860000, 0x2A60, (0x3 << 0), (0x3 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_CMU_SHUB_STATUS", 0x11860000,
			0x2A64, (0x3 << 0), (0x3 << 0), 0x11860000, 0x2A64,
			(0x3 << 0), (0x3 << 0)),

	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_OTP_SHUB_CONFIGURATION",
			0x11860000, 0x2B80, (0x3 << 0), (0x3 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_OTP_SHUB_STATUS", 0x11860000,
			0x2B84, (0x3 << 0), (0x3 << 0), 0x11860000, 0x2B84,
			(0x3 << 0), (0x3 << 0)),

	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "LOGIC_RESET_SHUB_CONFIGURATION",
			0x11860000, 0x2D80, (0x3 << 0), (0x3 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "LOGIC_RESET_SHUB_STATUS", 0x11860000,
			0x2D84, (0x3 << 0), (0x3 << 0), 0x11860000, 0x2D84,
			(0x3 << 0), (0x3 << 0)),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "TOP_BUS_SHUB_CONFIGURATION", 0x11860000,
			0x2C60, (0x7 << 0), (0x7 << 0), 0, 0, 0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "TOP_BUS_SHUB_STATUS", 0x11860000, 0x2C64,
			(0x7 << 0), (0x7 << 0), 0x11860000, 0x2C64, (0x7 << 0),
			(0x7 << 0)),
};

struct pmucal_seq shub_reset_release[] = {
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_SUBCPU_SHUB_CONFIGURATION",
			0x11860000, 0x3D20, (0x1 << 0), (0x1 << 0), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WRITE, "RESET_SUBCPU_SHUB_OPTION",
			0x11860000, 0x3D28, (0x1 << 15), (0x1 << 15), 0, 0,
			0xffffffff, 0),
	PMUCAL_SEQ_DESC(PMUCAL_WAIT, "RESET_SUBCPU_SHUB_STATUS", 0x11860000,
			0x3D24, (0x1 << 0), (0x1 << 0), 0, 0, 0xffffffff, 0)
};

struct pmucal_shub pmucal_shub_list = {
	.init = shub_init,
	.status = shub_standbywfi_status,
	.reset_assert = shub_reset_assert,
	.reset_release_config = shub_reset_release_config,
	.reset_release = shub_reset_release,
	.num_init = ARRAY_SIZE(shub_init),
	.num_status = ARRAY_SIZE(shub_standbywfi_status),
	.num_reset_assert = ARRAY_SIZE(shub_reset_assert),
	.num_reset_release_config = ARRAY_SIZE(shub_reset_release_config),
	.num_reset_release = ARRAY_SIZE(shub_reset_release),
};

unsigned int pmucal_shub_list_size = 1;
