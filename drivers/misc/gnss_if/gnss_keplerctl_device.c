/*
 * Copyright (C) 2010 Samsung Electronics.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/mcu_ipc.h>
#include <linux/pinctrl/consumer.h>

#include <asm/cacheflush.h>

#include "gnss_prj.h"
#include "gnss_link_device_shmem.h"
#include "pmu-gnss.h"

#define BCMD_WAKELOCK_TIMEOUT	(HZ / 10) /* 100 msec */
#define REQ_BCMD_TIMEOUT	(200) /* ms */
#define REQ_INIT_TIMEOUT	(1000) /* ms */

static void gnss_state_changed(struct gnss_ctl *gc, enum gnss_state state)
{
	struct io_device *iod = gc->iod;
	int old_state = gc->gnss_state;

	if (old_state != state) {
		gc->gnss_state = state;
		gif_err("%s state changed (%s -> %s)\n", gc->name,
			get_gnss_state_str(old_state), get_gnss_state_str(state));
	}

	if (state == STATE_OFFLINE || state == STATE_FAULT)
		wake_up(&iod->wq);
}

static irqreturn_t kepler_req_init_isr(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;

	gif_err("REQ_INIT Interrupt occurred!\n");

	disable_irq_nosync(gc->req_init_irq);
	complete_all(&gc->req_init_cmpl);

	return IRQ_HANDLED;
}

static irqreturn_t kepler_active_isr(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct io_device *iod = gc->iod;

	gif_err("ACTIVE Interrupt occurred!\n");

	if (!wake_lock_active(&gc->gc_fault_wake_lock))
		wake_lock_timeout(&gc->gc_fault_wake_lock, HZ);

	gnss_state_changed(gc, STATE_FAULT);
	wake_up(&iod->wq);

	gc->pmu_ops->clear_int(GNSS_INT_ACTIVE_CLEAR);

	return IRQ_HANDLED;
}

static irqreturn_t kepler_wdt_isr(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct io_device *iod = gc->iod;

	gif_err("WDT Interrupt occurred!\n");

	if (!wake_lock_active(&gc->gc_fault_wake_lock))
		wake_lock_timeout(&gc->gc_fault_wake_lock, HZ);

	gnss_state_changed(gc, STATE_FAULT);
	wake_up(&iod->wq);

	gc->pmu_ops->clear_int(GNSS_INT_WDT_RESET_CLEAR);
	return IRQ_HANDLED;
}

static irqreturn_t kepler_wakelock_isr(int irq, void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct gnss_mbox *mbx = gc->gnss_data->mbx;
	struct link_device *ld = gc->iod->ld;
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	/*
	u32 rx_tail, rx_head, tx_tail, tx_head, gnss_ipc_msg, ap_ipc_msg;
	*/

#ifdef USE_SIMPLE_WAKE_LOCK
	gif_err("Unexpected interrupt occurred(%s)!!!!\n", __func__);
	return IRQ_HANDLED;
#endif

	/* This is for debugging
	tx_head = get_txq_head(shmd);
	tx_tail = get_txq_tail(shmd);
	rx_head = get_rxq_head(shmd);
	rx_tail = get_rxq_tail(shmd);
	gnss_ipc_msg =  mbox_get_value(MCU_GNSS, shmd->irq_gnss2ap_ipc_msg);
	ap_ipc_msg = read_int2gnss(shmd);

	gif_err("RX_H[0x%x], RX_T[0x%x], TX_H[0x%x], TX_T[0x%x],\
			AP_IPC[0x%x], GNSS_IPC[0x%x]\n",
			rx_head, rx_tail, tx_head, tx_tail, ap_ipc_msg, gnss_ipc_msg);
	*/

	/* Clear wake_lock */
	if (wake_lock_active(&shmd->wlock))
		wake_unlock(&shmd->wlock);

	gif_info("Wake Lock ISR!!!!\n");
	gif_err(">>>>DBUS_SW_WAKE_INT\n");

	/* 1. Set wake-lock-timeout(). */
	if (!wake_lock_active(&gc->gc_wake_lock))
		wake_lock_timeout(&gc->gc_wake_lock, HZ); /* 1 sec */

	/* 2. Disable DBUS_SW_WAKE_INT interrupts. */
	disable_irq_nosync(gc->wake_lock_irq);

	/* 3. Write 0x1 to MBOX_reg[6]. */
	/* MBOX_req[6] is WAKE_LOCK */
	if (gnss_read_reg(shmd, GNSS_REG_WAKE_LOCK) == 0X1) {
		gif_err("@@ reg_wake_lock is already 0x1!!!!!!\n");
		return IRQ_HANDLED;
	} else {
		gnss_write_reg(shmd, GNSS_REG_WAKE_LOCK, 0x1);
	}

	/* 4. Send interrupt MBOX1[3]. */
	/* Interrupt MBOX1[3] is RSP_WAKE_LOCK_SET */
	mbox_set_interrupt(mbx->id, mbx->int_ap2gnss_ack_wake_set);

	return IRQ_HANDLED;
}

static void kepler_irq_bcmd_handler(void *data)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)data;

	/* Signal kepler_req_bcmd */
	complete_all(&gc->bcmd_cmpl);
}

#ifdef USE_SIMPLE_WAKE_LOCK
static void mbox_kepler_simple_lock(void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct gnss_mbox *mbx = gc->gnss_data->mbx;

	gif_info("[GNSS] WAKE interrupt(Mbox15) occurred\n");
	mbox_set_interrupt(mbx->id, mbx->int_ap2gnss_ack_wake_set);
	gc->pmu_ops->clear_int(GNSS_INT_WAKEUP_CLEAR);
}
#endif

static void mbox_kepler_wake_clr(void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;
	struct gnss_mbox *mbx = gc->gnss_data->mbx;
	struct link_device *ld = gc->iod->ld;
	struct shmem_link_device *shmd = to_shmem_link_device(ld);

	/*
	struct link_device *ld = gc->iod->ld;
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	u32 rx_tail, rx_head, tx_tail, tx_head, gnss_ipc_msg, ap_ipc_msg;
	*/
#ifdef USE_SIMPLE_WAKE_LOCK
	gif_err("Unexpected interrupt occurred(%s)!!!!\n", __func__);
	return ;
#endif
	/*
	tx_head = get_txq_head(shmd);
	tx_tail = get_txq_tail(shmd);
	rx_head = get_rxq_head(shmd);
	rx_tail = get_rxq_tail(shmd);
	gnss_ipc_msg = mbox_get_value(MCU_GNSS, shmd->irq_gnss2ap_ipc_msg);
	ap_ipc_msg = read_int2gnss(shmd);

	gif_eff("RX_H[0x%x], RX_T[0x%x], TX_H[0x%x], TX_T[0x%x], AP_IPC[0x%x], GNSS_IPC[0x%x]\n",
			rx_head, rx_tail, tx_head, tx_tail, ap_ipc_msg, gnss_ipc_msg);
	*/
	gc->pmu_ops->clear_int(GNSS_INT_WAKEUP_CLEAR);

	gif_info("Wake Lock Clear!!!!\n");
	gif_err(">>>>DBUS_SW_WAKE_INT CLEAR\n");

	wake_unlock(&gc->gc_wake_lock);
	enable_irq(gc->wake_lock_irq);
	if (gnss_read_reg(shmd, GNSS_REG_WAKE_LOCK) == 0X0) {
		gif_err("@@ reg_wake_lock is already 0x0!!!!!!\n");
		return ;
	}
	gnss_write_reg(shmd, GNSS_REG_WAKE_LOCK, 0x0);
	mbox_set_interrupt(mbx->id, mbx->int_ap2gnss_ack_wake_clr);

}

static void mbox_kepler_rsp_fault_info(void *arg)
{
	struct gnss_ctl *gc = (struct gnss_ctl *)arg;

	complete_all(&gc->fault_cmpl);
}

static DEFINE_MUTEX(reset_lock);

static int kepler_hold_reset(struct gnss_ctl *gc)
{
	gif_err("%s+++\n", __func__);

	if (gc->gnss_state == STATE_OFFLINE) {
		gif_err("Current Kerpler status is OFFLINE, so it will be ignored\n");
		return 0;
	}

	mutex_lock(&reset_lock);

	gnss_state_changed(gc, STATE_HOLD_RESET);

	if (gc->ccore_qch_lh_gnss) {
		clk_disable_unprepare(gc->ccore_qch_lh_gnss);
		gif_err("Disabled GNSS Qch\n");
	}

	gc->pmu_ops->hold_reset();
	mbox_sw_reset(gc->gnss_data->mbx->id);

	mutex_unlock(&reset_lock);

	gif_err("%s---\n", __func__);

	return 0;
}

static int kepler_release_reset(struct gnss_ctl *gc)
{
	int ret;
	unsigned long timeout = msecs_to_jiffies(REQ_INIT_TIMEOUT);

	gif_err("%s+++\n", __func__);

	mcu_ipc_clear_all_interrupt(MCU_GNSS);

	enable_irq(gc->req_init_irq);

	reinit_completion(&gc->req_init_cmpl);

	ret = gc->pmu_ops->release_reset();
	gif_info("pmu_ops release_reset ret : %d \n", ret);
	if (ret) {
		gif_err("failed to pmucal release reset\n");
		return ret;
	}
	gnss_state_changed(gc, STATE_ONLINE);

	if (gc->ccore_qch_lh_gnss) {
		ret = clk_prepare_enable(gc->ccore_qch_lh_gnss);
		if (!ret)
			gif_err("GNSS Qch enabled\n");
		else
			gif_err("Could not enable Qch (%d)\n", ret);
	}

	ret = wait_for_completion_timeout(&gc->req_init_cmpl, timeout);
	if (ret == 0) {
		gif_err("%s: req_init_cmpl TIMEOUT!\n", gc->name);
		disable_irq_nosync(gc->req_init_irq);
		return -EIO;
	}

	mdelay(100);
	
	gc->pmu_ops->check_status();

	ret = gc->pmu_ops->req_security();
	if (ret != 0) {
		gif_err("req_security error! %d\n", ret);
		return ret;
	}
	gc->pmu_ops->req_baaw();

	gif_err("%s---\n", __func__);

	return 0;
}

static int kepler_power_on(struct gnss_ctl *gc)
{
	int ret;
	unsigned long timeout = msecs_to_jiffies(REQ_INIT_TIMEOUT);

	gif_err("%s+++\n", __func__);

	gnss_state_changed(gc, STATE_ONLINE);
	mcu_ipc_clear_all_interrupt(MCU_GNSS);

	reinit_completion(&gc->req_init_cmpl);

	gc->pmu_ops->power_on(GNSS_POWER_ON);

	if (gc->ccore_qch_lh_gnss) {
		ret = clk_prepare_enable(gc->ccore_qch_lh_gnss);
		if (!ret)
			gif_err("GNSS Qch enabled\n");
		else
			gif_err("Could not enable Qch (%d)\n", ret);
	}

	enable_irq(gc->req_init_irq);
	ret = wait_for_completion_timeout(&gc->req_init_cmpl, timeout);
	if (ret == 0) {
		gif_err("%s: req_init_cmpl TIMEOUT!\n", gc->name);
		disable_irq_nosync(gc->req_init_irq);
		return -EIO;
	}
	ret = gc->pmu_ops->req_security();
	if (ret != 0) {
		gif_err("req_security error! %d\n", ret);
		return ret;
	}
	gc->pmu_ops->req_baaw();

	gif_err("%s---\n", __func__);

	return 0;
}

static int kepler_req_fault_info(struct gnss_ctl *gc)
{
	int ret;
	struct gnss_data *pdata;
	struct gnss_mbox *mbx;
	unsigned long timeout = msecs_to_jiffies(1000);
	u32 size = 0;

	mutex_lock(&reset_lock);

	if (!gc) {
		gif_err("No gnss_ctl info!\n");
		ret = -ENODEV;
		goto req_fault_exit;
	}

	pdata = gc->gnss_data;
	mbx = pdata->mbx;

	reinit_completion(&gc->fault_cmpl);

	mbox_set_interrupt(mbx->id, mbx->int_ap2gnss_req_fault_info);

	ret = wait_for_completion_timeout(&gc->fault_cmpl, timeout);
	if (ret == 0) {
		gif_err("Req Fault Info TIMEOUT!\n");
		ret = -EIO;
		goto req_fault_exit;
	}

	switch (pdata->fault_info.device) {
	case GNSS_IPC_MBOX:
		size = pdata->fault_info.size * sizeof(u32);
		ret = size;
		break;
	case GNSS_IPC_SHMEM:
		size = mbox_get_value(mbx->id, mbx->reg_bcmd_ctrl[CTRL3]);
		ret = size;
		break;
	default:
		gif_err("No device for fault dump!\n");
		ret = -ENODEV;
	}

req_fault_exit:
	wake_unlock(&gc->gc_fault_wake_lock);
	mutex_unlock(&reset_lock);

	return ret;
}


static int kepler_suspend(struct gnss_ctl *gc)
{
	return 0;
}

static int kepler_resume(struct gnss_ctl *gc)
{
#ifdef USE_SIMPLE_WAKE_LOCK
	gc->pmu_ops->clear_int(GNSS_INT_WAKEUP_CLEAR);
#endif

	return 0;
}

static int kepler_change_gpio(struct gnss_ctl *gc)
{
	int status = 0;

	gif_err("Change GPIO for sensor\n");
	if (!IS_ERR(gc->gnss_sensor_gpio)) {
		status = pinctrl_select_state(gc->gnss_gpio, gc->gnss_sensor_gpio);
		if (status) {
			gif_err("Can't change sensor GPIO(%d)\n", status);
		}
	} else {
		gif_err("gnss_sensor_gpio is not valid(0x%p)\n", gc->gnss_sensor_gpio);
		status = -EIO;
	}

	return status;
}

static int kepler_set_sensor_power(struct gnss_ctl *gc, enum sensor_power reg_en)
{
	int ret;

	if (reg_en == SENSOR_OFF) {
		ret = regulator_disable(gc->vdd_sensor_reg);
		if (ret != 0)
			gif_err("Failed : Disable sensor power.\n");
		else
			gif_err("Success : Disable sensor power.\n");
	} else {
		ret = regulator_enable(gc->vdd_sensor_reg);
		if (ret != 0)
			gif_err("Failed : Enable sensor power.\n");
		else
			gif_err("Success : Enable sensor power.\n");
	}
	return ret;
}

static int kepler_req_bcmd(struct gnss_ctl *gc, u16 cmd_id, u16 flags,
		u32 param1, u32 param2)
{
	u32 ctrl[BCMD_CTRL_COUNT], ret_val;
	unsigned long timeout = msecs_to_jiffies(REQ_BCMD_TIMEOUT);
	int ret;
	struct gnss_mbox *mbx = gc->gnss_data->mbx;
	struct link_device *ld = gc->iod->ld;

	mutex_lock(&reset_lock);

#if defined(CONFIG_SOC_EXYNOS9610)
	if (gc->gnss_state == STATE_OFFLINE) {
		gif_debug("Set POWER ON on kepler_req_bcmd!!!!\n");
		kepler_power_on(gc);
	} else if (gc->gnss_state == STATE_HOLD_RESET) {
		ld->reset_buffers(ld);
		gif_debug("Set RELEASE RESET on kepler_req_bcmd!!!!\n");
		ret = kepler_release_reset(gc);
		if (ret)
			return ret;
	}
#endif

#ifndef USE_SIMPLE_WAKE_LOCK
	wake_lock_timeout(&gc->gc_bcmd_wake_lock, BCMD_WAKELOCK_TIMEOUT);
#endif

	/* Parse arguments */
	/* Flags: Command flags */
	/* Param1/2 : Paramter 1/2 */

	ctrl[CTRL0] = (flags << 16) + cmd_id;
	ctrl[CTRL1] = param1;
	ctrl[CTRL2] = param2;
	gif_info("%s : set param  0 : 0x%x, 1 : 0x%x, 2 : 0x%x\n",
			__func__, ctrl[CTRL0], ctrl[CTRL1], ctrl[CTRL2]);
	mbox_set_value(mbx->id, mbx->reg_bcmd_ctrl[CTRL0], ctrl[CTRL0]);
	mbox_set_value(mbx->id, mbx->reg_bcmd_ctrl[CTRL1], ctrl[CTRL1]);
	mbox_set_value(mbx->id, mbx->reg_bcmd_ctrl[CTRL2], ctrl[CTRL2]);
	/*
	 * 0xff is MAGIC number to avoid confuging that
	 * register is set from Kepler.
	 */
	mbox_set_value(mbx->id, mbx->reg_bcmd_ctrl[CTRL3], 0xff);

	reinit_completion(&gc->bcmd_cmpl);

	mbox_set_interrupt(mbx->id, mbx->int_ap2gnss_bcmd);

#if !defined(CONFIG_SOC_EXYNOS9610)
	if (gc->gnss_state == STATE_OFFLINE) {
		gif_info("Set POWER ON!!!!\n");
		kepler_power_on(gc);
	} else if (gc->gnss_state == STATE_HOLD_RESET) {
		ld->reset_buffers(ld);
		gif_info("Set RELEASE RESET!!!!\n");
		kepler_release_reset(gc);
	}
#endif

	if (cmd_id == 0x4) { /* BLC_Branch does not have return value */
		mutex_unlock(&reset_lock);
		return 0;
	}

	ret = wait_for_completion_interruptible_timeout(&gc->bcmd_cmpl,
						timeout);
	if (ret == 0) {
#ifndef USE_SIMPLE_WAKE_LOCK
		wake_unlock(&gc->gc_bcmd_wake_lock);
#endif
		gif_err("%s: bcmd TIMEOUT!\n", gc->name);
		mutex_unlock(&reset_lock);
		return -EIO;
	}

	ret_val = mbox_get_value(mbx->id, mbx->reg_bcmd_ctrl[CTRL3]);
	gif_info("BCMD cmd_id 0x%x returned 0x%x\n", cmd_id, ret_val);

	mutex_unlock(&reset_lock);

	return ret_val;
}

static int kepler_pure_release(struct gnss_ctl *gc)
{
	int ret;
	unsigned long timeout = msecs_to_jiffies(REQ_INIT_TIMEOUT);

	gif_err("%s+++\n", __func__);

	gnss_state_changed(gc, STATE_ONLINE);
	mcu_ipc_clear_all_interrupt(MCU_GNSS);

	enable_irq(gc->req_init_irq);

	reinit_completion(&gc->req_init_cmpl);

	gc->pmu_ops->release_reset();

	if (gc->ccore_qch_lh_gnss) {
		ret = clk_prepare_enable(gc->ccore_qch_lh_gnss);
		if (!ret)
			gif_err("GNSS Qch enabled\n");
		else
			gif_err("Could not enable Qch (%d)\n", ret);
	}

	ret = wait_for_completion_timeout(&gc->req_init_cmpl, timeout);
	if (ret == 0) {
		gif_err("%s: req_init_cmpl TIMEOUT!\n", gc->name);
		disable_irq_nosync(gc->req_init_irq);
		return -EIO;
	}

	msleep(100);

	gif_err("%s---\n", __func__);

	return 0;
}

static void gnss_get_ops(struct gnss_ctl *gc)
{
	gc->ops.gnss_hold_reset = kepler_hold_reset;
	gc->ops.gnss_release_reset = kepler_release_reset;
	gc->ops.gnss_power_on = kepler_power_on;
	gc->ops.gnss_req_fault_info = kepler_req_fault_info;
	gc->ops.suspend_gnss_ctrl = kepler_suspend;
	gc->ops.resume_gnss_ctrl = kepler_resume;
	gc->ops.change_sensor_gpio = kepler_change_gpio;
	gc->ops.set_sensor_power = kepler_set_sensor_power;
	gc->ops.req_bcmd = kepler_req_bcmd;
	gc->ops.gnss_pure_release = kepler_pure_release;
}

int init_gnssctl_device(struct gnss_ctl *gc, struct gnss_data *pdata)
{
	int ret = 0, irq = 0;
	struct platform_device *pdev = NULL;
	struct gnss_mbox *mbox = gc->gnss_data->mbx;
	gif_err("[GNSS IF] Initializing GNSS Control\n");

	gnss_get_ops(gc);
	gnss_get_pmu_ops(gc);

	dev_set_drvdata(gc->dev, gc);

	wake_lock_init(&gc->gc_fault_wake_lock,
				WAKE_LOCK_SUSPEND, "gnss_fault_wake_lock");
	wake_lock_init(&gc->gc_wake_lock,
				WAKE_LOCK_SUSPEND, "gnss_wake_lock");

	init_completion(&gc->fault_cmpl);
	init_completion(&gc->bcmd_cmpl);
	init_completion(&gc->req_init_cmpl);

	pdev = to_platform_device(gc->dev);

	/* GNSS_ACTIVE */
	irq = platform_get_irq_byname(pdev, "ACTIVE");
	if (irq < 0)
		irq = platform_get_irq(pdev, 0);
	ret = devm_request_irq(&pdev->dev, irq, kepler_active_isr, 0,
			  "kepler_active_handler", gc);
	if (ret) {
		gif_err("Request irq fail - kepler_active_isr(%d)\n", ret);
		return ret;
	}
	enable_irq_wake(irq);

	/* GNSS_WATCHDOG */
	irq = platform_get_irq_byname(pdev, "WATCHDOG");
	if (irq < 0)
		irq = platform_get_irq(pdev, 1);
	ret = devm_request_irq(&pdev->dev, irq, kepler_wdt_isr, 0,
			  "kepler_wdt_handler", gc);
	if (ret) {
		gif_err("Request irq fail - kepler_wdt_isr(%d)\n", ret);
		return ret;
	}
	enable_irq_wake(irq);

	/* GNSS_WAKEUP */
	irq = platform_get_irq_byname(pdev, "WAKEUP");
	if (irq < 0)
		irq = platform_get_irq(pdev, 2);
	gc->wake_lock_irq = irq;
	ret = devm_request_irq(&pdev->dev, gc->wake_lock_irq, kepler_wakelock_isr,
			0, "kepler_wakelock_handler", gc);

	if (ret) {
		gif_err("Request irq fail - kepler_wakelock_isr(%d)\n", ret);
		return ret;
	}
	enable_irq_wake(irq);
#ifdef USE_SIMPLE_WAKE_LOCK
	disable_irq(gc->wake_lock_irq);

	gif_err("Using simple lock sequence!!!\n");
	mbox_request_irq(gc->gnss_data->mbx->id, 15, mbox_kepler_simple_lock, (void *)gc);

#endif

	/* GNSS2AP */
	irq = platform_get_irq_byname(pdev, "REQ_INIT");
	if (irq < 0)
		irq = platform_get_irq(pdev, 3);
	gc->req_init_irq = irq;
	ret = devm_request_irq(&pdev->dev, gc->req_init_irq, kepler_req_init_isr, 0,
			  "kepler_req_init_handler", gc);
	if (ret) {
		gif_err("Request irq fail - kepler_req_init_isr(%d)\n", ret);
		return ret;
	}
	enable_irq_wake(irq);
	disable_irq(gc->req_init_irq);

	/* Initializing Shared Memory for GNSS */
	gif_err("Initializing shared memory for GNSS.\n");
	gc->pmu_ops->init_conf(gc);
	gc->gnss_state = STATE_OFFLINE;

	gif_info("[GNSS IF] Register mailbox for GNSS2AP fault handling\n");
	mbox_request_irq(mbox->id, mbox->irq_gnss2ap_req_wake_clr,
			 mbox_kepler_wake_clr, (void *)gc);

	mbox_request_irq(mbox->id, mbox->irq_gnss2ap_rsp_fault_info,
			 mbox_kepler_rsp_fault_info, (void *)gc);

	mbox_request_irq(mbox->id, mbox->irq_gnss2ap_bcmd,
			kepler_irq_bcmd_handler, (void *)gc);

	gc->gnss_gpio = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(gc->gnss_gpio)) {
		gif_err("Can't get gpio for GNSS sensor.\n");
	} else {
		gc->gnss_sensor_gpio = pinctrl_lookup_state(gc->gnss_gpio,
				"gnss_sensor");
	}

	gc->vdd_sensor_reg = devm_regulator_get(gc->dev, "vdd_sensor_2p85");
	if (IS_ERR(gc->vdd_sensor_reg)) {
		gif_err("Cannot get the regulator \"vdd_sensor_2p85\"\n");
	}

	gif_err("---\n");

	return ret;
}
