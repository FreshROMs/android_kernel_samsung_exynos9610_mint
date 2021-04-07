/****************************************************************************
 Copyright (C) 2015 Samsung Electronics Co., Ltd. All rights reserved.

 ******************************************************************************/

#include "fm_low_struc.h"
#include "radio-s610.h"
#include "fm_ctrl.h"

extern struct s610_radio *gradio;

void fm_pwron(void)
{
	fmspeedy_set_reg_field(0xFFF226, 0, 0x0001, 1); /* FM reset assert */
	fmspeedy_set_reg(0xFFF212, 0); /*  last power on  */
	fmspeedy_set_reg(0xFFF211, 0); /*  first power on  */
	fmspeedy_set_reg_field(0xFFF227, 0, 0x0001, 1); /* FM reset deassert */
	fmspeedy_set_reg(0xFFF210, 0); /*  FM isolaton disable  */
}

void fm_pwroff(void)
{
	fmspeedy_set_reg_field(0xFFF226, 0, 0x0001, 1); /* FM reset assert */
	fmspeedy_set_reg(0xFFF210, 1); /*  FM isolaton enable  */
	fmspeedy_set_reg(0xFFF211, 1); /*  first power off  */
	fmspeedy_set_reg(0xFFF212, 1); /*  last power off  */
}

void fmspeedy_wakeup(void)
{
	write32(gradio->fmspeedy_base + FMSPDY_CTL, SPDY_WAKEUP);
	udelay(5);
}

void fm_en_speedy_m_int(void)
{
	SetBits(gradio->fmspeedy_base + FMSPDY_INTR_MASK,
		FM_SLV_INT_MASK_BIT, 1, 0);
}

void fm_dis_speedy_m_int(void)
{
	SetBits(gradio->fmspeedy_base + FMSPDY_INTR_MASK,
		FM_SLV_INT_MASK_BIT, 1, 1);
}

void fm_speedy_m_int_stat_clear(void)
{
	write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
}

void fm_speedy_m_int_stat_clear_all(void)
{
	write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x7F);
}

void fm_speedy_m_int_enable(void)
{
	fm_en_speedy_m_int();
	fm_speedy_m_int_stat_clear_all();
}

void fm_speedy_m_int_disable(void)
{
	fm_dis_speedy_m_int();
	fm_speedy_m_int_stat_clear_all();
}

u32 fmspeedy_get_reg_core(u32 addr)
{
	u16 jj = 0;
	u32 status1;
	u32 ret = 0;

	fm_dis_speedy_m_int();

	fm_speedy_m_int_stat_clear();
	write32(gradio->fmspeedy_base + FMSPDY_CMD,
			FMSPDY_READ | FMSPDY_RANDOM
			| ((addr & 0x1FF) << 7));

	for (jj = 0; jj < 100; jj++) {
		udelay(2);
		status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
		if ((status1 & STAT_DONE) == 1)
			break;
	}

	if (jj >= 99) {
		dev_err(gradio->dev, "%s(), Fail addr:0x%xh\n",
			__func__, addr);
		ret = -1;
		goto get_fail;
	}

	ret = read32(gradio->fmspeedy_base + FMSPDY_DATA);

get_fail:
	fm_en_speedy_m_int();

	return ret;

}

u32 fmspeedy_get_reg(u32 addr)
{
	u32 data;
	
	API_ENTRY(gradio);

	spin_lock_irq(&gradio->slock);

	atomic_set(&gradio->is_doing, 1);
	data = fmspeedy_get_reg_core(addr);
	if (data == -1)
		gradio->speedy_error++;
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);
	
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x]", __func__, addr, data);
	API_EXIT(gradio);
	return data;
}

u32 fmspeedy_get_reg_work(u32 addr)
{
	u32 data;
	
	API_ENTRY(gradio);

	data = fmspeedy_get_reg_core(addr);
	if (data == -1)
		gradio->speedy_error++;
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x]", __func__, addr, data);
	API_EXIT(gradio);
	return data;
}

int fmspeedy_set_reg_core(u32 addr, u32 data)
{
	u16 jj;
	u32 status1;
	int ret = 0;

	fm_dis_speedy_m_int();

	fm_speedy_m_int_stat_clear();
	write32(gradio->fmspeedy_base + FMSPDY_DATA, data);
	write32(gradio->fmspeedy_base + FMSPDY_CMD,
		FMSPDY_WRITE | FMSPDY_RANDOM
		| ((addr & 0x1FF) << 7));

	for (jj = 0; jj < 100; jj++) {
		udelay(2);
		status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
		if ((status1 & STAT_DONE) == 1)
			break;
	}

	if (jj >= 99) {
		dev_err(gradio->dev, "%s(), Fail addr:0x%xh, data:0x%xh\n",
			__func__, addr, data);
		ret = -1;
	}

	fm_en_speedy_m_int();

	return ret;
}

int fmspeedy_set_reg(u32 addr, u32 data)
{
	int ret = 0;
	
	API_ENTRY(gradio);
	
	spin_lock_irq(&gradio->slock);

	atomic_set(&gradio->is_doing, 1);
	ret = fmspeedy_set_reg_core(addr, data);
	if (ret == -1)
		gradio->speedy_error++;
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x], ret[0x%x]", __func__, addr, data, ret);
	API_EXIT(gradio);
	return ret;
}

int fmspeedy_set_reg_work(u32 addr, u32 data)
{
	int ret = 0;
	
	API_ENTRY(gradio);
	ret = fmspeedy_set_reg_core(addr, data);
	if (ret == -1)
		gradio->speedy_error++;
	
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x], ret[0x%x]", __func__, addr, data, ret);
	API_EXIT(gradio);
	return ret;
}

u32 fmspeedy_get_reg_field_core(u32 addr, u32 shift, u32 mask)
{
	u16 jj;
	u32 status1;
	u32 ret = 0;

	fm_dis_speedy_m_int();

	fm_speedy_m_int_stat_clear();
	write32(gradio->fmspeedy_base + FMSPDY_CMD,
			FMSPDY_READ | FMSPDY_RANDOM
			| ((addr & 0x1FF) << 7));

	for (jj = 0; jj < 100; jj++) {
		udelay(2);
		status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
		if ((status1 & STAT_DONE) == 1)
			break;
	}

	if (jj >= 99) {
		dev_err(gradio->dev, "%s(), Fail addr:0x%xh\n",
			__func__, addr);
		ret = -1;
		goto read_fail_f;
	}
	ret = (read32(gradio->fmspeedy_base + FMSPDY_DATA) & (mask)) >> shift;

read_fail_f:
	fm_en_speedy_m_int();

	return ret;
}

u32 fmspeedy_get_reg_field(u32 addr, u32 shift, u32 mask)
{
	u32 data;

	API_ENTRY(gradio);
	
	spin_lock_irq(&gradio->slock);

	atomic_set(&gradio->is_doing, 1);
	data = fmspeedy_get_reg_field_core(addr, shift, mask);
	if (data == -1)
		gradio->speedy_error++;
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);
	
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x]", __func__, addr, data);
	API_EXIT(gradio);
	return data;
}

u32 fmspeedy_get_reg_field_work(u32 addr, u32 shift, u32 mask)
{
	u32 data;
	
	API_ENTRY(gradio);
	
	data = fmspeedy_get_reg_field_core(addr, shift, mask);
	if (data == -1)
		gradio->speedy_error++;
	
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x]", __func__, addr, data);
	API_EXIT(gradio);

	return data;
}

int fmspeedy_set_reg_field_core(u32 addr, u32 shift, u32 mask, u32 data)
{
	u32 value, value1;
	u16 jj;
	u32 status1;
	int ret = 0;

	fm_dis_speedy_m_int();

	fm_speedy_m_int_stat_clear();
	write32(gradio->fmspeedy_base + FMSPDY_CMD,
			FMSPDY_READ | FMSPDY_RANDOM
			| ((addr & 0x1FF) << 7));

	for (jj = 0; jj < 100; jj++) {
		udelay(2);
		status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
		if ((status1 & STAT_DONE) == 1)
			break;
	}

	if (jj >= 99) {
		dev_err(gradio->dev, "%s(), Fail addr:0x%xh, data:0x%xh, cnt:%d\n",
			__func__, addr, data, jj);
		ret = -1;
		goto set_fail_f;
	}

	value1 = read32(gradio->fmspeedy_base + FMSPDY_DATA);
	value = (value1 & ~(mask)) | ((data) << (shift));

	write32(gradio->fmspeedy_base + FMSPDY_DATA, value);
	write32(gradio->fmspeedy_base + FMSPDY_STAT, 0x1F);
	write32(gradio->fmspeedy_base + FMSPDY_CMD,
		FMSPDY_WRITE | FMSPDY_RANDOM
		| ((addr & 0x1FF) << 7));

	for (jj = 0; jj < 100; jj++) {
		udelay(2);
		status1 = read32(gradio->fmspeedy_base + FMSPDY_STAT);
		if ((status1 & STAT_DONE) == 1)
			break;
	}

	if (jj >= 99) {
		dev_err(gradio->dev, "%s(), Fail addr:0x%xh, data:0x%xh, cnt:%d\n",
			__func__, addr, data, jj);
		ret = -1;
	}

set_fail_f:
	fm_en_speedy_m_int();

	return ret;
}

int fmspeedy_set_reg_field(u32 addr, u32 shift, u32 mask, u32 data)
{
	int ret = 0;
	
	API_ENTRY(gradio);
	spin_lock_irq(&gradio->slock);

	atomic_set(&gradio->is_doing, 1);
	ret = fmspeedy_set_reg_field_core(addr, shift, mask, data);
	if (ret == -1)
		gradio->speedy_error++;
	atomic_set(&gradio->is_doing, 0);

	spin_unlock_irq(&gradio->slock);
	
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x], ret[0x%x]", __func__, addr, data, ret);
	API_EXIT(gradio);
	return ret;
}

int fmspeedy_set_reg_field_work(u32 addr, u32 shift, u32 mask, u32 data)
{
	int ret = 0;
	
	API_ENTRY(gradio);
	ret = fmspeedy_set_reg_field_core(addr, shift, mask, data);
	if (ret == -1)
		gradio->speedy_error++;
	
	APIEBUG(gradio, "%s():addr[0x%x], data[0x%x], ret[0x%x]", __func__, addr, data, ret);
	API_EXIT(gradio);

	return ret;
}

/****************************************************************************
 NAME
 fm_audio_control   -  Audio out enable/disable

 FUNCTION
 Setting registers for Audio
 ****************************************************************************/
void fm_audio_control(struct s610_radio *radio,
		bool audio_out, bool lr_switch,
		u32 req_time, u32 audio_addr)
{
	write32(radio->fmspeedy_base + AUDIO_CTRL,
		((audio_out << 21) | (lr_switch << 20)
		| ((req_time & 0x07FF) << 9)
		| (audio_addr & 0x01FF)));
	udelay(15);
}

