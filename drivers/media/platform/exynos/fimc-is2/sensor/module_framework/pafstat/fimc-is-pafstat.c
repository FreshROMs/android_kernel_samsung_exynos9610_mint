/*
 * Samsung Exynos SoC series PAFSTAT driver
 *
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/of.h>

#include <media/v4l2-subdev.h>

#include "fimc-is-config.h"
#include "fimc-is-pafstat.h"
#include "fimc-is-hw-pafstat.h"
#include "fimc-is-interface-library.h"

static struct fimc_is_pafstat pafstat_devices[MAX_NUM_OF_PAFSTAT];
atomic_t	g_pafstat_rsccount;

static void prepare_pafstat_sfr_dump(struct fimc_is_pafstat *pafstat)
{
	int reg_size = 0;

	if (IS_ERR_OR_NULL(pafstat->regs_b) ||
		(pafstat->regs_b_start == 0) ||
		(pafstat->regs_b_end == 0)) {
		warn("[PAFSTAT:%d]reg iomem is invalid", pafstat->id);
		return;
	}

	/* alloc sfr dump memory */
	reg_size = (pafstat->regs_end - pafstat->regs_start + 1);
	pafstat->sfr_dump = kzalloc(reg_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(pafstat->sfr_dump))
		err("[PAFSTAT:%d]sfr dump memory alloc fail", pafstat->id);
	else
		info("[PAFSTAT:%d]sfr dump memory (V/P/S):(%p/%p/0x%X)[0x%llX~0x%llX]\n", pafstat->id,
				pafstat->sfr_dump, (void *)virt_to_phys(pafstat->sfr_dump),
				reg_size, pafstat->regs_start, pafstat->regs_end);

	if (IS_ERR_OR_NULL(pafstat->regs_b) ||
		(pafstat->regs_b_start == 0) ||
		(pafstat->regs_b_end == 0))
		return;

	/* alloc sfr B dump memory */
	reg_size = (pafstat->regs_b_end - pafstat->regs_b_start + 1);
	pafstat->sfr_b_dump = kzalloc(reg_size, GFP_KERNEL);
	if (IS_ERR_OR_NULL(pafstat->sfr_b_dump))
		err("[PAFSTAT:%d]sfr B dump memory alloc fail", pafstat->id);
	else
		info("[PAFSTAT:%d]sfr B dump memory (V/P/S):(%p/%p/0x%X)[0x%llX~0x%llX]\n", pafstat->id,
				pafstat->sfr_b_dump, (void *)virt_to_phys(pafstat->sfr_b_dump),
				reg_size, pafstat->regs_b_start, pafstat->regs_b_end);
}

void pafstat_sfr_dump(struct fimc_is_pafstat *pafstat)
{
	int reg_size = 0;

	reg_size = (pafstat->regs_end - pafstat->regs_start + 1);
	memcpy(pafstat->sfr_dump, pafstat->regs, reg_size);
	info("[PAFSTAT:%d]##### SFR DUMP(V/P/S):(%p/%p/0x%X)[0x%llX~0x%llX]\n", pafstat->id,
			pafstat->sfr_dump, (void *)virt_to_phys(pafstat->sfr_dump),
			reg_size, pafstat->regs_start, pafstat->regs_end);
#ifdef ENABLE_PANIC_SFR_PRINT
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 32, 4,
			pafstat->regs, reg_size, false);
#endif

	/* dump reg B */
	reg_size = (pafstat->regs_b_end - pafstat->regs_b_start + 1);
	memcpy(pafstat->sfr_b_dump, pafstat->regs_b, reg_size);

	info("[PAFSTAT:%d]##### SFR B DUMP(V/P/S):(%p/%p/0x%X)[0x%llX~0x%llX]\n", pafstat->id,
			pafstat->sfr_b_dump, (void *)virt_to_phys(pafstat->sfr_b_dump),
			reg_size, pafstat->regs_b_start, pafstat->regs_b_end);
#ifdef ENABLE_PANIC_SFR_PRINT
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 32, 4,
			pafstat->regs_b, reg_size, false);
#endif
}

static irqreturn_t fimc_is_isr_pafstat(int irq, void *data)
{
	struct fimc_is_pafstat *pafstat;
	u32 irq_src, irq_mask, status;
	bool err_intr_flag = false;

	pafstat = data;
	if (pafstat == NULL)
		return IRQ_NONE;

	irq_src = pafstat_hw_g_irq_src(pafstat->regs);
	irq_mask = pafstat_hw_g_irq_mask(pafstat->regs);
	status = (~irq_mask) & irq_src;

	pafstat_hw_s_irq_src(pafstat->regs, status);

	if (status & (1 << PAFSTAT_INT_FRAME_START)) {
		atomic_set(&pafstat->Vvalid, V_VALID);

		if (atomic_read(&pafstat->sfr_state) == PAFSTAT_SFR_READY)
			atomic_set(&pafstat->sfr_state, PAFSTAT_SFR_APPLIED);

		atomic_inc(&pafstat->fs);
		dbg_isr("[%d][F:%d] F.S (0x%x)", pafstat, pafstat->id, atomic_read(&pafstat->fs), status);
		atomic_add(pafstat->fro_cnt, &pafstat->fs);
	}

	if (status & (1 << PAFSTAT_INT_TIMEOUT)) {
		err("[PAFSTAT:%d] TIMEOUT (0x%x)", pafstat->id, status);
	}

	if (status & (1 << PAFSTAT_INT_BAYER_FRAME_END)) {
		atomic_inc(&pafstat->fe_img);
		dbg_isr("[%d][F:%d] F.E, img (0x%x)", pafstat, pafstat->id, atomic_read(&pafstat->fe_img), status);
		atomic_add(pafstat->fro_cnt, &pafstat->fe_img);
	}

	if (status & (1 << PAFSTAT_INT_STAT_FRAME_END)) {
		atomic_inc(&pafstat->fe_stat);
		dbg_isr("[%d][F:%d] F.E, stat (0x%x)", pafstat, pafstat->id, atomic_read(&pafstat->fe_stat), status);
		atomic_add(pafstat->fro_cnt, &pafstat->fe_stat);
	}

	if (status & (1 << PAFSTAT_INT_TOTAL_FRAME_END)) {
		atomic_inc(&pafstat->fe);
		dbg_isr("[%d][F:%d] F.E (0x%x)", pafstat, pafstat->id, atomic_read(&pafstat->fe), status);
		atomic_add(pafstat->fro_cnt, &pafstat->fe);
		pafstat_hw_s_timeout_cnt_clear(pafstat->regs);
		atomic_set(&pafstat->Vvalid, V_BLANK);
		wake_up(&pafstat->wait_queue);
	}

	if (status & (1 << PAFSTAT_INT_FRAME_LINE)) {
		atomic_inc(&pafstat->cl);
		dbg_isr("[%d][F:%d] LINE INTR (0x%x)", pafstat, pafstat->id, atomic_read(&pafstat->cl), status);
		atomic_add(pafstat->fro_cnt, &pafstat->cl);

		if (atomic_read(&pafstat->sfr_state) == PAFSTAT_SFR_APPLIED)
			tasklet_schedule(&pafstat->tasklet_fwin_stat);
	}

	if (status & (1 << PAFSTAT_INT_FRAME_FAIL)) {
		err("[PAFSTAT:%d] DMA OVERFLOW (0x%x)", pafstat->id, status);
		err_intr_flag = true;
	}

	if (status & (1 << PAFSTAT_INT_LIC_BUFFER_FULL)) {
		err("[PAFSTAT:%d] LIC BUFFER FULL (0x%x)", pafstat->id, status);
		err_intr_flag = true;
		/* TODO: recovery */
#ifdef ENABLE_FULLCHAIN_OVERFLOW_RECOVERY
		fimc_is_hw_overflow_recovery();
#endif
	}
#if 0 /* TODO */
	if (err_intr_flag) {
		pafstat_sfr_dump(pafstat);
		pafstat_hw_com_s_output_mask(pafstat->regs_com, 1);
	}
#endif

	return IRQ_HANDLED;
}

static void pafstat_tasklet_fwin_stat(unsigned long data)
{
	struct fimc_is_pafstat *pafstat;
	struct fimc_is_module_enum *module;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_device_sensor *sensor;
	struct fimc_is_device_csi *csi;
	struct fimc_is_subdev *dma_subdev;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	unsigned int frameptr;
	int ch;
	unsigned long flags;
	void __iomem *curr_regs;
	char *curr_set;

	pafstat = (struct fimc_is_pafstat *)data;
	if (!pafstat) {
		err("failed to get PAFSTAT");
		return;
	}

	if (pafstat_hw_g_current(pafstat->regs) == PAFSTAT_SEL_SET_A) {
		curr_regs = pafstat->regs;
		curr_set = "A";
	} else {
		curr_regs = pafstat->regs_b;
		curr_set = "B";
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdev_hostdata(pafstat->subdev);
	if (!module) {
		err("failed to get module");
		return;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		err("module's subdev was not probed");
		return;
	}

	sensor = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev_module);
	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);

	for (ch = CSI_VIRTUAL_CH_1; ch < CSI_VIRTUAL_CH_MAX; ch++) {
		if (sensor->cfg->output[ch].type == VC_PRIVATE) {
			dma_subdev = csi->dma_subdev[ch];
			if (!dma_subdev ||
				!test_bit(FIMC_IS_SUBDEV_START, &dma_subdev->state))
				continue;

			framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
			if (!framemgr) {
				err("failed to get framemgr");
				continue;
			}

			framemgr_e_barrier_irqs(framemgr, FMGR_IDX_29, flags);

			frameptr = atomic_read(&pafstat->frameptr_fwin_stat) % framemgr->num_frames;
			frame = &framemgr->frames[frameptr];
			frame->fcount = sensor->fcount;

			pafstat_hw_g_fwin_stat(curr_regs, (void *)frame->kvaddr_buffer[0],
				dma_subdev->output.width * dma_subdev->output.height);

			atomic_inc(&pafstat->frameptr_fwin_stat);

			framemgr_x_barrier_irqr(framemgr, FMGR_IDX_29, flags);
		}
	}

	if (pafstat->wq_fwin_stat)
		queue_work(pafstat->wq_fwin_stat, &pafstat->work_fwin_stat);
	else
		schedule_work(&pafstat->work_fwin_stat);

	dbg_pafstat(1, "%s, sensor fcount: %d, SFR curr(%s:%p)\n", __func__, sensor->fcount,
		curr_set, curr_regs);
}

static void pafstat_worker_fwin_stat(struct work_struct *work)
{
	struct fimc_is_pafstat *pafstat;
	struct fimc_is_module_enum *module;
	struct v4l2_subdev *subdev_module;
	struct fimc_is_device_sensor *sensor;
	struct paf_action *pa, *temp;
	unsigned long flag;

	pafstat = container_of(work, struct fimc_is_pafstat, work_fwin_stat);
	module = (struct fimc_is_module_enum *)v4l2_get_subdev_hostdata(pafstat->subdev);
	if (!module) {
		err("failed to get module");
		return;
	}

	subdev_module = module->subdev;
	if (!subdev_module) {
		err("module's subdev was not probed");
		return;
	}

	sensor = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(subdev_module);

	spin_lock_irqsave(&pafstat->slock_paf_action, flag);
	list_for_each_entry_safe(pa, temp, &pafstat->list_of_paf_action, list) {
		switch (pa->type) {
		case VC_STAT_TYPE_PAFSTAT_FLOATING:
#ifdef ENABLE_FPSIMD_FOR_USER
			fpsimd_get();
			pa->notifier(pa->type, *(unsigned int *)&sensor->fcount, pa->data);
			fpsimd_put();
#else
			pa->notifier(pa->type, *(unsigned int *)&sensor->fcount, pa->data);
#endif
			break;			
		default:
			break;
		}
	}
	spin_unlock_irqrestore(&pafstat->slock_paf_action, flag);

	dbg_pafstat(1, "%s, sensor fcount: %d\n", __func__, sensor->fcount);
}

int pafstat_set_num_buffers(struct v4l2_subdev *subdev, u32 num_buffers, struct fimc_is_device_sensor *sensor)
{
	struct fimc_is_pafstat *pafstat;
	struct fimc_is_device_csi *csi;
	u32 __iomem *base_reg;
	u32 csi_pixel_mode;
	int ret = 0;

	pafstat = v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("A subdev data of PAFSTAT is null");
		return -ENODEV;
	}

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
	FIMC_BUG(!csi);

	pafstat->fro_cnt = (num_buffers > 8 ? 7 : num_buffers - 1);
	pafstat_hw_com_s_fro(pafstat->regs_com, pafstat->fro_cnt);

	info("[PAFSTAT:%d] fro_cnt(%d,%d)\n", pafstat->id, pafstat->fro_cnt, num_buffers);

	base_reg = csi->base_reg;
	csi_pixel_mode = csi_hw_get_ppc_mode(base_reg);

	ret = pafstat_hw_s_4ppc(pafstat->regs, csi_pixel_mode);
	if (ret)
		err("pafstat ppc set error!\n");

	ret = pafstat_hw_s_4ppc(pafstat->regs_b, csi_pixel_mode);
	if (ret)
		err("pafstat ppc set error!\n");

	return ret;
}

int pafstat_hw_set_regs(struct v4l2_subdev *subdev,
		struct paf_setting_t *regs, u32 regs_cnt)
{
	int i;
	struct fimc_is_pafstat *pafstat;
	struct fimc_is_module_enum *module;
	int med_line;
	int sensor_mode;
	int distance_pd_pixel;

	pafstat = v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("A subdev data of PAFSTAT is null");
		return -ENODEV;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdev_hostdata(subdev);
	if (!module) {
		err("failed to get module");
		return -ENODEV;
	}

	sensor_mode = module->vc_extra_info[VC_BUF_DATA_TYPE_GENERAL_STAT1].sensor_mode;
	/* if distance_pd_pixel is 16 : 1 pd pixel per 16 bayer_line */
	switch (sensor_mode) {
	case VC_SENSOR_MODE_MSPD_TAIL:
	case VC_SENSOR_MODE_MSPD_GLOBAL_TAIL:
		distance_pd_pixel = 16;
		break;
	case VC_SENSOR_MODE_ULTRA_PD_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_TAIL:
		distance_pd_pixel = 8;
		break;
	case VC_SENSOR_MODE_IMX_2X1OCL_1_TAIL:
	case VC_SENSOR_MODE_IMX_2X1OCL_2_TAIL:
	case VC_SENSOR_MODE_SUPER_PD_2_TAIL:	
		distance_pd_pixel = 2;
		break;
	default:
		err("check for sensor pd mode\n");
		distance_pd_pixel = 16;
		break;
	}

	if (atomic_read(&pafstat->sfr_state) == PAFSTAT_SFR_INIT) {
		for (i = 0; i < regs_cnt; i++) {
			dbg_pafstat(2, "[%d] first ofs: 0x%x, val: 0x%x\n",
					i, regs[i].reg_addr, regs[i].reg_data);
			writel(regs[i].reg_data, pafstat->regs + regs[i].reg_addr);
			writel(regs[i].reg_data, pafstat->regs_b + regs[i].reg_addr);
		}

		pafstat_hw_com_s_med_line(pafstat->regs, distance_pd_pixel);
		med_line = pafstat_hw_com_s_med_line(pafstat->regs_b, distance_pd_pixel);
	} else {
		void __iomem *next_regs;
		char *next_set;

		if (pafstat_hw_g_current(pafstat->regs) == PAFSTAT_SEL_SET_A) {
			next_regs = pafstat->regs_b;
			next_set = "B";
		} else {
			next_regs = pafstat->regs;
			next_set = "A";
		}

		dbg_pafstat(1, "PAFSTAT SFR next(%s:%p) setting\n",
			next_set, next_regs);

		for (i = 0; i < regs_cnt; i++) {
			dbg_pafstat(2, "[%d] ofs: 0x%x, val: 0x%x\n",
					i, regs[i].reg_addr, regs[i].reg_data);
			writel(regs[i].reg_data, next_regs + regs[i].reg_addr);
		}

		med_line = pafstat_hw_com_s_med_line(next_regs, distance_pd_pixel);

		pafstat_hw_s_ready(next_regs, 1);
	}

	dbg_pafstat(1, "SensorPD mode(%d), distance_pd(%d), MED LINE_NUM(%d)\n",
		sensor_mode, distance_pd_pixel, med_line);

	atomic_set(&pafstat->sfr_state, PAFSTAT_SFR_READY);

	return 0;
}

int pafstat_hw_get_ready(struct v4l2_subdev *subdev, u32 *ready)
{
	struct fimc_is_pafstat *pafstat;

	pafstat = v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("A subdev data of PAFSTAT is null");
		return -ENODEV;
	}

	*ready = (u32)atomic_read(&pafstat->sfr_state);

	return 0;
}

int pafstat_register_notifier(struct v4l2_subdev *subdev, enum itf_vc_stat_type type,
		vc_dma_notifier_t notifier, void *data)
{
	struct fimc_is_pafstat *pafstat;
	struct paf_action *pa;
	unsigned long flag;

	pafstat = (struct fimc_is_pafstat *)v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("%s, failed to get PDP", __func__);
		return -ENODEV;
	}

	switch (type) {
	case VC_STAT_TYPE_PAFSTAT_FLOATING:
	case VC_STAT_TYPE_PAFSTAT_STATIC:
		pa = kzalloc(sizeof(struct paf_action), GFP_ATOMIC);
		if (!pa) {
			err_lib("failed to allocate a PAF action");
			return -ENOMEM;
		}

		pa->type = type;
		pa->notifier = notifier;
		pa->data = data;

		spin_lock_irqsave(&pafstat->slock_paf_action, flag);
		list_add(&pa->list, &pafstat->list_of_paf_action);
		spin_unlock_irqrestore(&pafstat->slock_paf_action, flag);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* TODO: below version removes all notifiers have been registered with same stat. type */
int pafstat_unregister_notifier(struct v4l2_subdev *subdev, enum itf_vc_stat_type type,
		vc_dma_notifier_t notifier)
{
	struct fimc_is_pafstat *pafstat;
	struct paf_action *pa, *temp;
	unsigned long flag;

	pafstat = (struct fimc_is_pafstat *)v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("%s, failed to get PAFSTAT", __func__);
		return -ENODEV;
	}

	switch (type) {
	case VC_STAT_TYPE_PAFSTAT_FLOATING:
	case VC_STAT_TYPE_PAFSTAT_STATIC:
		spin_lock_irqsave(&pafstat->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp,
				&pafstat->list_of_paf_action, list) {
			if (pa->type == type) {
				list_del(&pa->list);
				kfree(pa);
			}
		}
		spin_unlock_irqrestore(&pafstat->slock_paf_action, flag);

		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void pafstat_notify(struct v4l2_subdev *subdev, unsigned int type, void *data)
{
	struct fimc_is_pafstat *pafstat;
	struct paf_action *pa, *temp;
	unsigned long flag;

	pafstat = (struct fimc_is_pafstat *)v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("%s, failed to get PAFSTAT", __func__);
		return;
	}

	switch (type) {
	case CSIS_NOTIFY_DMA_END_VC_MIPISTAT:
		spin_lock_irqsave(&pafstat->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp, &pafstat->list_of_paf_action, list) {
			switch (pa->type) {
			case VC_STAT_TYPE_PAFSTAT_STATIC:
#ifdef ENABLE_FPSIMD_FOR_USER
				fpsimd_get();
				pa->notifier(pa->type, *(unsigned int *)data, pa->data);
				fpsimd_put();
#else
				pa->notifier(pa->type, *(unsigned int *)&sensor->fcount, pa->data);
#endif
				break;
			default:
				break;
			}
			dbg_pafstat(1, "%s, sensor fcount: %d\n", __func__, *(unsigned int *)data);
		}
		spin_unlock_irqrestore(&pafstat->slock_paf_action, flag);

	default:
		break;
	}
}

int pafstat_register(struct fimc_is_module_enum *module, int pafstat_ch)
{
	int ret = 0;
	struct fimc_is_pafstat *pafstat;
	struct fimc_is_device_sensor_peri *sensor_peri = module->private_data;
	u32 version = 0;

	if (test_bit(FIMC_IS_SENSOR_PAFSTAT_AVAILABLE, &sensor_peri->peri_state)) {
		err("already registered");
		ret = -EINVAL;
		goto p_err;
	}

	if (pafstat_ch >= MAX_NUM_OF_PAFSTAT) {
		err("A pafstat channel is invalide");
		ret = -EINVAL;
		goto p_err;
	}

	pafstat = &pafstat_devices[pafstat_ch];
	sensor_peri->pafstat = pafstat;
	sensor_peri->subdev_pafstat = pafstat->subdev;
	v4l2_set_subdev_hostdata(pafstat->subdev, module);
	spin_lock_init(&pafstat->slock_paf_action);
	INIT_LIST_HEAD(&pafstat->list_of_paf_action);

	atomic_set(&pafstat->sfr_state, PAFSTAT_SFR_INIT);
	atomic_set(&pafstat->fs, 0);
	atomic_set(&pafstat->cl, 0);
	atomic_set(&pafstat->fe, 0);
	atomic_set(&pafstat->fe_img, 0);
	atomic_set(&pafstat->fe_stat, 0);
	atomic_set(&pafstat->Vvalid, V_BLANK);
	init_waitqueue_head(&pafstat->wait_queue);

	pafstat->regs_com = pafstat_devices[0].regs;
	if (!atomic_read(&g_pafstat_rsccount)) {
		info("[PAFSTAT:%d] %s: hw_com_init()\n", pafstat->id, __func__);
		pafstat_hw_com_init(pafstat->regs_com);
	}
	atomic_inc(&g_pafstat_rsccount);
	info("[PAFSTAT:%d] %s: pafstat_hw_s_init(rsccount:%d)\n",
			pafstat->id, __func__, (u32)atomic_read(&g_pafstat_rsccount));

	set_bit(FIMC_IS_SENSOR_PAFSTAT_AVAILABLE, &sensor_peri->peri_state);
	version = pafstat_hw_com_g_version(pafstat->regs_com);
	info("[PAFSTAT:%d] %s: (HW_VER:0x%x)\n", pafstat->id, __func__, version);

	return ret;
p_err:
	return ret;
}

int pafstat_unregister(struct fimc_is_module_enum *module)
{
	int ret = 0;
	struct fimc_is_device_sensor_peri *sensor_peri = module->private_data;
	struct fimc_is_pafstat *pafstat;
	struct paf_action *pa, *temp;
	unsigned long flag;

	if (!test_bit(FIMC_IS_SENSOR_PAFSTAT_AVAILABLE, &sensor_peri->peri_state)) {
		err("already unregistered");
		ret = -EINVAL;
		goto p_err;
	}

	pafstat = v4l2_get_subdevdata(sensor_peri->subdev_pafstat);
	if (!pafstat) {
		err("A subdev data of PAFSTAT is null");
		ret = -ENODEV;
		goto p_err;
	}

	if (!list_empty(&pafstat->list_of_paf_action)) {
		err("flush remaining notifiers...");
		spin_lock_irqsave(&pafstat->slock_paf_action, flag);
		list_for_each_entry_safe(pa, temp,
				&pafstat->list_of_paf_action, list) {
			list_del(&pa->list);
			kfree(pa);
		}
		spin_unlock_irqrestore(&pafstat->slock_paf_action, flag);
	}

	sensor_peri->pafstat = NULL;
	sensor_peri->subdev_pafstat = NULL;
	pafstat_hw_s_enable(pafstat->regs, 0);

	clear_bit(FIMC_IS_SENSOR_PAFSTAT_AVAILABLE, &sensor_peri->peri_state);
	atomic_dec(&g_pafstat_rsccount);

	info("[PAFSTAT:%d] %s(ret:%d)\n", pafstat->id, __func__, ret);
	return ret;
p_err:
	return ret;
}

int pafstat_init(struct v4l2_subdev *subdev, u32 val)
{
	return 0;
}

static void _pafstat_wait_streamoff(struct fimc_is_pafstat *pafstat)
{
	uint8_t retry = 0;

	while (atomic_read(&pafstat->Vvalid) != V_BLANK
		&& retry++ < PAFSTAT_STREAMOFF_RETRY_COUNT) {
		usleep_range(PAFSTAT_STREAMOFF_WAIT_TIME, (PAFSTAT_STREAMOFF_WAIT_TIME + 100));
	}

	if (atomic_read(&pafstat->Vvalid) == V_BLANK)
		info("[PAFSTAT:%d] %s: done. retry(%d)\n", pafstat->id, __func__, retry);
	else
		warn("[PAFSTAT:%d] %s: fail.\n", pafstat->id, __func__);
}

static int pafstat_s_stream(struct v4l2_subdev *subdev, int enable)
{
	struct fimc_is_pafstat *pafstat;
	cis_shared_data *cis_data = NULL;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor_peri *sensor_peri;

	pafstat = v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("A subdev data of PAFSTAT is null");
		return -ENODEV;
	}

	module = (struct fimc_is_module_enum *)v4l2_get_subdev_hostdata(subdev);
	if (!module) {
		err("[PAFSTAT:%d] A host data of PAFSTAT is null", pafstat->id);
		return -ENODEV;
	}

	sensor_peri = module->private_data;
	WARN_ON(!sensor_peri);

	cis_data = sensor_peri->cis.cis_data;
	WARN_ON(!cis_data);

	if (cis_data->is_data.paf_stat_enable && enable) {
		tasklet_init(&pafstat->tasklet_fwin_stat, pafstat_tasklet_fwin_stat, (unsigned long)pafstat);
		atomic_set(&pafstat->frameptr_fwin_stat, 0);
		INIT_WORK(&pafstat->work_fwin_stat, pafstat_worker_fwin_stat);
		pafstat_hw_s_enable(pafstat->regs, enable);
	} else {
		pafstat_hw_s_enable(pafstat->regs, enable);
		_pafstat_wait_streamoff(pafstat);

		tasklet_kill(&pafstat->tasklet_fwin_stat);
		if (flush_work(&pafstat->work_fwin_stat))
			info("flush pafstat wq for fwin stat\n");
	}

	info("[PAFSTAT:%d] CORE_EN:%d\n", pafstat->id, enable);

	return 0;
}

static int pafstat_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	return 0;
}

static int pafstat_s_format(struct v4l2_subdev *subdev,
	struct v4l2_subdev_pad_config *cfg,
	struct v4l2_subdev_format *fmt)
{
	int ret = 0;
	size_t width, height;
	int irq_state = 0;
	int pd_enable = 0;
	int sensor_mode = VC_BUF_DATA_TYPE_INVALID;
	u32 lic_mode;
	int pd_mode = PD_NONE;
	enum pafstat_input_path input = PAFSTAT_INPUT_OTF;
	struct fimc_is_pafstat *pafstat;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *sensor = NULL;
	struct fimc_is_device_sensor_peri *sensor_peri;
	cis_shared_data *cis_data = NULL;

	pafstat = v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("A subdev data of PAFSTAT is null");
		ret = -ENODEV;
		goto p_err;
	}

	width = fmt->format.width;
	height = fmt->format.height;
	pafstat->in_width = width;
	pafstat->in_height = height;
	pafstat_hw_s_img_size(pafstat->regs, pafstat->in_width, pafstat->in_height);
	pafstat_hw_s_img_size(pafstat->regs_b, pafstat->in_width, pafstat->in_height);

	lic_mode = (pafstat->fro_cnt == 0 ? LIC_MODE_INTERLEAVING : LIC_MODE_SINGLE_BUFFER);
	pafstat_hw_com_s_lic_mode(pafstat->regs_com, pafstat->id, lic_mode, input);
	pafstat_hw_com_s_output_mask(pafstat->regs_com, 0);
	pafstat_hw_s_input_path(pafstat->regs, input);
	pafstat_hw_s_input_path(pafstat->regs_b, input);

	module = (struct fimc_is_module_enum *)v4l2_get_subdev_hostdata(subdev);
	if (!module) {
		err("[PAFSTAT:%d] A host data of PAFSTAT is null", pafstat->id);
		ret = -ENODEV;
		goto p_err;
	}

	sensor = (struct fimc_is_device_sensor *)v4l2_get_subdev_hostdata(module->subdev);
	if (!sensor) {
		err("device_sensor is null");
		ret = -ENODEV;
		goto p_err;
	}

	sensor_peri = module->private_data;
	WARN_ON(!sensor_peri);

	cis_data = sensor_peri->cis.cis_data;
	WARN_ON(!cis_data);

	if (sensor->cfg) {
		pafstat->pd_width = sensor->cfg->input[CSI_VIRTUAL_CH_1].width;
		pafstat->pd_height = sensor->cfg->input[CSI_VIRTUAL_CH_1].height;

		sensor_mode = module->vc_extra_info[VC_BUF_DATA_TYPE_GENERAL_STAT1].sensor_mode;
		if (sensor_mode == VC_SENSOR_MODE_IMX_2X1OCL_1_TAIL
			|| sensor_mode == VC_SENSOR_MODE_SUPER_PD_2_TAIL)
			pafstat->pd_height /= 2;
		else if (sensor_mode == VC_SENSOR_MODE_IMX_2X1OCL_2_TAIL)
			pafstat->pd_width /= 2;

		pafstat_hw_s_pd_size(pafstat->regs, pafstat->pd_width, pafstat->pd_height);
		pafstat_hw_s_pd_size(pafstat->regs_b, pafstat->pd_width, pafstat->pd_height);
		pd_mode = sensor->cfg->pd_mode;
	}

	pd_enable = pafstat_hw_s_sensor_mode(pafstat->regs, pd_mode);
	cis_data->is_data.paf_stat_enable = pd_enable;

	pafstat_hw_s_lbctrl(pafstat->regs,
			pafstat->pd_width, pafstat->pd_height);
	pafstat_hw_s_lbctrl(pafstat->regs_b,
			pafstat->pd_width, pafstat->pd_height);

	pafstat_hw_s_irq_mask(pafstat->regs, PAFSTAT_INT_MASK);
	irq_state = pafstat_hw_g_irq_src(pafstat->regs);

	pafstat_hw_s_ready(pafstat->regs, 1);

	info("[PAFSTAT:%d] PD_MODE:%d, PD_ENABLE:%d, IRQ:0x%x, IRQ_MASK:0x%x, LIC_MODE(%s)\n",
		pafstat->id, pd_mode, pd_enable, irq_state, PAFSTAT_INT_MASK,
		lic_mode == LIC_MODE_INTERLEAVING ? "INTERLEAVING" : "SINGLE_BUFFER");

	info("[PAFSTAT:%d] %s: image_size(%lux%lu) pd_size(%lux%lu) ret(%d)\n", pafstat->id, __func__,
		width, height, pafstat->pd_width, pafstat->pd_height, ret);

p_err:
	return ret;
}

int fimc_is_pafstat_reset_recovery(struct v4l2_subdev *subdev, u32 reset_mode, int pd_mode)
{
	int ret = 0;
	struct fimc_is_pafstat *pafstat;
	struct v4l2_subdev_pad_config *cfg = NULL;
	struct v4l2_subdev_format *fmt = NULL;

	pafstat = v4l2_get_subdevdata(subdev);
	if (!pafstat) {
		err("A subdev data of PAFSTAT is null");
		return -EINVAL;
	}

	if (reset_mode == 0) {	/* reset */
		pafstat_hw_com_s_output_mask(pafstat->regs_com, 1);
		pafstat_hw_sw_reset(pafstat->regs);
        atomic_set(&pafstat->sfr_state, PAFSTAT_SFR_INIT);
	} else {
		pafstat_s_format(subdev, cfg, fmt);
		pafstat_s_stream(subdev, 1);
		pafstat_hw_com_s_output_mask(pafstat->regs_com, 0);
	}

	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = pafstat_init
};

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = pafstat_s_stream,
	.s_parm = pafstat_s_param,
};

static const struct v4l2_subdev_pad_ops pad_ops = {
	.set_fmt = pafstat_s_format
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops,
	.pad = &pad_ops
};

struct fimc_is_pafstat_ops pafstat_ops = {
	.set_param = pafstat_hw_set_regs,
	.get_ready = pafstat_hw_get_ready,
	.register_notifier = pafstat_register_notifier,
	.unregister_notifier = pafstat_unregister_notifier,
	.notify = pafstat_notify,
	.set_num_buffers = pafstat_set_num_buffers,
};

static int __init pafstat_probe(struct platform_device *pdev)
{
	int ret = 0;
	int id = -1;
	struct resource *res, *res_b;
	struct fimc_is_pafstat *pafstat;
	struct device *dev = &pdev->dev;
	u32 reg_cnt = 0;

	WARN_ON(!fimc_is_dev);
	WARN_ON(!pdev);
	WARN_ON(!pdev->dev.of_node);

	id = of_alias_get_id(dev->of_node, "pafstat");
	if (id < 0 || id >= MAX_NUM_OF_PAFSTAT) {
		dev_err(dev, "invalid id (out-of-range)\n");
		return -EINVAL;
	}

	pafstat = &pafstat_devices[id];
	pafstat->id = id;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "can't get memory resource\n");
		return -ENODEV;
	}

	if (!devm_request_mem_region(dev, res->start, resource_size(res),
				dev_name(dev))) {
		dev_err(dev, "can't request region for resource %p\n", res);
		return -EBUSY;
	}

	pafstat->regs_start = res->start;
	pafstat->regs_end = res->end;
	pafstat->regs = devm_ioremap_nocache(dev, res->start, resource_size(res));
	if (!pafstat->regs) {
		dev_err(dev, "ioremap failed\n");
		ret = -ENOMEM;
		goto err_ioremap;
	}

	res_b = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res_b) {
		dev_err(dev, "can't get memory resource\n");
		ret = -ENODEV;
		goto err_get_rsc_b;
	}

	if (!devm_request_mem_region(dev, res_b->start, resource_size(res_b),
				dev_name(dev))) {
		dev_err(dev, "can't request region for resource %p\n", res_b);
		ret = -EBUSY;
		goto err_get_rsc_b;
	}

	pafstat->regs_b_start = res_b->start;
	pafstat->regs_b_end = res_b->end;
	pafstat->regs_b = devm_ioremap_nocache(dev, res_b->start, resource_size(res_b));
	if (!pafstat->regs_b) {
		dev_err(dev, "ioremap failed(reg_b)\n");
		ret = -ENOMEM;
		goto err_ioremap_b;
	}

	pafstat->irq = platform_get_irq(pdev, 0);
	if (pafstat->irq < 0) {
		dev_err(dev, "failed to get IRQ resource: %d\n", pafstat->irq);
		ret = pafstat->irq;
		goto err_irq;
	}

	ret = devm_request_irq(dev, pafstat->irq,
			fimc_is_isr_pafstat,
			FIMC_IS_HW_IRQ_FLAG | IRQF_SHARED,
			dev_name(dev),
			pafstat);
	if (ret) {
		dev_err(dev, "failed to request IRQ(%d): %d\n", pafstat->irq, ret);
		goto err_irq;
	}

	pafstat->subdev = devm_kzalloc(&pdev->dev, sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!pafstat->subdev) {
		probe_err("failed to alloc memory for pafstat-subdev\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	snprintf(pafstat->name, FIMC_IS_STR_LEN, "PAFSTAT%d", pafstat->id);
	reg_cnt = pafstat_hw_g_reg_cnt();
	pafstat->regs_set = devm_kzalloc(&pdev->dev, reg_cnt * sizeof(struct paf_setting_t), GFP_KERNEL);
	if (!pafstat->regs_set) {
		probe_err("pafstat->reg_set NULL");
		ret = -ENOMEM;
		goto err_alloc;
	}

	v4l2_subdev_init(pafstat->subdev, &subdev_ops);

	v4l2_set_subdevdata(pafstat->subdev, pafstat);
	snprintf(pafstat->subdev->name, V4L2_SUBDEV_NAME_SIZE, "pafstat-subdev.%d", pafstat->id);

	pafstat->pafstat_ops = &pafstat_ops;

	prepare_pafstat_sfr_dump(pafstat);
	atomic_set(&g_pafstat_rsccount, 0);
    atomic_set(&pafstat->sfr_state, PAFSTAT_SFR_INIT);

	platform_set_drvdata(pdev, pafstat);
	probe_info("%s(%s)\n", __func__, dev_name(&pdev->dev));
	return ret;

err_alloc:
	devm_free_irq(dev, pafstat->irq, pafstat);
err_irq:
	devm_iounmap(dev, pafstat->regs_b);
err_ioremap_b:
	devm_release_mem_region(dev, res_b->start, resource_size(res_b));
err_get_rsc_b:
	devm_iounmap(dev, pafstat->regs);
err_ioremap:
	devm_release_mem_region(dev, res->start, resource_size(res));

	return ret;
}

static const struct of_device_id sensor_paf_pafstat_match[] = {
	{
		.compatible = "samsung,sensor-paf-pafstat",
	},
	{},
};
MODULE_DEVICE_TABLE(of, sensor_paf_pafstat_match);

static struct platform_driver sensor_paf_pafstat_platform_driver = {
	.driver = {
		.name   = "Sensor-PAF-PAFSTAT",
		.owner  = THIS_MODULE,
		.of_match_table = sensor_paf_pafstat_match,
	}
};

static int __init sensor_paf_pafstat_init(void)
{
	int ret;

	ret = platform_driver_probe(&sensor_paf_pafstat_platform_driver, pafstat_probe);
	if (ret)
		err("failed to probe %s driver: %d\n",
			sensor_paf_pafstat_platform_driver.driver.name, ret);

	return ret;
}
late_initcall_sync(sensor_paf_pafstat_init);
