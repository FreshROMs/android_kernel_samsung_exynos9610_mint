/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/of_platform.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_EXYNOS_PD
#include <soc/samsung/exynos-pd.h>
#endif
#include <linux/suspend.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/sched/clock.h>

#include <soc/samsung/cal-if.h>
#include <soc/samsung/bcm.h>
#include <linux/memblock.h>
#include <asm/map.h>
#include <asm/tlbflush.h>

#define BCM_BDGGEN
#ifdef BCM_BDGGEN
#define BCM_BDG(x...) pr_info("bcm: " x)
#else
#define BCM_BDG(x...) do {} while (0)
#endif

#define BCM_MAX_DATA		(4 * 1024 * 1024 / sizeof(struct output_data))
#define MAX_STR			4 * 1024
#define BCM_SIZE		(SZ_64K)
#define NUM_CLK_MAX		8
#define FILE_STR		32

enum outform {
	OUT_FILE = 1,
	OUT_LOG,
};

static size_t fd_virt_addr;
static struct notifier_block panic_nb;

struct bcm_info {
	char *pd_name;
	int on;
	char *clk_name[NUM_CLK_MAX];
	struct clk *clk[NUM_CLK_MAX];
};

static struct fw_system_func {
	int (*fw_show_tb)(char *);
	int (*fw_show_cmd)(char *);
	char * (*fw_cmd)(const char *);
	struct output_data *(*fw_init)(const int *);
	struct output_data *(*fw_stop)(u64, unsigned long (*func)(unsigned int),const int *);
	int (*fw_periodic)(u64, unsigned long (*func)(unsigned int), const int *);
	char *(*fw_getresult)(char *str);
	int (*fw_exit)(void);
	int (*pd_sync)(struct bcm_info *, int, u64);
	struct bcm_info *(*get_pd)(void);
	int (*get_outform)(void);
} *fw_func;

static struct os_system_func {
	struct output_data *fdata;
	struct output_data *ldata;
	void __iomem *(*remap)(phys_addr_t phys_addr, size_t size);
	void (*unmap)(volatile void __iomem *addr);
	int (*sprint)(char *buf, const char *fmt, ...);
	int (*print)(const char *, ...);
} os_func;

static struct vm_struct bcm_early_vm;
static DEFINE_SPINLOCK(bcm_lock);
static char input_file[FILE_STR] = "/data/bcm.bin";

static struct hrtimer bcm_hrtimer;
static struct workqueue_struct *bcm_wq;
static struct bcm_work_struct {
	struct work_struct work;
	char *data;
} *work_file_out;

static void write_file(struct work_struct *work)
{
	char *result;
	char *filename;
	unsigned long flags;
	struct file *fp = NULL;
	mm_segment_t old_fs = get_fs();

	result = kzalloc(sizeof(char) * MAX_STR, GFP_KERNEL);
	if (!result) {
		pr_err("result memory allocation fail!\n");
		goto err_alloc;
	}

	spin_lock_irqsave(&bcm_lock, flags);
	if (fw_func)
		filename = fw_func->fw_getresult(result);
	spin_unlock_irqrestore(&bcm_lock, flags);
	if (!fw_func || !filename) {
		goto err_firm;
	}

	set_fs(KERNEL_DS);

	fp = filp_open(filename, O_WRONLY|O_CREAT|O_APPEND, 0);
	if (IS_ERR(fp)) {
		pr_err("name : %s filp_open fail!!\n", filename);
		goto err_filp_open;

	}
	do {
		if (result)
			vfs_write(fp, result, strlen(result), &fp->f_pos);
		spin_lock_irqsave(&bcm_lock, flags);
		filename = fw_func->fw_getresult(result);
		spin_unlock_irqrestore(&bcm_lock, flags);
	} while(filename);

	filp_close(fp, NULL);
err_filp_open:
	set_fs(old_fs);
err_firm:
	kfree(result);
err_alloc:
	return;
}

static void bcm_file_out (char *data)
{
	work_file_out->data = data;

	queue_work(bcm_wq, (struct work_struct *)work_file_out);
}

static u64 get_time(void)
{
	return sched_clock();
}

#ifdef CONFIG_EXYNOS_PD
int bcm_pd_sync(struct bcm_info *bcm, bool on)
{
	int ret = 0;
	unsigned long flags;
	int i;

	if (on ^ bcm->on) {
		if (on) {
			for (i = 0; bcm->clk[i] && i < NUM_CLK_MAX; i++)
				clk_enable(bcm->clk[i]);
		}
		spin_lock_irqsave(&bcm_lock, flags);
		if (fw_func)
			ret = fw_func->pd_sync(bcm, on, get_time());
		spin_unlock_irqrestore(&bcm_lock, flags);

		if (!on) {
			for (i = 0; bcm->clk[i] && i < NUM_CLK_MAX; i++)
				clk_disable(bcm->clk[i]);
		}
	}
	return ret;
}
EXPORT_SYMBOL(bcm_pd_sync);
#endif

static enum hrtimer_restart monitor_fn(struct hrtimer *hrtimer)
{
	unsigned long flags;
	int duration;
	enum hrtimer_restart ret = HRTIMER_NORESTART;

	spin_lock_irqsave(&bcm_lock, flags);
	if (fw_func) {
		duration = fw_func->fw_periodic(get_time(),
						cal_dfs_cached_get_rate, NULL);
	}
	spin_unlock_irqrestore(&bcm_lock, flags);

	if (duration > 0) {
		hrtimer_forward_now(hrtimer, ns_to_ktime(duration *
							 NSEC_PER_USEC));
		ret = HRTIMER_RESTART;
	}
	return ret;
}

struct output_data *bcm_start(const int *usr)
{
	unsigned long flags;
	struct output_data *data = NULL;
	int duration = 0;
	if (fw_func) {
		spin_lock_irqsave(&bcm_lock, flags);
		if (!fw_func) {
			spin_unlock_irqrestore(&bcm_lock, flags);
			return data;
		}

		data = fw_func->fw_init(usr);
		if (data) {
			duration = fw_func->fw_periodic(get_time(),
					cal_dfs_cached_get_rate, usr);
		}
		spin_unlock_irqrestore(&bcm_lock, flags);
		if (duration > 0) {
			if (bcm_hrtimer.state)
				hrtimer_try_to_cancel(&bcm_hrtimer);
			if (!bcm_hrtimer.state)
				hrtimer_start(&bcm_hrtimer,
						ns_to_ktime(duration *
							    NSEC_PER_USEC),
						HRTIMER_MODE_REL);
		}
	}
	return data;
}
EXPORT_SYMBOL(bcm_start);

static int bcm_log(void)
{
	unsigned long flags;
	spin_lock_irqsave(&bcm_lock, flags);
	while (fw_func->fw_getresult(NULL));
	spin_unlock_irqrestore(&bcm_lock, flags);
	return 0;
}

struct output_data *bcm_stop(const int *usr)
{
	unsigned long flags;
	struct output_data *data = NULL;
	if (fw_func) {
		spin_lock_irqsave(&bcm_lock, flags);
		if (!fw_func) {
			spin_unlock_irqrestore(&bcm_lock, flags);
			return data;
		}
		data = fw_func->fw_stop(get_time(),
				cal_dfs_cached_get_rate, usr);
		if (data)
			hrtimer_try_to_cancel(&bcm_hrtimer);
		spin_unlock_irqrestore(&bcm_lock, flags);
		switch (fw_func->get_outform()) {
		case OUT_FILE:
			bcm_file_out(NULL);
			break;
		case OUT_LOG:
			bcm_log();
			break;
		default:
			break;
		}
	}
	return data;
}
EXPORT_SYMBOL(bcm_stop);

static void __iomem *bcm_ioremap(phys_addr_t phys_addr, size_t size)
{
	void __iomem *ret;
	ret =  ioremap(phys_addr, size);
	if (!ret)
		pr_err("failed to map bcm physical address\n");
	return ret;
}

typedef struct fw_system_func*(*start_up_func_t)(void **func);

#ifdef CONFIG_EXYNOS_PD
static void pd_init(void)
{
	struct exynos_pm_domain *exypd = NULL;
	struct bcm_info *bcm;
	int ret;
	int i;
	while (bcm = fw_func->get_pd(), bcm) {
		for (i = 0; i < NUM_CLK_MAX; i++){
			if (bcm->clk_name[i]) {
				bcm->clk[i] = clk_get(NULL, bcm->clk_name[i]);
				if (IS_ERR(bcm->clk[i])) {
					pr_err("failed to get clk %s\n",
						bcm->clk_name[i]);
				} else {
					ret = clk_prepare(bcm->clk[i]);
					if (ret < 0)
						pr_err("failed to prepare clk %s\n",
						       bcm->clk_name[i]);
				}
			} else {
				bcm->clk[i] = NULL;
				break;
			}
		}
		exypd = NULL;
		bcm->on = false;
		exypd = exynos_pd_lookup_name(bcm->pd_name);
		if (exypd) {
			mutex_lock(&exypd->access_lock);
			exypd->bcm = bcm;
			if (cal_pd_status(exypd->cal_pdid)) {
				bcm_pd_sync(bcm, true);
			}
			mutex_unlock(&exypd->access_lock);
		} else {
			bcm_pd_sync(bcm, true);
		}
	}
}
#endif

static int load_bcm_bin(struct work_struct *work)
{
	int ret = 0;
	struct file *fp = NULL;
	long fsize, nread;
	unsigned long flags;
	u8 *buf = NULL;
	char *lib_isp = NULL;
	mm_segment_t old_fs;

	os_func.print = printk;
	os_func.sprint = sprintf;
	os_func.remap = bcm_ioremap;
	os_func.unmap = iounmap;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open(input_file, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		pr_err("name : %s filp_open fail!!\n", input_file);
		ret = -EIO;
		goto err_fopen;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	BCM_BDG("start, file path %s, size %ld Bytes\n",
		input_file, fsize);
	buf = vmalloc(fsize);
	if (!buf) {
		pr_err("failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;

	}

	nread = vfs_read(fp, (char __user *)buf, fsize, &fp->f_pos);
	if (nread != fsize) {
		pr_err("failed to read firmware file, %ld Bytes\n", nread);
		ret = -EIO;
		goto err_vfs_read;
	}

	lib_isp = (char *)fd_virt_addr;
	/* TODO: Must change below size of reserved memory */
	memset((char *)fd_virt_addr, 0x0, BCM_SIZE);

	spin_lock_irqsave(&bcm_lock, flags);
	flush_icache_range((unsigned long)lib_isp,
			   (unsigned long)lib_isp + BCM_SIZE);
	memcpy((void *)lib_isp, (void *)buf, fsize);
	flush_cache_all();

	spin_unlock_irqrestore(&bcm_lock, flags);
	fw_func = ((start_up_func_t)lib_isp)((void **)&os_func);
#ifdef CONFIG_EXYNOS_PD
	pd_init();
#endif

err_vfs_read:
	vfree((void *)buf);
err_alloc:
	filp_close(fp, NULL);
err_fopen:
	set_fs(old_fs);
	return ret;
}

#ifdef CONFIG_EXYNOS_PD
static void pd_exit(void)
{
	struct bcm_info *bcm;
	struct exynos_pm_domain *exypd = NULL;
	int i;
	while (bcm = fw_func->get_pd(), bcm) {
		exypd = exynos_pd_lookup_name(bcm->pd_name);
		if (exypd) {
			mutex_lock(&exypd->access_lock);
			exypd->bcm = NULL;
			bcm_pd_sync(bcm, false);
			mutex_unlock(&exypd->access_lock);
		} else {
			bcm_pd_sync(bcm, false);
		}
		if (bcm->on) {
			for (i = 0; bcm->clk[i] && i < NUM_CLK_MAX; i++)
				clk_disable_unprepare(bcm->clk[i]);
		} else {
			for (i = 0; bcm->clk[i] && i < NUM_CLK_MAX; i++)
				clk_unprepare(bcm->clk[i]);
		}
	}
}
#endif

struct page_change_data {
	pgprot_t set_mask;
	pgprot_t clear_mask;
};

static int bcm_change_page_range(pte_t *ptep, pgtable_t token, unsigned long addr,
			void *data)
{
	struct page_change_data *cdata = data;
	pte_t pte = *ptep;

	pte = clear_pte_bit(pte, cdata->clear_mask);
	pte = set_pte_bit(pte, cdata->set_mask);

	set_pte(ptep, pte);
	return 0;
}

static int bcm_change_memory_common(unsigned long addr, int numpages,
				pgprot_t set_mask, pgprot_t clear_mask)
{
	unsigned long start = addr;
	unsigned long size = PAGE_SIZE*numpages;
	unsigned long end = start + size;
	int ret;
	struct page_change_data data;

	if (!PAGE_ALIGNED(addr)) {
		start &= PAGE_MASK;
		end = start + size;
		WARN_ON_ONCE(1);
	}

	if (!numpages)
		return 0;

	data.set_mask = set_mask;
	data.clear_mask = clear_mask;

	ret = apply_to_page_range(&init_mm, start, size, bcm_change_page_range,
					&data);

	flush_tlb_kernel_range(start, end);
	return ret;
}
static ssize_t store_load_bcm_fw(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned long flags;
	int ret;
	bool value = true;

	if (buf[0] == '0') {
		value = false;
	} else if (buf[0] == '/' && strlen(buf) < FILE_STR) {
		ret = sscanf(buf, "%s", input_file);
		if (ret != 1) {
			dev_err(dev, "failed sscanf %d\n", ret);
			return -EINVAL;
		}
	}

	/* bcm stop and pd unprepare */
	if (fw_func) {
#ifdef CONFIG_EXYNOS_PD
		pd_exit();
#endif
		spin_lock_irqsave(&bcm_lock, flags);
		if (fw_func->fw_stop(0, NULL, NULL))
			hrtimer_try_to_cancel(&bcm_hrtimer);
		fw_func->fw_cmd("0");
		spin_unlock_irqrestore(&bcm_lock, flags);
		fw_func->fw_exit();
		spin_lock_irqsave(&bcm_lock, flags);
		fw_func = NULL;
		spin_unlock_irqrestore(&bcm_lock, flags);
	}
	if (value) {
		if (!os_func.fdata) {
			os_func.fdata = kzalloc(sizeof(struct output_data) *
						BCM_MAX_DATA, GFP_KERNEL);
			os_func.ldata = os_func.fdata + BCM_MAX_DATA;
		} else {
			memset((char *)os_func.fdata, 0x0,
			       sizeof(struct output_data) * BCM_MAX_DATA);
		}

		bcm_change_memory_common((unsigned long)bcm_early_vm.addr,
					 BCM_SIZE,
					 __pgprot(0),
					 __pgprot(PTE_PXN));
		/* load binary */
		ret = load_bcm_bin(NULL);
		if (!ret)
			return count;
	}

	kfree(os_func.fdata);
	os_func.fdata = NULL;
	os_func.ldata = NULL;
	return count;
}

static ssize_t show_load_bcm_fw(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;

	count += snprintf(buf, PAGE_SIZE, "[BCM] addr %llx size %d: ",
			  (u64)fd_virt_addr, BCM_SIZE);
	if(fw_func) {
		count += snprintf(buf + count, PAGE_SIZE, "%s done\n",
				  input_file);
		if (fw_func->fw_show_tb)
			count += fw_func->fw_show_tb(buf + count);
	} else {
		count += snprintf(buf + count, PAGE_SIZE, "%s not yet\n",
				  input_file);
	}
	return count;
}

#define BCM_START 1
#define BCM_STOP 0

static ssize_t store_cmd_bcm_fw(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	char * info_str = NULL;
	int cmd;
	int option = 1;
	int ret = 0;
	unsigned long flags;

	if (fw_func) {
		ret = sscanf(buf, "%d %d", &cmd, &option);
		switch (cmd) {
		case BCM_STOP:
			spin_lock_irqsave(&bcm_lock, flags);
			if (fw_func->fw_stop(get_time(),
					cal_dfs_cached_get_rate, NULL))
				hrtimer_try_to_cancel(&bcm_hrtimer);
			info_str = fw_func->fw_cmd(buf);
			spin_unlock_irqrestore(&bcm_lock, flags);
			switch (option) {
			case OUT_FILE:
				write_file(NULL);
				break;
			case OUT_LOG:
				bcm_log();
				break;
			default:
				break;
			}
			break;
		case BCM_START:
			spin_lock_irqsave(&bcm_lock, flags);
			info_str = fw_func->fw_cmd(buf);
			spin_unlock_irqrestore(&bcm_lock, flags);
			if (info_str && option)
				bcm_start(NULL);
			break;
		default:
#ifdef CONFIG_EXYNOS_PD
			pd_exit();
#endif
			spin_lock_irqsave(&bcm_lock, flags);
			if (fw_func->fw_stop(get_time(),
					cal_dfs_cached_get_rate, NULL))
				hrtimer_try_to_cancel(&bcm_hrtimer);
			info_str = fw_func->fw_cmd(buf);
			spin_unlock_irqrestore(&bcm_lock, flags);
#ifdef CONFIG_EXYNOS_PD
			pd_init();
#endif
			break;
		}

		if (info_str)
			BCM_BDG ("command: %s\n", info_str);
	} else {
		BCM_BDG ("bcm binary is not loaded!\n");
	}

	return count;
}

static ssize_t show_cmd_bcm_fw(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	ssize_t count = 0;
	unsigned long flags;

	if (fw_func) {
		spin_lock_irqsave(&bcm_lock, flags);

		if (fw_func->fw_show_cmd)
			count += fw_func->fw_show_cmd(buf);

		spin_unlock_irqrestore(&bcm_lock, flags);
	} else {
		BCM_BDG ("bcm binary is not loaded!\n");
	}
	return count;
}
static DEVICE_ATTR(load_bin, 0640, show_load_bcm_fw, store_load_bcm_fw);
static DEVICE_ATTR(command, 0640, show_cmd_bcm_fw, store_cmd_bcm_fw);

static struct attribute *bcm_sysfs_entries[] = {
	&dev_attr_load_bin.attr,
	&dev_attr_command.attr,
	NULL,
};

static struct attribute_group bcm_attr_group = {
	.attrs	= bcm_sysfs_entries,
};

static int exynos_bcm_notifier_event(struct notifier_block *this,
		unsigned long event,
		void *ptr)
{
	unsigned long flags;

	if (fw_func) {
		switch ((unsigned int)event) {
		case PM_POST_SUSPEND:
			bcm_start(NULL);
			return NOTIFY_OK;
		case PM_SUSPEND_PREPARE:
			spin_lock_irqsave(&bcm_lock, flags);
			if (fw_func) {
				if (fw_func->fw_stop(get_time(),
						     cal_dfs_cached_get_rate, NULL))
					hrtimer_try_to_cancel(&bcm_hrtimer);
				fw_func->fw_cmd(NULL);
			}
			spin_unlock_irqrestore(&bcm_lock, flags);
			return NOTIFY_OK;
		}
	}

	return NOTIFY_DONE;
}

static struct notifier_block exynos_bcm_notifier = {
	.notifier_call = exynos_bcm_notifier_event,
};

static int exynos_bcm_notify_panic(struct notifier_block *nb,
		unsigned long event, void *unused)
{
	if (fw_func) {
		unsigned long flags;
		spin_lock_irqsave(&bcm_lock, flags);
		if (!fw_func) {
			spin_unlock_irqrestore(&bcm_lock, flags);
			return NOTIFY_DONE;
		}
		fw_func->fw_stop(get_time(),
				 cal_dfs_cached_get_rate, NULL);
		while (fw_func->fw_getresult(NULL));
		spin_unlock_irqrestore(&bcm_lock, flags);
	}

	return NOTIFY_DONE;
}

static int bcm_probe(struct platform_device *pdev)
{
	int ret;
	int page_size, i;
	struct page *page;
	struct page **pages;

	page_size = bcm_early_vm.size / PAGE_SIZE;
	pages = kzalloc(sizeof(struct page *) * page_size, GFP_KERNEL);
	if (!pages) {
		pr_err("%s: could not alloc pages\n", __func__);
		return -ENOMEM;
	}
	page = phys_to_page(bcm_early_vm.phys_addr);
	for (i = 0; i < page_size; i++)
		pages[i] = page++;
	ret = map_vm_area(&bcm_early_vm, PAGE_KERNEL, pages);
	if (ret) {
		dev_err(&pdev->dev, "failed to mapping between virt and phys for firmware");
		kfree(pages);
		return -ENOMEM;
	}
	kfree(pages);

	ret = sysfs_create_group(&pdev->dev.kobj, &bcm_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "failed create sysfs for sci debug data\n");
		goto err_sysfs;
	}

	bcm_wq = create_freezable_workqueue("bcm_wq");
	if (IS_ERR(bcm_wq)) {
		pr_err("%s: couldn't create workqueue\n", __FILE__);
		goto err_workqueue;
	}

	work_file_out = (struct bcm_work_struct *)
		devm_kzalloc(&pdev->dev, sizeof(struct bcm_work_struct),
			     GFP_KERNEL);
	if(!work_file_out) {
		goto err_file_out;
	}
	INIT_WORK((struct work_struct *)work_file_out, write_file);

	register_pm_notifier(&exynos_bcm_notifier);

	hrtimer_init(&bcm_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bcm_hrtimer.function = &monitor_fn;


	panic_nb.notifier_call = exynos_bcm_notify_panic;
	panic_nb.next = NULL;
	panic_nb.priority = 0;
	atomic_notifier_chain_register(&panic_notifier_list, &panic_nb);

	BCM_BDG("bcm driver is probed\n");

	return 0;

err_file_out:
	destroy_workqueue(bcm_wq);
err_workqueue:
	sysfs_remove_group(&pdev->dev.kobj, &bcm_attr_group);
err_sysfs:
	return 0;
}

static int bcm_remove(struct platform_device *pdev)
{
	unregister_pm_notifier(&exynos_bcm_notifier);
	destroy_workqueue(bcm_wq);
	sysfs_remove_group(&pdev->dev.kobj, &bcm_attr_group);
	return 0;
}

static const struct of_device_id bcm_dt_match[] = {
	{
		.compatible = "samsung,bcm",
	},
	{},
};

static struct platform_driver bcm_driver = {
	.probe		= bcm_probe,
	.remove		= bcm_remove,
	.driver		= {
		.name	= "bcm",
		.owner	= THIS_MODULE,
	},
};

static struct platform_device bcm_device = {
	.name	= "bcm",
	.id	= -1,
};

static int __init bcm_setup(char *str)
{
	if (kstrtoul(str, 0, (unsigned long *)&fd_virt_addr))
		goto out;

	bcm_early_vm.phys_addr = memblock_alloc(BCM_SIZE, SZ_4K);
	bcm_early_vm.addr = (void *)fd_virt_addr;
	bcm_early_vm.size = BCM_SIZE + PAGE_SIZE;

	vm_area_add_early(&bcm_early_vm);

	return 0;
out:
	return -1;
}
__setup("bcm_setup=", bcm_setup);

static int __init bcm_drv_register(void)
{
	int ret;
	BCM_BDG("%s: bcm init\n", __func__);

	if (!fd_virt_addr)
		return -EINVAL;

	ret = platform_device_register(&bcm_device );
	if (ret)
		return ret;

	return platform_driver_register(&bcm_driver);
}
late_initcall(bcm_drv_register);

MODULE_AUTHOR("Seokju Yoon <sukju.yoon@samsung.com>");
MODULE_DESCRIPTION("Samsung BCM driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("interface:bcm");
