/*
 * Copyright (C) 2014 Samsung Electronics Co.Ltd
 * http://www.samsung.com
 *
 * Shared memory driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
*/

#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/shm_ipc.h>
#include <linux/of_reserved_mem.h>

#ifdef CONFIG_CP_RAM_LOGGING
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/memblock.h>

#define MEMSHARE_DEV_NAME "memshare"
#endif

struct shm_plat_data {
	unsigned long p_addr;
	unsigned p_acpm_addr;
	void __iomem *v_boot;
	void __iomem *v_ipc;
	void __iomem *v_zmb;
	void __iomem *v_vss;
	void __iomem *v_acpm;
	unsigned t_size;
	unsigned cp_size;
	unsigned vss_size;
	unsigned vparam_size;
	unsigned acpm_size;
	unsigned ipc_off;
	unsigned ipc_size;
	unsigned zmb_off;
	unsigned zmb_size;

	unsigned long p_sysram_addr;
	unsigned t_sysram_size;
	int use_cp_memory_map;

#ifdef CONFIG_CP_RAM_LOGGING
	int cplog_on;
	unsigned long p_cplog_addr;
	unsigned cplog_size;
	void __iomem *v_cplog;

	char name[256];
	struct miscdevice mdev;
	int data_ready;
#endif
} pdata;

#ifdef CONFIG_CP_RAM_LOGGING
static int memshare_open(struct inode *inode, struct file *filep)
{
	shm_get_cplog_region();
	return 0;
}

static int memshare_release(struct inode *inode, struct file *filep)
{
	if (pdata.v_cplog) {
		vunmap(pdata.v_cplog);
		pdata.v_cplog = NULL;
	}
	return 0;
}

static ssize_t memshare_read(struct file *filep, char __user *buf,
		size_t count, loff_t *pos)
{
	struct shm_plat_data *rd_dev = &pdata;
	void *device_mem = NULL;
	unsigned long data_left = 0;
	unsigned long addr = 0;
	int copy_size = 0;
	int ret = 0;

	if ((filep->f_flags & O_NONBLOCK) && !rd_dev->data_ready)
		return -EAGAIN;

	data_left = rd_dev->cplog_size - *pos;
	addr = rd_dev->p_cplog_addr + *pos;

	/* EOF check */
	if (data_left == 0) {
		pr_info("%s(%s): Ramdump complete. %lld bytes read.", __func__,
				rd_dev->name, *pos);
		ret = 0;
		goto ramdump_done;
	}

	copy_size = min(count, (size_t)SZ_1M);
	copy_size = min((unsigned long)copy_size, data_left);
	device_mem = shm_get_cplog_region() + *pos;

	if (device_mem == NULL) {
		pr_err("%s(%s): Unable to ioremap: addr %lx, size %d\n", __func__,
				pdata.name, addr, copy_size);
		ret = -ENOMEM;
		goto ramdump_done;
	}

	if (copy_to_user(buf, device_mem, copy_size)) {
		pr_err("%s(%s): Couldn't copy all data to user.", __func__,
				rd_dev->name);
		ret = -EFAULT;
		goto ramdump_done;
	}

	*pos += copy_size;

	pr_debug("%s(%s): Read %d bytes from address %lx.", __func__,
			pdata.name, copy_size, addr);

	return copy_size;

ramdump_done:
	*pos = 0;
	return ret;
}

static const struct file_operations memshare_file_ops = {
	.open = memshare_open,
	.release = memshare_release,
	.read = memshare_read
};

static int create_memshare_device(struct shm_plat_data *pdata,
		const char *dev_name, struct device *parent)
{
	int ret = -1;

	if (!dev_name) {
		pr_err("%s: Invalid device name\n", __func__);
		goto create_memshare_device_exit;
	}

	if (!pdata) {
		pr_err("%s: Invalid pdata", __func__);
		goto create_memshare_device_exit;
	}

	pdata->data_ready = 0;

	snprintf(pdata->name, ARRAY_SIZE(pdata->name), "ramdump_%s",
			dev_name);

	pdata->mdev.minor = MISC_DYNAMIC_MINOR;
	pdata->mdev.name = pdata->name;
	pdata->mdev.fops = &memshare_file_ops;
	pdata->mdev.parent = parent;

	ret = misc_register(&pdata->mdev);

	if (ret) {
		pr_err("%s: misc_register failed for %s (%d)", __func__,
				pdata->name, ret);
	}
	else
		pdata->data_ready = 1;

create_memshare_device_exit:
	return ret;
}

static void destroy_memshare_device(void)
{
	misc_deregister(&pdata.mdev);
}
#endif

unsigned long shm_get_phys_base(void)
{
	return pdata.p_addr;
}

unsigned shm_get_phys_size(void)
{
	return pdata.t_size;
}

unsigned long shm_get_sysram_base(void)
{
	return pdata.p_sysram_addr;
}

unsigned shm_get_sysram_size(void)
{
	return pdata.t_sysram_size;
}

unsigned shm_get_boot_size(void)
{
	return pdata.ipc_off;
}

unsigned shm_get_ipc_rgn_offset(void)
{
	return pdata.ipc_off;
}

unsigned shm_get_ipc_rgn_size(void)
{
	return pdata.ipc_size;
}

unsigned shm_get_zmb_size(void)
{
	return pdata.zmb_size;
}

unsigned shm_get_vss_base(void)
{
	return shm_get_phys_base() + shm_get_cp_size();
}

unsigned shm_get_vss_size(void)
{
	return pdata.vss_size;
}

unsigned shm_get_vparam_base(void)
{
	return shm_get_phys_base() + shm_get_cp_size() + shm_get_vss_size() +
			shm_get_ipc_rgn_size() +shm_get_zmb_size();
}

unsigned shm_get_vparam_size(void)
{
	return pdata.vparam_size;
}

unsigned shm_get_acpm_size(void)
{
	return pdata.acpm_size;
}

unsigned shm_get_cp_size(void)
{
	return pdata.cp_size;
}

#ifdef CONFIG_CP_RAM_LOGGING
unsigned long shm_get_cplog_base(void)
{
	return pdata.p_cplog_addr;
}

unsigned shm_get_cplog_size(void)
{
	return pdata.cplog_size;
}

int shm_get_cplog_flag(void)
{
	return pdata.cplog_on;
}
#endif

int shm_get_use_cp_memory_map_flag(void)
{
	return pdata.use_cp_memory_map;
}

int shm_get_security_param3(unsigned long mode, u32 main_size, unsigned long *param)
{
	int ret = 0;

	switch (mode) {
	case 0: /* CP_BOOT_MODE_NORMAL */
		*param = main_size;
		break;
	case 1: /* CP_BOOT_MODE_DUMP */
#ifdef CP_NONSECURE_BOOT
		*param = pdata.p_addr;
#else
		*param = pdata.p_addr + pdata.ipc_off;
#endif
		break;
	case 2: /* CP_BOOT_RE_INIT */
		*param = 0;
		break;
	case 7: /* CP_BOOT_MODE_MANUAL */
		*param = main_size;
		break;
	default:
		pr_info("%s: Invalid sec_mode(%lu)\n", __func__, mode);
		ret = -EINVAL;
		break;
	}

	return ret;
}

int shm_get_security_param2(unsigned long mode, u32 bl_size, unsigned long *param)
{
	int ret = 0;

	switch (mode) {
	case 0: /* CP_BOOT_MODE_NORMAL */
	case 1: /* CP_BOOT_MODE_DUMP */
		*param = bl_size;
		break;
	case 2: /* CP_BOOT_RE_INIT */
		*param = 0;
		break;
	case 7: /* CP_BOOT_MODE_MANUAL */
		*param = pdata.p_addr + bl_size;
		break;
	default:
		pr_info("%s: Invalid sec_mode(%lu)\n", __func__, mode);
		ret = -EINVAL;
		break;
	}

	return ret;
}

void __iomem *shm_request_region(unsigned long sh_addr, unsigned size)
{
	int i;
	unsigned int num_pages = (size >> PAGE_SHIFT);
	pgprot_t prot = pgprot_writecombine(PAGE_KERNEL);
	struct page **pages;
	void *v_addr;

	if (!sh_addr)
		return NULL;

	if (size > (num_pages << PAGE_SHIFT))
		num_pages++;

	pages = kmalloc(sizeof(struct page *) * num_pages, GFP_ATOMIC);
	if (!pages) {
		pr_err("%s: pages allocation fail!\n", __func__);
		return NULL;
	}

	for (i = 0; i < (num_pages); i++) {
		pages[i] = phys_to_page(sh_addr);
		sh_addr += PAGE_SIZE;
	}

	v_addr = vmap(pages, num_pages, VM_MAP, prot);
	if (v_addr == NULL)
		pr_err("%s: Failed to vmap pages\n", __func__);

	kfree(pages);

	return (void __iomem *)v_addr;
}

void __iomem *shm_get_boot_region(void)
{
	if (!pdata.v_boot)
		pdata.v_boot = shm_request_region(pdata.p_addr, pdata.ipc_off);

	return pdata.v_boot;
}

void __iomem *shm_get_ipc_region(void)
{
	if (!pdata.v_ipc)
		pdata.v_ipc = shm_request_region(pdata.p_addr + pdata.ipc_off,
				pdata.ipc_size);

	return pdata.v_ipc;
}

void __iomem *shm_get_zmb_region(void)
{
	if (!pdata.v_zmb)
		pdata.v_zmb = (void __iomem *)phys_to_virt(pdata.p_addr + pdata.zmb_off);

	return pdata.v_zmb;
}

void __iomem *shm_get_vss_region(void)
{
	if (!pdata.v_vss)
		pdata.v_vss = shm_request_region(pdata.p_addr + pdata.cp_size,
				pdata.vss_size);

	return pdata.v_vss;
}

#define VSS_MAGIC_OFFSET 0x500000
void clean_vss_magic_code(void)
{
	u8* vss_base;
	u32 __iomem * vss_magic;

	pr_err("%s: set vss magic code as 0\n", __func__);

	vss_base = (u8*)shm_get_vss_region();
	vss_magic = (u32 __iomem *)(vss_base + VSS_MAGIC_OFFSET);

	/* set VSS magic code as 0*/
	iowrite32(0, vss_magic);
}

void __iomem *shm_get_acpm_region(void)
{
	if (!pdata.v_acpm)
		pdata.v_acpm = shm_request_region(pdata.p_acpm_addr,
				pdata.acpm_size);

	return pdata.v_acpm;
}

#ifdef CONFIG_CP_RAM_LOGGING
void __iomem *shm_get_cplog_region(void)
{
	if (!pdata.v_cplog)
		pdata.v_cplog = shm_request_region(pdata.p_cplog_addr,
				pdata.cplog_size);

	return pdata.v_cplog;
}
#endif

void shm_release_region(void *v_addr)
{
	vunmap(v_addr);
}

void shm_release_regions(void)
{
	if (pdata.v_boot)
		vunmap(pdata.v_boot);

	if (pdata.v_ipc)
		vunmap(pdata.v_ipc);

	if (pdata.v_vss)
		vunmap(pdata.v_vss);

	if (pdata.v_acpm)
		vunmap(pdata.v_acpm);

	if (pdata.v_zmb)
		vunmap(pdata.v_zmb);

#ifdef CONFIG_CP_RAM_LOGGING
	if (pdata.v_cplog)
		vunmap(pdata.v_cplog);
#endif
}

#ifdef CONFIG_CP_RAM_LOGGING
static void shm_free_reserved_mem(unsigned long addr, unsigned size)
{
	int i;
	struct page *page;

	pr_err("Release cplog reserved memory\n");
	free_memsize_reserved(addr, size);
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		page = phys_to_page(addr);
		addr += PAGE_SIZE;
		free_reserved_page(page);
	}
}
#endif

#ifdef CONFIG_OF_RESERVED_MEM
static int __init modem_if_reserved_mem_setup(struct reserved_mem *remem)
{
   pdata.p_addr = remem->base;
   pdata.t_size = remem->size;

   pr_err("%s: memory reserved: paddr=%lu, t_size=%u\n",
        __func__, pdata.p_addr, pdata.t_size);

   return 0;
}
RESERVEDMEM_OF_DECLARE(modem_if, "exynos,modem_if", modem_if_reserved_mem_setup);

#ifdef CONFIG_CP_RAM_LOGGING
static int __init modem_if_reserved_cplog_setup(struct reserved_mem *remem)
{
   pdata.p_cplog_addr = remem->base;
   pdata.cplog_size = remem->size;

   pr_err("%s: cplog memory reserved: paddr=%lu, t_size=%u\n",
        __func__, pdata.p_cplog_addr, pdata.cplog_size);

   return 0;
}
RESERVEDMEM_OF_DECLARE(cp_ram_logging, "exynos,cp_ram_logging",
		modem_if_reserved_cplog_setup);
#endif

#if !defined (CONFIG_SOC_EXYNOS7570)
static int __init deliver_cp_reserved_mem_setup(struct reserved_mem *remem)
{
   pdata.p_sysram_addr = remem->base;
   pdata.t_sysram_size = remem->size;

   pr_err("%s: memory reserved: paddr=%u, t_size=%u\n",
        __func__, (u32)remem->base, (u32)remem->size);

   return 0;
}
RESERVEDMEM_OF_DECLARE(deliver_cp, "exynos,deliver_cp", deliver_cp_reserved_mem_setup);
#endif
#endif

#ifdef CONFIG_CP_RAM_LOGGING
static int __init console_setup(char *str)
{
	if (!strcmp(str, "ON") || !strcmp(str, "on"))
		pdata.cplog_on = 1;

	pr_info("cplog_on=%s, %d\n", str, pdata.cplog_on);
	return 0;
 }
__setup("androidboot.cp_reserved_mem=", console_setup);
#endif

#define EXTERN_BIN_MAX_COUNT		5

struct extern_mem_bin_info_tag {
	unsigned int	ext_bin_tag;			// Tag
	unsigned int	ext_bin_addr;			// Offset address
	unsigned int	ext_bin_size;			// binary size
};

struct cp_reserved_map_table {
	unsigned int	table_id_ver;			// MEMn
	unsigned int	dram_size;			// dram size
	unsigned int	ext_mem_size;			// extern mem size
	unsigned int	ext_bin_count;			// extern mem size
	struct extern_mem_bin_info_tag sExtBin[EXTERN_BIN_MAX_COUNT];
	unsigned int	end_flag_not_used;		// Memory guard for CP boot code
};

struct cp_toc_element {
	char name[12];					// Binary name
	u32 b_offset;					// Binary offset in the file
	u32 m_offset;					// Memory Offset to be loaded
	u32 size;					// Binary size
	u32 crc;					// CRC value
	u32 toc_count;					// Reserved
} __packed;

struct cp_reserved_map_table cp_mem_map;

static int verify_cp_memory_map(struct cp_reserved_map_table *tab)
{
	unsigned int total_bin = tab->ext_bin_count;
	int i;
	int verify = 1;

	if (tab->ext_bin_count <= 0 || (tab->ext_bin_count > EXTERN_BIN_MAX_COUNT)) {
		verify = 0;
		goto exit;
	}

	for (i = 1; i < total_bin; i++) {
		if (cp_mem_map.sExtBin[i-1].ext_bin_addr + cp_mem_map.sExtBin[i-1].ext_bin_size
				!=  cp_mem_map.sExtBin[i].ext_bin_addr) {
			verify = 0;
			goto exit;
		}
	}
exit:
	return verify;
}

static int shm_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;
	int verify;
	unsigned int tmp;
	int i;
	char *ptr;
	struct device_node *np_acpm = of_find_node_by_name(NULL, "acpm_ipc");
	void __iomem *cp_mem_base;
	struct cp_toc_element *cp_toc_info;
	char table_id_ver[5];

	dev_err(dev, "%s: shmem driver init\n", __func__);

	cp_mem_base = shm_request_region(pdata.p_addr, PAGE_SIZE);
	dev_info(dev, "cp_mem_base: 0x%lx, 0x%pK\n", pdata.p_addr, cp_mem_base);

	/* get 2nd TOC table : BOOT bin info */
	cp_toc_info = (struct cp_toc_element *)(cp_mem_base + sizeof(struct cp_toc_element));
	dev_info(dev, "cp_boot_offset: 0x%lx\n", cp_toc_info->b_offset);

	/* 0xa0: cp memory map offset */
	memcpy(&cp_mem_map, cp_mem_base + cp_toc_info->b_offset + 0xa0, sizeof(struct cp_reserved_map_table));

	if (cp_mem_base) {
		vunmap(cp_mem_base);
		cp_mem_base = NULL;
	}

	tmp = ntohl(cp_mem_map.table_id_ver + 0x30);
	if (strncmp((const char *)&tmp, "MEM", 3) == 0) {
		verify = verify_cp_memory_map(&cp_mem_map);
		dev_info(dev, "CP memory map verification: %s\n", verify == 1 ? "PASS!" : "FAIL!");
		if (verify) {
			pdata.use_cp_memory_map = 1;
			strncpy(table_id_ver, (void *)&tmp, 4);
			table_id_ver[4] = '\0';
			pdata.cp_size = cp_mem_map.dram_size;
			dev_info(dev, "table_id_ver: %s\n", table_id_ver);
			dev_info(dev, "bin_count: %d\n", cp_mem_map.ext_bin_count);
			dev_info(dev, "cp dram_size: 0x%08X\n", pdata.cp_size);
			dev_info(dev, "ext_mem_size: 0x%08X\n", cp_mem_map.ext_mem_size);
			for (i = 0; i < cp_mem_map.ext_bin_count; i++) {
				tmp = ntohl(cp_mem_map.sExtBin[i].ext_bin_tag);
				ptr = ((char *)&tmp) + 1;

				if (strncmp((const char *)ptr, "IPC", 3) == 0) {
					pdata.ipc_off = cp_mem_map.sExtBin[i].ext_bin_addr;
					pdata.ipc_size = cp_mem_map.sExtBin[i].ext_bin_size;
					dev_err(dev, "IPC: 0x%08x: 0x%08X\n", pdata.ipc_off, pdata.ipc_size);
				} else if (strncmp((const char *)ptr, "VSS", 3) == 0) {
					pdata.vss_size = cp_mem_map.sExtBin[i].ext_bin_size;
					dev_err(dev, "VSS: 0x%08X: 0x%08X\n",
							cp_mem_map.sExtBin[i].ext_bin_addr, pdata.vss_size);
				} else if (strncmp((const char *)ptr, "VPA", 3) == 0) {
					pdata.vparam_size = cp_mem_map.sExtBin[i].ext_bin_size;
					dev_err(dev, "VSS PARAM: 0x%08X: 0x%08X\n",
							cp_mem_map.sExtBin[i].ext_bin_addr, pdata.vparam_size);
				} else if (strncmp((const char *)ptr, "ZMC", 3) == 0) {
					pdata.zmb_off = cp_mem_map.sExtBin[i].ext_bin_addr;
					pdata.zmb_size = cp_mem_map.sExtBin[i].ext_bin_size;
					dev_err(dev, "ZMC: 0x%08X: 0x%08X\n", pdata.zmb_off, pdata.zmb_size);
				} else if (strncmp((const char *)ptr, "LOG", 3) == 0) {
					dev_err(dev, "LOG: 0x%08X: 0x%08X\n",
							cp_mem_map.sExtBin[i].ext_bin_addr, cp_mem_map.sExtBin[i].ext_bin_size);
				}
			}
		} else {
			WARN_ONCE(!verify, "cp memory map verification fail\n");
			return -EINVAL;
		}
	} else if (dev->of_node) {
		ret = of_property_read_u32(dev->of_node, "shmem,ipc_offset",
				&pdata.ipc_off);
		if (ret) {
			dev_err(dev, "failed to get property, ipc_offset\n");
			return -EINVAL;
		}

		ret = of_property_read_u32(dev->of_node, "shmem,ipc_size",
				&pdata.ipc_size);
		if (ret) {
			dev_err(dev, "failed to get property, ipc_size\n");
			return -EINVAL;
		}

#ifdef CONFIG_CP_ZEROCOPY
		ret = of_property_read_u32(dev->of_node, "shmem,zmb_offset",
				&pdata.zmb_off);
		if (ret) {
			dev_err(dev, "failed to get property, zmb_offset\n");
			return -EINVAL;
		}

		ret = of_property_read_u32(dev->of_node, "shmem,zmb_size",
				&pdata.zmb_size);
		if (ret) {
			dev_err(dev, "failed to get property, zmb_size\n");
			return -EINVAL;
		}
#endif

		ret = of_property_read_u32(dev->of_node, "shmem,cp_size",
				&pdata.cp_size);
		if (ret)
			dev_err(dev, "failed to get property, cp_size\n");

		ret = of_property_read_u32(dev->of_node, "shmem,vss_size",
				&pdata.vss_size);
		if (ret)
			dev_err(dev, "failed to get property, vss_size\n");
	} else {
		/* To do: In case of non-DT */
	}

	if (np_acpm) {
		ret = of_property_read_u32(np_acpm, "dump-base",
				&pdata.p_acpm_addr);
		if (ret)
			dev_err(dev, "failed to get property, acpm_base\n");

		ret = of_property_read_u32(np_acpm, "dump-size",
				&pdata.acpm_size);
		if (ret)
			dev_err(dev, "failed to get property, acpm_size\n");
	}

	dev_info(dev, "paddr=0x%lX, total_size=0x%08X, ipc_off=0x%08X, ipc_size=0x%08X, cp_size=0x%08X, vss_size=0x%08X\n",
		pdata.p_addr, pdata.t_size, pdata.ipc_off, pdata.ipc_size,
		pdata.cp_size, pdata.vss_size);

	dev_info(dev, "zmb_off=0x%08X, zmb_size=0x%08X\n", pdata.zmb_off,
			pdata.zmb_size);

	dev_info(dev, "acpm_base=0x%08X, acpm_size=0x%08X\n", pdata.p_acpm_addr,
			pdata.acpm_size);

#ifdef CONFIG_CP_RAM_LOGGING
	if (pdata.cplog_on) {
		dev_err(dev, "cplog_base=0x%08lX, cplog_size=0x%08X\n",
				pdata.p_cplog_addr, pdata.cplog_size);

		/* create memshare driver */
		ret = create_memshare_device(&pdata, MEMSHARE_DEV_NAME, dev);
		if (ret) {
			dev_err(dev, "failed to create memshare device\n");
		}
	} else {
		shm_free_reserved_mem(pdata.p_cplog_addr, pdata.cplog_size);
	}
#endif
	return 0;
}

static int shm_remove(struct platform_device *pdev)
{
#ifdef CONFIG_CP_RAM_LOGGING
	destroy_memshare_device();
#endif
	return 0;
}

static const struct of_device_id exynos_shm_dt_match[] = {
		{ .compatible = "samsung,exynos7580-shm_ipc", },
		{ .compatible = "samsung,exynos8890-shm_ipc", },
		{ .compatible = "samsung,exynos7870-shm_ipc", },
		{ .compatible = "samsung,exynos-shm_ipc", },
		{},
};
MODULE_DEVICE_TABLE(of, exynos_shm_dt_match);

static struct platform_driver shmem_driver = {
	.probe		= shm_probe,
	.remove		= shm_remove,
	.driver		= {
		.name = "shm_ipc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_shm_dt_match),
		.suppress_bind_attrs = true,
	},
};
module_platform_driver(shmem_driver);

MODULE_DESCRIPTION("");
MODULE_AUTHOR("");
MODULE_LICENSE("GPL");
