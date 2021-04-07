/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/uaccess.h>
#include <scsc/scsc_logring.h>
#include "pcie_proc.h"
#include "pcie_mif.h"

static struct proc_dir_entry *procfs_dir;
static bool                  pcie_val;

/* singleton */
struct pcie_mif              *pcie_global;

static int pcie_procfs_open_file_generic(struct inode *inode, struct file *file)
{
	file->private_data = PCIE_PDE_DATA(inode);
	return 0;
}

PCIE_PROCFS_RW_FILE_OPS(pcie_trg);
PCIE_PROCFS_SEQ_FILE_OPS(pcie_dbg);

static ssize_t pcie_procfs_pcie_trg_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	int          pos = 0;
	const size_t bufsz = sizeof(buf);

	pos += scnprintf(buf + pos, bufsz - pos, "%d\n", (pcie_val ? 1 : 0));

	return simple_read_from_buffer(user_buf, count, ppos, buf, pos);
}

#define ROW     52
#define COL     2
char *lookup_regs[ROW][COL] = {
	{ "NEWMSG", "0" },
	{ "SIGNATURE", "4" },
	{ "OFFSET", "8" },
	{ "RUNEN", "12" },
	{ "DEBUG", "16" },
	{ "AXIWCNT", "20" },
	{ "AXIRCNT", "24" },
	{ "AXIWADDR", "28" },
	{ "AXIRADDR", "32" },
	{ "TBD", "36" },
	{ "AXICTRL", "40" },
	{ "AXIDATA", "44" },
	{ "AXIRDBP", "48" },
	{ "IFAXIWCNT", "52" },
	{ "IFAXIRCNT", "56" },
	{ "IFAXIWADDR", "60" },
	{ "IFAXIRADDR", "64" },
	{ "IFAXICTRL", "68" },
	{ "GRST", "72" },
	{ "AMBA2TRANSAXIWCNT", "76" },
	{ "AMBA2TRANSAXIRCNT", "80" },
	{ "AMBA2TRANSAXIWADDR", "84" },
	{ "AMBA2TRANSAXIRADDR", "88" },
	{ "AMBA2TRANSAXICTR", "92" },
	{ "TRANS2PCIEREADALIGNAXIWCNT", "96" },
	{ "TRANS2PCIEREADALIGNAXIRCNT", "100" },
	{ "TRANS2PCIEREADALIGNAXIWADDR", "104" },
	{ "TRANS2PCIEREADALIGNAXIRADDR", "108" },
	{ "TRANS2PCIEREADALIGNAXICTRL", "112" },
	{ "READROUNDTRIPMIN", "116" },
	{ "READROUNDTRIPMAX", "120" },
	{ "READROUNDTRIPLAST", "124" },
	{ "CPTAW0", "128" },
	{ "CPTAW1", "132" },
	{ "CPTAR0", "136" },
	{ "CPTAR1", "140" },
	{ "CPTB0", "144" },
	{ "CPTW0", "148" },
	{ "CPTW1", "152" },
	{ "CPTW2", "156" },
	{ "CPTR0", "160" },
	{ "CPTR1", "164" },
	{ "CPTR2", "168" },
	{ "CPTRES", "172" },
	{ "CPTAWDELAY", "176" },
	{ "CPTARDELAY", "180" },
	{ "CPTSRTADDR", "184" },
	{ "CPTENDADDR", "188" },
	{ "CPTSZLTHID", "192" },
	{ "CPTPHSEL", "196" },
	{ "CPTRUN", "200" },
	{ "FPGAVER", "204" },
};

/* Trigger boot of Curator over SDIO without Chip Power Manager present */
static ssize_t pcie_procfs_pcie_trg_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char         buf[128];
	char         *sptr, *token;
	unsigned int len = 0, pass = 0;
	u32          value = 0;
	int          i = 0;
	int          rc;

	int          match = 0, offset = 0;

	len = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, len))
		return -EFAULT;

	buf[len] = '\0';
	sptr = buf;

	while ((token = strsep(&sptr, " ")) != NULL) {
		switch (pass) {
		/* register */
		case 0:
			SCSC_TAG_INFO(PCIE_MIF, "str %s\n", lookup_regs[0][0]);
			SCSC_TAG_INFO(PCIE_MIF, "token %s\n", token);
			SCSC_TAG_INFO(PCIE_MIF, "len %d\n", len);
			for (i = 0; i < ROW; i++)
				if (!strncmp(lookup_regs[i][0], token, len)) {
					rc = kstrtou32(lookup_regs[i][1], 0, &offset);
					if (rc)
						match = 0;
					else
						match = 1;
					break;
				}

			if (!match) {
				SCSC_TAG_INFO(PCIE_MIF, "Register %s not Found!!\n", token);
				SCSC_TAG_INFO(PCIE_MIF, "Type 'cat /proc/driver/pcie_ctrl/pcie_dbg' to get register names\n");
			}
			break;
		/* value */
		case 1:
			if ((token[0] == '0') && (token[1] == 'x')) {
				if (kstrtou32(token, 16, &value)) {
					SCSC_TAG_INFO(PCIE_MIF, "Incorrect format,,,address should start by 0x\n");
					SCSC_TAG_INFO(PCIE_MIF, "Example: \"0xaaaabbbb 256 8\"\n");
					goto error;
				}
			} else {
				SCSC_TAG_INFO(PCIE_MIF, "Incorrect format,,,address should start by 0x\n");
				SCSC_TAG_INFO(PCIE_MIF, "Example: \"0xaaaabbbb 256 8\"\n");
				goto error;
			}
			break;
		}
		pass++;
	}
	if (pass != 2 && !match) {
		SCSC_TAG_INFO(PCIE_MIF, "Wrong format: <register> <value (hex)>\n");
		SCSC_TAG_INFO(PCIE_MIF, "Example: \"DEBUGADDR 0xaaaabbbb\"\n");
		goto error;
	}
	SCSC_TAG_INFO(PCIE_MIF, "Setting value 0x%x to register %s offset %d\n", value, lookup_regs[i][0], offset);
	pcie_mif_set_bar0_register(pcie_global, value, offset);
error:
	return count;
}

static int pcie_procfs_pcie_dbg_show(struct seq_file *m, void *v)
{
	struct scsc_bar0_reg bar0;

	if (!pcie_global) {
		seq_puts(m, "endpoint not registered");
		return 0;
	}

	pcie_mif_get_bar0(pcie_global, &bar0);

	seq_puts(m, "\n---------BAR0---------\n");

	seq_printf(m, "NEWMSG         0x%08X\n", bar0.NEWMSG);
	seq_printf(m, "SIGNATURE         0x%08X\n", bar0.SIGNATURE);
	seq_printf(m, "OFFSET         0x%08X\n", bar0.OFFSET);
	seq_printf(m, "RUNEN         0x%08X\n", bar0.RUNEN);
	seq_printf(m, "DEBUG         0x%08X\n", bar0.DEBUG);
	seq_printf(m, "AXIWCNT         0x%08X\n", bar0.AXIWCNT);
	seq_printf(m, "AXIRCNT         0x%08X\n", bar0.AXIRCNT);
	seq_printf(m, "AXIWADDR         0x%08X\n", bar0.AXIWADDR);
	seq_printf(m, "AXIRADDR         0x%08X\n", bar0.AXIRADDR);
	seq_printf(m, "TBD         0x%08X\n", bar0.TBD);
	seq_printf(m, "AXICTRL         0x%08X\n", bar0.AXICTRL);
	seq_printf(m, "AXIDATA         0x%08X\n", bar0.AXIDATA);
	seq_printf(m, "AXIRDBP         0x%08X\n", bar0.AXIRDBP);
	seq_printf(m, "IFAXIWCNT         0x%08X\n", bar0.IFAXIWCNT);
	seq_printf(m, "IFAXIRCNT         0x%08X\n", bar0.IFAXIRCNT);
	seq_printf(m, "IFAXIWADDR         0x%08X\n", bar0.IFAXIWADDR);
	seq_printf(m, "IFAXIRADDR         0x%08X\n", bar0.IFAXIRADDR);
	seq_printf(m, "IFAXICTRL         0x%08X\n", bar0.IFAXICTRL);
	seq_printf(m, "GRST         0x%08X\n", bar0.GRST);
	seq_printf(m, "AMBA2TRANSAXIWCNT         0x%08X\n", bar0.AMBA2TRANSAXIWCNT);
	seq_printf(m, "AMBA2TRANSAXIRCNT         0x%08X\n", bar0.AMBA2TRANSAXIRCNT);
	seq_printf(m, "AMBA2TRANSAXIWADDR         0x%08X\n", bar0.AMBA2TRANSAXIWADDR);
	seq_printf(m, "AMBA2TRANSAXIRADDR         0x%08X\n", bar0.AMBA2TRANSAXIRADDR);
	seq_printf(m, "AMBA2TRANSAXICTR         0x%08X\n", bar0.AMBA2TRANSAXICTR);
	seq_printf(m, "TRANS2PCIEREADALIGNAXIWCNT         0x%08X\n", bar0.TRANS2PCIEREADALIGNAXIWCNT);
	seq_printf(m, "TRANS2PCIEREADALIGNAXIRCNT         0x%08X\n", bar0.TRANS2PCIEREADALIGNAXIRCNT);
	seq_printf(m, "TRANS2PCIEREADALIGNAXIWADDR         0x%08X\n", bar0.TRANS2PCIEREADALIGNAXIWADDR);
	seq_printf(m, "TRANS2PCIEREADALIGNAXIRADDR         0x%08X\n", bar0.TRANS2PCIEREADALIGNAXIRADDR);
	seq_printf(m, "TRANS2PCIEREADALIGNAXICTRL         0x%08X\n", bar0.TRANS2PCIEREADALIGNAXICTRL);
	seq_printf(m, "READROUNDTRIPMIN         0x%08X\n", bar0.READROUNDTRIPMIN);
	seq_printf(m, "READROUNDTRIPMAX         0x%08X\n", bar0.READROUNDTRIPMAX);
	seq_printf(m, "READROUNDTRIPLAST         0x%08X\n", bar0.READROUNDTRIPLAST);
	seq_printf(m, "CPTAW0         0x%08X\n", bar0.CPTAW0);
	seq_printf(m, "CPTAW1         0x%08X\n", bar0.CPTAW1);
	seq_printf(m, "CPTAR0         0x%08X\n", bar0.CPTAR0);
	seq_printf(m, "CPTAR1         0x%08X\n", bar0.CPTAR1);
	seq_printf(m, "CPTB0         0x%08X\n", bar0.CPTB0);
	seq_printf(m, "CPTW0         0x%08X\n", bar0.CPTW0);
	seq_printf(m, "CPTW1         0x%08X\n", bar0.CPTW1);
	seq_printf(m, "CPTW2         0x%08X\n", bar0.CPTW2);
	seq_printf(m, "CPTR0         0x%08X\n", bar0.CPTR0);
	seq_printf(m, "CPTR1         0x%08X\n", bar0.CPTR1);
	seq_printf(m, "CPTR2         0x%08X\n", bar0.CPTR2);
	seq_printf(m, "CPTRES         0x%08X\n", bar0.CPTRES);
	seq_printf(m, "CPTAWDELAY         0x%08X\n", bar0.CPTAWDELAY);
	seq_printf(m, "CPTARDELAY         0x%08X\n", bar0.CPTARDELAY);
	seq_printf(m, "CPTSRTADDR         0x%08X\n", bar0.CPTSRTADDR);
	seq_printf(m, "CPTENDADDR         0x%08X\n", bar0.CPTENDADDR);
	seq_printf(m, "CPTSZLTHID         0x%08X\n", bar0.CPTSZLTHID);
	seq_printf(m, "CPTPHSEL         0x%08X\n", bar0.CPTPHSEL);
	seq_printf(m, "CPTRUN         0x%08X\n", bar0.CPTRUN);
	seq_printf(m, "FPGAVER         0x%08X\n", bar0.FPGAVER);
	return 0;
}

static const char *procdir = "driver/pcie_ctrl";

#define PCIE_DIRLEN 128


int pcie_create_proc_dir(struct pcie_mif *pcie)
{
	char                  dir[PCIE_DIRLEN];
	struct proc_dir_entry *parent;

	(void)snprintf(dir, sizeof(dir), "%s", procdir);
	parent = proc_mkdir(dir, NULL);
	if (parent) {
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3, 4, 0))
		parent->data = NULL;
#endif
		procfs_dir = parent;
		PCIE_PROCFS_ADD_FILE(NULL, pcie_trg, parent, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
		PCIE_PROCFS_SEQ_ADD_FILE(NULL, pcie_dbg, parent, S_IRUSR | S_IRGRP | S_IROTH);
	} else {
		SCSC_TAG_INFO(PCIE_MIF, "failed to create /proc dir\n");
		return -EINVAL;
	}

	pcie_global = pcie;

	return 0;

err:
	return -EINVAL;
}

void pcie_remove_proc_dir(void)
{
	if (procfs_dir) {
		char dir[PCIE_DIRLEN];

		PCIE_PROCFS_REMOVE_FILE(pcie_trg, procfs_dir);
		PCIE_PROCFS_REMOVE_FILE(pcie_dbg, procfs_dir);

		(void)snprintf(dir, sizeof(dir), "%s", procdir);
		remove_proc_entry(dir, NULL);
		procfs_dir = NULL;
	}

	pcie_global = NULL;
}
