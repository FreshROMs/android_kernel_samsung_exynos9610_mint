/* sound/soc/samsung/abox/abox.c
 *
 * ALSA SoC Audio Layer - Samsung Abox driver
 *
 * Copyright (c) 2016 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
/* #define DEBUG */
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/regmap.h>
#include <linux/exynos_iovmm.h>
#include <linux/workqueue.h>
#include <linux/smc.h>
#include <linux/delay.h>
#include <linux/suspend.h>
#include <linux/sched/clock.h>
#include <linux/shm_ipc.h>
#include <linux/modem_notifier.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm_params.h>
#include <sound/tlv.h>
#include <sound/samsung/abox.h>
#include <sound/samsung/vts.h>
#include <linux/exynos_iovmm.h>

#include <soc/samsung/exynos-pmu.h>
#ifdef CONFIG_EXYNOS_ITMON
#include <soc/samsung/exynos-itmon.h>
#endif
#include "../../../../drivers/iommu/exynos-iommu.h"

#include "abox_util.h"
#include "abox_dbg.h"
#include "abox_log.h"
#include "abox_dump.h"
#include "abox_gic.h"
#include "abox_failsafe.h"
#include "abox_if.h"
#include "abox_vdma.h"
#include "abox_effect.h"
#ifdef CONFIG_SCSC_BT
#include "abox_bt.h"
#endif
#include "abox.h"

#undef EMULATOR
#ifdef EMULATOR
static void __iomem *pmu_alive;
static void update_mask_value(void __iomem *sfr,
		unsigned int mask, unsigned int value)
{
	unsigned int sfr_value = readl(sfr);

	set_mask_value(sfr_value, mask, value);
	writel(sfr_value, sfr);
}
#endif

#if IS_ENABLED(CONFIG_SOC_EXYNOS8895)
#define GPIO_MODE_ABOX_SYS_PWR_REG		(0x1308)
#define PAD_RETENTION_ABOX_OPTION		(0x3048)
#define ABOX_MAGIC				(0x0814)
#define ABOX_MAGIC_VALUE			(0xAB0CAB0C)
#define ABOX_CPU_CONFIGURATION			(0x2520)
#define ABOX_CPU_LOCAL_PWR_CFG			(0x00000001)
#define ABOX_CPU_STATUS				(0x2524)
#define ABOX_CPU_STATUS_STATUS_MASK		(0x00000001)
#define ABOX_CPU_STANDBY			ABOX_CPU_STATUS
#define ABOX_CPU_STANDBY_WFE_MASK		(0x20000000)
#define ABOX_CPU_STANDBY_WFI_MASK		(0x10000000)
#define ABOX_CPU_OPTION				(0x2528)
#define ABOX_CPU_OPTION_USE_STANDBYWFE_MASK	(0x00020000)
#define ABOX_CPU_OPTION_USE_STANDBYWFI_MASK	(0x00010000)
#define ABOX_CPU_OPTION_ENABLE_CPU_MASK		(0x00008000)
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9810)
#define GPIO_MODE_ABOX_SYS_PWR_REG		(0x1424)
#define PAD_RETENTION_ABOX_OPTION		(0x4170)
#define ABOX_MAGIC				(0x0814)
#define ABOX_MAGIC_VALUE			(0xAB0CAB0C)
#define ABOX_CPU_CONFIGURATION			(0x415C)
#define ABOX_CPU_LOCAL_PWR_CFG			(0x00000001)
#define ABOX_CPU_STATUS				(0x4160)
#define ABOX_CPU_STATUS_STATUS_MASK		(0x00000001)
#define ABOX_CPU_STANDBY			(0x3804)
#define ABOX_CPU_STANDBY_WFE_MASK		(0x20000000)
#define ABOX_CPU_STANDBY_WFI_MASK		(0x10000000)
#define ABOX_CPU_OPTION				(0x4164)
#define ABOX_CPU_OPTION_ENABLE_CPU_MASK		(0x10000000)
#elif IS_ENABLED(CONFIG_SOC_EXYNOS9610)
#define GPIO_MODE_ABOX_SYS_PWR_REG		(0x1308)
#define PAD_RETENTION_ABOX_OPTION		(0x3048)
#define ABOX_MAGIC				(0x0814)
#define ABOX_MAGIC_VALUE			(0xAB0CAB0C)
#define ABOX_CPU_CONFIGURATION			(0x2520)
#define ABOX_CPU_LOCAL_PWR_CFG			(0x00000001)
#define ABOX_CPU_STATUS				(0x2524)
#define ABOX_CPU_STATUS_STATUS_MASK		(0x00000001)
#define ABOX_CPU_STANDBY			(0x2524)
#define ABOX_CPU_STANDBY_WFE_MASK		(0x20000000)
#define ABOX_CPU_STANDBY_WFI_MASK		(0x10000000)
#define ABOX_CPU_OPTION				(0x2528)
#define ABOX_CPU_OPTION_ENABLE_CPU_MASK		(0x00008000)
#endif

#define DEFAULT_CPU_GEAR_ID		(0xAB0CDEFA)
#define BOOT_CPU_GEAR_ID		(0xB00DB00D)
#define TEST_CPU_GEAR_ID		(DEFAULT_CPU_GEAR_ID + 1)
#define DEFAULT_LIT_FREQ_ID		DEFAULT_CPU_GEAR_ID
#define DEFAULT_BIG_FREQ_ID		DEFAULT_CPU_GEAR_ID
#define DEFAULT_HMP_BOOST_ID		DEFAULT_CPU_GEAR_ID
#define DEFAULT_INT_FREQ_ID		DEFAULT_CPU_GEAR_ID
#define DEFAULT_MIF_FREQ_ID		DEFAULT_CPU_GEAR_ID
#define AUD_PLL_RATE_KHZ		(1179648)
#define AUD_PLL_RATE_HZ_BYPASS		(26000000)
#define AUDIF_RATE_HZ			(24576000)
#define CALLIOPE_ENABLE_TIMEOUT_MS	(1000)
#define IPC_TIMEOUT_US			(10000)
#define BOOT_DONE_TIMEOUT_MS		(10000)
#define IPC_RETRY			(10)

#define DMA_VOL_FACTOR_MAX_STEPS	(0xFFFFFF)

#define ERAP(wname, wcontrols, event_fn, wparams) \
{	.id = snd_soc_dapm_dai_link, .name = wname, \
	.reg = SND_SOC_NOPM, .event = event_fn, \
	.kcontrol_news = wcontrols, .num_kcontrols = 1, \
	.params = wparams, .num_params = ARRAY_SIZE(wparams), \
	.event_flags = SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_WILL_PMD }

/* For only external static functions */
static struct abox_data *p_abox_data;

struct abox_data *abox_get_abox_data(void)
{
	return p_abox_data;
}

static int abox_iommu_fault_handler(
		struct iommu_domain *domain, struct device *dev,
		unsigned long fault_addr, int fault_flags, void *token)
{
	struct abox_data *data = token;

	abox_dbg_print_gpr(&data->pdev->dev, data);
	return 0;
}

static void abox_cpu_power(bool on);
static int abox_cpu_enable(bool enable);
static int abox_cpu_pm_ipc(struct abox_data *data, bool resume);
static void abox_boot_done(struct device *dev, unsigned int version);
static int abox_enable(struct device *dev);

static void exynos_abox_panic_handler(void)
{
	static bool has_run;
	struct abox_data *data = p_abox_data;
	struct device *dev = data ? (data->pdev ? &data->pdev->dev : NULL) :
			NULL;

	dev_dbg(dev, "%s\n", __func__);

	if (abox_is_on() && dev) {
		if (has_run) {
			dev_info(dev, "already dumped\n");
			return;
		}
		has_run = true;

		abox_dbg_dump_gpr(dev, data, ABOX_DBG_DUMP_KERNEL, "panic");
		abox_cpu_pm_ipc(data, false);
		writel(0x504E4943, data->sram_base + data->sram_size -
				sizeof(u32));
		abox_cpu_enable(false);
		abox_cpu_power(false);
		abox_cpu_power(true);
		abox_cpu_enable(true);
		mdelay(100);
		abox_dbg_dump_mem(dev, data, ABOX_DBG_DUMP_KERNEL, "panic");
	} else {
		dev_info(dev, "%s: dump is skipped due to no power\n",
				__func__);
	}
}

static int abox_panic_handler(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	exynos_abox_panic_handler();
	return NOTIFY_OK;
}

static struct notifier_block abox_panic_notifier = {
	.notifier_call	= abox_panic_handler,
	.next		= NULL,
	.priority	= 0	/* priority: INT_MAX >= x >= 0 */
};

static struct platform_driver samsung_abox_driver;
static bool is_abox(struct device *dev)
{
	return (&samsung_abox_driver.driver) == dev->driver;
}

static void abox_probe_quirks(struct abox_data *data, struct device_node *np)
{
	#define QUIRKS "quirks"
	#define DEC_MAP(id) {ABOX_QUIRK_STR_##id, ABOX_QUIRK_##id}

	static const struct {
		const char *str;
		unsigned int bit;
	} map[] = {
		DEC_MAP(TRY_TO_ASRC_OFF),
		DEC_MAP(SHARE_VTS_SRAM),
		DEC_MAP(OFF_ON_SUSPEND),
		DEC_MAP(SCSC_BT),
		DEC_MAP(SCSC_BT_HACK),
	};

	int i, ret;

	for (i = 0; i < ARRAY_SIZE(map); i++) {
		ret = of_property_match_string(np, QUIRKS, map[i].str);
		if (ret >= 0)
			data->quirks |= map[i].bit;
	}
}


int abox_disable_qchannel(struct device *dev, struct abox_data *data,
		enum qchannel clk, int disable)
{
	return regmap_update_bits(data->regmap, ABOX_QCHANNEL_DISABLE,
			ABOX_QCHANNEL_DISABLE_MASK(clk),
			!!disable << ABOX_QCHANNEL_DISABLE_L(clk));
}

phys_addr_t abox_addr_to_phys_addr(struct abox_data *data, unsigned int addr)
{
	phys_addr_t ret;

	if (addr < IOVA_DRAM_FIRMWARE)
		ret = data->sram_base_phys + addr;
	else
		ret = iommu_iova_to_phys(data->iommu_domain, addr);

	return ret;
}

static void *abox_dram_addr_to_kernel_addr(struct abox_data *data,
		unsigned long iova)
{
	struct abox_iommu_mapping *m;
	void *ret = ERR_PTR(-EFAULT);

	rcu_read_lock();
	list_for_each_entry_rcu(m, &data->iommu_maps, list) {
		if (m->iova <= iova && iova < m->iova + m->bytes) {
			ret = m->area + (iova - m->iova);
			break;
		}
	}
	rcu_read_unlock();

	return ret;
}

void *abox_addr_to_kernel_addr(struct abox_data *data, unsigned int addr)
{
	void *ret;

	if (addr < IOVA_DRAM_FIRMWARE)
		ret = data->sram_base + addr;
	else
		ret = abox_dram_addr_to_kernel_addr(data, addr);

	return ret;
}

phys_addr_t abox_iova_to_phys(struct device *dev, unsigned long iova)
{
	return abox_addr_to_phys_addr(dev_get_drvdata(dev), iova);
}
EXPORT_SYMBOL(abox_iova_to_phys);

void *abox_iova_to_virt(struct device *dev, unsigned long iova)
{
	return abox_addr_to_kernel_addr(dev_get_drvdata(dev), iova);
}
EXPORT_SYMBOL(abox_iova_to_virt);

static int abox_sif_idx(enum ABOX_CONFIGMSG configmsg)
{
	return configmsg - ((configmsg < SET_MIXER_FORMAT) ?
			SET_MIXER_SAMPLE_RATE : SET_MIXER_FORMAT);
}

static unsigned int abox_get_sif_rate_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return data->sif_rate_min[abox_sif_idx(configmsg)];
}

static void abox_set_sif_rate_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, unsigned int val)
{
	data->sif_rate_min[abox_sif_idx(configmsg)] = val;
}

static snd_pcm_format_t abox_get_sif_format_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return data->sif_format_min[abox_sif_idx(configmsg)];
}

static void abox_set_sif_format_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, snd_pcm_format_t val)
{
	data->sif_format_min[abox_sif_idx(configmsg)] = val;
}

static int abox_get_sif_width_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return snd_pcm_format_width(abox_get_sif_format_min(data, configmsg));
}

static void abox_set_sif_width_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, int width)
{
	struct device *dev = &data->pdev->dev;
	snd_pcm_format_t format = SNDRV_PCM_FORMAT_S16;

	switch (width) {
	case 16:
		format = SNDRV_PCM_FORMAT_S16;
		break;
	case 24:
		format = SNDRV_PCM_FORMAT_S24;
		break;
	case 32:
		format = SNDRV_PCM_FORMAT_S32;
		break;
	default:
		dev_warn(dev, "%s(%d): invalid argument\n", __func__, width);
	}

	abox_set_sif_format_min(data, configmsg, format);
}

static unsigned int abox_get_sif_channels_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return data->sif_channels_min[abox_sif_idx(configmsg)];
}

static void __maybe_unused abox_set_sif_channels_min(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, unsigned int val)
{
	data->sif_channels_min[abox_sif_idx(configmsg)] = val;
}

static bool abox_get_sif_auto_config(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return data->sif_auto_config[abox_sif_idx(configmsg)];
}

static void abox_set_sif_auto_config(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, bool val)
{
	data->sif_auto_config[abox_sif_idx(configmsg)] = val;
}

static unsigned int abox_get_sif_rate(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	unsigned int val = data->sif_rate[abox_sif_idx(configmsg)];
	unsigned int min = abox_get_sif_rate_min(data, configmsg);

	return (abox_get_sif_auto_config(data, configmsg) && (min > val)) ?
			min : val;
}

static void abox_set_sif_rate(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, unsigned int val)
{
	data->sif_rate[abox_sif_idx(configmsg)] = val;
}

static snd_pcm_format_t abox_get_sif_format(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	snd_pcm_format_t val = data->sif_format[abox_sif_idx(configmsg)];
	snd_pcm_format_t min = abox_get_sif_format_min(data, configmsg);

	return (abox_get_sif_auto_config(data, configmsg) && (min > val)) ?
			min : val;
}

static void abox_set_sif_format(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, snd_pcm_format_t val)
{
	data->sif_format[abox_sif_idx(configmsg)] = val;
}

static int abox_get_sif_physical_width(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	snd_pcm_format_t format = abox_get_sif_format(data, configmsg);

	return snd_pcm_format_physical_width(format);
}

static int abox_get_sif_width(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	return snd_pcm_format_width(abox_get_sif_format(data, configmsg));
}

static void abox_set_sif_width(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, int width)
{
	struct device *dev = &data->pdev->dev;
	snd_pcm_format_t format = SNDRV_PCM_FORMAT_S16;

	switch (width) {
	case 16:
		format = SNDRV_PCM_FORMAT_S16;
		break;
	case 24:
		format = SNDRV_PCM_FORMAT_S24;
		break;
	case 32:
		format = SNDRV_PCM_FORMAT_S32;
		break;
	default:
		dev_err(dev, "%s(%d): invalid argument\n", __func__, width);
	}

	abox_set_sif_format(data, configmsg, format);
}

static unsigned int abox_get_sif_channels(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg)
{
	unsigned int val = data->sif_channels[abox_sif_idx(configmsg)];
	unsigned int min = abox_get_sif_channels_min(data, configmsg);

	return (abox_get_sif_auto_config(data, configmsg) && (min > val)) ?
			min : val;
}

static void abox_set_sif_channels(struct abox_data *data,
		enum ABOX_CONFIGMSG configmsg, unsigned int val)
{
	data->sif_channels[abox_sif_idx(configmsg)] = val;
}

static bool __abox_ipc_queue_empty(struct abox_data *data)
{
	return (data->ipc_queue_end == data->ipc_queue_start);
}

static bool __abox_ipc_queue_full(struct abox_data *data)
{
	size_t length = ARRAY_SIZE(data->ipc_queue);

	return (((data->ipc_queue_end + 1) % length) == data->ipc_queue_start);
}

static int abox_ipc_queue_put(struct abox_data *data, struct device *dev,
		int hw_irq, const void *supplement, size_t size)
{
	spinlock_t *lock = &data->ipc_queue_lock;
	size_t length = ARRAY_SIZE(data->ipc_queue);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(lock, flags);
	if (!__abox_ipc_queue_full(data)) {
		struct abox_ipc *ipc;

		ipc = &data->ipc_queue[data->ipc_queue_end];
		ipc->dev = dev;
		ipc->hw_irq = hw_irq;
		ipc->put_time = sched_clock();
		ipc->get_time = 0;
		memcpy(&ipc->msg, supplement, size);
		data->ipc_queue_end = (data->ipc_queue_end + 1) % length;

		ret = 0;
	} else {
		ret = -EBUSY;
	}
	spin_unlock_irqrestore(lock, flags);

	return ret;
}

static int abox_ipc_queue_get(struct abox_data *data, struct abox_ipc *ipc)
{
	spinlock_t *lock = &data->ipc_queue_lock;
	size_t length = ARRAY_SIZE(data->ipc_queue);
	unsigned long flags;
	int ret;

	spin_lock_irqsave(lock, flags);
	if (!__abox_ipc_queue_empty(data)) {
		struct abox_ipc *tmp;

		tmp = &data->ipc_queue[data->ipc_queue_start];
		tmp->get_time = sched_clock();
		*ipc = *tmp;
		data->ipc_queue_start = (data->ipc_queue_start + 1) % length;

		ret = 0;
	} else {
		ret = -ENODATA;
	}
	spin_unlock_irqrestore(lock, flags);

	return ret;
}

static bool abox_can_calliope_ipc(struct device *dev,
		struct abox_data *data)
{
	bool ret = true;

	switch (data->calliope_state) {
	case CALLIOPE_DISABLING:
	case CALLIOPE_ENABLED:
		break;
	case CALLIOPE_ENABLING:
		wait_event_timeout(data->ipc_wait_queue,
				data->calliope_state == CALLIOPE_ENABLED,
				msecs_to_jiffies(CALLIOPE_ENABLE_TIMEOUT_MS));
		if (data->calliope_state == CALLIOPE_ENABLED)
			break;
		/* Fallthrough */
	case CALLIOPE_DISABLED:
	default:
		dev_warn(dev, "Invalid calliope state: %d\n",
				data->calliope_state);
		ret = false;
	}

	dev_dbg(dev, "%s: %d\n", __func__, ret);

	return ret;
}

static int __abox_process_ipc(struct device *dev, struct abox_data *data,
		int hw_irq, const ABOX_IPC_MSG *msg)
{
	static unsigned int tout_cnt;
	static DEFINE_SPINLOCK(lock);

	void __iomem *tx_base = data->sram_base + data->ipc_tx_offset;
	void __iomem *tx_ack = data->sram_base + data->ipc_tx_ack_offset;
	int ret, i;

	dev_dbg(dev, "%s(%d, %d, %d)\n", __func__, hw_irq,
			msg->ipcid, msg->msg.system.msgtype);

	do {
		spin_lock(&lock);

		memcpy_toio(tx_base, msg, sizeof(*msg));
		writel(1, tx_ack);
		abox_gic_generate_interrupt(data->dev_gic, hw_irq);
		for (i = IPC_TIMEOUT_US; i && readl(tx_ack); i--)
			udelay(1);

		if (readl(tx_ack)) {
			tout_cnt++;
			dev_warn_ratelimited(dev, "Transaction timeout(%d)\n",
					tout_cnt);

			if (tout_cnt == 1)
				abox_dbg_dump_simple(dev, data,
						"Transaction timeout");

			if ((tout_cnt % IPC_RETRY) == 0) {
				abox_failsafe_report(dev);
				writel(0, tx_ack);
			}

			ret = -EIO;
		} else {
			tout_cnt = 0;
			ret = 0;
		}
		spin_unlock(&lock);
	} while (readl(tx_ack));

	return ret;
}

static void abox_process_ipc(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data, ipc_work);
	struct device *dev = &data->pdev->dev;
	struct abox_ipc ipc;

	dev_dbg(dev, "%s: %d %d\n", __func__, data->ipc_queue_start,
			data->ipc_queue_end);

	pm_runtime_get_sync(dev);

	if (abox_can_calliope_ipc(dev, data)) {
		while (abox_ipc_queue_get(data, &ipc) == 0) {
			struct device *dev = ipc.dev;
			int hw_irq = ipc.hw_irq;
			ABOX_IPC_MSG *msg = &ipc.msg;

			__abox_process_ipc(dev, data, hw_irq, msg);

			/* giving time to ABOX for processing */
			usleep_range(10, 100);
		}
	}

	pm_runtime_mark_last_busy(dev);
	pm_runtime_put_autosuspend(dev);
}

static int abox_schedule_ipc(struct device *dev, struct abox_data *data,
		int hw_irq, const void *supplement, size_t size,
		bool atomic, bool sync)
{
	struct abox_ipc *ipc;
	int retry = 0;
	int ret;

	dev_dbg(dev, "%s(%d, %zu, %d, %d)\n", __func__, hw_irq,
			size, atomic, sync);

	if (unlikely(sizeof(ipc->msg) < size)) {
		dev_err(dev, "%s: too large supplement\n", __func__);
		return -EINVAL;
	}

	do {
		ret = abox_ipc_queue_put(data, dev, hw_irq, supplement, size);
		queue_work(data->ipc_workqueue, &data->ipc_work);
		if (!atomic && sync)
			flush_work(&data->ipc_work);
		if (ret >= 0)
			break;

		if (!atomic) {
			dev_info(dev, "%s: flush(%d)\n", __func__, retry);
			flush_work(&data->ipc_work);
		} else {
			dev_info(dev, "%s: delay(%d)\n", __func__, retry);
			mdelay(10);
		}
	} while (retry++ < IPC_RETRY);

	if (ret < 0) {
		dev_err(dev, "%s(%d): ipc queue overflow\n", __func__, hw_irq);
		abox_failsafe_report(dev);
	}

	return ret;
}

int abox_request_ipc(struct device *dev,
		int hw_irq, const void *supplement,
		size_t size, int atomic, int sync)
{
	struct abox_data *data = dev_get_drvdata(dev);
	int ret;

	if (atomic && sync) {
		ret = __abox_process_ipc(dev, data, hw_irq, supplement);
	} else {
		ret = abox_schedule_ipc(dev, data, hw_irq, supplement, size,
				!!atomic, !!sync);
	}

	return ret;
}
EXPORT_SYMBOL(abox_request_ipc);

bool abox_is_on(void)
{
	return p_abox_data && p_abox_data->enabled;
}
EXPORT_SYMBOL(abox_is_on);

int abox_register_bclk_usage(struct device *dev, struct abox_data *data,
		enum abox_dai dai_id, unsigned int rate, unsigned int channels,
		unsigned int width)
{
	unsigned long target_pll, audif_rate;
	int id = dai_id - ABOX_UAIF0;
	int ret = 0;
	int i;

	dev_dbg(dev, "%s(%d, %d)\n", __func__, id, rate);

	if (id < 0 || id >= ABOX_DAI_COUNT) {
		dev_err(dev, "invalid dai_id: %d\n", dai_id);
		return -EINVAL;
	}

	if (rate == 0) {
		data->audif_rates[id] = 0;
		return 0;
	}

	target_pll = ((rate % 44100) == 0) ? AUD_PLL_RATE_HZ_FOR_44100 :
			AUD_PLL_RATE_HZ_FOR_48000;
	if (target_pll != clk_get_rate(data->clk_pll)) {
		dev_info(dev, "Set AUD_PLL rate: %lu -> %lu\n",
			clk_get_rate(data->clk_pll), target_pll);
		ret = clk_set_rate(data->clk_pll, target_pll);
		if (ret < 0) {
			dev_err(dev, "AUD_PLL set error=%d\n", ret);
			return ret;
		}
	}

	if (data->uaif_max_div <= 32) {
		if ((rate % 44100) == 0)
			audif_rate = ((rate > 176400) ? 352800 : 176400) *
					width * 2;
		else
			audif_rate = ((rate > 192000) ? 384000 : 192000) *
					width * 2;

		while (audif_rate / rate / channels / width >
				data->uaif_max_div)
			audif_rate /= 2;
	} else {
		int clk_width = 96; /* LCM of 24 and 32 */
		int clk_channels = 2;

		if ((rate % 44100) == 0)
			audif_rate = 352800 * clk_width * clk_channels;
		else
			audif_rate = 384000 * clk_width * clk_channels;

		if (audif_rate < rate * width * channels)
			audif_rate = rate * width * channels;
	}

	data->audif_rates[id] = audif_rate;

	for (i = 0; i < ARRAY_SIZE(data->audif_rates); i++) {
		if (data->audif_rates[i] > 0 &&
				data->audif_rates[i] > audif_rate) {
			audif_rate = data->audif_rates[i];
		}
	}

	ret = clk_set_rate(data->clk_audif, audif_rate);
	if (ret < 0)
		dev_err(dev, "Failed to set audif clock: %d\n", ret);

	dev_info(dev, "audif clock: %lu\n", clk_get_rate(data->clk_audif));

	return ret;
}

static int abox_sif_format_put_ipc(struct device *dev, snd_pcm_format_t format,
		int channels, enum ABOX_CONFIGMSG configmsg)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_ABOX_CONFIG_MSG *abox_config_msg = &msg.msg.config;
	int width = snd_pcm_format_width(format);
	int ret;

	dev_dbg(dev, "%s(%d, %d, %d)\n", __func__, width, channels, configmsg);

	abox_set_sif_format(data, configmsg, format);
	abox_set_sif_channels(data, configmsg, channels);

	/* update manually for regmap cache sync */
	switch (configmsg) {
	case SET_MIXER_SAMPLE_RATE:
	case SET_MIXER_FORMAT:
		regmap_update_bits(data->regmap, ABOX_SPUS_CTRL1,
				ABOX_SPUS_MIXP_FORMAT_MASK,
				abox_get_format(width, channels) <<
				ABOX_SPUS_MIXP_FORMAT_L);
		break;
	case SET_RECP_SAMPLE_RATE:
	case SET_RECP_FORMAT:
		regmap_update_bits(data->regmap, ABOX_SPUM_CTRL1,
				ABOX_RECP_SRC_FORMAT_MASK,
				abox_get_format(width, channels) <<
				ABOX_RECP_SRC_FORMAT_L);
		break;
	default:
		/* Nothing to do */
		break;
	}

	msg.ipcid = IPC_ABOX_CONFIG;
	abox_config_msg->param1 = abox_get_sif_width(data, configmsg);
	abox_config_msg->param2 = abox_get_sif_channels(data, configmsg);
	abox_config_msg->msgtype = configmsg;
	ret = abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
	if (ret < 0)
		dev_err(dev, "%d(%d bit, %d channels) failed: %d\n", configmsg,
				width, channels, ret);

	return ret;
}

static unsigned int abox_sifsx_cnt_val(unsigned long aclk, unsigned int rate,
		unsigned int physical_width, unsigned int channels)
{
	static const int correction = -2;
	unsigned int n, d;

	/* k = n / d */
	d = channels;
	n = 2 * (32 / physical_width);

	return ((aclk * n) / d / rate) - 5 + correction;
}

static int abox_sifs_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *be = substream->private_data;
	struct snd_soc_dpcm *dpcm;
	int stream = substream->stream;
	struct device *dev = dai->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct regmap *regmap = data->regmap;
	enum abox_dai id = dai->id;
	unsigned int rate = params_rate(params);
	unsigned int width = params_width(params);
	unsigned int pwidth = params_physical_width(params);
	unsigned int channels = params_channels(params);
	unsigned long aclk;
	unsigned int cnt_val;
	bool skip = true;
	int ret = 0;

	if (stream != SNDRV_PCM_STREAM_CAPTURE)
		goto out;

	/* sifs count is needed only when SIFS is connected to NSRC */
	list_for_each_entry(dpcm, &be->dpcm[stream].fe_clients, list_fe) {
		if (dpcm->fe->cpu_dai->id != ABOX_WDMA0) {
			skip = false;
			break;
		}
	}
	if (skip)
		goto out;

	abox_request_cpu_gear_dai(dev, data, dai, ABOX_CPU_GEAR_MAX);
	abox_cpu_gear_barrier(data);

	aclk = clk_get_rate(data->clk_bus);
	cnt_val = abox_sifsx_cnt_val(aclk, rate, pwidth, channels);

	dev_info(dev, "%s[%d](%ubit %uchannel %uHz at %luHz): %u\n",
			__func__, id, width, channels, rate, aclk, cnt_val);

	switch (id) {
	case ABOX_SIFS0:
		ret = regmap_update_bits(regmap, ABOX_SPUS_CTRL_SIFS_CNT0,
			ABOX_SIFS0_CNT_VAL_MASK,
			cnt_val << ABOX_SIFS0_CNT_VAL_L);
		break;
	case ABOX_SIFS1:
		ret = regmap_update_bits(regmap, ABOX_SPUS_CTRL_SIFS_CNT0,
			ABOX_SIFS1_CNT_VAL_MASK,
			cnt_val << ABOX_SIFS1_CNT_VAL_L);
		break;
	case ABOX_SIFS2:
		ret = regmap_update_bits(regmap, ABOX_SPUS_CTRL_SIFS_CNT1,
			ABOX_SIFS2_CNT_VAL_MASK,
			cnt_val << ABOX_SIFS2_CNT_VAL_L);
		break;
	default:
		dev_err(dev, "%s: invalid id(%d)\n", __func__, id);
		ret = -EINVAL;
		break;
	}
out:
	return ret;
}

static int abox_sifs_hw_free(struct snd_pcm_substream *substream,
		struct snd_soc_dai *dai)
{
	struct device *dev = dai->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct regmap *regmap = data->regmap;
	enum abox_dai id = dai->id;
	int ret = 0;

	if (substream->stream != SNDRV_PCM_STREAM_CAPTURE)
		goto out;

	dev_info(dev, "%s[%d]\n", __func__, id);

	switch (id) {
	case ABOX_SIFS0:
		ret = regmap_update_bits(regmap, ABOX_SPUS_CTRL_SIFS_CNT0,
			ABOX_SIFS0_CNT_VAL_MASK, 0);
		break;
	case ABOX_SIFS1:
		ret = regmap_update_bits(regmap, ABOX_SPUS_CTRL_SIFS_CNT0,
			ABOX_SIFS1_CNT_VAL_MASK, 0);
		break;
	case ABOX_SIFS2:
		ret = regmap_update_bits(regmap, ABOX_SPUS_CTRL_SIFS_CNT1,
			ABOX_SIFS2_CNT_VAL_MASK, 0);
		break;
	default:
		dev_err(dev, "%s: invalid id(%d)\n", __func__, id);
		ret = -EINVAL;
		break;
	}

	abox_request_cpu_gear_dai(dev, data, dai, ABOX_CPU_GEAR_MIN);
out:
	return ret;
}

static const struct snd_soc_dai_ops abox_sifs_dai_ops = {
	.hw_params	= abox_sifs_hw_params,
	.hw_free	= abox_sifs_hw_free,
};

static struct snd_soc_dai_driver abox_dais[] = {
	{
		.name = "RDMA0",
		.id = ABOX_RDMA0,
		.playback = {
			.stream_name = "RDMA0 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "RDMA1",
		.id = ABOX_RDMA1,
		.playback = {
			.stream_name = "RDMA1 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "RDMA2",
		.id = ABOX_RDMA2,
		.playback = {
			.stream_name = "RDMA2 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "RDMA3",
		.id = ABOX_RDMA3,
		.playback = {
			.stream_name = "RDMA3 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "RDMA4",
		.id = ABOX_RDMA4,
		.playback = {
			.stream_name = "RDMA4 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "RDMA5",
		.id = ABOX_RDMA5,
		.playback = {
			.stream_name = "RDMA5 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.compress_new = snd_soc_new_compress,
	},
	{
		.name = "RDMA6",
		.id = ABOX_RDMA6,
		.playback = {
			.stream_name = "RDMA6 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "RDMA7",
		.id = ABOX_RDMA7,
		.playback = {
			.stream_name = "RDMA7 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
	},
	{
		.name = "WDMA0",
		.id = ABOX_WDMA0,
		.capture = {
			.stream_name = "WDMA0 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_WDMA_SAMPLE_FORMATS,
		},
	},
	{
		.name = "WDMA1",
		.id = ABOX_WDMA1,
		.capture = {
			.stream_name = "WDMA1 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_WDMA_SAMPLE_FORMATS,
		},
	},
	{
		.name = "WDMA2",
		.id = ABOX_WDMA2,
		.capture = {
			.stream_name = "WDMA2 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_WDMA_SAMPLE_FORMATS,
		},
	},
	{
		.name = "WDMA3",
		.id = ABOX_WDMA3,
		.capture = {
			.stream_name = "WDMA3 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_WDMA_SAMPLE_FORMATS,
		},
	},
	{
		.name = "WDMA4",
		.id = ABOX_WDMA4,
		.capture = {
			.stream_name = "WDMA4 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_WDMA_SAMPLE_FORMATS,
		},
	},
	{
		.name = "SIFS0",
		.id = ABOX_SIFS0,
		.playback = {
			.stream_name = "SIFS0 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "SIFS0 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &abox_sifs_dai_ops,
	},
	{
		.name = "SIFS1",
		.id = ABOX_SIFS1,
		.playback = {
			.stream_name = "SIFS1 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "SIFS1 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &abox_sifs_dai_ops,
	},
	{
		.name = "SIFS2",
		.id = ABOX_SIFS2,
		.playback = {
			.stream_name = "SIFS2 Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.capture = {
			.stream_name = "SIFS2 Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = ABOX_SAMPLING_RATES,
			.rate_min = 8000,
			.rate_max = 384000,
			.formats = ABOX_SAMPLE_FORMATS,
		},
		.ops = &abox_sifs_dai_ops,
	},
};

static int abox_cmpnt_probe(struct snd_soc_component *component)
{
	struct device *dev = component->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);

	data->cmpnt = component;
	snd_soc_component_update_bits(component, ABOX_SPUM_CTRL0,
			ABOX_FUNC_CHAIN_RSRC_RECP_MASK,
			ABOX_FUNC_CHAIN_RSRC_RECP_MASK);

	return 0;
}

static void abox_cmpnt_remove(struct snd_soc_component *component)
{
	struct device *dev = component->dev;

	dev_info(dev, "%s\n", __func__);

	snd_soc_component_exit_regmap(component);
}

static int abox_sample_rate_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = abox_get_sif_rate(data, reg);

	dev_dbg(dev, "%s(0x%08x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_sample_rate_put_ipc(struct device *dev, unsigned int val,
		enum ABOX_CONFIGMSG configmsg)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_ABOX_CONFIG_MSG *abox_config_msg = &msg.msg.config;
	int ret;

	dev_dbg(dev, "%s(%u, 0x%08x)\n", __func__, val, configmsg);

	abox_set_sif_rate(data, configmsg, val);

	msg.ipcid = IPC_ABOX_CONFIG;
	abox_config_msg->param1 = abox_get_sif_rate(data, configmsg);
	abox_config_msg->msgtype = configmsg;
	ret = abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
	if (ret < 0) {
		dev_err(dev, "%s(%u, 0x%08x) failed: %d\n", __func__, val,
				configmsg, ret);
	}

	return ret;
}

static int abox_sample_rate_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(0x%08x, %u)\n", __func__, reg, val);

	return abox_sample_rate_put_ipc(dev, val, reg);
}

static int abox_bit_width_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = abox_get_sif_width(data, reg);

	dev_dbg(dev, "%s(0x%08x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_bit_width_put_ipc(struct device *dev, unsigned int val,
		enum ABOX_CONFIGMSG configmsg)
{
	struct abox_data *data = dev_get_drvdata(dev);
	snd_pcm_format_t format = SNDRV_PCM_FORMAT_S16;
	int channels = data->sif_channels[abox_sif_idx(configmsg)];

	dev_dbg(dev, "%s(%u, 0x%08x)\n", __func__, val, configmsg);

	switch (val) {
	case 16:
		format = SNDRV_PCM_FORMAT_S16;
		break;
	case 24:
		format = SNDRV_PCM_FORMAT_S24;
		break;
	case 32:
		format = SNDRV_PCM_FORMAT_S32;
		break;
	default:
		dev_warn(dev, "%s(%u, 0x%08x) invalid argument\n", __func__,
				val, configmsg);
		break;
	}

	return abox_sif_format_put_ipc(dev, format, channels, configmsg);
}

static int abox_bit_width_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(0x%08x, %u)\n", __func__, reg, val);

	return abox_bit_width_put_ipc(dev, val, reg);
}

static int abox_sample_rate_min_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = abox_get_sif_rate_min(data, reg);

	dev_dbg(dev, "%s(0x%08x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_sample_rate_min_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(0x%08x, %u)\n", __func__, reg, val);

	abox_set_sif_rate_min(data, reg, val);

	return 0;
}

static int abox_bit_width_min_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = abox_get_sif_width_min(data, reg);

	dev_dbg(dev, "%s(0x%08x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_bit_width_min_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(0x%08x, %u)\n", __func__, reg, val);

	abox_set_sif_width_min(data, reg, val);

	return 0;
}

static int abox_auto_config_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = abox_get_sif_auto_config(data, reg);

	dev_dbg(dev, "%s(0x%08x): %u\n", __func__, reg, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int abox_auto_config_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(0x%08x, %u)\n", __func__, reg, val);

	abox_set_sif_auto_config(data, reg, !!val);

	return 0;
}

static int abox_erap_handler_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	enum ABOX_ERAP_TYPE type = (enum ABOX_ERAP_TYPE)mc->reg;

	dev_dbg(dev, "%s(%d)\n", __func__, type);

	ucontrol->value.integer.value[0] = data->erap_status[type];

	return 0;
}

static int abox_erap_handler_put_ipc(struct device *dev,
		enum ABOX_ERAP_TYPE type, unsigned int val)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_ERAP_MSG *erap_msg = &msg.msg.erap;
	struct ERAP_ONOFF_PARAM *erap_param = &erap_msg->param.onoff;
	int ret;

	dev_dbg(dev, "%s(%u, %d)\n", __func__, val, type);

	msg.ipcid = IPC_ERAP;
	erap_msg->msgtype = val ? REALTIME_OPEN : REALTIME_CLOSE;
	erap_param->type = type;
	erap_param->channel_no = 0;
	erap_param->version = val;
	ret = abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
	if (ret < 0)
		dev_err(dev, "erap control failed(type:%d, status:%d)\n",
				type, val);

	data->erap_status[type] = val;

	return ret;
}

static int abox_erap_handler_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	enum ABOX_ERAP_TYPE type = (enum ABOX_ERAP_TYPE)mc->reg;
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(%u, %d)\n", __func__, val, type);

	return abox_erap_handler_put_ipc(dev, type, val);
}

static int abox_audio_mode_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int item;

	dev_dbg(dev, "%s: %u\n", __func__, data->audio_mode);

	item = snd_soc_enum_val_to_item(e, data->audio_mode);
	ucontrol->value.enumerated.item[0] = item;

	return 0;
}

static int abox_audio_mode_put_ipc(struct device *dev, enum audio_mode mode)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;

	dev_dbg(dev, "%s(%d)\n", __func__, mode);

	data->audio_mode_time = local_clock();

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_SET_MODE;
	system_msg->param1 = data->audio_mode = mode;
	return abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
}

static int abox_audio_mode_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum audio_mode mode;

	if (item[0] >= e->items)
		return -EINVAL;

	mode = snd_soc_enum_item_to_val(e, item[0]);
	dev_info(dev, "%s(%u)\n", __func__, mode);

	return abox_audio_mode_put_ipc(dev, mode);
}

static const char * const abox_audio_mode_enum_texts[] = {
	"NORMAL",
	"RINGTONE",
	"IN_CALL",
	"IN_COMMUNICATION",
	"IN_VIDEOCALL",
};
static const unsigned int abox_audio_mode_enum_values[] = {
	MODE_NORMAL,
	MODE_RINGTONE,
	MODE_IN_CALL,
	MODE_IN_COMMUNICATION,
	MODE_IN_VIDEOCALL,
};
SOC_VALUE_ENUM_SINGLE_DECL(abox_audio_mode_enum, SND_SOC_NOPM, 0, 0,
		abox_audio_mode_enum_texts, abox_audio_mode_enum_values);

static int abox_sound_type_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int item;

	dev_dbg(dev, "%s: %u\n", __func__, data->sound_type);

	item = snd_soc_enum_val_to_item(e, data->sound_type);
	ucontrol->value.enumerated.item[0] = item;

	return 0;
}

static int abox_sound_type_put_ipc(struct device *dev, enum sound_type type)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;

	dev_dbg(dev, "%s(%d)\n", __func__, type);

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_SET_TYPE;
	system_msg->param1 = data->sound_type = type;

	return abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
}

static int abox_sound_type_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	enum sound_type type;

	if (item[0] >= e->items)
		return -EINVAL;

	type = snd_soc_enum_item_to_val(e, item[0]);
	dev_info(dev, "%s(%d)\n", __func__, type);

	return abox_sound_type_put_ipc(dev, type);
}
static const char * const abox_sound_type_enum_texts[] = {
	"VOICE",
	"SPEAKER",
	"HEADSET",
	"BTVOICE",
	"USB",
};
static const unsigned int abox_sound_type_enum_values[] = {
	SOUND_TYPE_VOICE,
	SOUND_TYPE_SPEAKER,
	SOUND_TYPE_HEADSET,
	SOUND_TYPE_BTVOICE,
	SOUND_TYPE_USB,
};
SOC_VALUE_ENUM_SINGLE_DECL(abox_sound_type_enum, SND_SOC_NOPM, 0, 0,
		abox_sound_type_enum_texts, abox_sound_type_enum_values);

static int abox_tickle_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);

	ucontrol->value.integer.value[0] = data->enabled;

	return 0;
}


static void abox_tickle_work_func(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct abox_data *data = container_of(dwork, struct abox_data,
			tickle_work);
	struct device *dev = &data->pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	pm_request_idle(dev);
}
static DECLARE_DELAYED_WORK(abox_tickle_work, abox_tickle_work_func);

static int abox_tickle_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	long val = ucontrol->value.integer.value[0];

	dev_dbg(dev, "%s(%ld)\n", __func__, val);

	if (!!val) {
		pm_request_resume(dev);
		schedule_delayed_work(&data->tickle_work, 1 * HZ);
	}

	return 0;
}

static int wake_lock_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned int val = data->ws.active;

	dev_dbg(dev, "%s: %u\n", __func__, val);

	ucontrol->value.integer.value[0] = val;

	return 0;
}

static int wake_lock_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned int val = (unsigned int)ucontrol->value.integer.value[0];

	dev_info(dev, "%s(%u)\n", __func__, val);

	if (val)
		__pm_stay_awake(&data->ws);
	else
		__pm_relax(&data->ws);

	return 0;
}

static int abox_rdma_vol_factor_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	int id = (int)mc->reg;
	unsigned int volumes;
	unsigned int value = 0;
	int ret = 0;

	ret = regmap_read(data->regmap,
			ABOX_RDMA_VOL_FACTOR(id), &value);
	if (ret < 0) {
		dev_err(dev, "sfr access failed: %d\n", ret);

		return ret;
	}

	volumes = (value & ABOX_RDMA_VOL_FACTOR_MASK);

	dev_dbg(dev, "%s(0x%08x, %u)\n", __func__, id, volumes);

	ucontrol->value.integer.value[0] = volumes;

	return 0;
}

static int abox_rdma_vol_factor_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	int id = (int)mc->reg;
	unsigned int volumes;
	unsigned int value = 0;
	int ret = 0;

	volumes = (unsigned int)ucontrol->value.integer.value[0];
	dev_dbg(dev, "%s[%d]: %u\n", __func__, id, volumes);

	ret = regmap_read(data->regmap,
			ABOX_RDMA_VOL_FACTOR(id), &value);
	if (ret < 0) {
		dev_err(dev, "sfr access failed: %d\n", ret);

		return ret;
	}

	set_value_by_name(value, ABOX_RDMA_VOL_FACTOR, volumes);

	ret = regmap_write(data->regmap,
			ABOX_RDMA_VOL_FACTOR(id), value);
	if (ret < 0) {
		dev_err(dev, "sfr access failed: %d\n", ret);

		return ret;
	}

	return 0;
}

static const DECLARE_TLV_DB_LINEAR(abox_rdma_vol_factor_gain, 0,
		DMA_VOL_FACTOR_MAX_STEPS);

static const struct snd_kcontrol_new abox_cmpnt_controls[] = {
	SOC_SINGLE_EXT("Sampling Rate Mixer", SET_MIXER_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Out1", SET_OUT1_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Out2", SET_OUT2_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Recp", SET_RECP_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux0", SET_INMUX0_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux1", SET_INMUX1_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux2", SET_INMUX2_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux3", SET_INMUX3_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux4", SET_INMUX4_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_get, abox_sample_rate_put),
	SOC_SINGLE_EXT("Bit Width Mixer", SET_MIXER_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Out1", SET_OUT1_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Out2", SET_OUT2_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Recp", SET_RECP_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Inmux0", SET_INMUX0_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Inmux1", SET_INMUX1_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Inmux2", SET_INMUX2_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Inmux3", SET_INMUX3_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Bit Width Inmux4", SET_INMUX4_FORMAT, 16, 32, 0,
			abox_bit_width_get, abox_bit_width_put),
	SOC_SINGLE_EXT("Sampling Rate Mixer Min", SET_MIXER_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Out1 Min", SET_OUT1_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Out2 Min", SET_OUT2_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Recp Min", SET_RECP_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux0 Min", SET_INMUX0_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux1 Min", SET_INMUX1_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux2 Min", SET_INMUX2_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux3 Min", SET_INMUX3_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Sampling Rate Inmux4 Min", SET_INMUX4_SAMPLE_RATE,
			8000, 384000, 0,
			abox_sample_rate_min_get, abox_sample_rate_min_put),
	SOC_SINGLE_EXT("Bit Width Mixer Min", SET_MIXER_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Out1 Min", SET_OUT1_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Out2 Min", SET_OUT2_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Recp Min", SET_RECP_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Inmux0 Min", SET_INMUX0_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Inmux1 Min", SET_INMUX1_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Inmux2 Min", SET_INMUX2_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Inmux3 Min", SET_INMUX3_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Bit Width Inmux4 Min", SET_INMUX4_FORMAT, 16, 32, 0,
			abox_bit_width_min_get, abox_bit_width_min_put),
	SOC_SINGLE_EXT("Auto Config Mixer", SET_MIXER_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Out1", SET_OUT1_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Out2", SET_OUT2_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Recp", SET_RECP_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Inmux0", SET_INMUX0_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Inmux1", SET_INMUX1_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Inmux2", SET_INMUX2_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Inmux3", SET_INMUX3_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Auto Config Inmux4", SET_INMUX4_SAMPLE_RATE, 0, 1, 0,
			abox_auto_config_get, abox_auto_config_put),
	SOC_SINGLE_EXT("Echo Cancellation", ERAP_ECHO_CANCEL, 0, 2, 0,
			abox_erap_handler_get, abox_erap_handler_put),
	SOC_SINGLE_EXT("VI Sensing", ERAP_VI_SENSE, 0, 2, 0,
			abox_erap_handler_get, abox_erap_handler_put),
	SOC_VALUE_ENUM_EXT("Audio Mode", abox_audio_mode_enum,
			abox_audio_mode_get, abox_audio_mode_put),
	SOC_VALUE_ENUM_EXT("Sound Type", abox_sound_type_enum,
			abox_sound_type_get, abox_sound_type_put),
	SOC_SINGLE_EXT("Tickle", 0, 0, 1, 0, abox_tickle_get, abox_tickle_put),
	SOC_SINGLE_EXT("Wakelock", 0, 0, 1, 0,
			wake_lock_get, wake_lock_put),
	SOC_SINGLE_EXT_TLV("RDMA VOL FACTOR3", 3, 0,
			DMA_VOL_FACTOR_MAX_STEPS, 0,
			abox_rdma_vol_factor_get, abox_rdma_vol_factor_put,
			abox_rdma_vol_factor_gain),
};

static const char * const spus_inx_texts[] = {"RDMA", "SIFSM"};
static SOC_ENUM_SINGLE_DECL(spus_in0_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(0), spus_inx_texts);
static const struct snd_kcontrol_new spus_in0_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in0_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in1_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(1), spus_inx_texts);
static const struct snd_kcontrol_new spus_in1_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in1_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in2_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(2), spus_inx_texts);
static const struct snd_kcontrol_new spus_in2_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in2_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in3_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(3), spus_inx_texts);
static const struct snd_kcontrol_new spus_in3_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in3_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in4_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(4), spus_inx_texts);
static const struct snd_kcontrol_new spus_in4_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in4_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in5_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(5), spus_inx_texts);
static const struct snd_kcontrol_new spus_in5_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in5_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in6_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(6), spus_inx_texts);
static const struct snd_kcontrol_new spus_in6_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in6_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_in7_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_IN_L(7), spus_inx_texts);
static const struct snd_kcontrol_new spus_in7_controls[] = {
	SOC_DAPM_ENUM("MUX", spus_in7_enum),
};

static const char * const spus_asrcx_texts[] = {"Off", "On"};
static SOC_ENUM_SINGLE_DECL(spus_asrc0_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(0), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc0_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc0_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_asrc1_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(1), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc1_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc1_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_asrc2_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(2), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc2_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc2_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_asrc3_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(3), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc3_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc3_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_asrc4_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(4), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc4_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc4_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_asrc5_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(5), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc5_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc5_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_asrc6_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(6), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc6_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc6_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_asrc7_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_ASRC_L(7), spus_asrcx_texts);
static const struct snd_kcontrol_new spus_asrc7_controls[] = {
	SOC_DAPM_ENUM("ASRC", spus_asrc7_enum),
};

static const char * const spus_outx_texts[] = {"SIFS1", "SIFS0", "SIFS2"};
static SOC_ENUM_SINGLE_DECL(spus_out0_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(0), spus_outx_texts);
static const struct snd_kcontrol_new spus_out0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out0_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out1_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(1), spus_outx_texts);
static const struct snd_kcontrol_new spus_out1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out1_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out2_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(2), spus_outx_texts);
static const struct snd_kcontrol_new spus_out2_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out2_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out3_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(3), spus_outx_texts);
static const struct snd_kcontrol_new spus_out3_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out3_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out4_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(4), spus_outx_texts);
static const struct snd_kcontrol_new spus_out4_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out4_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out5_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(5), spus_outx_texts);
static const struct snd_kcontrol_new spus_out5_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out5_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out6_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(6), spus_outx_texts);
static const struct snd_kcontrol_new spus_out6_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out6_enum),
};
static SOC_ENUM_SINGLE_DECL(spus_out7_enum, ABOX_SPUS_CTRL0,
		ABOX_FUNC_CHAIN_SRC_OUT_L(7), spus_outx_texts);
static const struct snd_kcontrol_new spus_out7_controls[] = {
	SOC_DAPM_ENUM("DEMUX", spus_out7_enum),
};

static const char * const spusm_texts[] = {
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"UAIF0", "UAIF1", "UAIF2", "UAIF3", "UAIF4",
	"RESERVED", "RESERVED", "SPDY",
};
static SOC_ENUM_SINGLE_DECL(spusm_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_SPUSM_L,
		spusm_texts);
static const struct snd_kcontrol_new spusm_controls[] = {
	SOC_DAPM_ENUM("MUX", spusm_enum),
};

static int abox_flush_mixp(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	dev_info(dev, "%s: flush\n", __func__);
	snd_soc_component_update_bits(cmpnt, ABOX_SPUS_CTRL2,
			ABOX_SPUS_MIXP_FLUSH_MASK,
			1 << ABOX_SPUS_MIXP_FLUSH_L);

	return 0;
}

static int abox_flush_sifm(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_input_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUS_CTRL3,
				ABOX_SPUS_SIFM_FLUSH_MASK,
				1 << ABOX_SPUS_SIFM_FLUSH_L);
	}

	return 0;
}

static int abox_flush_sifs1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_input_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUS_CTRL3,
				ABOX_SPUS_SIFS1_FLUSH_MASK,
				1 << ABOX_SPUS_SIFS1_FLUSH_L);
	}

	return 0;
}

static int abox_flush_sifs2(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_input_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUS_CTRL3,
				ABOX_SPUS_SIFS2_FLUSH_MASK,
				1 << ABOX_SPUS_SIFS2_FLUSH_L);
	}

	return 0;
}

static int abox_flush_recp(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_output_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUM_CTRL2,
				ABOX_SPUM_RECP_FLUSH_MASK,
				1 << ABOX_SPUM_RECP_FLUSH_L);
	}

	return 0;
}

static int abox_flush_sifm0(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_output_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUM_CTRL3,
				ABOX_SPUM_SIFM0_FLUSH_MASK,
				1 << ABOX_SPUM_SIFM0_FLUSH_L);
	}

	return 0;
}

static int abox_flush_sifm1(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_output_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUM_CTRL3,
				ABOX_SPUM_SIFM1_FLUSH_MASK,
				1 << ABOX_SPUM_SIFM1_FLUSH_L);
	}

	return 0;
}

static int abox_flush_sifm2(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_output_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUM_CTRL3,
				ABOX_SPUM_SIFM2_FLUSH_MASK,
				1 << ABOX_SPUM_SIFM2_FLUSH_L);
	}

	return 0;
}

static int abox_flush_sifm3(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *cmpnt = snd_soc_dapm_to_component(w->dapm);
	struct device *dev = cmpnt->dev;

	dev_dbg(dev, "%s\n", __func__);

	if (!snd_soc_dapm_connected_output_ep(w, NULL)) {
		dev_info(dev, "%s: flush\n", __func__);
		snd_soc_component_update_bits(cmpnt, ABOX_SPUM_CTRL3,
				ABOX_SPUM_SIFM3_FLUSH_MASK,
				1 << ABOX_SPUM_SIFM3_FLUSH_L);
	}

	return 0;
}

static const char * const sifsx_texts[] = {
	"SPUS OUT0", "SPUS OUT1", "SPUS OUT2", "SPUS OUT3",
	"SPUS OUT4", "SPUS OUT5", "SPUS OUT6", "SPUS OUT7",
};
static SOC_ENUM_SINGLE_DECL(sifs1_enum, ABOX_SPUS_CTRL1, ABOX_SIFS_OUT1_SEL_L,
		sifsx_texts);
static const struct snd_kcontrol_new sifs1_controls[] = {
	SOC_DAPM_ENUM("MUX", sifs1_enum),
};
static SOC_ENUM_SINGLE_DECL(sifs2_enum, ABOX_SPUS_CTRL1, ABOX_SIFS_OUT2_SEL_L,
		sifsx_texts);
static const struct snd_kcontrol_new sifs2_controls[] = {
	SOC_DAPM_ENUM("MUX", sifs2_enum),
};

static const char * const sifsm_texts[] = {
	"SPUS IN0", "SPUS IN1", "SPUS IN2", "SPUS IN3",
	"SPUS IN4", "SPUS IN5", "SPUS IN6", "SPUS IN7",
};
static SOC_ENUM_SINGLE_DECL(sifsm_enum, ABOX_SPUS_CTRL1, ABOX_SIFM_IN_SEL_L,
		sifsm_texts);
static const struct snd_kcontrol_new sifsm_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifsm_enum),
};

static const char * const uaif_spkx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "SIFS2",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"SIFMS",
};
static SOC_ENUM_SINGLE_DECL(uaif0_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(0), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif0_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif0_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif1_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(1), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif1_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif1_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif2_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(2), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif2_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif2_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif3_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(3), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif3_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif3_spk_enum),
};
static SOC_ENUM_SINGLE_DECL(uaif4_spk_enum, ABOX_ROUTE_CTRL0,
		ABOX_ROUTE_UAIF_SPK_L(4), uaif_spkx_texts);
static const struct snd_kcontrol_new uaif4_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", uaif4_spk_enum),
};

static const char * const dsif_spk_texts[] = {
	"RESERVED", "RESERVED", "SIFS1", "SIFS2",
};
static SOC_ENUM_SINGLE_DECL(dsif_spk_enum, ABOX_ROUTE_CTRL0, 20,
		dsif_spk_texts);
static const struct snd_kcontrol_new dsif_spk_controls[] = {
	SOC_DAPM_ENUM("MUX", dsif_spk_enum),
};

static const char * const rsrcx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "SIFS2",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"NSRC0", "NSRC1", "NSRC2", "NSRC3",
};
static SOC_ENUM_SINGLE_DECL(rsrc0_enum, ABOX_ROUTE_CTRL2, ABOX_ROUTE_RSRC_L(0),
		rsrcx_texts);
static const struct snd_kcontrol_new rsrc0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", rsrc0_enum),
};
static SOC_ENUM_SINGLE_DECL(rsrc1_enum, ABOX_ROUTE_CTRL2, ABOX_ROUTE_RSRC_L(1),
		rsrcx_texts);
static const struct snd_kcontrol_new rsrc1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", rsrc1_enum),
};

static const char * const nsrcx_texts[] = {
	"RESERVED", "SIFS0", "SIFS1", "SIFS2",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"UAIF0", "UAIF1", "UAIF2", "UAIF3", "UAIF4",
	"RESERVED", "RESERVED", "SPDY",
};
static SOC_ENUM_SINGLE_DECL(nsrc0_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(0),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc0_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc1_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(1),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc1_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc2_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(2),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc2_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc2_enum),
};
static SOC_ENUM_SINGLE_DECL(nsrc3_enum, ABOX_ROUTE_CTRL1, ABOX_ROUTE_NSRC_L(3),
		nsrcx_texts);
static const struct snd_kcontrol_new nsrc3_controls[] = {
	SOC_DAPM_ENUM("DEMUX", nsrc3_enum),
};

static const struct snd_kcontrol_new recp_controls[] = {
	SOC_DAPM_SINGLE("PIFS0", ABOX_SPUM_CTRL1, ABOX_RECP_SRC_VALID_L, 1, 0),
	SOC_DAPM_SINGLE("PIFS1", ABOX_SPUM_CTRL1, ABOX_RECP_SRC_VALID_H, 1, 0),
};

static const char * const spum_asrcx_texts[] = {"Off", "On"};
static SOC_ENUM_SINGLE_DECL(spum_asrc0_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_RSRC_ASRC_L, spum_asrcx_texts);
static const struct snd_kcontrol_new spum_asrc0_controls[] = {
	SOC_DAPM_ENUM("ASRC", spum_asrc0_enum),
};
static SOC_ENUM_SINGLE_DECL(spum_asrc1_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_ASRC_L(0), spum_asrcx_texts);
static const struct snd_kcontrol_new spum_asrc1_controls[] = {
	SOC_DAPM_ENUM("ASRC", spum_asrc1_enum),
};
static SOC_ENUM_SINGLE_DECL(spum_asrc2_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_ASRC_L(1), spum_asrcx_texts);
static const struct snd_kcontrol_new spum_asrc2_controls[] = {
	SOC_DAPM_ENUM("ASRC", spum_asrc2_enum),
};
static SOC_ENUM_SINGLE_DECL(spum_asrc3_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_ASRC_L(2), spum_asrcx_texts);
static const struct snd_kcontrol_new spum_asrc3_controls[] = {
	SOC_DAPM_ENUM("ASRC", spum_asrc3_enum),
};

static const char * const sifmx_texts[] = {
	"WDMA", "SIFMS",
};
static SOC_ENUM_SINGLE_DECL(sifm0_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(0), sifmx_texts);
static const struct snd_kcontrol_new sifm0_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm0_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm1_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(1), sifmx_texts);
static const struct snd_kcontrol_new sifm1_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm1_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm2_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(2), sifmx_texts);
static const struct snd_kcontrol_new sifm2_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm2_enum),
};
static SOC_ENUM_SINGLE_DECL(sifm3_enum, ABOX_SPUM_CTRL0,
		ABOX_FUNC_CHAIN_NSRC_OUT_L(3), sifmx_texts);
static const struct snd_kcontrol_new sifm3_controls[] = {
	SOC_DAPM_ENUM("DEMUX", sifm3_enum),
};

static const char * const sifms_texts[] = {
	"RESERVED", "SIFM0", "SIFM1", "SIFM2", "SIFM3",
};
static SOC_ENUM_SINGLE_DECL(sifms_enum, ABOX_SPUM_CTRL1, ABOX_SIFS_OUT_SEL_L,
		sifms_texts);
static const struct snd_kcontrol_new sifms_controls[] = {
	SOC_DAPM_ENUM("MUX", sifms_enum),
};

static const struct snd_soc_dapm_widget abox_cmpnt_dapm_widgets[] = {
	SND_SOC_DAPM_MUX("SPUSM", SND_SOC_NOPM, 0, 0, spusm_controls),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN3", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN4", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN5", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN6", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFSM-SPUS IN7", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_DEMUX_E("SIFSM", SND_SOC_NOPM, 0, 0, sifsm_controls,
			abox_flush_sifm,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SPUS IN0", SND_SOC_NOPM, 0, 0, spus_in0_controls),
	SND_SOC_DAPM_MUX("SPUS IN1", SND_SOC_NOPM, 0, 0, spus_in1_controls),
	SND_SOC_DAPM_MUX("SPUS IN2", SND_SOC_NOPM, 0, 0, spus_in2_controls),
	SND_SOC_DAPM_MUX("SPUS IN3", SND_SOC_NOPM, 0, 0, spus_in3_controls),
	SND_SOC_DAPM_MUX("SPUS IN4", SND_SOC_NOPM, 0, 0, spus_in4_controls),
	SND_SOC_DAPM_MUX("SPUS IN5", SND_SOC_NOPM, 0, 0, spus_in5_controls),
	SND_SOC_DAPM_MUX("SPUS IN6", SND_SOC_NOPM, 0, 0, spus_in6_controls),
	SND_SOC_DAPM_MUX("SPUS IN7", SND_SOC_NOPM, 0, 0, spus_in7_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC0", SND_SOC_NOPM, 0, 0, spus_asrc0_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC1", SND_SOC_NOPM, 0, 0, spus_asrc1_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC2", SND_SOC_NOPM, 0, 0, spus_asrc2_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC3", SND_SOC_NOPM, 0, 0, spus_asrc3_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC4", SND_SOC_NOPM, 0, 0, spus_asrc4_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC5", SND_SOC_NOPM, 0, 0, spus_asrc5_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC6", SND_SOC_NOPM, 0, 0, spus_asrc6_controls),
	SND_SOC_DAPM_MUX("SPUS ASRC7", SND_SOC_NOPM, 0, 0, spus_asrc7_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT0", SND_SOC_NOPM, 0, 0, spus_out0_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT1", SND_SOC_NOPM, 0, 0, spus_out1_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT2", SND_SOC_NOPM, 0, 0, spus_out2_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT3", SND_SOC_NOPM, 0, 0, spus_out3_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT4", SND_SOC_NOPM, 0, 0, spus_out4_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT5", SND_SOC_NOPM, 0, 0, spus_out5_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT6", SND_SOC_NOPM, 0, 0, spus_out6_controls),
	SND_SOC_DAPM_DEMUX("SPUS OUT7", SND_SOC_NOPM, 0, 0, spus_out7_controls),

	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT0-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT1-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT2-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT3-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT4-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT5-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT6-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SPUS OUT7-SIFS2", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MIXER_E("SIFS0", SND_SOC_NOPM, 0, 0, NULL, 0,
			abox_flush_mixp,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SIFS1", SND_SOC_NOPM, 0, 0, sifs1_controls,
			abox_flush_sifs1,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_MUX_E("SIFS2", SND_SOC_NOPM, 0, 0, sifs2_controls,
			abox_flush_sifs2,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("UAIF0 SPK", SND_SOC_NOPM, 0, 0, uaif0_spk_controls),
	SND_SOC_DAPM_MUX("UAIF1 SPK", SND_SOC_NOPM, 0, 0, uaif1_spk_controls),
	SND_SOC_DAPM_MUX("UAIF2 SPK", SND_SOC_NOPM, 0, 0, uaif2_spk_controls),
	SND_SOC_DAPM_MUX("UAIF3 SPK", SND_SOC_NOPM, 0, 0, uaif3_spk_controls),
	SND_SOC_DAPM_MUX("UAIF4 SPK", SND_SOC_NOPM, 0, 0, uaif4_spk_controls),
	SND_SOC_DAPM_MUX("DSIF SPK", SND_SOC_NOPM, 0, 0, dsif_spk_controls),

	SND_SOC_DAPM_MUX("RSRC0", SND_SOC_NOPM, 0, 0, rsrc0_controls),
	SND_SOC_DAPM_MUX("RSRC1", SND_SOC_NOPM, 0, 0, rsrc1_controls),
	SND_SOC_DAPM_MUX("NSRC0", SND_SOC_NOPM, 0, 0, nsrc0_controls),
	SND_SOC_DAPM_MUX("NSRC1", SND_SOC_NOPM, 0, 0, nsrc1_controls),
	SND_SOC_DAPM_MUX("NSRC2", SND_SOC_NOPM, 0, 0, nsrc2_controls),
	SND_SOC_DAPM_MUX("NSRC3", SND_SOC_NOPM, 0, 0, nsrc3_controls),

	SND_SOC_DAPM_PGA("PIFS0", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PIFS1", SND_SOC_NOPM, 0, 0, NULL, 0),
	SOC_MIXER_E_ARRAY("RECP", SND_SOC_NOPM, 0, 0, recp_controls,
			abox_flush_recp,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_MUX("SPUM ASRC0", SND_SOC_NOPM, 0, 0, spum_asrc0_controls),
	SND_SOC_DAPM_MUX("SPUM ASRC1", SND_SOC_NOPM, 0, 0, spum_asrc1_controls),
	SND_SOC_DAPM_MUX("SPUM ASRC2", SND_SOC_NOPM, 0, 0, spum_asrc2_controls),
	SND_SOC_DAPM_MUX("SPUM ASRC3", SND_SOC_NOPM, 0, 0, spum_asrc3_controls),
	SND_SOC_DAPM_DEMUX_E("SIFM0", SND_SOC_NOPM, 0, 0, sifm0_controls,
			abox_flush_sifm0,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DEMUX_E("SIFM1", SND_SOC_NOPM, 0, 0, sifm1_controls,
			abox_flush_sifm1,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DEMUX_E("SIFM2", SND_SOC_NOPM, 0, 0, sifm2_controls,
			abox_flush_sifm2,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),
	SND_SOC_DAPM_DEMUX_E("SIFM3", SND_SOC_NOPM, 0, 0, sifm3_controls,
			abox_flush_sifm3,
			SND_SOC_DAPM_PRE_PMU | SND_SOC_DAPM_POST_PMD),

	SND_SOC_DAPM_PGA("SIFM0-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFM1-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFM2-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("SIFM3-SIFMS", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("SIFMS", SND_SOC_NOPM, 0, 0, sifms_controls),

	SND_SOC_DAPM_AIF_IN("UAIF0IN", "UAIF0 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("UAIF1IN", "UAIF1 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("UAIF2IN", "UAIF2 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("UAIF3IN", "UAIF3 Capture", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_AIF_IN("UAIF4IN", "UAIF4 Capture", 0, SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_AIF_OUT("UAIF0OUT", "UAIF0 Playback", 0, SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_AIF_OUT("UAIF1OUT", "UAIF1 Playback", 0, SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_AIF_OUT("UAIF2OUT", "UAIF2 Playback", 0, SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_AIF_OUT("UAIF3OUT", "UAIF3 Playback", 0, SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_AIF_OUT("UAIF4OUT", "UAIF4 Playback", 0, SND_SOC_NOPM,
			0, 0),
	SND_SOC_DAPM_AIF_OUT("DSIFOUT", "DSIF Playback", 0, SND_SOC_NOPM, 0, 0),
};

static const struct snd_soc_dapm_route abox_cmpnt_dapm_routes[] = {
	/* sink, control, source */
	{"SIFSM", NULL, "SPUSM"},
	{"SIFSM-SPUS IN0", "SPUS IN0", "SIFSM"},
	{"SIFSM-SPUS IN1", "SPUS IN1", "SIFSM"},
	{"SIFSM-SPUS IN2", "SPUS IN2", "SIFSM"},
	{"SIFSM-SPUS IN3", "SPUS IN3", "SIFSM"},
	{"SIFSM-SPUS IN4", "SPUS IN4", "SIFSM"},
	{"SIFSM-SPUS IN5", "SPUS IN5", "SIFSM"},
	{"SIFSM-SPUS IN6", "SPUS IN6", "SIFSM"},
	{"SIFSM-SPUS IN7", "SPUS IN7", "SIFSM"},

	{"SPUS IN0", "RDMA", "RDMA0 Playback"},
	{"SPUS IN0", "SIFSM", "SIFSM-SPUS IN0"},
	{"SPUS IN1", "RDMA", "RDMA1 Playback"},
	{"SPUS IN1", "SIFSM", "SIFSM-SPUS IN1"},
	{"SPUS IN2", "RDMA", "RDMA2 Playback"},
	{"SPUS IN2", "SIFSM", "SIFSM-SPUS IN2"},
	{"SPUS IN3", "RDMA", "RDMA3 Playback"},
	{"SPUS IN3", "SIFSM", "SIFSM-SPUS IN3"},
	{"SPUS IN4", "RDMA", "RDMA4 Playback"},
	{"SPUS IN4", "SIFSM", "SIFSM-SPUS IN4"},
	{"SPUS IN5", "RDMA", "RDMA5 Playback"},
	{"SPUS IN5", "SIFSM", "SIFSM-SPUS IN5"},
	{"SPUS IN6", "RDMA", "RDMA6 Playback"},
	{"SPUS IN6", "SIFSM", "SIFSM-SPUS IN6"},
	{"SPUS IN7", "RDMA", "RDMA7 Playback"},
	{"SPUS IN7", "SIFSM", "SIFSM-SPUS IN7"},

	{"SPUS ASRC0", "On", "SPUS IN0"},
	{"SPUS ASRC0", "Off", "SPUS IN0"},
	{"SPUS ASRC1", "On", "SPUS IN1"},
	{"SPUS ASRC1", "Off", "SPUS IN1"},
	{"SPUS ASRC2", "On", "SPUS IN2"},
	{"SPUS ASRC2", "Off", "SPUS IN2"},
	{"SPUS ASRC3", "On", "SPUS IN3"},
	{"SPUS ASRC3", "Off", "SPUS IN3"},
	{"SPUS ASRC4", "On", "SPUS IN4"},
	{"SPUS ASRC4", "Off", "SPUS IN4"},
	{"SPUS ASRC5", "On", "SPUS IN5"},
	{"SPUS ASRC5", "Off", "SPUS IN5"},
	{"SPUS ASRC6", "On", "SPUS IN6"},
	{"SPUS ASRC6", "Off", "SPUS IN6"},
	{"SPUS ASRC7", "On", "SPUS IN7"},
	{"SPUS ASRC7", "Off", "SPUS IN7"},

	{"SPUS OUT0", NULL, "SPUS ASRC0"},
	{"SPUS OUT1", NULL, "SPUS ASRC1"},
	{"SPUS OUT2", NULL, "SPUS ASRC2"},
	{"SPUS OUT3", NULL, "SPUS ASRC3"},
	{"SPUS OUT4", NULL, "SPUS ASRC4"},
	{"SPUS OUT5", NULL, "SPUS ASRC5"},
	{"SPUS OUT6", NULL, "SPUS ASRC6"},
	{"SPUS OUT7", NULL, "SPUS ASRC7"},

	{"SPUS OUT0-SIFS0", "SIFS0", "SPUS OUT0"},
	{"SPUS OUT1-SIFS0", "SIFS0", "SPUS OUT1"},
	{"SPUS OUT2-SIFS0", "SIFS0", "SPUS OUT2"},
	{"SPUS OUT3-SIFS0", "SIFS0", "SPUS OUT3"},
	{"SPUS OUT4-SIFS0", "SIFS0", "SPUS OUT4"},
	{"SPUS OUT5-SIFS0", "SIFS0", "SPUS OUT5"},
	{"SPUS OUT6-SIFS0", "SIFS0", "SPUS OUT6"},
	{"SPUS OUT7-SIFS0", "SIFS0", "SPUS OUT7"},
	{"SPUS OUT0-SIFS1", "SIFS1", "SPUS OUT0"},
	{"SPUS OUT1-SIFS1", "SIFS1", "SPUS OUT1"},
	{"SPUS OUT2-SIFS1", "SIFS1", "SPUS OUT2"},
	{"SPUS OUT3-SIFS1", "SIFS1", "SPUS OUT3"},
	{"SPUS OUT4-SIFS1", "SIFS1", "SPUS OUT4"},
	{"SPUS OUT5-SIFS1", "SIFS1", "SPUS OUT5"},
	{"SPUS OUT6-SIFS1", "SIFS1", "SPUS OUT6"},
	{"SPUS OUT7-SIFS1", "SIFS1", "SPUS OUT7"},
	{"SPUS OUT0-SIFS2", "SIFS2", "SPUS OUT0"},
	{"SPUS OUT1-SIFS2", "SIFS2", "SPUS OUT1"},
	{"SPUS OUT2-SIFS2", "SIFS2", "SPUS OUT2"},
	{"SPUS OUT3-SIFS2", "SIFS2", "SPUS OUT3"},
	{"SPUS OUT4-SIFS2", "SIFS2", "SPUS OUT4"},
	{"SPUS OUT5-SIFS2", "SIFS2", "SPUS OUT5"},
	{"SPUS OUT6-SIFS2", "SIFS2", "SPUS OUT6"},
	{"SPUS OUT7-SIFS2", "SIFS2", "SPUS OUT7"},

	{"SIFS0", NULL, "SPUS OUT0-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT1-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT2-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT3-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT4-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT5-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT6-SIFS0"},
	{"SIFS0", NULL, "SPUS OUT7-SIFS0"},
	{"SIFS1", "SPUS OUT0", "SPUS OUT0-SIFS1"},
	{"SIFS1", "SPUS OUT1", "SPUS OUT1-SIFS1"},
	{"SIFS1", "SPUS OUT2", "SPUS OUT2-SIFS1"},
	{"SIFS1", "SPUS OUT3", "SPUS OUT3-SIFS1"},
	{"SIFS1", "SPUS OUT4", "SPUS OUT4-SIFS1"},
	{"SIFS1", "SPUS OUT5", "SPUS OUT5-SIFS1"},
	{"SIFS1", "SPUS OUT6", "SPUS OUT6-SIFS1"},
	{"SIFS1", "SPUS OUT7", "SPUS OUT7-SIFS1"},
	{"SIFS2", "SPUS OUT0", "SPUS OUT0-SIFS2"},
	{"SIFS2", "SPUS OUT1", "SPUS OUT1-SIFS2"},
	{"SIFS2", "SPUS OUT2", "SPUS OUT2-SIFS2"},
	{"SIFS2", "SPUS OUT3", "SPUS OUT3-SIFS2"},
	{"SIFS2", "SPUS OUT4", "SPUS OUT4-SIFS2"},
	{"SIFS2", "SPUS OUT5", "SPUS OUT5-SIFS2"},
	{"SIFS2", "SPUS OUT6", "SPUS OUT6-SIFS2"},
	{"SIFS2", "SPUS OUT7", "SPUS OUT7-SIFS2"},

	{"SIFS0 Playback", NULL, "SIFS0"},
	{"SIFS1 Playback", NULL, "SIFS1"},
	{"SIFS2 Playback", NULL, "SIFS2"},

	{"UAIF0 SPK", "SIFS0", "SIFS0"},
	{"UAIF0 SPK", "SIFS1", "SIFS1"},
	{"UAIF0 SPK", "SIFS2", "SIFS2"},
	{"UAIF0 SPK", "SIFMS", "SIFMS"},
	{"UAIF1 SPK", "SIFS0", "SIFS0"},
	{"UAIF1 SPK", "SIFS1", "SIFS1"},
	{"UAIF1 SPK", "SIFS2", "SIFS2"},
	{"UAIF1 SPK", "SIFMS", "SIFMS"},
	{"UAIF2 SPK", "SIFS0", "SIFS0"},
	{"UAIF2 SPK", "SIFS1", "SIFS1"},
	{"UAIF2 SPK", "SIFS2", "SIFS2"},
	{"UAIF2 SPK", "SIFMS", "SIFMS"},
	{"UAIF3 SPK", "SIFS0", "SIFS0"},
	{"UAIF3 SPK", "SIFS1", "SIFS1"},
	{"UAIF3 SPK", "SIFS2", "SIFS2"},
	{"UAIF3 SPK", "SIFMS", "SIFMS"},
	{"UAIF4 SPK", "SIFS0", "SIFS0"},
	{"UAIF4 SPK", "SIFS1", "SIFS1"},
	{"UAIF4 SPK", "SIFS2", "SIFS2"},
	{"UAIF4 SPK", "SIFMS", "SIFMS"},
	{"DSIF SPK", "SIFS1", "SIFS1"},
	{"DSIF SPK", "SIFS2", "SIFS2"},

	{"RSRC0", "SIFS0", "SIFS0 Capture"},
	{"RSRC0", "SIFS1", "SIFS1 Capture"},
	{"RSRC0", "SIFS2", "SIFS2 Capture"},
	{"RSRC0", "NSRC0", "NSRC0"},
	{"RSRC0", "NSRC1", "NSRC1"},
	{"RSRC0", "NSRC2", "NSRC2"},
	{"RSRC0", "NSRC3", "NSRC3"},
	{"RSRC1", "SIFS0", "SIFS0 Capture"},
	{"RSRC1", "SIFS1", "SIFS1 Capture"},
	{"RSRC1", "SIFS2", "SIFS2 Capture"},
	{"RSRC1", "NSRC0", "NSRC0"},
	{"RSRC1", "NSRC1", "NSRC1"},
	{"RSRC1", "NSRC2", "NSRC2"},
	{"RSRC1", "NSRC3", "NSRC3"},

	{"NSRC0", "SIFS0", "SIFS0 Capture"},
	{"NSRC0", "SIFS1", "SIFS1 Capture"},
	{"NSRC0", "SIFS2", "SIFS2 Capture"},
	{"NSRC1", "SIFS0", "SIFS0 Capture"},
	{"NSRC1", "SIFS1", "SIFS1 Capture"},
	{"NSRC1", "SIFS2", "SIFS2 Capture"},
	{"NSRC2", "SIFS0", "SIFS0 Capture"},
	{"NSRC2", "SIFS1", "SIFS1 Capture"},
	{"NSRC2", "SIFS2", "SIFS2 Capture"},
	{"NSRC3", "SIFS0", "SIFS0 Capture"},
	{"NSRC3", "SIFS1", "SIFS1 Capture"},
	{"NSRC3", "SIFS2", "SIFS2 Capture"},

	{"PIFS0", NULL, "RSRC0"},
	{"PIFS1", NULL, "RSRC1"},
	{"RECP", "PIFS0", "PIFS0"},
	{"RECP", "PIFS1", "PIFS1"},

	{"SPUM ASRC0", "On", "RECP"},
	{"SPUM ASRC0", "Off", "RECP"},
	{"SPUM ASRC1", "On", "NSRC0"},
	{"SPUM ASRC1", "Off", "NSRC0"},
	{"SPUM ASRC2", "On", "NSRC1"},
	{"SPUM ASRC2", "Off", "NSRC1"},
	{"SPUM ASRC3", "On", "NSRC2"},
	{"SPUM ASRC3", "Off", "NSRC2"},

	{"SIFM0", NULL, "SPUM ASRC1"},
	{"SIFM1", NULL, "SPUM ASRC2"},
	{"SIFM2", NULL, "SPUM ASRC3"},
	{"SIFM3", NULL, "NSRC3"},

	{"SIFM0-SIFMS", "SIFMS", "SIFM0"},
	{"SIFM1-SIFMS", "SIFMS", "SIFM1"},
	{"SIFM2-SIFMS", "SIFMS", "SIFM2"},
	{"SIFM3-SIFMS", "SIFMS", "SIFM3"},

	{"SIFMS", "SIFM0", "SIFM0-SIFMS"},
	{"SIFMS", "SIFM1", "SIFM1-SIFMS"},
	{"SIFMS", "SIFM2", "SIFM2-SIFMS"},
	{"SIFMS", "SIFM3", "SIFM3-SIFMS"},

	{"WDMA0 Capture", NULL, "SPUM ASRC0"},
	{"WDMA1 Capture", "WDMA", "SIFM0"},
	{"WDMA2 Capture", "WDMA", "SIFM1"},
	{"WDMA3 Capture", "WDMA", "SIFM2"},
	{"WDMA4 Capture", "WDMA", "SIFM3"},
};

static bool abox_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ABOX_SYSPOWER_CTRL:
	case ABOX_SYSPOWER_STATUS:
	case ABOX_SPUS_CTRL2:
	case ABOX_SPUS_CTRL3:
	case ABOX_SPUM_CTRL2:
	case ABOX_SPUM_CTRL3:
	case ABOX_UAIF_STATUS(0):
	case ABOX_UAIF_STATUS(1):
	case ABOX_UAIF_STATUS(2):
	case ABOX_UAIF_STATUS(3):
	case ABOX_UAIF_STATUS(4):
	case ABOX_DSIF_STATUS:
	case ABOX_TIMER_CTRL0(0):
	case ABOX_TIMER_CTRL1(0):
	case ABOX_TIMER_CTRL0(1):
	case ABOX_TIMER_CTRL1(1):
	case ABOX_TIMER_CTRL0(2):
	case ABOX_TIMER_CTRL1(2):
	case ABOX_TIMER_CTRL0(3):
	case ABOX_TIMER_CTRL1(3):
		return true;
	default:
		return false;
	}
}

static bool abox_readable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ABOX_IP_INDEX:
	case ABOX_VERSION:
	case ABOX_SYSPOWER_CTRL:
	case ABOX_SYSPOWER_STATUS:
	case ABOX_SYSTEM_CONFIG0:
	case ABOX_REMAP_MASK:
	case ABOX_REMAP_ADDR:
	case ABOX_DYN_CLOCK_OFF:
	case ABOX_QCHANNEL_DISABLE:
	case ABOX_ROUTE_CTRL0:
	case ABOX_ROUTE_CTRL1:
	case ABOX_ROUTE_CTRL2:
	case ABOX_TICK_DIV_RATIO:
	case ABOX_SPUS_CTRL0:
	case ABOX_SPUS_CTRL1:
	case ABOX_SPUS_CTRL2:
	case ABOX_SPUS_CTRL3:
	case ABOX_SPUS_CTRL_SIFS_CNT0:
	case ABOX_SPUS_CTRL_SIFS_CNT1:
	case ABOX_SPUM_CTRL0:
	case ABOX_SPUM_CTRL1:
	case ABOX_SPUM_CTRL2:
	case ABOX_SPUM_CTRL3:
	case ABOX_UAIF_CTRL0(0):
	case ABOX_UAIF_CTRL1(0):
	case ABOX_UAIF_STATUS(0):
	case ABOX_UAIF_CTRL0(1):
	case ABOX_UAIF_CTRL1(1):
	case ABOX_UAIF_STATUS(1):
	case ABOX_UAIF_CTRL0(2):
	case ABOX_UAIF_CTRL1(2):
	case ABOX_UAIF_STATUS(2):
	case ABOX_UAIF_CTRL0(3):
	case ABOX_UAIF_CTRL1(3):
	case ABOX_UAIF_STATUS(3):
	case ABOX_UAIF_CTRL0(4):
	case ABOX_UAIF_CTRL1(4):
	case ABOX_UAIF_STATUS(4):
	case ABOX_DSIF_CTRL:
	case ABOX_DSIF_STATUS:
	case ABOX_SPDYIF_CTRL:
	case ABOX_TIMER_CTRL0(0):
	case ABOX_TIMER_CTRL1(0):
	case ABOX_TIMER_CTRL0(1):
	case ABOX_TIMER_CTRL1(1):
	case ABOX_TIMER_CTRL0(2):
	case ABOX_TIMER_CTRL1(2):
	case ABOX_TIMER_CTRL0(3):
	case ABOX_TIMER_CTRL1(3):
	case ABOX_RDMA_VOL_FACTOR(0):
	case ABOX_RDMA_VOL_FACTOR(1):
	case ABOX_RDMA_VOL_FACTOR(2):
	case ABOX_RDMA_VOL_FACTOR(3):
	case ABOX_RDMA_VOL_FACTOR(4):
	case ABOX_RDMA_VOL_FACTOR(5):
	case ABOX_RDMA_VOL_FACTOR(6):
	case ABOX_RDMA_VOL_FACTOR(7):
		return true;
	default:
		return false;
	}
}

static bool abox_writeable_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case ABOX_SYSPOWER_CTRL:
	case ABOX_SYSTEM_CONFIG0:
	case ABOX_REMAP_MASK:
	case ABOX_REMAP_ADDR:
	case ABOX_DYN_CLOCK_OFF:
	case ABOX_QCHANNEL_DISABLE:
	case ABOX_ROUTE_CTRL0:
	case ABOX_ROUTE_CTRL1:
	case ABOX_ROUTE_CTRL2:
	case ABOX_SPUS_CTRL0:
	case ABOX_SPUS_CTRL1:
	case ABOX_SPUS_CTRL2:
	case ABOX_SPUS_CTRL3:
	case ABOX_SPUS_CTRL_SIFS_CNT0:
	case ABOX_SPUS_CTRL_SIFS_CNT1:
	case ABOX_SPUM_CTRL0:
	case ABOX_SPUM_CTRL1:
	case ABOX_SPUM_CTRL2:
	case ABOX_SPUM_CTRL3:
	case ABOX_UAIF_CTRL0(0):
	case ABOX_UAIF_CTRL1(0):
	case ABOX_UAIF_CTRL0(1):
	case ABOX_UAIF_CTRL1(1):
	case ABOX_UAIF_CTRL0(2):
	case ABOX_UAIF_CTRL1(2):
	case ABOX_UAIF_CTRL0(3):
	case ABOX_UAIF_CTRL1(3):
	case ABOX_UAIF_CTRL0(4):
	case ABOX_UAIF_CTRL1(4):
	case ABOX_DSIF_CTRL:
	case ABOX_SPDYIF_CTRL:
	case ABOX_TIMER_CTRL0(0):
	case ABOX_TIMER_CTRL1(0):
	case ABOX_TIMER_CTRL0(1):
	case ABOX_TIMER_CTRL1(1):
	case ABOX_TIMER_CTRL0(2):
	case ABOX_TIMER_CTRL1(2):
	case ABOX_TIMER_CTRL0(3):
	case ABOX_TIMER_CTRL1(3):
	case ABOX_RDMA_VOL_FACTOR(0):
	case ABOX_RDMA_VOL_FACTOR(1):
	case ABOX_RDMA_VOL_FACTOR(2):
	case ABOX_RDMA_VOL_FACTOR(3):
	case ABOX_RDMA_VOL_FACTOR(4):
	case ABOX_RDMA_VOL_FACTOR(5):
	case ABOX_RDMA_VOL_FACTOR(6):
	case ABOX_RDMA_VOL_FACTOR(7):
		return true;
	default:
		return false;
	}
}

static const struct reg_default abox_reg_defaults_8895[] = {
	{0x0000, 0x41424F58},
	{0x0010, 0x00000000},
	{0x0014, 0x00000000},
	{0x0020, 0x00000000},
	{0x0024, 0xFFF00000},
	{0x0028, 0x13F00000},
	{0x0030, 0x7FFFFFFF},
	{0x0040, 0x00000000},
	{0x0044, 0x00000000},
	{0x0048, 0x00000000},
	{0x0200, 0x00000000},
	{0x0204, 0x00000000},
	{0x0208, 0x00000000},
	{0x020C, 0x00000000},
	{0x0220, 0x00000000},
	{0x0224, 0x00000000},
	{0x0228, 0x00000000},
	{0x022C, 0x00000000},
	{0x0230, 0x00000000},
	{0x0234, 0x00000000},
	{0x0238, 0x00000000},
	{0x023C, 0x00000000},
	{0x0240, 0x00000000},
	{0x0260, 0x00000000},
	{0x0300, 0x00000000},
	{0x0304, 0x00000000},
	{0x0308, 0x00000000},
	{0x030C, 0x00000000},
	{0x0320, 0x00000000},
	{0x0324, 0x00000000},
	{0x0328, 0x00000000},
	{0x032C, 0x00000000},
	{0x0330, 0x00000000},
	{0x0334, 0x00000000},
	{0x0338, 0x00000000},
	{0x033C, 0x00000000},
	{0x0340, 0x00000000},
	{0x0344, 0x00000000},
	{0x0348, 0x00000000},
	{0x0500, 0x01000000},
	{0x0504, 0x00000000},
	{0x050C, 0x00000000},
	{0x0510, 0x01000000},
	{0x0514, 0x00000000},
	{0x051C, 0x00000000},
	{0x0520, 0x01000000},
	{0x0524, 0x00000000},
	{0x052C, 0x00000000},
	{0x0530, 0x01000000},
	{0x0534, 0x00000000},
	{0x053C, 0x00000000},
	{0x0540, 0x01000000},
	{0x0544, 0x00000000},
	{0x054C, 0x00000000},
	{0x0550, 0x00000000},
	{0x0554, 0x00000000},
};

static const struct reg_default abox_reg_defaults_9810[] = {
	{0x0000, 0x41424F58},
	{0x0010, 0x00000000},
	{0x0014, 0x00000000},
	{0x0020, 0x00004444},
	{0x0024, 0xFFF00000},
	{0x0028, 0x17D00000},
	{0x0030, 0x7FFFFFFF},
	{0x0038, 0x00000000},
	{0x0040, 0x00000000},
	{0x0044, 0x00000000},
	{0x0048, 0x00000000},
	{0x0200, 0x00000000},
	{0x0204, 0x00000000},
	{0x0208, 0x00000000},
	{0x020C, 0x00000000},
	{0x0220, 0x00000000},
	{0x0224, 0x00000000},
	{0x0228, 0x00000000},
	{0x022C, 0x00000000},
	{0x0230, 0x00000000},
	{0x0234, 0x00000000},
	{0x0238, 0x00000000},
	{0x023C, 0x00000000},
	{0x0240, 0x00000000},
	{0x0260, 0x00000000},
	{0x0280, 0x00000000},
	{0x0284, 0x00000000},
	{0x0300, 0x00000000},
	{0x0304, 0x00000000},
	{0x0308, 0x00000000},
	{0x030C, 0x00000000},
	{0x0320, 0x00000000},
	{0x0324, 0x00000000},
	{0x0328, 0x00000000},
	{0x032C, 0x00000000},
	{0x0330, 0x00000000},
	{0x0334, 0x00000000},
	{0x0338, 0x00000000},
	{0x033C, 0x00000000},
	{0x0340, 0x00000000},
	{0x0344, 0x00000000},
	{0x0348, 0x00000000},
	{0x0500, 0x01000010},
	{0x0504, 0x00000000},
	{0x050C, 0x00000000},
	{0x0510, 0x01000010},
	{0x0514, 0x00000000},
	{0x051C, 0x00000000},
	{0x0520, 0x01000010},
	{0x0524, 0x00000000},
	{0x052C, 0x00000000},
	{0x0530, 0x01000010},
	{0x0534, 0x00000000},
	{0x053C, 0x00000000},
	{0x0540, 0x01000010},
	{0x0544, 0x00000000},
	{0x054C, 0x00000000},
	{0x0550, 0x00000000},
	{0x0554, 0x00000000},
	{0x1318, 0x00000000},
};

static const struct reg_default abox_reg_defaults_9610[] = {
	{0x0000, 0x41424F58},
	{0x0010, 0x00000000},
	{0x0014, 0x00000000},
	{0x0020, 0x00000000},
	{0x0024, 0xFFF00000},
	{0x0028, 0x14B00000},
	{0x0030, 0x7FFFFFFF},
	{0x0038, 0x00000000},
	{0x0040, 0x00000000},
	{0x0044, 0x00000000},
	{0x0048, 0x00000000},
	{0x0050, 0x000004E1},
	{0x0200, 0x00000000},
	{0x0204, 0x00000000},
	{0x0208, 0x00000000},
	{0x020C, 0x00000000},
	{0x0220, 0x00000000},
	{0x0224, 0x00000000},
	{0x0228, 0x00000000},
	{0x022C, 0x00000000},
	{0x0230, 0x00000000},
	{0x0234, 0x00000000},
	{0x0238, 0x00000000},
	{0x023C, 0x00000000},
	{0x0240, 0x00000000},
	{0x0244, 0x00000000},
	{0x0248, 0x00000000},
	{0x024C, 0x00000000},
	{0x0250, 0x00000000},
	{0x0254, 0x00000000},
	{0x0258, 0x00000000},
	{0x025C, 0x00000000},
	{0x0260, 0x00000000},
	{0x0280, 0x00000000},
	{0x0284, 0x00000000},
	{0x0300, 0x00000000},
	{0x0304, 0x00000000},
	{0x0308, 0x00000000},
	{0x030C, 0x00000000},
	{0x0320, 0x00000000},
	{0x0324, 0x00000000},
	{0x0328, 0x00000000},
	{0x032C, 0x00000000},
	{0x0330, 0x00000000},
	{0x0334, 0x00000000},
	{0x0338, 0x00000000},
	{0x033C, 0x00000000},
	{0x0340, 0x00000000},
	{0x0344, 0x00000000},
	{0x0348, 0x00000000},
	{0x0500, 0x01000010},
	{0x0504, 0x00000000},
	{0x050C, 0x00000000},
	{0x0510, 0x01000010},
	{0x0514, 0x00000000},
	{0x051C, 0x00000000},
	{0x0520, 0x01000010},
	{0x0524, 0x00000000},
	{0x052C, 0x00000000},
	{0x0530, 0x01000010},
	{0x0534, 0x00000000},
	{0x053C, 0x00000000},
	{0x0540, 0x01000010},
	{0x0544, 0x00000000},
	{0x054C, 0x00000000},
	{0x0550, 0x00000000},
	{0x0554, 0x00000000},
	{0x0560, 0x00000000},
	{0x1318, 0x00000000},
};

static struct regmap_config abox_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = ABOX_MAX_REGISTERS,
	.volatile_reg = abox_volatile_reg,
	.readable_reg = abox_readable_reg,
	.writeable_reg = abox_writeable_reg,
	.cache_type = REGCACHE_RBTREE,
	.fast_io = true,
};

static const struct snd_soc_component_driver abox_cmpnt = {
	.probe			= abox_cmpnt_probe,
	.remove			= abox_cmpnt_remove,
	.controls		= abox_cmpnt_controls,
	.num_controls		= ARRAY_SIZE(abox_cmpnt_controls),
	.dapm_widgets		= abox_cmpnt_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(abox_cmpnt_dapm_widgets),
	.dapm_routes		= abox_cmpnt_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(abox_cmpnt_dapm_routes),
	.probe_order		= SND_SOC_COMP_ORDER_FIRST,
};

struct abox_name_rate_format {
	const char *name;
	int stream;
	const enum ABOX_CONFIGMSG rate;
	const enum ABOX_CONFIGMSG format;
	bool slave;
};

static const struct abox_name_rate_format abox_nrf[] = {
	{"SIFS0", SNDRV_PCM_STREAM_PLAYBACK, SET_MIXER_SAMPLE_RATE,
			SET_MIXER_FORMAT, false},
	{"SIFS1", SNDRV_PCM_STREAM_PLAYBACK, SET_OUT1_SAMPLE_RATE,
			SET_OUT1_FORMAT, false},
	{"SIFS2", SNDRV_PCM_STREAM_PLAYBACK, SET_OUT2_SAMPLE_RATE,
			SET_OUT2_FORMAT, false},
	{"RECP", SNDRV_PCM_STREAM_CAPTURE, SET_RECP_SAMPLE_RATE,
			SET_RECP_FORMAT, true},
	{"SIFM0", SNDRV_PCM_STREAM_CAPTURE, SET_INMUX0_SAMPLE_RATE,
			SET_INMUX0_FORMAT, false},
	{"SIFM1", SNDRV_PCM_STREAM_CAPTURE, SET_INMUX1_SAMPLE_RATE,
			SET_INMUX1_FORMAT, false},
	{"SIFM2", SNDRV_PCM_STREAM_CAPTURE, SET_INMUX2_SAMPLE_RATE,
			SET_INMUX2_FORMAT, false},
	{"SIFM3", SNDRV_PCM_STREAM_CAPTURE, SET_INMUX3_SAMPLE_RATE,
			SET_INMUX3_FORMAT, false},
};

static bool abox_find_nrf_stream(const struct snd_soc_dapm_widget *w,
		int stream, enum ABOX_CONFIGMSG *rate,
		enum ABOX_CONFIGMSG *format, bool *slave)
{
	struct snd_soc_component *cmpnt = w->dapm->component;
	const char *name_prefix = cmpnt ? cmpnt->name_prefix : NULL;
	size_t prefix_len = name_prefix ? strlen(name_prefix) + 1 : 0;
	const char *name = w->name + prefix_len;
	const struct abox_name_rate_format *nrf;

	for (nrf = abox_nrf; nrf - abox_nrf < ARRAY_SIZE(abox_nrf); nrf++) {
		if ((nrf->stream == stream) && (strcmp(nrf->name, name) == 0)) {
			*rate = nrf->rate;
			*format = nrf->format;
			*slave = nrf->slave;
			return true;
		}
	}

	return false;
}

static bool abox_find_nrf(const struct snd_soc_dapm_widget *w,
		enum ABOX_CONFIGMSG *rate, enum ABOX_CONFIGMSG *format,
		int *stream, bool *slave)
{
	struct snd_soc_component *cmpnt = w->dapm->component;
	const char *name_prefix = cmpnt ? cmpnt->name_prefix : NULL;
	size_t prefix_len = name_prefix ? strlen(name_prefix) + 1 : 0;
	const char *name = w->name + prefix_len;
	const struct abox_name_rate_format *nrf;

	for (nrf = abox_nrf; nrf - abox_nrf < ARRAY_SIZE(abox_nrf); nrf++) {
		if (strcmp(nrf->name, name) == 0) {
			*rate = nrf->rate;
			*format = nrf->format;
			*stream = nrf->stream;
			*slave = nrf->slave;
			return true;
		}
	}

	return false;
}

static struct snd_soc_dapm_widget *abox_sync_params(struct abox_data *data,
		struct list_head *widget_list, int *stream,
		const struct snd_soc_dapm_widget *w_src,
		enum ABOX_CONFIGMSG rate, enum ABOX_CONFIGMSG format)
{
	struct device *dev = &data->pdev->dev;
	enum ABOX_CONFIGMSG msg_rate, msg_format;
	struct snd_soc_dapm_widget *w = NULL;
	bool slave;


	list_for_each_entry(w, widget_list, work_list) {
		if (!abox_find_nrf(w, &msg_rate, &msg_format, stream, &slave))
			continue;
		if (slave)
			continue;

		dev_dbg(dev, "%s: %s => %s\n", __func__, w->name, w_src->name);
		abox_set_sif_format(data, format,
				abox_get_sif_format(data, msg_format));
		abox_set_sif_rate(data, rate,
				abox_get_sif_rate(data, msg_rate));
		abox_set_sif_channels(data, format,
				abox_get_sif_channels(data, msg_format));
		break;
	}

	return w;
}

int abox_hw_params_fixup_helper(struct snd_soc_pcm_runtime *rtd,
		struct snd_pcm_hw_params *params, int stream)
{
	struct snd_soc_dai *dai = rtd->cpu_dai;
	struct snd_soc_component *cmpnt = dai->component;
	struct device *dev = is_abox(dai->dev) ? dai->dev : dai->dev->parent;
	struct abox_data *data = dev_get_drvdata(dev);
	struct snd_soc_dapm_widget *w, *w_tgt = NULL;
	struct snd_soc_dapm_path *path;
	LIST_HEAD(widget_list);
	enum ABOX_CONFIGMSG msg_rate, msg_format;
	unsigned int rate, channels, width;
	snd_pcm_format_t format;
	struct snd_soc_dapm_widget *w_mst = NULL;
	int stream_mst;

	dev_info(dev, "%s[%s](%d)\n", __func__, dai->name, stream);

	if (params_channels(params) < 1) {
		dev_info(dev, "channel is fixed from %d to 2\n",
				params_channels(params));
		hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS)->min = 2;
	}

	if (params_width(params) < 16) {
		dev_info(dev, "width is fixed from %d to 16\n",
				params_width(params));
		params_set_format(params, SNDRV_PCM_FORMAT_S16);
	}

	snd_soc_dapm_mutex_lock(snd_soc_component_get_dapm(cmpnt));

	/*
	 * For snd_soc_dapm_connected_{output,input}_ep fully discover the graph
	 * we need to reset the cached number of inputs and outputs.
	 */
	list_for_each_entry(w, &cmpnt->card->widgets, list) {
		w->endpoints[SND_SOC_DAPM_DIR_IN] = -1;
		w->endpoints[SND_SOC_DAPM_DIR_OUT] = -1;
	}

	if (!dai->playback_widget)
		goto skip_playback;
	snd_soc_dapm_widget_for_each_source_path(dai->playback_widget, path) {
		if (path->connect) {
			w = path->node[SND_SOC_DAPM_DIR_IN];
			snd_soc_dapm_connected_input_ep(w, &widget_list);
		}
	}
skip_playback:
	if (!dai->capture_widget)
		goto skip_capture;
	snd_soc_dapm_widget_for_each_sink_path(dai->capture_widget, path) {
		if (path->connect) {
			w = path->node[SND_SOC_DAPM_DIR_OUT];
			snd_soc_dapm_connected_output_ep(w, &widget_list);
		}
	}
skip_capture:

	/* find current params */
	list_for_each_entry(w, &widget_list, work_list) {
		bool slave;

		dev_dbg(dev, "%s\n", w->name);
		if (!abox_find_nrf_stream(w, stream, &msg_rate, &msg_format,
				&slave))
			continue;

		if (slave)
			w_mst = abox_sync_params(data, &widget_list, &stream_mst,
					w, msg_rate, msg_format);

		format = abox_get_sif_format(data, msg_format);
		width = snd_pcm_format_width(format);
		rate = abox_get_sif_rate(data, msg_rate);
		channels = abox_get_sif_channels(data, msg_format);
		dev_dbg(dev, "%s: %s: find %d bit, %u channel, %uHz\n",
				__func__, w->name, width, channels, rate);
		w_tgt = w;
		break;
	}

	if (!w_tgt)
		goto unlock;

	if (!w_mst) {
		w_mst = w_tgt;
		stream_mst = stream;
	}

	/* channel mixing isn't supported */
	abox_set_sif_channels(data, msg_format, params_channels(params));

	/* override formats to UAIF format, if it is connected */
	if (abox_if_hw_params_fixup(rtd, params, stream) >= 0) {
		abox_set_sif_auto_config(data, msg_rate, true);
	} else if (w_mst) {
		list_for_each_entry(w, &cmpnt->card->widgets, list) {
			w->endpoints[SND_SOC_DAPM_DIR_IN] = -1;
			w->endpoints[SND_SOC_DAPM_DIR_OUT] = -1;
		}
		list_del_init(&widget_list);
		if (stream_mst == SNDRV_PCM_STREAM_PLAYBACK)
			snd_soc_dapm_connected_output_ep(w_mst, &widget_list);
		else
			snd_soc_dapm_connected_input_ep(w_mst, &widget_list);

		list_for_each_entry(w, &widget_list, work_list) {
			struct snd_soc_dai *dai;

			if (!w->sname)
				continue;

			dai = w->priv;
			if (abox_if_hw_params_fixup_by_dai(dai, params, stream)
					>= 0) {
				abox_set_sif_auto_config(data, msg_rate, true);
				break;
			}
		}
	}

	if (!abox_get_sif_auto_config(data, msg_rate))
		goto unlock;

	format = params_format(params);
	width = params_width(params);
	rate = params_rate(params);
	channels = params_channels(params);

	if (dai->driver->symmetric_samplebits && dai->sample_width &&
			dai->sample_width != width) {
		width = dai->sample_width;
		abox_set_sif_width(data, msg_format, dai->sample_width);
		format = abox_get_sif_format(data, msg_format);
	}

	if (dai->driver->symmetric_channels && dai->channels &&
			dai->channels != channels)
		channels = dai->channels;

	if (dai->driver->symmetric_rates && dai->rate && dai->rate != rate)
		rate = dai->rate;

	dev_dbg(dev, "%s: set to %u bit, %u channel, %uHz\n", __func__,
			width, channels, rate);
unlock:
	snd_soc_dapm_mutex_unlock(snd_soc_component_get_dapm(cmpnt));

	if (!w_tgt)
		goto out;

	abox_sample_rate_put_ipc(dev, rate, msg_rate);
	abox_sif_format_put_ipc(dev, format, channels, msg_format);

	hw_param_interval(params, SNDRV_PCM_HW_PARAM_RATE)->min =
			abox_get_sif_rate(data, msg_rate);
	hw_param_interval(params, SNDRV_PCM_HW_PARAM_CHANNELS)->min =
			abox_get_sif_channels(data, msg_format);
	snd_mask_none(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT));
	snd_mask_set(hw_param_mask(params, SNDRV_PCM_HW_PARAM_FORMAT),
			abox_get_sif_format(data, msg_format));
	dev_info(dev, "%s: %s: %d bit, %u channel, %uHz\n",
			__func__, w_tgt->name,
			abox_get_sif_width(data, msg_format),
			abox_get_sif_channels(data, msg_format),
			abox_get_sif_rate(data, msg_rate));
out:
	return 0;
}
EXPORT_SYMBOL(abox_hw_params_fixup_helper);

static struct pm_qos_request abox_pm_qos_aud;
static struct pm_qos_request abox_pm_qos_int;
static struct pm_qos_request abox_pm_qos_mif;
static struct pm_qos_request abox_pm_qos_lit;
static struct pm_qos_request abox_pm_qos_big;

unsigned int abox_get_requiring_int_freq_in_khz(void)
{
	struct abox_data *data = p_abox_data;
	unsigned int gear;
	unsigned int int_freq;

	if (data == NULL)
		return 0;

	gear = data->cpu_gear;

	if (gear <= ARRAY_SIZE(data->pm_qos_int))
		int_freq = data->pm_qos_int[gear - 1];
	else
		int_freq = 0;

	return int_freq;
}
EXPORT_SYMBOL(abox_get_requiring_int_freq_in_khz);

unsigned int abox_get_requiring_aud_freq_in_khz(void)
{
	struct abox_data *data = p_abox_data;
	unsigned int gear;
	unsigned int aud_freq;

	if (data == NULL)
		return 0;

	gear = data->cpu_gear;

	if (gear <= ARRAY_SIZE(data->pm_qos_aud))
		aud_freq = data->pm_qos_aud[gear - 1];
	else
		aud_freq = 0;

	return aud_freq;
}
EXPORT_SYMBOL(abox_get_requiring_aud_freq_in_khz);

bool abox_cpu_gear_idle(struct device *dev, struct abox_data *data,
		unsigned int id)
{
	struct abox_qos_request *request;

	dev_dbg(dev, "%s(%x)\n", __func__, id);

	for (request = data->cpu_gear_requests;
			request - data->cpu_gear_requests <
			ARRAY_SIZE(data->cpu_gear_requests)
			&& request->id;
			request++) {
		if (id == request->id) {
			if (request->value >= ABOX_CPU_GEAR_MIN)
				return true;
			else
				return false;
		}
	}

	return true;
}

static void abox_check_cpu_gear(struct device *dev,
		struct abox_data *data,
		unsigned int old_id, unsigned int old_gear,
		unsigned int id, unsigned int gear)
{
	struct device *dev_abox = &data->pdev->dev;
	struct platform_device *pdev = to_platform_device(dev);

	if (id != ABOX_CPU_GEAR_BOOT)
		return;

	if (data->calliope_state == CALLIOPE_ENABLING)
		abox_boot_done(dev_abox, data->calliope_version);

	if (old_id != id) {
		if (gear < ABOX_CPU_GEAR_MIN) {
			/* new */
			dev_dbg(dev, "%s(%x): new\n", __func__, id);
			pm_wakeup_event(dev_abox, BOOT_DONE_TIMEOUT_MS);
			__pm_stay_awake(&data->ws_boot);
			abox_request_dram_on(pdev, (void *)BOOT_CPU_GEAR_ID, true);
		}
	} else {
		if ((old_gear >= ABOX_CPU_GEAR_MIN) &&
				(gear < ABOX_CPU_GEAR_MIN)) {
			/* on */
			dev_dbg(dev, "%s(%x): on\n", __func__, id);
			pm_wakeup_event(dev_abox, BOOT_DONE_TIMEOUT_MS);
			__pm_stay_awake(&data->ws_boot);
			abox_request_dram_on(pdev, (void *)BOOT_CPU_GEAR_ID, true);
		} else if ((old_gear < ABOX_CPU_GEAR_MIN) &&
				(gear >= ABOX_CPU_GEAR_MIN)) {
			/* off */
			dev_dbg(dev, "%s(%x): off\n", __func__, id);
			pm_relax(dev_abox);
			abox_request_dram_on(pdev, (void *)BOOT_CPU_GEAR_ID, false);
		}
	}
}

static void abox_notify_cpu_gear(struct abox_data *data, unsigned int freq, unsigned int clksrc_to_aclk)
{
	const unsigned long long TIMER_RATE = 26000000;
	struct device *dev = &data->pdev->dev;
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	unsigned long long ktime, atime;
	unsigned long long time = sched_clock();
	unsigned long rem = do_div(time, NSEC_PER_SEC);

	switch (data->calliope_state) {
	case CALLIOPE_ENABLING:
	case CALLIOPE_ENABLED:
		dev_dbg(dev, "%s\n", __func__);

		msg.ipcid = IPC_SYSTEM;
		system_msg->msgtype = ABOX_CHANGED_GEAR;
		system_msg->param1 = (int)freq;
		system_msg->param2 = (int)time; /* SEC */
		system_msg->param3 = (int)rem; /* NSEC */
		if (clksrc_to_aclk) {
			ktime = sched_clock();
			atime = readq_relaxed(data->sfr_base + ABOX_TIMER_CURVALUD_LSB(1));
			/* clock to ns */
			atime *= 500;
			do_div(atime, TIMER_RATE / 2000000);
		} else {
			ktime = ULLONG_MAX;
			atime = ULLONG_MAX;
		}

		system_msg->bundle.param_u64[0] = ktime;
		system_msg->bundle.param_u64[1] = atime;

		abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
		break;
	case CALLIOPE_DISABLING:
	case CALLIOPE_DISABLED:
	default:
		/* notification to passing by context is not needed */
		break;
	}
}

static void abox_change_cpu_gear_legacy(struct device *dev,
		struct abox_data *data)
{
	struct abox_qos_request *request;
	unsigned int gear = UINT_MAX;
	int ret;
	bool increasing;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->cpu_gear_requests;
			request - data->cpu_gear_requests <
			ARRAY_SIZE(data->cpu_gear_requests)
			&& request->id;
			request++) {
		if (gear > request->value)
			gear = request->value;

		dev_dbg(dev, "id=%x, value=%u, gear=%u\n", request->id,
				request->value, gear);
	}

	if (data->cpu_gear == gear)
		goto skip;

	increasing = (gear < data->cpu_gear);
	data->cpu_gear = gear;

	if (increasing) {
		if (gear <= ARRAY_SIZE(data->pm_qos_int))
			pm_qos_update_request(&abox_pm_qos_int,
					data->pm_qos_int[gear - 1]);
		else
			pm_qos_update_request(&abox_pm_qos_int, 0);
	}

	if (gear >= ABOX_CPU_GEAR_MIN) {
		ret = clk_set_rate(data->clk_pll, 0);
		if (ret < 0)
			dev_warn(dev, "setting pll clock to 0 is failed: %d\n",
					ret);
		dev_info(dev, "pll clock: %lu\n", clk_get_rate(data->clk_pll));

		ret = clk_set_rate(data->clk_cpu, AUD_PLL_RATE_KHZ);
		if (ret < 0)
			dev_warn(dev, "setting cpu clock gear to %d is failed: %d\n",
					gear, ret);
	} else {
		ret = clk_set_rate(data->clk_cpu, AUD_PLL_RATE_KHZ / gear);
		if (ret < 0)
			dev_warn(dev, "setting cpu clock gear to %d is failed: %d\n",
					gear, ret);

		if (clk_get_rate(data->clk_pll) <= AUD_PLL_RATE_HZ_BYPASS) {
			ret = clk_set_rate(data->clk_pll,
					AUD_PLL_RATE_HZ_FOR_48000);
			if (ret < 0)
				dev_warn(dev, "setting pll clock to 0 is failed: %d\n",
						ret);
			dev_info(dev, "pll clock: %lu\n",
					clk_get_rate(data->clk_pll));
		}
	}
	dev_info(dev, "cpu clock: %lukHz\n", clk_get_rate(data->clk_cpu));

	if (!increasing) {
		if (gear <= ARRAY_SIZE(data->pm_qos_int))
			pm_qos_update_request(&abox_pm_qos_int,
					data->pm_qos_int[gear - 1]);
		else
			pm_qos_update_request(&abox_pm_qos_int, 0);
	}
skip:
	abox_notify_cpu_gear(data, clk_get_rate(data->clk_cpu) * 1000, 0);
}

static void abox_change_cpu_gear(struct device *dev, struct abox_data *data)
{
	struct abox_qos_request *request;
	unsigned int gear = UINT_MAX;
	unsigned int clksrc_to_aclk = 0;
	s32 freq;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->cpu_gear_requests;
			request - data->cpu_gear_requests <
			ARRAY_SIZE(data->cpu_gear_requests)
			&& request->id;
			request++) {
		if (gear > request->value)
			gear = request->value;

		dev_dbg(dev, "id=%x, value=%u, gear=%u\n", request->id,
				request->value, gear);
	}

	if ((data->cpu_gear >= ABOX_CPU_GEAR_MIN) &&
			(gear < ABOX_CPU_GEAR_MIN)) {
		/* first cpu gear request */
		clk_set_rate(data->clk_pll, AUD_PLL_RATE_HZ_FOR_48000);
		dev_info(dev, "pll clock: %lu\n", clk_get_rate(data->clk_pll));
		clksrc_to_aclk = 1;
		pm_runtime_get(dev);
	}

	if (data->cpu_gear != gear) {
		freq = (gear <= ARRAY_SIZE(data->pm_qos_aud)) ?
				data->pm_qos_aud[gear - 1] : 0;
		pm_qos_update_request(&abox_pm_qos_aud, freq);
	}

	dev_info(dev, "pm qos request aud: req=%dkHz ret=%dkHz\n", freq,
			pm_qos_request(abox_pm_qos_aud.pm_qos_class));
	abox_notify_cpu_gear(data,
			pm_qos_request(abox_pm_qos_aud.pm_qos_class) * 1000,
			clksrc_to_aclk);

	if ((data->cpu_gear < ABOX_CPU_GEAR_MIN) &&
			(gear >= ABOX_CPU_GEAR_MIN)) {
		/* no more cpu gear request */
		pm_runtime_mark_last_busy(dev);
		pm_runtime_put_autosuspend(dev);
		clk_set_rate(data->clk_pll, 0);
		dev_info(dev, "pll clock: %lu\n", clk_get_rate(data->clk_pll));
	}

	data->cpu_gear = gear;
}

static void abox_change_cpu_gear_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			change_cpu_gear_work);

	if (IS_ENABLED(CONFIG_SOC_EXYNOS8895))
		abox_change_cpu_gear_legacy(&data->pdev->dev, data);
	else
		abox_change_cpu_gear(&data->pdev->dev, data);
}

int abox_request_cpu_gear(struct device *dev, struct abox_data *data,
		unsigned int id, unsigned int gear)
{
	struct abox_qos_request *request;
	unsigned int old_id, old_gear;
	size_t len = ARRAY_SIZE(data->cpu_gear_requests);

	dev_info(dev, "%s(%x, %u)\n", __func__, id, gear);

	for (request = data->cpu_gear_requests;
			request - data->cpu_gear_requests < len
			&& request->id && request->id != id;
			request++) {
	}

	old_id = request->id;
	old_gear = request->value;
	request->value = gear;
	wmb(); /* value is read after id in reading function */
	request->id = id;

	if (request - data->cpu_gear_requests >=
			ARRAY_SIZE(data->cpu_gear_requests)) {
		dev_err(dev, "%s: out of index. id=%x, gear=%u\n", __func__,
				id, gear);
		return -ENOMEM;
	}

	queue_work(data->gear_workqueue, &data->change_cpu_gear_work);
	abox_check_cpu_gear(dev, data, old_id, old_gear, id, gear);

	return 0;
}

void abox_cpu_gear_barrier(struct abox_data *data)
{
	flush_work(&data->change_cpu_gear_work);
}

int abox_request_cpu_gear_sync(struct device *dev, struct abox_data *data,
		unsigned int id, unsigned int gear)
{
	int ret = abox_request_cpu_gear(dev, data, id, gear);

	abox_cpu_gear_barrier(data);
	return ret;
}

void abox_clear_cpu_gear_requests(struct device *dev, struct abox_data *data)
{
	struct abox_qos_request *req;
	size_t len = ARRAY_SIZE(data->cpu_gear_requests);

	dev_info(dev, "%s\n", __func__);

	for (req = data->cpu_gear_requests; req - data->cpu_gear_requests < len
			&& req->id; req++) {
		if (req->value < ABOX_CPU_GEAR_MIN)
			abox_request_cpu_gear(dev, data, req->id,
					ABOX_CPU_GEAR_MIN);
	}
}

static void abox_change_int_freq_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			change_int_freq_work);
	struct device *dev = &data->pdev->dev;
	struct abox_qos_request *request;
	unsigned int freq = 0;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->int_requests; request - data->int_requests <
			ARRAY_SIZE(data->int_requests) && request->id;
			request++) {
		if (freq < request->value)
			freq = request->value;

		dev_dbg(dev, "id=%x, value=%u, freq=%u\n", request->id,
				request->value, freq);
	}

	data->int_freq = freq;
	pm_qos_update_request(&abox_pm_qos_int, data->int_freq);

	dev_info(dev, "pm qos request int: %dHz\n", pm_qos_request(
			abox_pm_qos_int.pm_qos_class));
}

int abox_request_int_freq(struct device *dev, struct abox_data *data,
		unsigned int id, unsigned int int_freq)
{
	struct abox_qos_request *request;

	dev_info(dev, "%s(%x, %u)\n", __func__, id, int_freq);

	if (!id)
		id = DEFAULT_INT_FREQ_ID;

	for (request = data->int_requests; request - data->int_requests <
			ARRAY_SIZE(data->int_requests) && request->id &&
			request->id != id; request++) {
	}

	request->value = int_freq;
	wmb(); /* value is read after id in reading function */
	request->id = id;

	if (request - data->int_requests >= ARRAY_SIZE(data->int_requests)) {
		dev_err(dev, "%s: out of index. id=%x, int_freq=%u\n", __func__,
				id, int_freq);
		return -ENOMEM;
	}

	schedule_work(&data->change_int_freq_work);

	return 0;
}

static void abox_change_mif_freq_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			change_mif_freq_work);
	struct device *dev = &data->pdev->dev;
	struct abox_qos_request *request;
	unsigned int freq = 0;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->mif_requests; request - data->mif_requests <
			ARRAY_SIZE(data->mif_requests) && request->id;
			request++) {
		if (freq < request->value)
			freq = request->value;

		dev_dbg(dev, "id=%x, value=%u, freq=%u\n", request->id,
				request->value, freq);
	}

	data->mif_freq = freq;
	pm_qos_update_request(&abox_pm_qos_mif, data->mif_freq);

	dev_info(dev, "pm qos request mif: %dHz\n", pm_qos_request(
			abox_pm_qos_mif.pm_qos_class));
}

static int abox_request_mif_freq(struct device *dev, struct abox_data *data,
		unsigned int id, unsigned int mif_freq)
{
	struct abox_qos_request *request;

	dev_info(dev, "%s(%x, %u)\n", __func__, id, mif_freq);

	if (!id)
		id = DEFAULT_MIF_FREQ_ID;

	for (request = data->mif_requests; request - data->mif_requests <
			ARRAY_SIZE(data->mif_requests) && request->id &&
			request->id != id; request++) {
	}

	request->value = mif_freq;
	wmb(); /* value is read after id in reading function */
	request->id = id;

	if (request - data->mif_requests >= ARRAY_SIZE(data->mif_requests)) {
		dev_err(dev, "%s: out of index. id=%x, mif_freq=%u\n", __func__,
				id, mif_freq);
		return -ENOMEM;
	}

	schedule_work(&data->change_mif_freq_work);

	return 0;
}

static void abox_change_lit_freq_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			change_lit_freq_work);
	struct device *dev = &data->pdev->dev;
	size_t array_size = ARRAY_SIZE(data->lit_requests);
	struct abox_qos_request *request;
	unsigned int freq = 0;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->lit_requests;
			request - data->lit_requests < array_size &&
			request->id; request++) {
		if (freq < request->value)
			freq = request->value;

		dev_dbg(dev, "id=%x, value=%u, freq=%u\n", request->id,
				request->value, freq);
	}

	data->lit_freq = freq;
	pm_qos_update_request(&abox_pm_qos_lit, data->lit_freq);

	dev_info(dev, "pm qos request little: %dkHz\n",
			pm_qos_request(abox_pm_qos_lit.pm_qos_class));
}

int abox_request_lit_freq(struct device *dev, struct abox_data *data,
		unsigned int id, unsigned int freq)
{
	size_t array_size = ARRAY_SIZE(data->lit_requests);
	struct abox_qos_request *request;

	if (!id)
		id = DEFAULT_LIT_FREQ_ID;

	for (request = data->lit_requests;
			request - data->lit_requests < array_size &&
			request->id && request->id != id; request++) {
	}

	if ((request->id == id) && (request->value == freq))
		return 0;

	request->value = freq;
	wmb(); /* value is read after id in reading function */
	request->id = id;

	dev_info(dev, "%s(%x, %u)\n", __func__, id, freq);

	if (request - data->lit_requests >= ARRAY_SIZE(data->lit_requests)) {
		dev_err(dev, "%s: out of index. id=%x, freq=%u\n",
				__func__, id, freq);
		return -ENOMEM;
	}

	schedule_work(&data->change_lit_freq_work);

	return 0;
}

static void abox_change_big_freq_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			change_big_freq_work);
	struct device *dev = &data->pdev->dev;
	size_t array_size = ARRAY_SIZE(data->big_requests);
	struct abox_qos_request *request;
	unsigned int freq = 0;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->big_requests;
			request - data->big_requests < array_size &&
			request->id; request++) {
		if (freq < request->value)
			freq = request->value;

		dev_dbg(dev, "id=%x, value=%u, freq=%u\n", request->id,
				request->value, freq);
	}

	data->big_freq = freq;
	pm_qos_update_request(&abox_pm_qos_big, data->big_freq);

	dev_info(dev, "pm qos request big: %dkHz\n",
			pm_qos_request(abox_pm_qos_big.pm_qos_class));
}

int abox_request_big_freq(struct device *dev, struct abox_data *data,
		unsigned int id, unsigned int freq)
{
	size_t array_size = ARRAY_SIZE(data->big_requests);
	struct abox_qos_request *request;

	if (!id)
		id = DEFAULT_BIG_FREQ_ID;

	for (request = data->big_requests;
			request - data->big_requests < array_size &&
			request->id && request->id != id; request++) {
	}

	if ((request->id == id) && (request->value == freq))
		return 0;

	dev_info(dev, "%s(%x, %u)\n", __func__, id, freq);

	request->value = freq;
	wmb(); /* value is read after id in reading function */
	request->id = id;

	if (request - data->big_requests >= ARRAY_SIZE(data->big_requests)) {
		dev_err(dev, "%s: out of index. id=%x, freq=%u\n",
				__func__, id, freq);
		return -ENOMEM;
	}

	schedule_work(&data->change_big_freq_work);

	return 0;
}

static void abox_change_hmp_boost_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			change_hmp_boost_work);
	struct device *dev = &data->pdev->dev;
	size_t array_size = ARRAY_SIZE(data->hmp_requests);
	struct abox_qos_request *request;
	unsigned int on = 0;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->hmp_requests;
			request - data->hmp_requests < array_size &&
			request->id; request++) {
		if (request->value)
			on = request->value;

		dev_dbg(dev, "id=%x, value=%u, on=%u\n", request->id,
				request->value, on);
	}

	if (data->hmp_boost != on) {
		dev_info(dev, "request hmp boost: %d\n", on);

		data->hmp_boost = on;
#ifdef CONFIG_SCHED_HMP
		set_hmp_boost(on);
#endif
	}
}

int abox_request_hmp_boost(struct device *dev, struct abox_data *data,
		unsigned int id, unsigned int on)
{
	size_t array_size = ARRAY_SIZE(data->hmp_requests);
	struct abox_qos_request *request;

	if (!id)
		id = DEFAULT_HMP_BOOST_ID;

	for (request = data->hmp_requests;
			request - data->hmp_requests < array_size &&
			request->id && request->id != id; request++) {
	}

	if ((request->id == id) && (request->value == on))
		return 0;

	dev_info(dev, "%s(%x, %u)\n", __func__, id, on);

	request->value = on;
	wmb(); /* value is read after id in reading function */
	request->id = id;

	if (request - data->hmp_requests >= ARRAY_SIZE(data->hmp_requests)) {
		dev_err(dev, "%s: out of index. id=%x, on=%u\n",
				__func__, id, on);
		return -ENOMEM;
	}

	schedule_work(&data->change_hmp_boost_work);

	return 0;
}

void abox_request_dram_on(struct platform_device *pdev_abox, void *id, bool on)
{
	struct device *dev = &pdev_abox->dev;
	struct abox_data *data = platform_get_drvdata(pdev_abox);
	struct abox_dram_request *request;
	unsigned int val = 0x0;

	dev_dbg(dev, "%s(%d)\n", __func__, on);

	for (request = data->dram_requests; request - data->dram_requests <
			ARRAY_SIZE(data->dram_requests) && request->id &&
			request->id != id; request++) {
	}

	request->on = on;
	wmb(); /* on is read after id in reading function */
	request->id = id;

	for (request = data->dram_requests; request - data->dram_requests <
			ARRAY_SIZE(data->dram_requests) && request->id;
			request++) {
		if (request->on) {
			val = ABOX_SYSPOWER_CTRL_MASK;
			break;
		}
	}

	regmap_write(data->regmap, ABOX_SYSPOWER_CTRL, val);
	dev_dbg(dev, "%s: SYSPOWER_CTRL=%08x\n", __func__,
			({regmap_read(data->regmap, ABOX_SYSPOWER_CTRL, &val);
			val; }));
}

int abox_iommu_map(struct device *dev, unsigned long iova,
		phys_addr_t addr, size_t bytes, void *area)
{
	struct abox_data *data = dev_get_drvdata(dev);
	struct abox_iommu_mapping *mapping;
	int ret;

	dev_info(dev, "%s(%#lx, %pa, %#zx, %p)\n", __func__,
			iova, &addr, bytes, area);

	mutex_lock(&data->iommu_lock);
	ret = iommu_map(data->iommu_domain, iova, addr, bytes, 0);
	if (ret < 0) {
		dev_err(dev, "Failed to iommu_map: %d\n", ret);
		return ret;
	}

	mapping = devm_kmalloc(dev, sizeof(*mapping), GFP_KERNEL);
	if (!mapping)
		return -ENOMEM;

	if (!area) {
		dev_warn(dev, "%s: no virtual address\n", __func__);
		area = phys_to_virt(addr);
	}

	mapping->iova = iova;
	mapping->addr = addr;
	mapping->area = area;
	mapping->bytes = bytes;
	list_add_rcu(&mapping->list, &data->iommu_maps);
	mutex_unlock(&data->iommu_lock);

	return 0;
}
EXPORT_SYMBOL(abox_iommu_map);

int abox_iommu_unmap(struct device *dev, unsigned long iova)
{
	struct abox_data *data = dev_get_drvdata(dev);
	struct abox_iommu_mapping *mapping;
	size_t bytes;
	int ret = 0;

	dev_info(dev, "%s(%#lx)\n", __func__, iova);

	mutex_lock(&data->iommu_lock);
	list_for_each_entry(mapping, &data->iommu_maps, list) {
		if (iova != mapping->iova)
			continue;

		bytes = mapping->bytes;
		ret = iommu_unmap(data->iommu_domain, iova, bytes);
		if (ret < 0)
			dev_err(dev, "Failed to iommu_unmap: %d\n", ret);

		exynos_sysmmu_tlb_invalidate(data->iommu_domain, iova, bytes);
		list_del_rcu(&mapping->list);
		synchronize_rcu();
		devm_kfree(dev, mapping);
		break;
	}
	mutex_unlock(&data->iommu_lock);

	return ret;
}
EXPORT_SYMBOL(abox_iommu_unmap);

int abox_iommu_map_sg(struct device *dev, unsigned long iova,
		struct scatterlist *sg, unsigned int nents,
		int prot, size_t bytes, void *area)
{
	struct abox_data *data = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "%s(%#lx)\n", __func__, iova);

	ret = iommu_map_sg(data->iommu_domain, iova, sg, nents, prot);
	if (ret < 0) {
		dev_err(dev, "Failed to iommu_map_sg(%#lx): %d\n", iova, ret);
		return ret;
	}

	return 0;
}
EXPORT_SYMBOL(abox_iommu_map_sg);

int abox_register_irq_handler(struct device *dev, int ipc_id,
		abox_irq_handler_t irq_handler, void *dev_id)
{
	struct abox_data *data = dev_get_drvdata(dev);
	struct abox_irq_action *irq_action = NULL;
	bool new_handler = true;

	if (ipc_id >= IPC_ID_COUNT)
		return -EINVAL;

	list_for_each_entry(irq_action, &data->irq_actions, list) {
		if (irq_action->irq == ipc_id && irq_action->dev_id == dev_id) {
			new_handler = false;
			break;
		}
	}

	if (new_handler) {
		irq_action = devm_kzalloc(dev, sizeof(struct abox_irq_action),
				GFP_KERNEL);
		if (IS_ERR_OR_NULL(irq_action)) {
			dev_err(dev, "%s: kmalloc fail\n", __func__);
			return -ENOMEM;
		}
		irq_action->irq = ipc_id;
		irq_action->dev_id = dev_id;
		list_add_tail(&irq_action->list, &data->irq_actions);
	}

	irq_action->irq_handler = irq_handler;

	return 0;
}
EXPORT_SYMBOL(abox_register_irq_handler);

static int abox_control_asrc(struct snd_soc_dapm_widget *w, bool on)
{
	struct snd_kcontrol *kcontrol;
	struct snd_soc_component *cmpnt;
	struct soc_enum *e;
	unsigned int reg, mask, val;

	if (!w || !w->name || !w->num_kcontrols)
		return -EINVAL;

	kcontrol = w->kcontrols[0];
	cmpnt = w->dapm->component;
	e = (struct soc_enum *)kcontrol->private_value;
	reg = e->reg;
	mask = e->mask << e->shift_l;
	val = (on ? 1 : 0) << e->shift_l;

	return snd_soc_component_update_bits(cmpnt, reg, mask, val);
}

static bool abox_is_asrc_widget(struct snd_soc_dapm_widget *w)
{
	return w->name && !!strstr(w->name, "ASRC");
}

int abox_try_to_asrc_off(struct device *dev, struct abox_data *data,
		struct snd_soc_pcm_runtime *fe, int stream)
{
	struct snd_soc_dai *dai = fe->cpu_dai;
	struct snd_soc_component *cmpnt = dai->component;
	struct snd_soc_dapm_widget *w, *w_asrc = NULL;
	LIST_HEAD(widget_list);
	enum ABOX_CONFIGMSG rate, format;
	unsigned int out_rate = 0, out_width = 0;
	unsigned int pcm_rate, pcm_width;
	bool slave;

	if (!abox_test_quirk(data, ABOX_QUIRK_TRY_TO_ASRC_OFF))
		return 0;

	dev_dbg(dev, "%s(%s)\n", __func__, dai->name);

	pcm_rate = params_rate(&fe->dpcm[stream].hw_params);
	pcm_width = params_width(&fe->dpcm[stream].hw_params);

	snd_soc_dapm_mutex_lock(snd_soc_component_get_dapm(cmpnt));
	/*
	 * For snd_soc_dapm_connected_{output,input}_ep fully discover the graph
	 * we need to reset the cached number of inputs and outputs.
	 */
	list_for_each_entry(w, &cmpnt->card->widgets, list) {
		w->endpoints[SND_SOC_DAPM_DIR_IN] = -1;
		w->endpoints[SND_SOC_DAPM_DIR_OUT] = -1;
	}
	if (dai->playback_widget)
		snd_soc_dapm_connected_output_ep(dai->playback_widget,
				&widget_list);
	if (dai->capture_widget)
		snd_soc_dapm_connected_input_ep(dai->capture_widget,
				&widget_list);

	list_for_each_entry(w, &widget_list, work_list) {
		dev_dbg(dev, "%s", w->name);

		if (abox_find_nrf_stream(w, stream, &rate, &format, &slave)) {
			out_rate = abox_get_sif_rate(data, rate);
			out_width = abox_get_sif_width(data, format);
			dev_dbg(dev, "%s: rate=%u, width=%u\n",
					w->name, out_rate, out_width);
		}

		if (abox_is_asrc_widget(w)) {
			w_asrc = w;
			dev_dbg(dev, "%s is asrc\n", w->name);
		}

		if (w_asrc && out_rate && out_width)
			break;
	}
	snd_soc_dapm_mutex_unlock(snd_soc_component_get_dapm(cmpnt));

	if (!w_asrc || !out_rate || !out_width) {
		dev_warn(dev, "%s: incomplete path: w_asrc=%s, out_rate=%u, out_width=%u",
				__func__, w_asrc ? w_asrc->name : "(null)",
				out_rate, out_width);
		return -EINVAL;
	}

	return abox_control_asrc(w_asrc, (pcm_rate != out_rate) ||
			(pcm_width != out_width));
}

static int abox_register_if_routes(struct device *dev,
		const struct snd_soc_dapm_route *route_base, int num,
		struct snd_soc_dapm_context *dapm, const char *name)
{
	struct snd_soc_dapm_route *route;
	int i;

	route = devm_kmemdup(dev, route_base, sizeof(*route_base) * num,
			GFP_KERNEL);
	if (!route) {
		dev_err(dev, "%s: insufficient memory\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < num; i++) {
		if (route[i].sink)
			route[i].sink = devm_kasprintf(dev, GFP_KERNEL,
					route[i].sink, name);
		if (route[i].control)
			route[i].control = devm_kasprintf(dev, GFP_KERNEL,
					route[i].control, name);
		if (route[i].source)
			route[i].source = devm_kasprintf(dev, GFP_KERNEL,
					route[i].source, name);
	}

	snd_soc_dapm_add_routes(dapm, route, num);
	devm_kfree(dev, route);

	return 0;
}

int abox_register_if(struct platform_device *pdev_abox,
		struct platform_device *pdev_if, unsigned int id,
		struct snd_soc_dapm_context *dapm, const char *name,
		bool playback, bool capture)
{
	struct device *dev = &pdev_if->dev;
	struct abox_data *data = platform_get_drvdata(pdev_abox);
	int ret;

	static const struct snd_soc_dapm_route route_base_pla[] = {
		/* sink, control, source */
		{"%s Playback", NULL, "%s SPK"},
	};

	static const struct snd_soc_dapm_route route_base_cap[] = {
		/* sink, control, source */
		{"SPUSM", "%s", "%s Capture"},
		{"NSRC0", "%s", "%s Capture"},
		{"NSRC1", "%s", "%s Capture"},
		{"NSRC2", "%s", "%s Capture"},
		{"NSRC3", "%s", "%s Capture"},
	};

	if (id >= ARRAY_SIZE(data->pdev_if)) {
		dev_err(dev, "%s: invalid id(%u)\n", __func__, id);
		return -EINVAL;
	}

	if (data->cmpnt->name_prefix && dapm->component->name_prefix &&
			strcmp(data->cmpnt->name_prefix,
			dapm->component->name_prefix)) {
		dev_err(dev, "%s: name prefix is different: %s != %s\n",
				__func__, data->cmpnt->name_prefix,
				dapm->component->name_prefix);
		return -EINVAL;
	}

	data->pdev_if[id] = pdev_if;
	if (id > data->if_count)
		data->if_count = id + 1;

	if (playback) {
		ret = abox_register_if_routes(dev, route_base_pla,
				ARRAY_SIZE(route_base_pla), dapm, name);
		if (ret < 0)
			return ret;
	}

	if (capture) {
		ret = abox_register_if_routes(dev, route_base_cap,
				ARRAY_SIZE(route_base_cap), dapm, name);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int abox_register_rdma(struct platform_device *pdev_abox,
		struct platform_device *pdev_rdma, unsigned int id)
{
	struct abox_data *data = platform_get_drvdata(pdev_abox);

	if (id < ARRAY_SIZE(data->pdev_rdma)) {
		data->pdev_rdma[id] = pdev_rdma;
		if (id > data->rdma_count)
			data->rdma_count = id + 1;
	} else {
		dev_err(&data->pdev->dev, "%s: invalid id(%u)\n", __func__, id);
		return -EINVAL;
	}

	return 0;
}

int abox_register_wdma(struct platform_device *pdev_abox,
		struct platform_device *pdev_wdma, unsigned int id)
{
	struct abox_data *data = platform_get_drvdata(pdev_abox);

	if (id < ARRAY_SIZE(data->pdev_wdma)) {
		data->pdev_wdma[id] = pdev_wdma;
		if (id > data->wdma_count)
			data->wdma_count = id + 1;
	} else {
		dev_err(&data->pdev->dev, "%s: invalid id(%u)\n", __func__, id);
		return -EINVAL;
	}

	return 0;
}

static int abox_component_control_info(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_info *uinfo)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_component_kcontrol_value *value =
			(void *)kcontrol->private_value;

	dev_dbg(dev, "%s(%s)\n", __func__, kcontrol->id.name);

	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = value->control->count;
	uinfo->value.integer.min = value->control->min;
	uinfo->value.integer.max = value->control->max;
	return 0;
}

static ABOX_IPC_MSG abox_component_control_get_msg;

static int abox_component_control_get(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_data *data = dev_get_drvdata(dev);
	struct abox_component_kcontrol_value *value =
			(void *)kcontrol->private_value;
	ABOX_IPC_MSG *msg = &abox_component_control_get_msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg->msg.system;
	int i, ret;

	dev_dbg(dev, "%s\n", __func__);

	if (value->cache_only) {
		for (i = 0; i < value->control->count; i++)
			ucontrol->value.integer.value[i] = value->cache[i];
		return 0;
	}

	msg->ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_REQUEST_COMPONENT_CONTROL;
	system_msg->param1 = value->desc->id;
	system_msg->param2 = value->control->id;
	ret = abox_request_ipc(dev, msg->ipcid, msg, sizeof(*msg), 0, 1);
	if (ret < 0)
		return ret;

	ret = wait_event_timeout(data->ipc_wait_queue,
			system_msg->msgtype == ABOX_REPORT_COMPONENT_CONTROL,
			msecs_to_jiffies(1000));
	if (system_msg->msgtype != ABOX_REPORT_COMPONENT_CONTROL)
		return -ETIME;

	for (i = 0; i < value->control->count; i++) {
		long val = (long)system_msg->bundle.param_s32[i];

		ucontrol->value.integer.value[i] = val;
	}

	return 0;
}

static int abox_component_control_put_ipc(struct device *dev,
		struct abox_component_kcontrol_value *value)
{
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	int i;

	dev_dbg(dev, "%s\n", __func__);

	for (i = 0; i < value->control->count; i++) {
		int val = value->cache[i];
		char *name = value->control->name;

		system_msg->bundle.param_s32[i] = val;
		dev_dbg(dev, "%s: %s[%d] <= %d", __func__, name, i, val);
	}

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_UPDATE_COMPONENT_CONTROL;
	system_msg->param1 = value->desc->id;
	system_msg->param2 = value->control->id;

	return abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
}

static int abox_component_control_put(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *cmpnt = snd_soc_kcontrol_component(kcontrol);
	struct device *dev = cmpnt->dev;
	struct abox_component_kcontrol_value *value =
			(void *)kcontrol->private_value;
	int i;

	dev_dbg(dev, "%s\n", __func__);

	for (i = 0; i < value->control->count; i++) {
		int val = (int)ucontrol->value.integer.value[i];
		char *name = kcontrol->id.name;

		value->cache[i] = val;
		dev_dbg(dev, "%s: %s[%d] <= %d", __func__, name, i, val);
	}

	return abox_component_control_put_ipc(dev, value);
}

#define ABOX_COMPONENT_KCONTROL(xname, xdesc, xcontrol)	\
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = (xname), \
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE, \
	.info = abox_component_control_info, \
	.get = abox_component_control_get, \
	.put = abox_component_control_put, \
	.private_value = \
		(unsigned long)&(struct abox_component_kcontrol_value) \
		{.desc = xdesc, .control = xcontrol} }

struct snd_kcontrol_new abox_component_kcontrols[] = {
	ABOX_COMPONENT_KCONTROL(NULL, NULL, NULL),
};

static void abox_register_component_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			register_component_work);
	struct device *dev = &data->pdev->dev;
	struct abox_component *component;
	int i;

	dev_dbg(dev, "%s\n", __func__);

	for (component = data->components; ((component - data->components) <
			ARRAY_SIZE(data->components)); component++) {
		struct ABOX_COMPONENT_DESCRIPTIOR *desc = component->desc;

		if (!component->desc || component->registered)
			continue;

		for (i = 0; i < desc->control_count; i++) {
			struct ABOX_COMPONENT_CONTROL *control =
					&desc->controls[i];
			struct abox_component_kcontrol_value *value;
			char kcontrol_name[64];

			value = devm_kzalloc(dev, sizeof(*value) +
					(control->count *
					sizeof(value->cache[0])), GFP_KERNEL);
			if (IS_ERR_OR_NULL(value)) {
				dev_err(dev, "%s: kmalloc fail\n", __func__);
				continue;
			}
			value->desc = desc;
			value->control = control;
			list_add_tail(&value->list, &component->value_list);

			snprintf(kcontrol_name, sizeof(kcontrol_name), "%s %s",
					desc->name, control->name);

			abox_component_kcontrols[0].name = devm_kstrdup(dev,
					kcontrol_name, GFP_KERNEL);
			abox_component_kcontrols[0].private_value =
					(unsigned long)value;
			if (data->cmpnt) {
				snd_soc_add_component_controls(data->cmpnt,
						abox_component_kcontrols, 1);
			}
		}
		component->registered = true;
	}
}


static int abox_register_component(struct device *dev,
		struct ABOX_COMPONENT_DESCRIPTIOR *desc)
{
	struct abox_data *data = dev_get_drvdata(dev);
	struct abox_component *component;

	dev_dbg(dev, "%s(%d, %s)\n", __func__, desc->id, desc->name);

	for (component = data->components;
			((component - data->components) <
			ARRAY_SIZE(data->components)) &&
			component->desc && component->desc != desc;
			component++) {
	}

	if (!component->desc) {
		component->desc = desc;
		INIT_LIST_HEAD(&component->value_list);
		schedule_work(&data->register_component_work);
	}

	return 0;
}

static void abox_restore_component_control(struct device *dev,
		struct abox_component_kcontrol_value *value)
{
	int count = value->control->count;
	int *val;

	for (val = value->cache; val - value->cache < count; val++) {
		if (*val) {
			abox_component_control_put_ipc(dev, value);
			break;
		}
	}
	value->cache_only = false;
}

static void abox_restore_components(struct device *dev, struct abox_data *data)
{
	struct abox_component *component;
	struct abox_component_kcontrol_value *value;
	size_t len = ARRAY_SIZE(data->components);

	dev_dbg(dev, "%s\n", __func__);
	for (component = data->components; component - data->components < len;
			component++) {
		if (!component->registered)
			break;
		list_for_each_entry(value, &component->value_list, list) {
			abox_restore_component_control(dev, value);
		}
	}
}

static void abox_cache_components(struct device *dev, struct abox_data *data)
{
	struct abox_component *component;
	struct abox_component_kcontrol_value *value;
	size_t len = ARRAY_SIZE(data->components);

	dev_dbg(dev, "%s\n", __func__);

	for (component = data->components; component - data->components < len;
			component++) {
		if (!component->registered)
			break;
		list_for_each_entry(value, &component->value_list, list) {
			value->cache_only = true;
		}
	}
}

static bool abox_is_calliope_incompatible(struct device *dev)
{
	struct abox_data *data = dev_get_drvdata(dev);
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;

	memcpy(&msg, data->sram_base + 0x30040, 0x3C);

	return ((system_msg->param3 >> 24) == 'A');
}

static void abox_restore_data(struct device *dev)
{
	struct abox_data *data = dev_get_drvdata(dev);
	int i;

	dev_info(dev, "%s\n", __func__);

	for (i = SET_MIXER_SAMPLE_RATE; i <= SET_INMUX4_SAMPLE_RATE; i++)
		abox_sample_rate_put_ipc(dev,
				data->sif_rate[abox_sif_idx(i)], i);
	for (i = SET_MIXER_FORMAT; i <= SET_INMUX4_FORMAT; i++)
		abox_sif_format_put_ipc(dev,
				data->sif_format[abox_sif_idx(i)],
				data->sif_channels[abox_sif_idx(i)], i);
	if (data->erap_status[ERAP_ECHO_CANCEL])
		abox_erap_handler_put_ipc(dev, ERAP_ECHO_CANCEL,
				data->erap_status[ERAP_ECHO_CANCEL]);
	if (data->erap_status[ERAP_VI_SENSE])
		abox_erap_handler_put_ipc(dev, ERAP_VI_SENSE,
				data->erap_status[ERAP_VI_SENSE]);
	if (data->audio_mode)
		abox_audio_mode_put_ipc(dev, data->audio_mode);
	if (data->sound_type)
		abox_sound_type_put_ipc(dev, data->sound_type);
	abox_restore_components(dev, data);
	abox_effect_restore();
}

static void abox_boot_done_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data,
			boot_done_work);
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;

	dev_dbg(dev, "%s\n", __func__);

	abox_cpu_pm_ipc(data, true);
	abox_restore_data(dev);
	abox_request_cpu_gear(dev, data, DEFAULT_CPU_GEAR_ID,
			ABOX_CPU_GEAR_MIN);

	__pm_relax(&data->ws_boot);
	dev_info(dev, "%s:release wake lock\n", __func__);
}

static void abox_boot_done(struct device *dev, unsigned int version)
{
	struct abox_data *data = dev_get_drvdata(dev);
	char ver_char[4];

	dev_dbg(dev, "%s\n", __func__);

	data->calliope_version = version;
	memcpy(ver_char, &version, sizeof(ver_char));
	dev_info(dev, "Calliope is ready to sing (version:%c%c%c%c)\n",
			ver_char[3], ver_char[2], ver_char[1], ver_char[0]);
	data->calliope_state = CALLIOPE_ENABLED;
	schedule_work(&data->boot_done_work);

	wake_up(&data->ipc_wait_queue);
}

static irqreturn_t abox_dma_irq_handler(int irq, struct abox_data *data)
{
	struct device *dev = &data->pdev->dev;
	int id;
	struct platform_device **pdev_dma;
	struct abox_platform_data *platform_data;

	dev_dbg(dev, "%s(%d)\n", __func__, irq);

	switch (irq) {
	case RDMA0_BUF_EMPTY:
		id = 0;
		pdev_dma = data->pdev_rdma;
		break;
	case RDMA1_BUF_EMPTY:
		id = 1;
		pdev_dma = data->pdev_rdma;
		break;
	case RDMA2_BUF_EMPTY:
		id = 2;
		pdev_dma = data->pdev_rdma;
		break;
	case RDMA3_BUF_EMPTY:
		id = 3;
		pdev_dma = data->pdev_rdma;
		break;
	case WDMA0_BUF_FULL:
		id = 0;
		pdev_dma = data->pdev_wdma;
		break;
	case WDMA1_BUF_FULL:
		id = 1;
		pdev_dma = data->pdev_wdma;
		break;
	default:
		return IRQ_NONE;
	}

	if (unlikely(!pdev_dma[id])) {
		dev_err(dev, "spurious dma irq: irq=%d id=%d\n", irq, id);
		return IRQ_HANDLED;
	}

	platform_data = platform_get_drvdata(pdev_dma[id]);
	if (unlikely(!platform_data)) {
		dev_err(dev, "dma irq with null data: irq=%d id=%d\n", irq, id);
		return IRQ_HANDLED;
	}

	platform_data->pointer = 0;
	snd_pcm_period_elapsed(platform_data->substream);

	return IRQ_HANDLED;
}

static irqreturn_t abox_registered_ipc_handler(struct device *dev, int irq,
		struct abox_data *data, ABOX_IPC_MSG *msg, bool broadcast)
{
	struct abox_irq_action *action;
	irqreturn_t ret = IRQ_NONE;

	dev_dbg(dev, "%s: irq=%d\n", __func__, irq);

	list_for_each_entry(action, &data->irq_actions, list) {
		if (action->irq != irq)
			continue;

		ret = action->irq_handler(irq, action->dev_id, msg);
		if (!broadcast && ret == IRQ_HANDLED)
			break;
	}

	return ret;
}

static void abox_system_ipc_handler(struct device *dev,
		struct abox_data *data, ABOX_IPC_MSG *msg)
{
	struct IPC_SYSTEM_MSG *system_msg = &msg->msg.system;
	int ret;

	dev_dbg(dev, "msgtype=%d\n", system_msg->msgtype);

	switch (system_msg->msgtype) {
	case ABOX_BOOT_DONE:
		if (abox_is_calliope_incompatible(dev))
			dev_err(dev, "Calliope is not compatible with the driver\n");

		abox_boot_done(dev, system_msg->param3);
		abox_registered_ipc_handler(dev, IPC_SYSTEM, data, msg, true);
		break;
	case ABOX_CHANGE_GEAR:
		abox_request_cpu_gear(dev, data, system_msg->param2,
				system_msg->param1);
		break;
	case ABOX_END_L2C_CONTROL:
		data->l2c_controlled = true;
		wake_up(&data->ipc_wait_queue);
		break;
	case ABOX_REQUEST_L2C:
	{
		void *id = (void *)(long)system_msg->param2;
		bool on = !!system_msg->param1;

		abox_request_l2c(dev, data, id, on);
		break;
	}
	case ABOX_REQUEST_SYSCLK:
		switch (system_msg->param2) {
		default:
			/* fall through */
		case 0:
			abox_request_mif_freq(dev, data, system_msg->param3,
					system_msg->param1);
			break;
		case 1:
			abox_request_int_freq(dev, data, system_msg->param3,
					system_msg->param1);
			break;
		}
		break;
	case ABOX_REPORT_LOG:
		ret = abox_log_register_buffer(dev, system_msg->param1,
				abox_addr_to_kernel_addr(data,
				system_msg->param2));
		if (ret < 0) {
			dev_err(dev, "log buffer registration failed: %u, %u\n",
					system_msg->param1, system_msg->param2);
		}
		break;
	case ABOX_FLUSH_LOG:
		break;
	case ABOX_REPORT_DUMP:
		ret = abox_dump_register_buffer(dev, system_msg->param1,
				system_msg->bundle.param_bundle,
				abox_addr_to_kernel_addr(data,
				system_msg->param2),
				abox_addr_to_phys_addr(data,
				system_msg->param2),
				system_msg->param3);
		if (ret < 0) {
			dev_err(dev, "dump buffer registration failed: %u, %u\n",
					system_msg->param1, system_msg->param2);
		}
		break;
	case ABOX_FLUSH_DUMP:
		abox_dump_period_elapsed(system_msg->param1,
				system_msg->param2);
		break;
	case ABOX_REPORT_COMPONENT:
		abox_register_component(dev,
				abox_addr_to_kernel_addr(data,
				system_msg->param1));
		break;
	case ABOX_REPORT_COMPONENT_CONTROL:
		abox_component_control_get_msg = *msg;
		wake_up(&data->ipc_wait_queue);
		break;
	case ABOX_REPORT_FAULT:
	{
		const char *type;

		switch (system_msg->param1) {
		case 1:
			type = "data abort";
			break;
		case 2:
			type = "prefetch abort";
			break;
		case 3:
			type = "os error";
			break;
		case 4:
			type = "vss error";
			break;
		case 5:
			type = "undefined exception";
			break;
		default:
			type = "unknown error";
			break;
		}
		dev_err(dev, "%s(%08X, %08X, %08X) is reported from calliope\n",
				type, system_msg->param1, system_msg->param2,
				system_msg->param3);

		switch (system_msg->param1) {
		case 1:
		case 2:
			abox_dbg_print_gpr_from_addr(dev, data,
					abox_addr_to_kernel_addr(data,
					system_msg->bundle.param_s32[0]));
			abox_dbg_dump_gpr_from_addr(dev,
					abox_addr_to_kernel_addr(data,
					system_msg->bundle.param_s32[0]),
					ABOX_DBG_DUMP_FIRMWARE, type);
			abox_dbg_dump_mem(dev, data, ABOX_DBG_DUMP_FIRMWARE,
					type);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
			abox_debug_string_update(system_msg->param1,
				abox_addr_to_kernel_addr(data, system_msg->bundle.param_s32[0]));
#endif
			break;
		case 4:
			abox_dbg_print_gpr(dev, data);
			abox_dbg_dump_gpr(dev, data, ABOX_DBG_DUMP_VSS, type);
			abox_dbg_dump_mem(dev, data, ABOX_DBG_DUMP_VSS, type);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
			abox_debug_string_update(system_msg->param1, NULL);
#endif
			break;
		default:
			abox_dbg_print_gpr(dev, data);
			abox_dbg_dump_gpr(dev, data, ABOX_DBG_DUMP_FIRMWARE,
					type);
			abox_dbg_dump_mem(dev, data, ABOX_DBG_DUMP_FIRMWARE,
					type);
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
			abox_debug_string_update(system_msg->param1, NULL);
#endif
			break;
		}
		abox_failsafe_report(dev);
		break;
	}
	default:
		dev_warn(dev, "Redundant system message: %d(%d, %d, %d)\n",
				system_msg->msgtype, system_msg->param1,
				system_msg->param2, system_msg->param3);
		break;
	}
}

static void abox_playback_ipc_handler(struct device *dev,
		struct abox_data *data, ABOX_IPC_MSG *msg)
{
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg->msg.pcmtask;
	struct abox_platform_data *platform_data;
	int id = pcmtask_msg->channel_id;

	dev_dbg(dev, "msgtype=%d\n", pcmtask_msg->msgtype);

	if ((id >= ARRAY_SIZE(data->pdev_rdma)) || !data->pdev_rdma[id]) {
		irqreturn_t ret;

		ret = abox_registered_ipc_handler(dev, IPC_PCMPLAYBACK, data,
				msg, false);
		if (ret != IRQ_HANDLED)
			dev_err(dev, "pcm playback irq: id=%d\n", id);
		return;
	}

	platform_data = platform_get_drvdata(data->pdev_rdma[id]);

	switch (pcmtask_msg->msgtype) {
	case PCM_PLTDAI_POINTER:
		platform_data->pointer = pcmtask_msg->param.pointer;
		snd_pcm_period_elapsed(platform_data->substream);
		break;
	case PCM_PLTDAI_ACK:
		platform_data->ack_enabled = !!pcmtask_msg->param.trigger;
		break;
	default:
		dev_warn(dev, "Redundant pcmtask message: %d\n",
				pcmtask_msg->msgtype);
		break;
	}
}

static void abox_capture_ipc_handler(struct device *dev,
		struct abox_data *data, ABOX_IPC_MSG *msg)
{
	struct IPC_PCMTASK_MSG *pcmtask_msg = &msg->msg.pcmtask;
	struct abox_platform_data *platform_data;
	int id = pcmtask_msg->channel_id;

	dev_dbg(dev, "msgtype=%d\n", pcmtask_msg->msgtype);

	if ((id >= ARRAY_SIZE(data->pdev_wdma)) || (!data->pdev_wdma[id])) {
		irqreturn_t ret;

		ret = abox_registered_ipc_handler(dev, IPC_PCMCAPTURE, data,
				msg, false);
		if (ret != IRQ_HANDLED)
			dev_err(dev, "pcm capture irq: id=%d\n", id);
		return;
	}

	platform_data = platform_get_drvdata(data->pdev_wdma[id]);

	switch (pcmtask_msg->msgtype) {
	case PCM_PLTDAI_POINTER:
		platform_data->pointer = pcmtask_msg->param.pointer;
		snd_pcm_period_elapsed(platform_data->substream);
		break;
	case PCM_PLTDAI_ACK:
		platform_data->ack_enabled = !!pcmtask_msg->param.trigger;
		break;
	default:
		dev_warn(dev, "Redundant pcmtask message: %d\n",
				pcmtask_msg->msgtype);
		break;
	}
}

static void abox_offload_ipc_handler(struct device *dev,
		struct abox_data *data, ABOX_IPC_MSG *msg)
{
	struct IPC_OFFLOADTASK_MSG *offloadtask_msg = &msg->msg.offload;
	int id = offloadtask_msg->channel_id;
	struct abox_platform_data *platform_data;

	if (id != 5) {
		dev_warn(dev, "%s: unknown channel id(%d)\n", __func__, id);
		id = 5;
	}
	platform_data = platform_get_drvdata(data->pdev_rdma[id]);

	if (platform_data->compr_data.isr_handler)
		platform_data->compr_data.isr_handler(data->pdev_rdma[id]);
	else
		dev_warn(dev, "Redundant offload message on rdma[%d]", id);
}

static irqreturn_t abox_irq_handler(int irq, void *dev_id)
{
	struct platform_device *pdev = dev_id;
	struct device *dev = &pdev->dev;
	struct abox_data *data = platform_get_drvdata(pdev);
	ABOX_IPC_MSG msg;

	if (abox_dma_irq_handler(irq, data) == IRQ_HANDLED)
		goto out;

	memcpy(&msg, data->sram_base + data->ipc_rx_offset, sizeof(msg));
	writel(0, data->sram_base + data->ipc_rx_ack_offset);

	dev_dbg(dev, "%s: irq=%d, ipcid=%d\n", __func__, irq, msg.ipcid);

	switch (irq) {
	case IPC_SYSTEM:
		abox_system_ipc_handler(dev, data, &msg);
		break;
	case IPC_PCMPLAYBACK:
		abox_playback_ipc_handler(dev, data, &msg);
		break;
	case IPC_PCMCAPTURE:
		abox_capture_ipc_handler(dev, data, &msg);
		break;
	case IPC_OFFLOAD:
		abox_offload_ipc_handler(dev, data, &msg);
		break;
	case IPC_ERAP:
		abox_registered_ipc_handler(dev, irq, data, &msg, true);
		break;
	default:
		abox_registered_ipc_handler(dev, irq, data, &msg, false);
		break;
	}
out:
	abox_log_schedule_flush_all(dev);

	dev_dbg(dev, "%s: exit\n", __func__);
	return IRQ_HANDLED;
}

static int abox_cpu_pm_ipc(struct abox_data *data, bool resume)
{
	const unsigned long long TIMER_RATE = 26000000;
	struct device *dev = &data->pdev->dev;
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system = &msg.msg.system;
	unsigned long long ktime, atime;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	ktime = sched_clock();
	atime = readq_relaxed(data->sfr_base + ABOX_TIMER_CURVALUD_LSB(1));
	/* clock to ns */
	atime *= 500;
	do_div(atime, TIMER_RATE / 2000000);

	msg.ipcid = IPC_SYSTEM;
	system->msgtype = resume ? ABOX_RESUME : ABOX_SUSPEND;
	system->bundle.param_u64[0] = ktime;
	system->bundle.param_u64[1] = atime;
	ret = abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 1, 1);
	if (!resume) {
		int i = 1000;
		unsigned int val;

		do {
			exynos_pmu_read(ABOX_CPU_STANDBY, &val);
		} while (--i && !(val & ABOX_CPU_STANDBY_WFI_MASK));

		if (!(val & ABOX_CPU_STANDBY_WFI_MASK)) {
			dev_warn(dev, "calliope suspend time out\n");
			ret = -ETIME;
		}
	}

	return ret;
}

static void abox_pad_retention(bool retention)
{
	if (retention) {
		pr_debug("%s:Do not need\n", __func__);
	} else {
#ifndef EMULATOR
		exynos_pmu_update(PAD_RETENTION_ABOX_OPTION,
				0x10000000, 0x10000000);
#else
		update_mask_value(pmu_alive + PAD_RETENTION_ABOX_OPTION,
				0x10000000, 0x10000000);
#endif
	}
}

static void abox_cpu_power(bool on)
{
	pr_info("%s(%d)\n", __func__, on);

#ifndef EMULATOR
	exynos_pmu_update(ABOX_CPU_CONFIGURATION, ABOX_CPU_LOCAL_PWR_CFG,
			on ? ABOX_CPU_LOCAL_PWR_CFG : 0);
#else
	update_mask_value(pmu_alive + ABOX_CPU_CONFIGURATION,
			ABOX_CPU_LOCAL_PWR_CFG,
			on ? ABOX_CPU_LOCAL_PWR_CFG : 0);
#endif
}

static int abox_cpu_enable(bool enable)
{
	unsigned int mask = ABOX_CPU_OPTION_ENABLE_CPU_MASK;
	unsigned int val = (enable ? mask : 0);
	unsigned int status = 0;
	unsigned long after;


	pr_info("%s(%d)\n", __func__, enable);

#ifndef EMULATOR
	exynos_pmu_update(ABOX_CPU_OPTION, mask, val);
#else
	update_mask_value(pmu_alive + ABOX_CPU_OPTION, mask, val);
#endif
	if (enable) {
		after = jiffies + LIMIT_IN_JIFFIES;
		do {
#ifndef EMULATOR
			exynos_pmu_read(ABOX_CPU_STATUS, &status);
#else
			status = readl(pmu_alive + ABOX_CPU_STATUS);
#endif
		} while (((status & ABOX_CPU_STATUS_STATUS_MASK)
				!= ABOX_CPU_STATUS_STATUS_MASK)
				&& time_is_after_eq_jiffies(after));
		if (time_is_before_jiffies(after)) {
			pr_err("abox cpu enable timeout\n");
			return -ETIME;
		}
	}

	return 0;

}

static void abox_save_register(struct abox_data *data)
{
	regcache_cache_only(data->regmap, true);
	regcache_mark_dirty(data->regmap);
}

static void abox_restore_register(struct abox_data *data)
{
	regcache_cache_only(data->regmap, false);
	regcache_sync(data->regmap);
}

static void abox_reload_extra_firmware(struct abox_data *data, const char *name)
{
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	struct abox_extra_firmware *ext_fw;
	int ret;

	dev_dbg(dev, "%s(%s)\n", __func__, name);

	for (ext_fw = data->firmware_extra; ext_fw - data->firmware_extra <
			ARRAY_SIZE(data->firmware_extra); ext_fw++) {
		if (!ext_fw->name || strcmp(ext_fw->name, name))
			continue;

		release_firmware(ext_fw->firmware);
		ret = request_firmware(&ext_fw->firmware, ext_fw->name, dev);
		if (ret < 0) {
			dev_err(dev, "%s: %s request failed\n", __func__,
					ext_fw->name);
			break;
		}
		dev_info(dev, "%s is reloaded\n", name);
	}
}

static void abox_request_extra_firmware(struct abox_data *data)
{
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *child_np;
	struct abox_extra_firmware *ext_fw;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	ext_fw = data->firmware_extra;
	for_each_child_of_node(np, child_np) {
		const char *status;

		status = of_get_property(child_np, "status", NULL);
		if (status && strcmp("okay", status) && strcmp("ok", status))
			continue;

		ret = of_property_read_string(child_np, "samsung,name",
				&ext_fw->name);
		if (ret < 0)
			continue;

		ret = of_property_read_u32(child_np, "samsung,area",
				&ext_fw->area);
		if (ret < 0)
			continue;

		ret = of_property_read_u32(child_np, "samsung,offset",
				&ext_fw->offset);
		if (ret < 0)
			continue;

		dev_dbg(dev, "%s: name=%s, area=%u, offset=%u\n", __func__,
				ext_fw->name, ext_fw->area, ext_fw->offset);

		if (!ext_fw->firmware) {
			dev_dbg(dev, "%s: request %s\n", __func__,
					ext_fw->name);
			ret = request_firmware(&ext_fw->firmware,
					ext_fw->name, dev);
			if (ret < 0)
				dev_err(dev, "%s: %s request failed\n",
						__func__, ext_fw->name);
		}
		ext_fw++;
	}

}

static void abox_download_extra_firmware(struct abox_data *data)
{
	struct device *dev = &data->pdev->dev;
	struct abox_extra_firmware *ext_fw;
	void __iomem *base;
	size_t size;

	dev_dbg(dev, "%s\n", __func__);

	for (ext_fw = data->firmware_extra; ext_fw - data->firmware_extra <
			ARRAY_SIZE(data->firmware_extra); ext_fw++) {
		if (!ext_fw->firmware)
			continue;

		switch (ext_fw->area) {
		case 0:
			base = data->sram_base;
			size = data->sram_size;
			break;
		case 1:
			base = data->dram_base;
			size = DRAM_FIRMWARE_SIZE;
			break;
		case 2:
			base = phys_to_virt(shm_get_vss_base());
			size = shm_get_vss_size();
			break;
		default:
			dev_err(dev, "%s: area is invalid name=%s, area=%u, offset=%u\n",
					__func__, ext_fw->name, ext_fw->area,
					ext_fw->offset);
			continue;
		}

		if (ext_fw->offset + ext_fw->firmware->size > size) {
			dev_err(dev, "%s: firmware is too large name=%s, area=%u, offset=%u\n",
					__func__, ext_fw->name, ext_fw->area,
					ext_fw->offset);
			continue;
		}

		memcpy(base + ext_fw->offset, ext_fw->firmware->data,
				ext_fw->firmware->size);
		dev_info(dev, "%s is downloaded at area %u offset %u\n",
				ext_fw->name, ext_fw->area, ext_fw->offset);
	}
}

static int abox_request_firmware(struct device *dev,
		const struct firmware **fw, const char *name)
{
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	release_firmware(*fw);
	ret = request_firmware(fw, name, dev);
	if (ret < 0) {
		dev_err(dev, "%s: %s request failed\n", __func__, name);
	} else {
		dev_info(dev, "%s is loaded\n", name);
	}

	return ret;
}

static void abox_complete_sram_firmware_request(const struct firmware *fw,
		void *context)
{
	struct platform_device *pdev = context;
	struct device *dev = &pdev->dev;
	struct abox_data *data = platform_get_drvdata(pdev);

	if (!fw) {
		dev_err(dev, "Failed to request firmware\n");
		return;
	}

	if (data->firmware_sram)
		release_firmware(data->firmware_sram);

	data->firmware_sram = fw;

	dev_info(dev, "SRAM firmware loaded\n");

	abox_request_firmware(dev, &data->firmware_dram, "calliope_dram.bin");
	abox_request_extra_firmware(data);

	if (abox_test_quirk(data, ABOX_QUIRK_OFF_ON_SUSPEND))
		if (pm_runtime_active(dev))
			abox_enable(dev);
}

static int abox_download_firmware(struct platform_device *pdev)
{
	struct abox_data *data = platform_get_drvdata(pdev);
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);

	if (unlikely(!data->firmware_sram)) {
		request_firmware_nowait(THIS_MODULE,
			FW_ACTION_HOTPLUG,
			"calliope_sram.bin",
			dev,
			GFP_KERNEL,
			pdev,
			abox_complete_sram_firmware_request);
		dev_warn(dev, "SRAM firmware downloading is deferred\n");
		return -EAGAIN;
	}
	memcpy_toio(data->sram_base, data->firmware_sram->data,
			data->firmware_sram->size);
	memset_io(data->sram_base + data->firmware_sram->size, 0,
			data->sram_size - data->firmware_sram->size);

	if (unlikely(!data->firmware_dram)) {
		dev_warn(dev, "DRAM firmware downloading is defferred\n");
		return -EAGAIN;
	}
	memcpy(data->dram_base, data->firmware_dram->data,
			data->firmware_dram->size);
	memset(data->dram_base + data->firmware_dram->size, 0,
			DRAM_FIRMWARE_SIZE - data->firmware_dram->size);

	abox_download_extra_firmware(data);

	return 0;
}

static void abox_cfg_gpio(struct device *dev, const char *name)
{
	struct abox_data *data = dev_get_drvdata(dev);
	struct pinctrl_state *pin_state;
	int ret;

	dev_info(dev, "%s(%s)\n", __func__, name);

	if (!data->pinctrl)
		return;

	pin_state = pinctrl_lookup_state(data->pinctrl, name);
	if (IS_ERR(pin_state)) {
		dev_err(dev, "Couldn't find pinctrl %s\n", name);
	} else {
		ret = pinctrl_select_state(data->pinctrl, pin_state);
		if (ret < 0)
			dev_err(dev, "Unable to configure pinctrl %s\n", name);
	}
}

#undef MANUAL_SECURITY_CHANGE
#ifdef MANUAL_SECURITY_CHANGE
static void work_temp_function(struct work_struct *work)
{
	exynos_smc(0x82000701, 0, 0, 0);
	pr_err("%s: ABOX_CA7 security changed!!!\n", __func__);
}
static DECLARE_DELAYED_WORK(work_temp, work_temp_function);
#endif

static void __abox_control_l2c(struct abox_data *data, bool enable)
{
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	struct device *dev = &data->pdev->dev;

	if (data->l2c_enabled == enable)
		return;

	dev_info(dev, "%s(%d)\n", __func__, enable);

	data->l2c_controlled = false;

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_START_L2C_CONTROL;
	system_msg->param1 = enable ? 1 : 0;

	if (enable) {
		vts_acquire_sram(data->pdev_vts, 0);

		abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 1, 0);
		wait_event_timeout(data->ipc_wait_queue,
				data->l2c_controlled, LIMIT_IN_JIFFIES);
		if (!data->l2c_controlled)
			dev_err(dev, "l2c enable failed\n");
	} else {
		abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 1, 0);
		wait_event_timeout(data->ipc_wait_queue,
				data->l2c_controlled, LIMIT_IN_JIFFIES);
		if (!data->l2c_controlled)
			dev_err(dev, "l2c disable failed\n");

		vts_release_sram(data->pdev_vts, 0);
	}

	data->l2c_enabled = enable;
}

static void abox_l2c_work_func(struct work_struct *work)
{
	struct abox_data *data = container_of(work, struct abox_data, l2c_work);
	struct platform_device *pdev = data->pdev;
	struct device *dev = &pdev->dev;
	size_t length = ARRAY_SIZE(data->l2c_requests);
	struct abox_l2c_request *request;
	bool enable = false;

	dev_dbg(dev, "%s\n", __func__);

	for (request = data->l2c_requests;
			request - data->l2c_requests < length
			&& request->id;
			request++) {
		if (request->on) {
			enable = true;
			break;
		}
	}

	__abox_control_l2c(data, enable);
}

int abox_request_l2c(struct device *dev, struct abox_data *data,
		void *id, bool on)
{
	struct abox_l2c_request *request;
	size_t length = ARRAY_SIZE(data->l2c_requests);

	if (!abox_test_quirk(data, ABOX_QUIRK_SHARE_VTS_SRAM))
		return 0;

	dev_info(dev, "%s(%#lx, %d)\n", __func__, (unsigned long)id, on);

	for (request = data->l2c_requests;
			request - data->l2c_requests < length
			&& request->id && request->id != id;
			request++) {
	}

	request->on = on;
	wmb(); /* on is read after id in reading function */
	request->id = id;

	if (request - data->l2c_requests >= ARRAY_SIZE(data->l2c_requests)) {
		dev_err(dev, "%s: out of index. id=%#lx, on=%d\n",
				__func__, (unsigned long)id, on);
		return -ENOMEM;
	}

	schedule_work(&data->l2c_work);

	return 0;
}

int abox_request_l2c_sync(struct device *dev, struct abox_data *data,
		void *id, bool on)
{
	if (!abox_test_quirk(data, ABOX_QUIRK_SHARE_VTS_SRAM))
		return 0;

	abox_request_l2c(dev, data, id, on);
	flush_work(&data->l2c_work);
	return 0;
}

static void abox_clear_l2c_requests(struct device *dev, struct abox_data *data)
{
	struct abox_l2c_request *req;
	size_t len = ARRAY_SIZE(data->l2c_requests);

	if (!abox_test_quirk(data, ABOX_QUIRK_SHARE_VTS_SRAM))
		return;

	dev_info(dev, "%s\n", __func__);

	for (req = data->l2c_requests; req - data->l2c_requests < len &&
			req->id; req++) {
		req->on = false;
	}

	__abox_control_l2c(data, false);
}

static bool abox_is_timer_set(struct abox_data *data)
{
	unsigned int val;
	int ret;

	ret = regmap_read(data->regmap, ABOX_TIMER_CTRL1(0), &val);
	if (ret < 0)
		val = 0;

	return !!val;
}

static void abox_start_timer(struct abox_data *data)
{
	struct regmap *regmap = data->regmap;

	regmap_write(regmap, ABOX_TIMER_CTRL0(0), 1 << ABOX_TIMER_START_L);
}

static int abox_enable(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned int i, value;
	bool has_reset;
	int ret = 0;

	dev_info(dev, "%s\n", __func__);

	abox_gic_enable_irq(data->dev_gic);

	abox_request_cpu_gear(dev, data, DEFAULT_CPU_GEAR_ID, ABOX_CPU_GEAR_MAX);
	ret = clk_set_rate(data->clk_pll, AUD_PLL_RATE_HZ_FOR_48000);
	if (ret < 0)
		dev_warn(dev, "setting audio pll clock to 1.2Ghz is failed: %d\n",	ret);

	if (is_secure_gic()) {
		exynos_pmu_write(ABOX_MAGIC, 0);
		ret = exynos_smc(0x82000501, 0, 0, 0);
		dev_dbg(dev, "%s: smc ret=%d\n", __func__, ret);

		for (i = 1000; i; i--) {
			exynos_pmu_read(ABOX_MAGIC, &value);
			if (value == ABOX_MAGIC_VALUE)
				break;
		}
		if (value != ABOX_MAGIC_VALUE)
			dev_warn(dev, "%s: abox magic timeout\n", __func__);
		abox_cpu_enable(false);
		abox_cpu_power(false);
	}

	if (abox_test_quirk(data, ABOX_QUIRK_SHARE_VTS_SRAM)) {
		writel(0x1, data->sysreg_base + ABOX_SYSREG_MISC_CON);
		writel(0x1, data->sysreg_base + ABOX_SYSREG_L2_CACHE_CON);
	}

	ret = clk_enable(data->clk_cpu);
	if (ret < 0) {
		dev_err(dev, "Failed to enable cpu clock: %d\n", ret);
		goto error;
	}

	ret = clk_set_rate(data->clk_audif, AUDIF_RATE_HZ);
	if (ret < 0) {
		dev_err(dev, "Failed to set audif clock: %d\n", ret);
		goto error;
	}
	dev_info(dev, "audif clock: %lu\n", clk_get_rate(data->clk_audif));

	ret = clk_enable(data->clk_audif);
	if (ret < 0) {
		dev_err(dev, "Failed to enable audif clock: %d\n", ret);
		goto error;
	}

	abox_cfg_gpio(dev, "default");

	abox_restore_register(data);
	has_reset = !abox_is_timer_set(data);
	if (!has_reset) {
		dev_info(dev, "wakeup from WFI\n");
		abox_start_timer(data);
	} else {
		abox_gic_init_gic(data->dev_gic);

		ret = abox_download_firmware(pdev);
		if (ret < 0) {
			if (ret != -EAGAIN)
				dev_err(dev, "Failed to download firmware\n");
			else
				ret = 0;

			abox_request_cpu_gear(dev, data, DEFAULT_CPU_GEAR_ID,
					ABOX_CPU_GEAR_MIN);
			goto error;
		}
	}

	abox_request_dram_on(pdev, dev, true);
	if (has_reset) {
		abox_cpu_power(true);
		abox_cpu_enable(true);
	}
	data->calliope_state = CALLIOPE_ENABLING;

	abox_pad_retention(false);
#ifdef MANUAL_SECURITY_CHANGE
	schedule_delayed_work(&work_temp, msecs_to_jiffies(3000));
#endif

	data->enabled = true;

	if (has_reset)
		pm_wakeup_event(dev, BOOT_DONE_TIMEOUT_MS);
	else
		abox_boot_done(dev, data->calliope_version);

error:
	return ret;
}

static int abox_disable(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct abox_data *data = dev_get_drvdata(dev);
	enum calliope_state state = data->calliope_state;

	dev_info(dev, "%s\n", __func__);

	/* AUD_PLL must be normal during suspend */
	clk_set_rate(data->clk_pll, AUD_PLL_RATE_HZ_FOR_48000);

	data->calliope_state = CALLIOPE_DISABLING;
	abox_cache_components(dev, data);
	abox_clear_l2c_requests(dev, data);
	flush_work(&data->boot_done_work);
	flush_work(&data->l2c_work);
	if (state != CALLIOPE_DISABLED)
		abox_cpu_pm_ipc(data, false);
	data->calliope_state = CALLIOPE_DISABLED;
	abox_log_drain_all(dev);
	abox_request_dram_on(pdev, dev, false);
	abox_save_register(data);
	abox_cfg_gpio(dev, "idle");
	abox_pad_retention(true);
	data->enabled = false;
	clk_disable(data->clk_cpu);
	abox_gic_disable_irq(data->dev_gic);
	abox_failsafe_report_reset(dev);

	return 0;
}

void abox_poweroff(void)
{
	struct platform_device *pdev = p_abox_data->pdev;
	struct device *dev = &pdev->dev;
	struct abox_data *data = dev_get_drvdata(dev);

	if (data->calliope_state == CALLIOPE_DISABLED) {
		dev_dbg(dev, "already disabled\n");
		return;
	}
	dev_info(dev, "%s\n", __func__);

	abox_disable(dev);

	exynos_sysmmu_control(dev, false);
}

static int abox_runtime_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	p_abox_data->enabled = false;

	return 0;
}

static int abox_runtime_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);

	exynos_sysmmu_control(dev, true);

	return abox_enable(dev);
}

static int abox_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	/* nothing to do */
	return 0;
}

static int abox_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	/* nothing to do */
	return 0;
}

static int abox_qos_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct abox_data *data = container_of(nb, struct abox_data, qos_nb);
	struct device *dev = &data->pdev->dev;
	long value = (long)action;
	long qos_class = (long)nb_data;
	unsigned long aclk = clk_get_rate(data->clk_bus);
	unsigned int sifs_cnt0, sifs_cnt1, cnt_val, rate, pwidth, channels;
	unsigned long sifs0_cnt, sifs1_cnt, sifs2_cnt;
	int ret;

	dev_dbg(dev, "%s(%ldkHz, %ld)\n", __func__, value, qos_class);

	ret = regmap_read(data->regmap, ABOX_SPUS_CTRL_SIFS_CNT0, &sifs_cnt0);
	if (ret < 0) {
		dev_err(dev, "%s: SPUS_CTRL_SIFS_CNT0 read fail: %d\n",
				__func__, ret);
		goto out;
	}
	ret = regmap_read(data->regmap, ABOX_SPUS_CTRL_SIFS_CNT1, &sifs_cnt1);
	if (ret < 0) {
		dev_err(dev, "%s: SPUS_CTRL_SIFS_CNT1 read fail: %d\n",
				__func__, ret);
		goto out;
	}

	sifs0_cnt = (sifs_cnt0 & ABOX_SIFS0_CNT_VAL_MASK) >>
			ABOX_SIFS0_CNT_VAL_L;
	sifs1_cnt = (sifs_cnt0 & ABOX_SIFS1_CNT_VAL_MASK) >>
			ABOX_SIFS1_CNT_VAL_L;
	sifs2_cnt = (sifs_cnt1 & ABOX_SIFS2_CNT_VAL_MASK) >>
			ABOX_SIFS2_CNT_VAL_L;

	if (sifs0_cnt) {
		rate = abox_get_sif_rate(data, SET_MIXER_SAMPLE_RATE);
		pwidth = abox_get_sif_physical_width(data, SET_MIXER_FORMAT);
		channels = abox_get_sif_channels(data, SET_MIXER_FORMAT);
		cnt_val = abox_sifsx_cnt_val(aclk, rate, pwidth, channels);
		dev_info(dev, "%s: %s <= %u\n", __func__, "SIFS0_CNT_VAL",
				cnt_val);
		ret = regmap_update_bits(data->regmap, ABOX_SPUS_CTRL_SIFS_CNT0,
			ABOX_SIFS0_CNT_VAL_MASK,
			(unsigned int)cnt_val << ABOX_SIFS0_CNT_VAL_L);
		if (ret < 0)
			dev_err(dev, "regmap update failed: %d\n", ret);
	}
	if (sifs1_cnt) {
		rate = abox_get_sif_rate(data, SET_OUT1_SAMPLE_RATE);
		pwidth = abox_get_sif_physical_width(data, SET_OUT1_FORMAT);
		channels = abox_get_sif_channels(data, SET_OUT1_FORMAT);
		cnt_val = abox_sifsx_cnt_val(aclk, rate, pwidth, channels);
		dev_info(dev, "%s: %s <= %u\n", __func__, "SIFS0_CNT_VAL",
				cnt_val);
		ret = regmap_update_bits(data->regmap, ABOX_SPUS_CTRL_SIFS_CNT0,
			ABOX_SIFS1_CNT_VAL_MASK,
			(unsigned int)cnt_val << ABOX_SIFS1_CNT_VAL_L);
		if (ret < 0)
			dev_err(dev, "regmap update failed: %d\n", ret);
	}
	if (sifs2_cnt) {
		rate = abox_get_sif_rate(data, SET_OUT2_SAMPLE_RATE);
		pwidth = abox_get_sif_physical_width(data, SET_OUT2_FORMAT);
		channels = abox_get_sif_channels(data, SET_OUT2_FORMAT);
		cnt_val = abox_sifsx_cnt_val(aclk, rate, pwidth, channels);
		dev_info(dev, "%s: %s <= %u\n", __func__, "SIFS0_CNT_VAL",
				cnt_val);
		ret = regmap_update_bits(data->regmap, ABOX_SPUS_CTRL_SIFS_CNT1,
			ABOX_SIFS2_CNT_VAL_MASK,
			(unsigned int)cnt_val << ABOX_SIFS2_CNT_VAL_L);
		if (ret < 0)
			dev_err(dev, "regmap update failed: %d\n", ret);
	}
out:
	return NOTIFY_DONE;
}

static int abox_print_power_usage(struct device *dev, void *data)
{
	dev_dbg(dev, "%s\n", __func__);

	if (pm_runtime_enabled(dev) && pm_runtime_active(dev)) {
		dev_info(dev, "usage_count:%d\n",
				atomic_read(&dev->power.usage_count));
		device_for_each_child(dev, data, abox_print_power_usage);
	}

	return 0;
}

static int abox_pm_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct abox_data *data = container_of(nb, struct abox_data, pm_nb);
	struct device *dev = &data->pdev->dev;
	int ret;

	dev_dbg(dev, "%s(%lu)\n", __func__, action);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		if (data->audio_mode != MODE_IN_CALL) {
			enum calliope_state state;

			pm_runtime_barrier(dev);
			state = data->calliope_state;
			if (state == CALLIOPE_ENABLING) {
				dev_info(dev, "calliope state: %d\n", state);
				return NOTIFY_BAD;
			}
			/* clear cpu gears to abox power off */
			abox_clear_cpu_gear_requests(dev, data);
			abox_cpu_gear_barrier(data);
			flush_workqueue(data->ipc_workqueue);
			if (abox_test_quirk(data, ABOX_QUIRK_OFF_ON_SUSPEND)) {
				ret = pm_runtime_put_sync(dev);
				if (ret < 0) {
					pm_runtime_get(dev);
					dev_info(dev, "runtime put sync: %d\n", ret);
					abox_print_power_usage(dev, NULL);
					return NOTIFY_BAD;
				} else if (ret == 0 && atomic_read(&dev->power.usage_count) > 0) {
					dev_info(dev, "runtime put sync: %d uc(%d)\n",
							ret, atomic_read(&dev->power.usage_count));
					pm_runtime_get(dev);
					abox_print_power_usage(dev, NULL);
					return NOTIFY_BAD;
				}
				ret = pm_runtime_suspend(dev);
				if (ret < 0) {
					dev_info(dev, "runtime suspend: %d\n", ret);
					abox_print_power_usage(dev, NULL);
					return NOTIFY_BAD;
				}
				atomic_set(&data->suspend_state, 1);
				dev_info(dev, "(%d)s suspend_state: %d\n", __LINE__,
						atomic_read(&data->suspend_state));
			} else {
				ret = pm_runtime_suspend(dev);
				if (ret < 0) {
					dev_info(dev, "runtime suspend: %d\n",
							ret);
					return NOTIFY_BAD;
				}
			}
		} else {
			dev_info(dev, "abox is not clearable()\n");
		}
		break;
	case PM_POST_SUSPEND:
		if (abox_test_quirk(data, ABOX_QUIRK_OFF_ON_SUSPEND)) {
			dev_info(dev, "(%d)r suspend_state: %d\n", __LINE__,
					atomic_read(&data->suspend_state));
			if (atomic_read(&data->suspend_state) == 1) {
				pm_runtime_get_sync(&data->pdev->dev);
				atomic_set(&data->suspend_state, 0);
			}
		}
		break;
	default:
		/* Nothing to do */
		break;
	}
	return NOTIFY_DONE;
}

static int abox_modem_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct abox_data *data = container_of(nb, struct abox_data, modem_nb);
	struct device *dev = &data->pdev->dev;
	ABOX_IPC_MSG msg;
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;

	dev_info(&data->pdev->dev, "%s(%lu)\n", __func__, action);

	switch (action) {
	case MODEM_EVENT_ONLINE:
		msg.ipcid = IPC_SYSTEM;
		system_msg->msgtype = ABOX_START_VSS;
		abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 1, 0);
		break;
#ifdef CONFIG_SND_SOC_SAMSUNG_AUDIO
	case MODEM_EVENT_RESET:
	case MODEM_EVENT_EXIT:
	case MODEM_EVENT_WATCHDOG:
		abox_debug_string_update(TYPE_ABOX_VSSERROR, NULL);
		break;
#endif
	}

	return NOTIFY_DONE;
}

#ifdef CONFIG_EXYNOS_ITMON
static int abox_itmon_notifier(struct notifier_block *nb,
		unsigned long action, void *nb_data)
{
	struct abox_data *data = container_of(nb, struct abox_data, itmon_nb);
	struct device *dev = &data->pdev->dev;
	struct itmon_notifier *itmon_data = nb_data;

	if (itmon_data && itmon_data->dest && (strncmp("ABOX", itmon_data->dest,
			sizeof("ABOX") - 1) == 0)) {
		dev_info(dev, "%s(%lu)\n", __func__, action);
		data->enabled = false;
		return NOTIFY_OK;
	}

	return NOTIFY_DONE;
}
#endif

static ssize_t calliope_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct abox_data *data = dev_get_drvdata(dev);
	unsigned int version = be32_to_cpu(data->calliope_version);

	memcpy(buf, &version, sizeof(version));
	buf[4] = '\n';
	buf[5] = '\0';

	return 6;
}

static ssize_t calliope_debug_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	ABOX_IPC_MSG msg = {0,};
	struct IPC_SYSTEM_MSG *system_msg = &msg.msg.system;
	int ret;

	dev_dbg(dev, "%s\n", __func__);

	msg.ipcid = IPC_SYSTEM;
	system_msg->msgtype = ABOX_REQUEST_DEBUG;
	ret = sscanf(buf, "%10d,%10d,%10d,%739s", &system_msg->param1,
			&system_msg->param2, &system_msg->param3,
			system_msg->bundle.param_bundle);
	if (ret < 0)
		return ret;

	ret = abox_request_ipc(dev, msg.ipcid, &msg, sizeof(msg), 0, 0);
	if (ret < 0)
		return ret;

	return count;
}

static ssize_t calliope_cmd_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	static const char cmd_reload_ext_bin[] = "RELOAD EXT BIN";
	static const char cmd_failsafe[] = "FAILSAFE";
	static const char cmd_cpu_gear[] = "CPU GEAR";
	struct abox_data *data = dev_get_drvdata(dev);
	char name[80];

	dev_dbg(dev, "%s(%s)\n", __func__, buf);
	if (!strncmp(cmd_reload_ext_bin, buf, sizeof(cmd_reload_ext_bin) - 1)) {
		dev_dbg(dev, "reload ext bin\n");
		if (sscanf(buf, "RELOAD EXT BIN:%63s", name) == 1)
			abox_reload_extra_firmware(data, name);
	} else if (!strncmp(cmd_failsafe, buf, sizeof(cmd_failsafe) - 1)) {
		dev_dbg(dev, "failsafe\n");
		abox_failsafe_report(dev);
	} else if (!strncmp(cmd_cpu_gear, buf, sizeof(cmd_cpu_gear) - 1)) {
		unsigned int gear;
		int ret;

		dev_info(dev, "set clk\n");
		ret = kstrtouint(buf + sizeof(cmd_cpu_gear), 10, &gear);
		if (!ret) {
			dev_info(dev, "gear = %u\n", gear);
			pm_runtime_get_sync(dev);
			abox_request_cpu_gear(dev, data, TEST_CPU_GEAR_ID,
					gear);
			dev_info(dev, "bus clk = %lu\n",
					clk_get_rate(data->clk_bus));
			pm_runtime_mark_last_busy(dev);
			pm_runtime_put_autosuspend(dev);
		}
	}

	return count;
}

static DEVICE_ATTR_RO(calliope_version);
static DEVICE_ATTR_WO(calliope_debug);
static DEVICE_ATTR_WO(calliope_cmd);

static int samsung_abox_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct device_node *np_tmp;
	struct platform_device *pdev_tmp;
	struct abox_data *data;
	phys_addr_t paddr;
	int ret, i;

	dev_info(dev, "%s\n", __func__);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	platform_set_drvdata(pdev, data);
	data->pdev = pdev;
	p_abox_data = data;

	atomic_set(&data->suspend_state, 0);

	abox_probe_quirks(data, np);
	init_waitqueue_head(&data->ipc_wait_queue);
	spin_lock_init(&data->ipc_queue_lock);
	mutex_init(&data->iommu_lock);
	device_init_wakeup(dev, true);
	data->cpu_gear = ABOX_CPU_GEAR_MIN;
	data->cpu_gear_min = 3; /* default value from kangchen */
	for (i = 0; i < ARRAY_SIZE(data->sif_rate); i++) {
		data->sif_rate_min[i] = data->sif_rate[i] = 48000;
		data->sif_format_min[i] = data->sif_format[i] =
				SNDRV_PCM_FORMAT_S16;
		data->sif_channels_min[i] = data->sif_channels[i] = 2;
	}
	INIT_WORK(&data->ipc_work, abox_process_ipc);
	INIT_WORK(&data->change_cpu_gear_work, abox_change_cpu_gear_work_func);
	INIT_WORK(&data->change_int_freq_work, abox_change_int_freq_work_func);
	INIT_WORK(&data->change_mif_freq_work, abox_change_mif_freq_work_func);
	INIT_WORK(&data->change_lit_freq_work, abox_change_lit_freq_work_func);
	INIT_WORK(&data->change_big_freq_work, abox_change_big_freq_work_func);
	INIT_WORK(&data->change_hmp_boost_work,
			abox_change_hmp_boost_work_func);
	INIT_WORK(&data->register_component_work,
			abox_register_component_work_func);
	INIT_WORK(&data->boot_done_work, abox_boot_done_work_func);
	INIT_WORK(&data->l2c_work, abox_l2c_work_func);
	INIT_DELAYED_WORK(&data->tickle_work, abox_tickle_work_func);
	INIT_LIST_HEAD(&data->irq_actions);
	INIT_LIST_HEAD_RCU(&data->iommu_maps);

	data->gear_workqueue = alloc_ordered_workqueue("abox_gear",
			WQ_FREEZABLE | WQ_MEM_RECLAIM);
	if (!data->gear_workqueue) {
		dev_err(dev, "Couldn't create workqueue %s\n", "abox_gear");
		return -ENOMEM;
	}

	data->ipc_workqueue = alloc_ordered_workqueue("abox_ipc",
			WQ_MEM_RECLAIM);
	if (!data->ipc_workqueue) {
		dev_err(dev, "Couldn't create workqueue %s\n", "abox_ipc");
		return -ENOMEM;
	}

	data->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(data->pinctrl)) {
		dev_err(dev, "Couldn't get pins (%li)\n",
				PTR_ERR(data->pinctrl));
		data->pinctrl = NULL;
	}

	data->sfr_base = devm_request_and_map_byname(pdev, "sfr",
			NULL, NULL);
	if (IS_ERR(data->sfr_base))
		return PTR_ERR(data->sfr_base);

	data->sysreg_base = devm_request_and_map_byname(pdev, "sysreg",
			NULL, NULL);
	if (IS_ERR(data->sysreg_base))
		return PTR_ERR(data->sysreg_base);

	data->sram_base = devm_request_and_map_byname(pdev, "sram",
			&data->sram_base_phys, &data->sram_size);
	if (IS_ERR(data->sram_base))
		return PTR_ERR(data->sram_base);

	data->iommu_domain = get_domain_from_dev(dev);
	if (IS_ERR(data->iommu_domain)) {
		dev_err(dev, "Unable to get iommu domain\n");
		return PTR_ERR(data->iommu_domain);
	}

	ret = iommu_attach_device(data->iommu_domain, dev);
	if (ret < 0) {
		dev_err(dev, "Unable to attach device to iommu (%d)\n", ret);
		return ret;
	}

	data->dram_base = dmam_alloc_coherent(dev, DRAM_FIRMWARE_SIZE,
			&data->dram_base_phys, GFP_KERNEL);
	if (IS_ERR_OR_NULL(data->dram_base)) {
		dev_err(dev, "Failed to allocate coherent memory: %ld\n",
				PTR_ERR(data->dram_base));
		return PTR_ERR(data->dram_base);
	}
	dev_info(&pdev->dev, "%s(%pa) is mapped on %p with size of %d\n",
			"dram firmware", &data->dram_base_phys, data->dram_base,
			DRAM_FIRMWARE_SIZE);
	abox_iommu_map(dev, IOVA_DRAM_FIRMWARE, data->dram_base_phys,
			DRAM_FIRMWARE_SIZE, data->dram_base);

	data->priv_base = dmam_alloc_coherent(dev, PRIVATE_SIZE,
			&data->priv_base_phys, GFP_KERNEL);
	if (!data->priv_base) {
		dev_err(dev, "%s: no memory\n", "private");
		return -ENOMEM;
	}
	dev_info(dev, "%s: %pa(%#x) => %p\n", "private",
			&data->priv_base_phys, PRIVATE_SIZE, data->priv_base);
	iommu_map(data->iommu_domain, IOVA_PRIVATE, data->priv_base_phys,
			PRIVATE_SIZE, 0);

	paddr = shm_get_vss_base();
	dev_info(&pdev->dev, "%s(%pa) is mapped on %p with size of %d\n",
			"vss firmware", &paddr, phys_to_virt(paddr),
			shm_get_vss_size());
	abox_iommu_map(dev, IOVA_VSS_FIRMWARE, paddr, shm_get_vss_size(),
			shm_get_vss_region());

	paddr = shm_get_vparam_base();
	dev_info(dev, "%s(%#x) alloc\n", "vss parameter",
			shm_get_vparam_size());
	abox_iommu_map(dev, IOVA_VSS_PARAMETER, paddr, shm_get_vparam_size(), 0);

	abox_iommu_map(dev, 0x10000000, 0x10000000, PAGE_SIZE, 0);
	iovmm_set_fault_handler(&pdev->dev, abox_iommu_fault_handler, data);

	data->clk_pll = devm_clk_get_and_prepare(pdev, "pll");
	if (IS_ERR(data->clk_pll))
		return PTR_ERR(data->clk_pll);

	data->clk_audif = devm_clk_get_and_prepare(pdev, "audif");
	if (IS_ERR(data->clk_audif))
		return PTR_ERR(data->clk_audif);

	data->clk_cpu = devm_clk_get_and_prepare(pdev, "cpu");
	if (IS_ERR(data->clk_cpu))
		return PTR_ERR(data->clk_cpu);

	data->clk_bus = devm_clk_get_and_prepare(pdev, "bus");
	if (IS_ERR(data->clk_bus))
		return PTR_ERR(data->clk_bus);

	ret = of_property_read_u32(np, "uaif_max_div", &data->uaif_max_div);
	if (ret < 0) {
		dev_warn(dev, "Failed to read %s: %d\n", "uaif_max_div", ret);
		data->uaif_max_div = 32;
	}

	ret = of_property_read_u32(np, "ipc_tx_offset", &data->ipc_tx_offset);
	if (ret < 0) {
		dev_err(dev, "Failed to read %s: %d\n", "ipc_tx_offset", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "ipc_rx_offset", &data->ipc_rx_offset);
	if (ret < 0) {
		dev_err(dev, "Failed to read %s: %d\n", "ipc_rx_offset", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "ipc_tx_ack_offset",
			&data->ipc_tx_ack_offset);
	if (ret < 0) {
		dev_err(dev, "Failed to read %s: %d\n", "ipc_tx_ack_offset",
				ret);
		return ret;
	}

	ret = of_property_read_u32(np, "ipc_rx_ack_offset",
			&data->ipc_rx_ack_offset);
	if (ret < 0) {
		dev_err(dev, "Failed to read %s: %d\n", "ipc_rx_ack_offset",
				ret);
		return ret;
	}

	ret = of_property_read_u32_array(np, "pm_qos_int", data->pm_qos_int,
			ARRAY_SIZE(data->pm_qos_int));
	if (ret < 0)
		dev_warn(dev, "Failed to read %s: %d\n", "pm_qos_int", ret);

	ret = of_property_read_u32_array(np, "pm_qos_aud", data->pm_qos_aud,
			ARRAY_SIZE(data->pm_qos_aud));
	if (ret < 0) {
		dev_warn(dev, "Failed to read %s: %d\n", "pm_qos_aud", ret);
	} else {
		for (i = 0; i < ARRAY_SIZE(data->pm_qos_aud); i++) {
			if (!data->pm_qos_aud[i]) {
				data->cpu_gear_min = i;
				break;
			}
		}
	}

	np_tmp = of_parse_phandle(np, "abox_gic", 0);
	if (!np_tmp) {
		dev_err(dev, "Failed to get abox_gic device node\n");
		return -EPROBE_DEFER;
	}
	pdev_tmp = of_find_device_by_node(np_tmp);
	if (!pdev_tmp) {
		dev_err(dev, "Failed to get abox_gic platform device\n");
		return -EPROBE_DEFER;
	}
	data->dev_gic = &pdev_tmp->dev;

	if (abox_test_quirk(data, ABOX_QUIRK_SHARE_VTS_SRAM)) {
		np_tmp = of_parse_phandle(np, "vts", 0);
		if (!np_tmp) {
			dev_err(dev, "Failed to get vts device node\n");
			return -EPROBE_DEFER;
		}
		data->pdev_vts = of_find_device_by_node(np_tmp);
		if (!data->pdev_vts) {
			dev_err(dev, "Failed to get vts platform device\n");
			return -EPROBE_DEFER;
		}
	}

#ifdef EMULATOR
	pmu_alive = ioremap(0x16480000, 0x10000);
#endif
	pm_qos_add_request(&abox_pm_qos_aud, PM_QOS_AUD_THROUGHPUT, 0);
	pm_qos_add_request(&abox_pm_qos_int, PM_QOS_DEVICE_THROUGHPUT, 0);
	pm_qos_add_request(&abox_pm_qos_mif, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&abox_pm_qos_lit, PM_QOS_CLUSTER0_FREQ_MIN, 0);
	pm_qos_add_request(&abox_pm_qos_big, PM_QOS_CLUSTER1_FREQ_MIN, 0);

	for (i = 0; i < ABOX_GIC_IRQ_COUNT; i++)
		abox_gic_register_irq_handler(data->dev_gic, i,
				abox_irq_handler, pdev);

	if (IS_ENABLED(CONFIG_SOC_EXYNOS8895)) {
		abox_regmap_config.reg_defaults = abox_reg_defaults_8895;
		abox_regmap_config.num_reg_defaults =
				ARRAY_SIZE(abox_reg_defaults_8895);
	} else if (IS_ENABLED(CONFIG_SOC_EXYNOS9810)) {
		abox_regmap_config.reg_defaults = abox_reg_defaults_9810;
		abox_regmap_config.num_reg_defaults =
				ARRAY_SIZE(abox_reg_defaults_9810);
	} else if (IS_ENABLED(CONFIG_SOC_EXYNOS9610)) {
		abox_regmap_config.reg_defaults = abox_reg_defaults_9610;
		abox_regmap_config.num_reg_defaults =
				ARRAY_SIZE(abox_reg_defaults_9610);
	}
	data->regmap = devm_regmap_init_mmio(dev,
			data->sfr_base,
			&abox_regmap_config);

	pm_runtime_enable(dev);
	pm_runtime_set_autosuspend_delay(dev, 1);
	pm_runtime_use_autosuspend(dev);
	pm_runtime_get(dev);

	data->qos_nb.notifier_call = abox_qos_notifier;
	pm_qos_add_notifier(PM_QOS_AUD_THROUGHPUT, &data->qos_nb);

	data->pm_nb.notifier_call = abox_pm_notifier;
	register_pm_notifier(&data->pm_nb);

	data->modem_nb.notifier_call = abox_modem_notifier;
	register_modem_event_notifier(&data->modem_nb);

#ifdef CONFIG_EXYNOS_ITMON
	data->itmon_nb.notifier_call = abox_itmon_notifier;
	itmon_notifier_chain_register(&data->itmon_nb);
#endif

	abox_failsafe_init(dev);

	wakeup_source_init(&data->ws, "abox");
	wakeup_source_init(&data->ws_boot, "abox_boot");

	ret = device_create_file(dev, &dev_attr_calliope_version);
	if (ret < 0)
		dev_warn(dev, "Failed to create file: %s\n", "version");

	ret = device_create_file(dev, &dev_attr_calliope_debug);
	if (ret < 0)
		dev_warn(dev, "Failed to create file: %s\n", "debug");

	ret = device_create_file(dev, &dev_attr_calliope_cmd);
	if (ret < 0)
		dev_warn(dev, "Failed to create file: %s\n", "cmd");

	atomic_notifier_chain_register(&panic_notifier_list,
			&abox_panic_notifier);

	ret = snd_soc_register_component(dev, &abox_cmpnt, abox_dais,
			ARRAY_SIZE(abox_dais));
	if (ret < 0)
		dev_err(dev, "component register failed:%d\n", ret);

	dev_info(dev, "%s: probe complete\n", __func__);

	of_platform_populate(np, NULL, NULL, dev);

	return 0;
}

static int samsung_abox_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct abox_data *data = platform_get_drvdata(pdev);

	dev_info(dev, "%s\n", __func__);

	pm_runtime_disable(dev);
#ifndef CONFIG_PM
	abox_runtime_suspend(dev);
#endif
	device_init_wakeup(dev, false);
	destroy_workqueue(data->ipc_workqueue);
	pm_qos_remove_request(&abox_pm_qos_aud);
	pm_qos_remove_request(&abox_pm_qos_int);
	pm_qos_remove_request(&abox_pm_qos_mif);
	pm_qos_remove_request(&abox_pm_qos_lit);
	pm_qos_remove_request(&abox_pm_qos_big);
	snd_soc_unregister_component(dev);
	abox_iommu_unmap(dev, IOVA_DRAM_FIRMWARE);
#ifdef EMULATOR
	iounmap(pmu_alive);
#endif
	wakeup_source_trash(&data->ws);
	wakeup_source_trash(&data->ws_boot);

	return 0;
}

static void samsung_abox_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s\n", __func__);
	pm_runtime_disable(dev);
}

static const struct of_device_id samsung_abox_match[] = {
	{
		.compatible = "samsung,abox",
	},
	{},
};
MODULE_DEVICE_TABLE(of, samsung_abox_match);

static const struct dev_pm_ops samsung_abox_pm = {
	SET_SYSTEM_SLEEP_PM_OPS(abox_suspend, abox_resume)
	SET_RUNTIME_PM_OPS(abox_runtime_suspend, abox_runtime_resume, NULL)
};

static struct platform_driver samsung_abox_driver = {
	.probe  = samsung_abox_probe,
	.remove = samsung_abox_remove,
	.shutdown = samsung_abox_shutdown,
	.driver = {
		.name = "samsung-abox",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(samsung_abox_match),
		.pm = &samsung_abox_pm,
	},
};

module_platform_driver(samsung_abox_driver);

static int __init samsung_abox_late_initcall(void)
{
	pr_info("%s\n", __func__);

	if (p_abox_data && p_abox_data->pdev) {
		if (!abox_test_quirk(p_abox_data, ABOX_QUIRK_OFF_ON_SUSPEND))
			pm_runtime_put(&p_abox_data->pdev->dev);
	} else {
		pr_err("%s: p_abox_data or pdev is null", __func__);
	}

	return 0;
}
late_initcall(samsung_abox_late_initcall);

/* Module information */
MODULE_AUTHOR("Gyeongtaek Lee, <gt82.lee@samsung.com>");
MODULE_DESCRIPTION("Samsung ASoC A-Box Driver");
MODULE_ALIAS("platform:samsung-abox");
MODULE_LICENSE("GPL");
