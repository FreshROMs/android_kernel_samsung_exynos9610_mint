/*
 * Copyrights (C) 2018 Samsung Electronics, Inc.
 * Copyrights (C) 2018 Silicon Mitus, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/usb/typec/sm5713/sm5713_pd.h>
#include <linux/usb/typec/sm5713/sm5713_typec.h>
#include <linux/of_gpio.h>
#include <linux/mfd/sm5713.h>
#if defined(CONFIG_CCIC_NOTIFIER)
#include <linux/usb/typec/pdic_notifier.h>
#endif
#if defined(CONFIG_USB_HW_PARAM)
#include <linux/usb_notify.h>
#endif
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
#include <linux/battery/battery_notifier.h>
extern struct pdic_notifier_struct pd_noti;
void sm5713_usbpd_inform_pdo_list(void);

void sm5713_select_pdo(int num)
{
	struct sm5713_usbpd_data *psubpd = pd_noti.pusbpd;
	struct sm5713_phydrv_data *pdic_data = psubpd->phy_driver_data;
	struct sm5713_usbpd_manager_data *manager = &psubpd->manager;
	bool vbus_short = false;

	if (!pdic_data->is_attached) {
		pr_info(" %s : PDO(%d) is ignored becasue of plug detached\n", __func__, num);
		return;
	}

	psubpd->phy_ops.get_short_state(psubpd, &vbus_short);

	if (vbus_short) {
		pr_info(" %s : PDO(%d) is ignored becasue of vbus short\n", __func__, num);
		return;
	}

	if (num > 1 && (manager->fled_torch_enable || manager->fled_flash_enable)) {
		pr_info(" %s : PDO(%d) is ignored becasue of [torch(%d) or flash(%d)]\n",
			__func__, num, manager->fled_torch_enable, manager->fled_flash_enable);
		return;
	}

	if (pd_noti.sink_status.selected_pdo_num == num) {
		sm5713_usbpd_inform_pdo_list();	
		return;
	} else if (num > pd_noti.sink_status.available_pdo_num)
		pd_noti.sink_status.selected_pdo_num =
			pd_noti.sink_status.available_pdo_num;
	else if (num < 1)
		pd_noti.sink_status.selected_pdo_num = 1;
	else
		pd_noti.sink_status.selected_pdo_num = num;

	manager->pn_flag = false;

	pr_info(" %s : PDO(%d) is selected to change\n",
		__func__, pd_noti.sink_status.selected_pdo_num);

	sm5713_usbpd_inform_event(psubpd, MANAGER_NEW_POWER_SRC);
}

void sm5713_usbpd_inform_pdo_list(void)
{
	CC_NOTI_ATTACH_TYPEDEF pd_notifier;

	pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;;
	pd_notifier.src = CCIC_NOTIFY_DEV_CCIC;
	pd_notifier.dest = CCIC_NOTIFY_DEV_BATTERY;
	pd_notifier.id = CCIC_NOTIFY_ID_POWER_STATUS;
	pd_notifier.attach = 1;
#if defined(CONFIG_CCIC_NOTIFIER)
	ccic_notifier_notify((CC_NOTI_TYPEDEF *)&pd_notifier,
			&pd_noti, 1/* pdic_attach */);
#endif
}

void sm5713_usbpd_change_available_pdo(struct device *dev)
{
	CC_NOTI_ATTACH_TYPEDEF pd_notifier;

	pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
	pd_notifier.src = CCIC_NOTIFY_DEV_CCIC;
	pd_notifier.dest = CCIC_NOTIFY_DEV_BATTERY;
	pd_notifier.id = CCIC_NOTIFY_ID_POWER_STATUS;
	pd_notifier.attach = 1;
#if defined(CONFIG_CCIC_NOTIFIER)
	ccic_notifier_notify((CC_NOTI_TYPEDEF *)&pd_notifier,
			&pd_noti, 1/* pdic_attach */);
#endif
}

void sm5713_request_default_power_src(void)
{
	struct sm5713_usbpd_data *pd_data = pd_noti.pusbpd;
	int pdo_num = pd_noti.sink_status.selected_pdo_num;
	pr_info(" %s : policy->state = (0x%x), pdo_num = %d, max vol = %d\n", __func__,
		pd_data->policy.state, pdo_num,
		pd_noti.sink_status.power_list[pdo_num].max_voltage);

	if ((pdo_num > 1) &&
		(pd_noti.sink_status.power_list[pdo_num].max_voltage > 5000)) {
		pd_noti.sink_status.available_pdo_num = 1;
		sm5713_usbpd_change_available_pdo(pd_data->dev);
	}
}

int sm5713_usbpd_check_fled_state(bool enable, u8 mode)
{
	struct sm5713_usbpd_data *pd_data = pd_noti.pusbpd;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	int pdo_num = pd_noti.sink_status.selected_pdo_num;

	pr_info("[%s] enable(%d), mode(%d)\n", __func__, enable, mode);

	if (mode == FLED_MODE_TORCH) { /* torch */
		cancel_delayed_work(&manager->new_power_handler);
		pr_info("[%s] new_power_handler cancel\n", __func__);

		manager->fled_torch_enable = enable;
	} else if (mode == FLED_MODE_FLASH) { /* flash */
		manager->fled_flash_enable = enable;
	}

	pr_info("[%s] fled_torch_enable(%d), fled_flash_enable(%d)\n", __func__,
		manager->fled_torch_enable, manager->fled_flash_enable);

	if ((manager->fled_torch_enable == false) &&
			(manager->fled_flash_enable == false)) {
		if ((mode == FLED_MODE_TORCH) && (enable == false)) {
			cancel_delayed_work(&manager->new_power_handler);
			schedule_delayed_work(&manager->new_power_handler,
				msecs_to_jiffies(5000));
			pr_info("[%s] new_power_handler start(5sec)\n", __func__);
		} else {
			if (pdic_data->is_attached && (pdo_num > 0)) {
				pd_noti.sink_status.available_pdo_num = manager->origin_available_pdo_num;
				sm5713_usbpd_change_available_pdo(pd_data->dev);
			}
		}
	}

	return 0;
}
#endif

int sm5713_usbpd_uvdm_ready(void)
{
	int uvdm_ready = 0;
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct sm5713_phydrv_data *phy_data;
	struct sm5713_usbpd_data *pd_data;
	struct sm5713_usbpd_manager_data *manager;

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__); 
		return -ENODEV;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
 		pr_err("ccic_data is null\n");
		return -ENXIO;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
 		pr_err("phy_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
 		pr_err("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s: manager is null\n", __func__);
		return -ENXIO;
	}

	if (manager->is_samsung_accessory_enter_mode && manager->pn_flag)
		uvdm_ready = 1;

	pr_info("%s: uvdm ready is %s, entermode : %d, pn_flag : %d\n", __func__,
		uvdm_ready ? "true" : "false",
		manager->is_samsung_accessory_enter_mode,
		manager->pn_flag);
	return uvdm_ready;
}

void sm5713_usbpd_uvdm_close(void)
{
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct sm5713_phydrv_data *phy_data;
	struct sm5713_usbpd_data *pd_data;
	struct sm5713_usbpd_manager_data *manager;

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__);
		return;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
 		pr_err("ccic_data is null\n");
		return;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
 		pr_err("phy_data is null\n");
		return;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
 		pr_err("pd_data is null\n");
		return;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s: manager is null\n", __func__);
		return;
	}
	manager->uvdm_out_ok = 1;
	manager->uvdm_in_ok = 1;
	wake_up(&manager->uvdm_out_wq);
	wake_up(&manager->uvdm_in_wq);
	pr_info("%s\n", __func__);
}

int sm5713_usbpd_uvdm_out_request_message(void *data, int size)
{
	uint8_t *SEC_DATA;
	uint8_t rcv_data[MAX_INPUT_DATA] = {0,};
	int need_set_cnt = 0;
	int cur_set_data = 0;
	int cur_set_num = 0;
	int remained_data_size = 0;
	int accumulated_data_size = 0;
	int received_data_index = 0;
	int time_left = 0;
	int i;
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct sm5713_phydrv_data *phy_data;
	struct sm5713_usbpd_data *pd_data;
	struct sm5713_usbpd_manager_data *manager;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__); 
		return -ENODEV;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
 		pr_err("ccic_data is null\n");
		return -ENXIO;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
 		pr_err("phy_data is null\n");
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
 		pr_err("pd_data is null\n");
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s: manager is null\n", __func__);
		return -ENXIO;
	}

	set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 7);
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);
	set_endian(data, rcv_data, size);

	if (size <= 1) {
		pr_info("%s - process short data\n", __func__);
		/* VDM Header + 6 VDOs = MAX 7 */
		manager->uvdm_msg_header.num_data_objs = 2;
		manager->uvdm_data_obj[1].sec_uvdm_header.total_set_num = 1;
		manager->uvdm_data_obj[1].sec_uvdm_header.data = rcv_data[0];
		manager->uvdm_out_ok = 0;
		sm5713_usbpd_inform_event(
			pd_data, MANAGER_UVDM_SEND_MESSAGE);
		time_left =
			wait_event_interruptible_timeout(
				manager->uvdm_out_wq, manager->uvdm_out_ok,
				msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (manager->uvdm_out_ok == 2)	{
			pr_err("%s NAK\n", __func__);
			return -ENODATA;
		} else if (manager->uvdm_out_ok == 3) {
			pr_err("%s BUSY\n", __func__);
			return -EBUSY;
		} else if (!time_left) {
			pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
			event = NOTIFY_EXTRA_UVDM_TIMEOUT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return -ETIME;
		} else if (time_left == -ERESTARTSYS)
			return -ERESTARTSYS;
	} else {
		pr_info("%s - process long data\n", __func__);
		need_set_cnt = set_uvdmset_count(size);
		manager->uvdm_first_req = true;
		manager->uvdm_dir =  DIR_OUT;
		cur_set_num = 1;
		accumulated_data_size = 0;
		remained_data_size = size;

		if (manager->uvdm_first_req)
			set_sec_uvdm_header(&manager->uvdm_data_obj[0],
					manager->Product_ID,
					SEC_UVDM_LONG_DATA,
					SEC_UVDM_ININIATOR, DIR_OUT,
					need_set_cnt, 0);
		while (cur_set_num <= need_set_cnt) {
			cur_set_data = 0;
			time_left = 0;
			set_sec_uvdm_tx_header(
					&manager->uvdm_data_obj[0], manager->uvdm_first_req,
					cur_set_num, size, remained_data_size);
			cur_set_data =
				get_data_size(manager->uvdm_first_req, remained_data_size);

			pr_info("%s - cur_set_data:%d, size:%ld, cur_set_num:%d\n",
				__func__, cur_set_data, size, cur_set_num);

			if (manager->uvdm_first_req) {
				SEC_DATA =
					(uint8_t *)&manager->uvdm_data_obj[3];
				for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			} else {
				SEC_DATA =
					(uint8_t *)&manager->uvdm_data_obj[2];
				for (i = 0; i < SEC_UVDM_MAXDATA_NORMAL; i++)
					SEC_DATA[i] =
						rcv_data[received_data_index++];
			}

			set_sec_uvdm_tx_tailer(&manager->uvdm_data_obj[0]);
			manager->uvdm_out_ok = 0;
			sm5713_usbpd_inform_event(
				pd_data, MANAGER_UVDM_SEND_MESSAGE);
			time_left =
				wait_event_interruptible_timeout(
					manager->uvdm_out_wq, manager->uvdm_out_ok,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
			if (manager->uvdm_out_ok == 2 ||
				manager->uvdm_out_ok == 4)	{
				pr_err("%s NAK\n", __func__);
				return -ENODATA;
			} else if (manager->uvdm_out_ok == 3 ||
					   manager->uvdm_out_ok == 5) {
				pr_err("%s BUSY\n", __func__);
				return -EBUSY;
			} else if (!time_left) {
				pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
				event = NOTIFY_EXTRA_UVDM_TIMEOUT;
				store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
				return -ETIME;
			} else if (time_left == -ERESTARTSYS)
				return -ERESTARTSYS;

			accumulated_data_size += cur_set_data;
			remained_data_size -= cur_set_data;
			if (manager->uvdm_first_req)
				manager->uvdm_first_req = false;
			cur_set_num++;
		}
	}
	return size;
}

int sm5713_usbpd_uvdm_in_request_message(void *data)
{
	uint8_t in_data[MAX_INPUT_DATA] = {0,};

	s_uvdm_header	SEC_RES_HEADER;
	s_tx_header	SEC_TX_HEADER;
	s_tx_tailer	SEC_TX_TAILER;
	data_obj_type	uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	msg_header_type	uvdm_msg_header;

	int cur_set_data = 0;
	int cur_set_num = 0;
	int total_set_num = 0;
	int rcv_data_size = 0;
	int total_rcv_size = 0;
	int ack = 0;
	int size = 0;
	int time_left = 0;
	int i;
	int cal_checksum = 0;
	struct device *ccic_device = get_ccic_device(); 
	pccic_data_t ccic_data;
	struct sm5713_phydrv_data *phy_data;
	struct sm5713_usbpd_data *pd_data;
	struct sm5713_usbpd_manager_data *manager;
	struct sm5713_policy_data *policy;
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
	int event;
#endif

	if (!ccic_device) { 
		pr_err("%s: ccic_device is null.\n", __func__); 
		return -ENODEV;
	}
	ccic_data = dev_get_drvdata(ccic_device);
	if (!ccic_data) {
 		pr_err("ccic_data is null\n"); 
		return -ENXIO;
	}
	phy_data = ccic_data->drv_data;
	if (!phy_data) {
 		pr_err("phy_data is null\n"); 
		return -ENXIO;
	}
	pd_data = dev_get_drvdata(phy_data->dev);
	if (!pd_data) {
 		pr_err("pd_data is null\n"); 
		return -ENXIO;
	}
	manager = &pd_data->manager;
	if (!manager) {
		pr_err("%s: manager is null\n", __func__);
		return -ENXIO;
	}
	policy = &pd_data->policy;
	if (!policy) {
		pr_err("%s: policy is null\n", __func__);
		return -ENXIO;
	}
	pr_info("%s\n", __func__);

	manager->uvdm_dir = DIR_IN;
	manager->uvdm_first_req = true;
	uvdm_msg_header.word = policy->rx_msg_header.word;

	/* 2. Common : Fill the MSGHeader */
	set_msg_header(&manager->uvdm_msg_header, USBPD_Vendor_Defined, 2);
	/* 3. Common : Fill the UVDMHeader*/
	set_uvdm_header(&manager->uvdm_data_obj[0], SAMSUNG_VENDOR_ID, 0);

	/* 4. Common : Fill the First SEC_VDMHeader*/
	if (manager->uvdm_first_req)
		set_sec_uvdm_header(&manager->uvdm_data_obj[0],
				manager->Product_ID,
				SEC_UVDM_LONG_DATA,
				SEC_UVDM_ININIATOR, DIR_IN, 0, 0);
	/* 5. Send data to PDIC */
	manager->uvdm_in_ok = 0;
	sm5713_usbpd_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);

	cur_set_num = 0;
	total_set_num = 1;

	do {
		time_left =
			wait_event_interruptible_timeout(
					manager->uvdm_in_wq, manager->uvdm_in_ok,
					msecs_to_jiffies(SEC_UVDM_WAIT_MS));
		if (manager->uvdm_in_ok == 2)	{
			pr_err("%s NAK\n", __func__);
			return -ENODATA;
		} else if (manager->uvdm_in_ok == 3) {
			pr_err("%s BUSY\n", __func__);
			return -EBUSY;
		} else if (!time_left) {
			pr_err("%s timeout\n", __func__);
#ifdef CONFIG_USB_NOTIFY_PROC_LOG
			event = NOTIFY_EXTRA_UVDM_TIMEOUT;
			store_usblog_notify(NOTIFY_EXTRA, (void *)&event, NULL);
#endif
			return -ETIME;
		} else if (time_left == -ERESTARTSYS)
			return -ERESTARTSYS;

		/* read data */
		uvdm_msg_header.word = policy->rx_msg_header.word;
		for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
			uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

		if (manager->uvdm_first_req) {
			SEC_RES_HEADER.object = uvdm_data_obj[1].object;
			SEC_TX_HEADER.object = uvdm_data_obj[2].object;

			if (SEC_RES_HEADER.data_type == TYPE_SHORT) {
				pr_info("%s - process short data\n", __func__);
				in_data[rcv_data_size++] = SEC_RES_HEADER.data;
				return rcv_data_size;
			}
			/* 1. check the data size received */
			size = SEC_TX_HEADER.total_size;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			total_set_num = SEC_RES_HEADER.total_set_num;

			manager->uvdm_first_req = false;
			/* 2. copy data to buffer */
			for (i = 0; i < SEC_UVDM_MAXDATA_FIRST; i++)
				in_data[rcv_data_size++] =
					uvdm_data_obj[3+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
			manager->uvdm_first_req = false;
		} else {
			SEC_TX_HEADER.object = uvdm_data_obj[1].object;
			cur_set_data = SEC_TX_HEADER.cur_size;
			cur_set_num = SEC_TX_HEADER.order_cur_set;
			/* 2. copy data to buffer */
			for (i = 0 ; i < SEC_UVDM_MAXDATA_NORMAL; i++)
				in_data[rcv_data_size++] =
					uvdm_data_obj[2+i/SEC_UVDM_ALIGN].byte[i%SEC_UVDM_ALIGN];
			total_rcv_size += cur_set_data;
		}
		/* 3. Check Checksum */
		SEC_TX_TAILER.object = uvdm_data_obj[6].object;
		cal_checksum =
			get_checksum((char *)&uvdm_data_obj[0], 4, SEC_UVDM_CHECKSUM_COUNT);
		ack = (cal_checksum == SEC_TX_TAILER.checksum) ? RX_ACK : RX_NAK;

		/* 5. Common : Fill the MSGHeader */
		set_msg_header(&manager->uvdm_msg_header,
				USBPD_Vendor_Defined, 2);
		/* 5.1. Common : Fill the UVDMHeader*/
		set_uvdm_header(&manager->uvdm_data_obj[0],
				SAMSUNG_VENDOR_ID, 0);
		/* 5.2. Common : Fill the First SEC_VDMHeader*/
		set_sec_uvdm_rx_header(&manager->uvdm_data_obj[0],
				cur_set_num, cur_set_data, ack);
		manager->uvdm_in_ok = 0;
		sm5713_usbpd_inform_event(pd_data, MANAGER_UVDM_SEND_MESSAGE);
	} while (cur_set_num < total_set_num);

	set_endian(in_data, data, size);
	return size;
}

static void sm5713_usbpd_receive_samsung_uvdm_message(
					struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_policy_data *policy = &pd_data->policy;
	int i = 0;
	msg_header_type		uvdm_msg_header;
	data_obj_type		uvdm_data_obj[USBPD_MAX_COUNT_MSG_OBJECT];
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	s_uvdm_header SEC_UVDM_RES_HEADER;
	s_rx_header SEC_UVDM_RX_HEADER;

	uvdm_msg_header.word = policy->rx_msg_header.word;

	for (i = 0; i < uvdm_msg_header.num_data_objs; i++)
		uvdm_data_obj[i].object = policy->rx_data_obj[i].object;

	uvdm_msg_header.word = policy->rx_msg_header.word;
	pr_info("%s dir %s\n", __func__, (manager->uvdm_dir == DIR_OUT)
		? "OUT":"IN");

	if (manager->uvdm_dir == DIR_OUT) {
		if (manager->uvdm_first_req) {
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.data_type == TYPE_LONG) {
				SEC_UVDM_RX_HEADER.object = uvdm_data_obj[2].object;
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
						manager->uvdm_out_ok = 1;
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
						pr_err("%s SEC_UVDM_RX_HEADER : RX_NAK\n", __func__);
						manager->uvdm_out_ok = 4;
					} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
						pr_err("%s SEC_UVDM_RX_HEADER : RX_BUSY\n", __func__);
						manager->uvdm_out_ok = 5;
					}
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
					manager->uvdm_out_ok = 2;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
					manager->uvdm_out_ok = 3;
				}
			} else if (SEC_UVDM_RES_HEADER.data_type == TYPE_SHORT) {
				if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
					manager->uvdm_out_ok = 1;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
					manager->uvdm_out_ok = 2;
				} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
					pr_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
					manager->uvdm_out_ok = 3;
				}
			}
		} else {
			SEC_UVDM_RX_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RX_HEADER.result_value == RX_ACK) {
				manager->uvdm_out_ok = 1;
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_NAK) {
				pr_err("%s SEC_UVDM_RX_HEADER : RX_NAK\n", __func__);
				manager->uvdm_out_ok = 4;
			} else if (SEC_UVDM_RX_HEADER.result_value == RX_BUSY) {
				pr_err("%s SEC_UVDM_RX_HEADER : RX_BUSY\n", __func__);
				manager->uvdm_out_ok = 5;
			}
		}
		wake_up(&manager->uvdm_out_wq);
	} else { /* DIR_IN */
		if (manager->uvdm_first_req) { /* Long = Short */
			SEC_UVDM_RES_HEADER.object = uvdm_data_obj[1].object;
			if (SEC_UVDM_RES_HEADER.cmd_type == RES_ACK) {
				manager->uvdm_in_ok = 1;
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_NAK) {
				pr_err("%s SEC_UVDM_RES_HEADER : RES_NAK\n", __func__);
				manager->uvdm_in_ok = 2;
			} else if (SEC_UVDM_RES_HEADER.cmd_type == RES_BUSY) {
				pr_err("%s SEC_UVDM_RES_HEADER : RES_BUSY\n", __func__);
				manager->uvdm_in_ok = 3;
			}
		} else {
			/* IN response case, SEC_TX_HEADER has no ACK message.
			   So uvdm_is_ok is always 1. */
			manager->uvdm_in_ok = 1;
		}
		wake_up(&manager->uvdm_in_wq);
	}
}

void sm5713_usbpd_dp_detach(struct device *dev)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: dp_is_connect %d\n", __func__, manager->dp_is_connect);

	sm5713_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_USB_DP,
		CCIC_NOTIFY_ID_USB_DP, 0, manager->dp_hs_connect, 0);
	sm5713_ccic_event_work(pdic_data, CCIC_NOTIFY_DEV_DP,
		CCIC_NOTIFY_ID_DP_CONNECT, 0, 0, 0);
	manager->dp_selected_pin = 0;
	manager->dp_is_connect = 0;
	manager->dp_hs_connect = 0;
	manager->pin_assignment = 0;
}

void sm5713_usbpd_acc_detach(struct device *dev)
{	
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	pr_info("%s: acc_type %d\n",
		__func__, manager->acc_type);
	if (manager->acc_type != CCIC_DOCK_DETACHED) {
		if (manager->acc_type == CCIC_DOCK_HMT)
			schedule_delayed_work(&manager->acc_detach_handler,
				msecs_to_jiffies(GEAR_VR_DETACH_WAIT_MS));
		else
			schedule_delayed_work(&manager->acc_detach_handler,
				msecs_to_jiffies(0));
	}
}

static void sm5713_usbpd_manager_new_power_handler(struct work_struct *wk)
{
	struct sm5713_usbpd_data *pd_data = pd_noti.pusbpd;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	int pdo_num = pd_noti.sink_status.selected_pdo_num;

	pr_info("[%s] pdic_data->is_attached = %d\n", __func__, pdic_data->is_attached);
	if (pdic_data->is_attached && (pdo_num > 0)) {
		pd_noti.sink_status.available_pdo_num = manager->origin_available_pdo_num;
		sm5713_usbpd_change_available_pdo(pd_data->dev);
	}
}

static void sm5713_usbpd_acc_detach_handler(struct work_struct *wk)
{
	struct sm5713_usbpd_manager_data *manager =
		container_of(wk, struct sm5713_usbpd_manager_data,
				acc_detach_handler.work);

	pr_info("%s: acc_type %d\n",
		__func__, manager->acc_type);
	if (manager->acc_type != CCIC_DOCK_DETACHED) {
		if (manager->acc_type != CCIC_DOCK_NEW)
			ccic_send_dock_intent(CCIC_DOCK_DETACHED);
		ccic_send_dock_uevent(manager->Vendor_ID,
				manager->Product_ID,
				CCIC_DOCK_DETACHED);
		manager->acc_type = CCIC_DOCK_DETACHED;
		manager->Vendor_ID = 0;
		manager->Product_ID = 0;
		manager->Device_Version = 0;
		manager->alt_sended = 0;
		manager->SVID_0 = 0;
		manager->SVID_1 = 0;
		manager->Standard_Vendor_ID = 0;
		manager->is_samsung_accessory_enter_mode = 0;
		manager->uvdm_out_ok = 1;
		manager->uvdm_in_ok = 1;
		wake_up(&manager->uvdm_out_wq);
		wake_up(&manager->uvdm_in_wq);
	}
}

int sm5713_usbpd_check_accessory(
		struct sm5713_usbpd_manager_data *manager)
{
	uint16_t vid = manager->Vendor_ID;
	uint16_t pid = manager->Product_ID;
	uint16_t acc_type = CCIC_DOCK_DETACHED;
#if defined(CONFIG_USB_HW_PARAM)
	struct otg_notify *o_notify = get_otg_notify();
#endif

	/* detect Gear VR */
	if (manager->acc_type == CCIC_DOCK_DETACHED) {
		if (vid == SAMSUNG_VENDOR_ID) {
			switch (pid) {
			/* GearVR: Reserved GearVR PID+6 */
			case GEARVR_PRODUCT_ID ... GEARVR_PRODUCT_ID_5:
				acc_type = CCIC_DOCK_HMT;
				pr_info("%s : Samsung Gear VR connected\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_VR_USE_COUNT);
#endif
				break;
			case DEXDOCK_PRODUCT_ID:
				acc_type = CCIC_DOCK_DEX;
				pr_info("%s : Samsung DEX connected\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case DEXPAD_PRODUCT_ID:
				acc_type = CCIC_DOCK_DEXPAD;
				pr_info("%s : Samsung DEX PAD connected\n",
					__func__);
#if defined(CONFIG_USB_HW_PARAM)
				if (o_notify)
					inc_hw_param(o_notify, USB_CCIC_DEX_USE_COUNT);
#endif
				break;
			case HDMI_PRODUCT_ID:
				acc_type = CCIC_DOCK_HDMI;
				pr_info("%s : Samsung HDMI adapter(EE-HG950) connected\n",
					__func__);
				break;
			default:
				acc_type = CCIC_DOCK_NEW;
				if (pid == FRIENDS_PRODUCT_ID)
					pr_info("%s : Kakao Friends Stand connected\n",
						__func__);
				else
					pr_info("%s : default device connected\n",
						__func__);
				break;
			}
		} else if (vid == SAMSUNG_MPA_VENDOR_ID) {
			switch (pid) {
			case MPA_PRODUCT_ID:
				acc_type = CCIC_DOCK_MPA;
				pr_info("%s : Samsung MPA connected.\n",
					__func__);
				break;
			default:
				acc_type = CCIC_DOCK_NEW;
				pr_info("%s : default device connected\n",
					__func__);
				break;
			}
		}
		manager->acc_type = acc_type;
	} else
		acc_type = manager->acc_type;

	if (acc_type != CCIC_DOCK_NEW)
		ccic_send_dock_intent(acc_type);

	ccic_send_dock_uevent(vid, pid, acc_type);
	return (acc_type != CCIC_DOCK_NEW || vid == SAMSUNG_VENDOR_ID) ? 1 : 0;
}

void sm5713_usbpd_power_ready(struct device *dev,
	CCIC_OTP_MODE power_role)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	CC_NOTI_ATTACH_TYPEDEF pd_notifier;
	bool short_cable = false;
#if defined(CONFIG_TYPEC)
	enum typec_pwr_opmode mode = TYPEC_PWR_MODE_USB;
#endif
	if (!pdic_data->pd_support) {
		pd_data->phy_ops.get_short_state(pd_data, &short_cable);
		if (short_cable) {
			pd_noti.sink_status.available_pdo_num = 1;
			pd_noti.sink_status.power_list[1].max_current =
				pd_noti.sink_status.power_list[1].max_current > 1800 ?
				1800 : pd_noti.sink_status.power_list[1].max_current;
		}
		pdic_data->pd_support = 1;
		pr_info("%s : pd_support : %d\n", __func__, pdic_data->pd_support);
#if defined(CONFIG_TYPEC)
		mode = sm5713_get_pd_support(pdic_data);
		typec_set_pwr_opmode(pdic_data->port, mode);
#endif
	}

	if (power_role == TYPE_C_ATTACH_SNK &&
		policy->last_state == PE_SNK_Transition_Sink) {
		if (policy->send_sink_cap) {
			pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK_CAP;
			policy->send_sink_cap = 0;
		} else
			pd_noti.event = PDIC_NOTIFY_EVENT_PD_SINK;
		pd_notifier.src = CCIC_NOTIFY_DEV_CCIC;
		pd_notifier.dest = CCIC_NOTIFY_DEV_BATTERY;
		pd_notifier.id = CCIC_NOTIFY_ID_POWER_STATUS;
		pd_notifier.attach = 1;
#if defined(CONFIG_CCIC_NOTIFIER)
		ccic_notifier_notify((CC_NOTI_TYPEDEF *)&pd_notifier,
				&pd_noti, 1/* pdic_attach */);
#endif
	}
}

static int sm5713_usbpd_command_to_policy(struct device *dev,
		sm5713_usbpd_manager_command_type command)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	manager->cmd = command;

	sm5713_usbpd_kick_policy_work(dev);

	return 0;
}

void sm5713_usbpd_inform_event(struct sm5713_usbpd_data *pd_data,
		sm5713_usbpd_manager_event_type event)
{
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	manager->event = event;

	switch (event) {
	case MANAGER_DISCOVER_IDENTITY_ACKED:
		sm5713_usbpd_get_identity(pd_data);
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DISCOVER_SVID);
		break;
	case MANAGER_DISCOVER_SVID_ACKED:
		sm5713_usbpd_get_svids(pd_data);
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DISCOVER_MODE);
		break;
	case MANAGER_DISCOVER_MODE_ACKED:
		sm5713_usbpd_get_modes(pd_data);
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_ENTER_MODE);
		break;
	case MANAGER_ENTER_MODE_ACKED:
		sm5713_usbpd_enter_mode(pd_data);
		if (manager->SVID_0 == PD_SID_1)
			sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_STATUS_UPDATE);
		break;
	case MANAGER_STATUS_UPDATE_ACKED:
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_VDM_DisplayPort_Configure);
		break;
	case MANAGER_DisplayPort_Configure_ACKED:
		break;
	case MANAGER_NEW_POWER_SRC:
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_NEW_POWER_SRC);
		break;
	case MANAGER_UVDM_SEND_MESSAGE:
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_UVDM_SEND_MESSAGE);
		break;
	case MANAGER_UVDM_RECEIVE_MESSAGE:
		sm5713_usbpd_receive_samsung_uvdm_message(pd_data);
		break;
	case MANAGER_PR_SWAP_REQUEST:
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_PR_SWAP);
		break;
	case MANAGER_DR_SWAP_REQUEST:
		sm5713_usbpd_command_to_policy(pd_data->dev,
					MANAGER_REQ_DR_SWAP);
		break;
	default:
		pr_info("%s: not matched event(%d)\n", __func__, event);
	}
}

bool sm5713_usbpd_vdm_request_enabled(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	bool ret;

	if (manager->alt_sended)
		ret = false;
	else {
		manager->alt_sended = 1;
		ret = true;
	}
	return ret;
}

bool sm5713_usbpd_power_role_swap(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	return manager->power_role_swap;
}

bool sm5713_usbpd_vconn_source_swap(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	return manager->vconn_source_swap;
}

void sm5713_usbpd_turn_on_source(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;

	pr_info("%s\n", __func__);

	sm5713_vbus_turn_on_ctrl(pdic_data, 1);

	sm5713_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_MUIC, CCIC_NOTIFY_ID_ATTACH,
		CCIC_NOTIFY_ATTACH/*attach*/,
		USB_STATUS_NOTIFY_ATTACH_DFP/*rprd*/, 0);
}

void sm5713_usbpd_turn_off_power_supply(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;

	pr_info("%s\n", __func__);

	sm5713_vbus_turn_on_ctrl(pdic_data, 0);
	sm5713_usbpd_set_vbus_dischg_gpio(pdic_data, 1);
}

void sm5713_usbpd_turn_off_power_sink(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;

	pr_info("%s\n", __func__);

	pd_noti.event = PDIC_NOTIFY_EVENT_PD_PRSWAP_SNKTOSRC;
	pd_noti.sink_status.selected_pdo_num = 0;
	pd_noti.sink_status.available_pdo_num = 0;
	pd_noti.sink_status.current_pdo_num = 0;

	sm5713_ccic_event_work(pdic_data,
		CCIC_NOTIFY_DEV_BATTERY, CCIC_NOTIFY_ID_POWER_STATUS,
		0/*attach*/, 0, 0);
}

bool sm5713_usbpd_data_role_swap(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;

	pr_info("%s - %s, %s, data_role_swap : %d\n", __func__, 
		pdic_data->typec_power_role == TYPEC_DEVICE ? "ufp":"dfp",
		pdic_data->typec_data_role == TYPEC_SINK ? "snk":"src",
		manager->data_role_swap);

	return manager->data_role_swap;
}

int sm5713_usbpd_get_identity(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	manager->Vendor_ID = policy->rx_data_obj[1].id_header.usb_vendor_id;
	manager->Product_ID = policy->rx_data_obj[3].product_vdo.product_id;
	manager->Device_Version =
		policy->rx_data_obj[3].product_vdo.device_version;

	pr_info("%s, Vendor_ID : 0x%x, Product_ID : 0x%x, Device Version : 0x%x\n",
		__func__, manager->Vendor_ID, manager->Product_ID,
		manager->Device_Version);

	if (sm5713_usbpd_check_accessory(manager))
		pr_info("%s, Samsung Accessory Connected.\n", __func__);

	return 0;
}

int sm5713_usbpd_get_svids(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;

	manager->SVID_0 = policy->rx_data_obj[1].vdm_svid.svid_0;
	manager->SVID_1 = policy->rx_data_obj[1].vdm_svid.svid_1;

	pr_info("%s, SVID_0 : 0x%x, SVID_1 : 0x%x\n", __func__,
		manager->SVID_0, manager->SVID_1);

	if (manager->SVID_0 == PD_SID_1 || manager->SVID_1 == PD_SID_1) {
		manager->dp_is_connect = 1;
		/* If you want to support USB SuperSpeed when you connect
		 * Display port dongle, You should change dp_hs_connect depend
		 * on Pin assignment.If DP use 4lane(Pin Assignment C,E,A),
		 * dp_hs_connect is 1. USB can support HS.If DP use
		 * 2lane(Pin Assignment B,D,F), dp_hs_connect is 0. USB
		 * can support SS
		 */
		manager->dp_hs_connect = 1;
		/* notify to dp event */
		sm5713_ccic_event_work(pdic_data,
				CCIC_NOTIFY_DEV_DP,
				CCIC_NOTIFY_ID_DP_CONNECT,
				CCIC_NOTIFY_ATTACH,
				manager->Vendor_ID,
				manager->Product_ID);
		/* recheck this notifier */
		sm5713_ccic_event_work(pdic_data,
			CCIC_NOTIFY_DEV_USB_DP,
			CCIC_NOTIFY_ID_USB_DP,
			manager->dp_is_connect /*attach*/,
			manager->dp_hs_connect, 0);
	}

	return 0;
}

int sm5713_usbpd_get_modes(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	manager->Standard_Vendor_ID =
			policy->rx_data_obj[0].structured_vdm.svid;

	pr_info("%s, Standard_Vendor_ID = 0x%x\n", __func__,
		manager->Standard_Vendor_ID);

	return 0;
}

int sm5713_usbpd_enter_mode(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	manager->Standard_Vendor_ID =
		policy->rx_data_obj[0].structured_vdm.svid;
	manager->is_samsung_accessory_enter_mode = 1;

	pr_info("%s, entermode = %s\n", __func__,
		manager->is_samsung_accessory_enter_mode ? "true" : "false");

	return 0;
}

int sm5713_usbpd_exit_mode(struct sm5713_usbpd_data *pd_data, unsigned int mode)
{
	return 0;
}

data_obj_type sm5713_usbpd_select_capability(struct sm5713_usbpd_data *pd_data)
{
	data_obj_type obj;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	int pdo_num = pd_noti.sink_status.selected_pdo_num;
#endif
	obj.request_data_object.no_usb_suspend = 1;
	obj.request_data_object.usb_comm_capable = 1;
	obj.request_data_object.capability_mismatch = 0;
	obj.request_data_object.give_back = 0;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	obj.request_data_object.min_current =
		pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
	obj.request_data_object.op_current =
		pd_noti.sink_status.power_list[pdo_num].max_current / USBPD_CURRENT_UNIT;
	obj.request_data_object.object_position =
		pd_noti.sink_status.selected_pdo_num;
#endif
	return obj;
}

int sm5713_usbpd_evaluate_capability(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;
	int i = 0;
	int power_type = 0;
	int pd_volt = 0, pd_current;
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	int available_pdo_num = 0;
	PDIC_SINK_STATUS *pdic_sink_status = &pd_noti.sink_status;
#endif
	data_obj_type *pd_obj;

	for (i = 0; i < policy->rx_msg_header.num_data_objs; i++) {
		pd_obj = &policy->rx_data_obj[i];
		power_type = pd_obj->power_data_obj_supply_type.supply_type;
		switch (power_type) {
		case POWER_TYPE_FIXED:
			pd_volt = pd_obj->power_data_obj.voltage;
			pd_current = pd_obj->power_data_obj.max_current;
			dev_info(pd_data->dev, "[%d] FIXED volt(%d)mV, max current(%d)\n",
				i+1, pd_volt * USBPD_VOLT_UNIT,
				pd_current * USBPD_CURRENT_UNIT);

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
			if (pd_volt * USBPD_VOLT_UNIT <= MAX_CHARGING_VOLT)
				available_pdo_num = i + 1;
			pdic_sink_status->power_list[i + 1].max_voltage =
						pd_volt * USBPD_VOLT_UNIT;
			pdic_sink_status->power_list[i + 1].max_current =
						pd_current * USBPD_CURRENT_UNIT;
#endif
			break;
		case POWER_TYPE_BATTERY:
			pd_volt = pd_obj->power_data_obj_battery.max_voltage;
			dev_info(pd_data->dev, "[%d] BATTERY volt(%d)mV\n",
						i+1, pd_volt * USBPD_VOLT_UNIT);
			break;
		case POWER_TYPE_VARIABLE:
			pd_volt = pd_obj->power_data_obj_variable.max_voltage;
			dev_info(pd_data->dev, "[%d] VARIABLE volt(%d)mV\n",
						i+1, pd_volt * USBPD_VOLT_UNIT);
			break;
		default:
			dev_err(pd_data->dev, "[%d] Power Type Error\n", i+1);
			break;
		}
	}

	if (pdic_sink_status->rp_currentlvl == RP_CURRENT_ABNORMAL) {
		available_pdo_num = 1;
		pdic_sink_status->power_list[1].max_current =
			pdic_sink_status->power_list[1].max_current > 1800 ?
			1800 : pdic_sink_status->power_list[1].max_current;
		dev_info(pd_data->dev, "Fixed max_current to 1.8A because of vbus short\n");
	}

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	if ((pdic_sink_status->available_pdo_num > 0) &&
			(pdic_sink_status->available_pdo_num != available_pdo_num)) {
		policy->send_sink_cap = 1;
		pdic_sink_status->selected_pdo_num = 1;
	}
	pdic_sink_status->available_pdo_num = available_pdo_num;
	manager->origin_available_pdo_num = available_pdo_num;
	return available_pdo_num;
#else
	return 1; /* select default first obj */
#endif
}

/* return: 0: cab be met, -1: cannot be met, -2: could be met later */
int sm5713_usbpd_match_request(struct sm5713_usbpd_data *pd_data)
{
	unsigned int supply_type
	= pd_data->source_request_obj.power_data_obj_supply_type.supply_type;
	unsigned int mismatch, max_min, op, pos;

	if (supply_type == POWER_TYPE_FIXED) {
		pr_info("REQUEST: FIXED\n");
		goto log_fixed_variable;
	} else if (supply_type == POWER_TYPE_VARIABLE) {
		pr_info("REQUEST: VARIABLE\n");
		goto log_fixed_variable;
	} else if (supply_type == POWER_TYPE_BATTERY) {
		pr_info("REQUEST: BATTERY\n");
		goto log_battery;
	} else {
		pr_info("REQUEST: UNKNOWN Supply type.\n");
		return -1;
	}

log_fixed_variable:
	mismatch =
		pd_data->source_request_obj.request_data_object.capability_mismatch;
	max_min = pd_data->source_request_obj.request_data_object.min_current;
	op = pd_data->source_request_obj.request_data_object.op_current;
	pos = pd_data->source_request_obj.request_data_object.object_position;
	pr_info("Obj position: %d\n", pos);
	pr_info("Mismatch: %d\n", mismatch);
	pr_info("Operating Current: %d mA\n", op*10);
	if (pd_data->source_request_obj.request_data_object.give_back)
		pr_info("Min current: %d mA\n", max_min*10);
	else
		pr_info("Max current: %d mA\n", max_min*10);

	if ((pos > pd_data->source_msg_header.num_data_objs) ||
			(op > pd_data->source_data_obj.power_data_obj.max_current)) {
		pr_info("Invalid Request Message.\n");
		return -1;
	}
	return 0;

log_battery:
	mismatch =
		pd_data->source_request_obj.request_data_object_battery.capability_mismatch;
	return 0;
}

static void sm5713_usbpd_read_msg(struct sm5713_usbpd_data *pd_data)
{
	int i;

	pd_data->policy.rx_msg_header.word
		= pd_data->protocol_rx.msg_header.word;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++) {
		pd_data->policy.rx_data_obj[i].object
			= pd_data->protocol_rx.data_obj[i].object;
	}
}

static void sm5713_usbpd_protocol_tx(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_protocol_data *tx = &pd_data->protocol_tx;

	if (tx->msg_header.num_data_objs == 0 &&
			tx->msg_header.msg_type == USBPD_Soft_Reset)
		pd_data->protocol_rx.state = PRL_Rx_Wait_for_PHY_Message;

	tx->status = DEFAULT_PROTOCOL_NONE;

	if (pd_data->phy_ops.tx_msg(pd_data, &tx->msg_header, tx->data_obj))
		dev_err(pd_data->dev, "%s error\n", __func__);
	tx->state = PRL_Tx_Message_Sent;
	tx->msg_header.word = 0;
}

/* return 1: sent with goodcrc, 0: fail */
bool sm5713_usbpd_send_msg(struct sm5713_usbpd_data *pd_data, msg_header_type *header,
		data_obj_type *obj)
{
	int i;

	if (obj)
		for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
			pd_data->protocol_tx.data_obj[i].object = obj[i].object;
	else
		header->num_data_objs = 0;

	header->spec_revision = USBPD_REV_20;
	pd_data->protocol_tx.msg_header.word = header->word;
	sm5713_usbpd_protocol_tx(pd_data);

	return true;
}

inline bool sm5713_usbpd_send_ctrl_msg(struct sm5713_usbpd_data *d, msg_header_type *h,
		unsigned int msg, unsigned int dr, unsigned int pr)
{
	h->msg_type = msg;
	h->port_data_role = dr;
	h->port_power_role = pr;
	h->num_data_objs = 0;
	return sm5713_usbpd_send_msg(d, h, 0);
}

/* return: 0 if timed out, positive is status */
inline unsigned int sm5713_usbpd_wait_msg(struct sm5713_usbpd_data *pd_data,
				unsigned int msg_status, unsigned int ms)
{
	unsigned long ret;

	ret = pd_data->phy_ops.get_status(pd_data, msg_status);
	if (ret) {
		pd_data->policy.abnormal_state = false;
		return ret;
	}
	dev_info(pd_data->dev,
		"%s: msg_status = %d, time = %d\n", __func__, msg_status, ms);
	/* wait */
	reinit_completion(&pd_data->msg_arrived);
	pd_data->wait_for_msg_arrived = msg_status;
	ret = wait_for_completion_timeout(&pd_data->msg_arrived,
						msecs_to_jiffies(ms));

	if (!pd_data->policy.state) {
		dev_err(pd_data->dev,
			"%s : return for policy state error\n", __func__);
		pd_data->policy.abnormal_state = true;
		return 0;
	}

	pd_data->policy.abnormal_state = false;

	return pd_data->phy_ops.get_status(pd_data, msg_status);
}

static void sm5713_usbpd_check_vdm(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	unsigned int cmd, cmd_type, vdm_type;

	cmd = pd_data->policy.rx_data_obj[0].structured_vdm.command;
	cmd_type = pd_data->policy.rx_data_obj[0].structured_vdm.command_type;
	vdm_type = pd_data->policy.rx_data_obj[0].structured_vdm.vdm_type;

	dev_info(pd_data->dev, "%s: cmd = %x, cmd_type = %x, vdm_type = %x\n",
			__func__, cmd, cmd_type, vdm_type);

	if (vdm_type == Unstructured_VDM) {
		pdic_data->status_reg |= UVDM_MSG;
		return;
	}

	if (cmd_type == Initiator) {
		switch (cmd) {
		case Discover_Identity:
			pdic_data->status_reg |= VDM_DISCOVER_IDENTITY;
			break;
		case Discover_SVIDs:
			pdic_data->status_reg |= VDM_DISCOVER_SVID;
			break;
		case Discover_Modes:
			pdic_data->status_reg |= VDM_DISCOVER_MODE;
			break;
		case Enter_Mode:
			pdic_data->status_reg |= VDM_ENTER_MODE;
			break;
		case Exit_Mode:
			pdic_data->status_reg |= VDM_EXIT_MODE;
			break;
		case Attention:
			pdic_data->status_reg |= VDM_ATTENTION;
			break;
		case DisplayPort_Status_Update:
			pdic_data->status_reg |= MSG_PASS;
			break;
		case DisplayPort_Configure:
			pdic_data->status_reg |= MSG_PASS;
			break;
		}
	} else if (cmd_type == Responder_ACK) {
		switch (cmd) {
		case Discover_Identity:
			pdic_data->status_reg |= VDM_DISCOVER_IDENTITY;
			break;
		case Discover_SVIDs:
			pdic_data->status_reg |= VDM_DISCOVER_SVID;
			break;
		case Discover_Modes:
			pdic_data->status_reg |= VDM_DISCOVER_MODE;
			break;
		case Enter_Mode:
			pdic_data->status_reg |= VDM_ENTER_MODE;
			break;
		case Exit_Mode:
			pdic_data->status_reg |= VDM_EXIT_MODE;
			break;
		case DisplayPort_Status_Update:
			pdic_data->status_reg |= VDM_DP_STATUS_UPDATE;
			break;
		case DisplayPort_Configure:
			pdic_data->status_reg |= VDM_DP_CONFIGURE;
			break;
		}
	}
}

void sm5713_usbpd_protocol_rx(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_protocol_data *rx = &pd_data->protocol_rx;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;
	u8 ext_msg;

	if (pd_data->phy_ops.rx_msg(pd_data, &rx->msg_header, rx->data_obj)) {
		dev_err(pd_data->dev, "%s IO Error\n", __func__);
		return;
	}
	dev_info(pd_data->dev, "%s: stored_message_id = %x, msg_id = %d\n",
			__func__, rx->stored_message_id, rx->msg_header.msg_id);
	if (rx->msg_header.word == 0) {
		dev_err(pd_data->dev, "[Rx] No Message.\n");
		return; /* no message */
	} else if (rx->msg_header.msg_type == USBPD_Soft_Reset) {
		pr_info("%s : Got SOFT_RESET.\n", __func__);
		pdic_data->status_reg |= MSG_SOFTRESET;
		return;
	}
	dev_err(pd_data->dev, "[Rx] [0x%x] [0x%x]\n",
		rx->msg_header.word, rx->data_obj[0].object);

	if (rx->stored_message_id != rx->msg_header.msg_id) {
		rx->stored_message_id = rx->msg_header.msg_id;

		sm5713_usbpd_read_msg(pd_data);

		ext_msg = pd_data->policy.rx_msg_header.byte[1] & 0x80;
		dev_info(pd_data->dev, "%s: ext_msg = %x, obj_num = %d, msg_type = %d\n",
			__func__, ext_msg, pd_data->policy.rx_msg_header.num_data_objs,
					pd_data->policy.rx_msg_header.msg_type);

		if (ext_msg && pd_data->policy.rx_msg_header.spec_revision == 0) {
			return;
		} else if (pd_data->policy.rx_msg_header.num_data_objs > 0) {
			switch (pd_data->policy.rx_msg_header.msg_type) {
			case USBPD_Source_Capabilities:
				pdic_data->status_reg |= MSG_SRC_CAP;
				break;
			case USBPD_Request:
				pdic_data->status_reg |= MSG_REQUEST;
				break;
			case USBPD_BIST:
				if (pd_data->policy.state == PE_SNK_Ready ||
						pd_data->policy.state == PE_SRC_Ready) {
					if (pd_data->policy.rx_data_obj[0].bist_data_object.bist_mode ==
							BIST_Carrier_Mode2) {
						pdic_data->status_reg |= MSG_BIST_M2;
					} else if (pd_data->policy.rx_data_obj[0].bist_data_object.bist_mode ==
							BIST_Test_Mode) {
						pdic_data->status_reg |= MSG_NONE;
					} else {
						/* Not Support */
					}
				}
				break;
			case USBPD_Sink_Capabilities:
				pdic_data->status_reg |= MSG_SNK_CAP;
				break;
			case USBPD_Vendor_Defined:
				if (!sm5713_get_rx_buf_st(pd_data))
					sm5713_usbpd_check_vdm(pd_data);
				break;
			default:
				break;
			}
		} else {
			switch (pd_data->policy.rx_msg_header.msg_type) {
			case USBPD_GoodCRC:
				/* Do nothing */
				break;
			case USBPD_Ping:
				/* Do nothing */
				break;
			case USBPD_GotoMin:
				if (pd_data->policy.state == PE_SNK_Ready)
					pd_data->policy.state =
						PE_SNK_Transition_Sink;
				break;
			case USBPD_Accept:
				pdic_data->status_reg |= MSG_ACCEPT;
				break;
			case USBPD_Reject:
				pdic_data->status_reg |= MSG_REJECT;
				break;
			case USBPD_PS_RDY:
				pdic_data->status_reg |= MSG_PSRDY;
				break;
			case USBPD_Get_Source_Cap:
				pdic_data->status_reg |= MSG_GET_SRC_CAP;
				break;
			case USBPD_Get_Sink_Cap:
				pdic_data->status_reg |= MSG_GET_SNK_CAP;
				break;
			case USBPD_DR_Swap:
				pdic_data->status_reg |= MSG_DR_SWAP;
				break;
			case USBPD_PR_Swap:
				pdic_data->status_reg |= MSG_PR_SWAP;
				break;
			case USBPD_VCONN_Swap:
				pdic_data->status_reg |= MSG_VCONN_SWAP;
				break;
			case USBPD_Wait:
				pdic_data->status_reg |= MSG_WAIT;
				break;
			case USBPD_Soft_Reset:
				pdic_data->status_reg |= MSG_SOFTRESET;
				break;
			default:
				break;
			}
		}
	}
}

void sm5713_usbpd_rx_hard_reset(struct device *dev)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);

	sm5713_usbpd_reinit(dev);
	sm5713_usbpd_policy_reset(pd_data, HARDRESET_RECEIVED);
}

void sm5713_usbpd_rx_soft_reset(struct sm5713_usbpd_data *pd_data)
{
	sm5713_usbpd_reinit(pd_data->dev);
	sm5713_usbpd_policy_reset(pd_data, SOFTRESET_RECEIVED);
}

void sm5713_usbpd_set_ops(struct device *dev, usbpd_phy_ops_type *ops)
{
	struct sm5713_usbpd_data *pd_data = (struct sm5713_usbpd_data *) dev_get_drvdata(dev);

	pd_data->phy_ops.tx_msg = ops->tx_msg;
	pd_data->phy_ops.rx_msg = ops->rx_msg;
	pd_data->phy_ops.hard_reset = ops->hard_reset;
	pd_data->phy_ops.set_power_role = ops->set_power_role;
	pd_data->phy_ops.get_power_role = ops->get_power_role;
	pd_data->phy_ops.set_data_role = ops->set_data_role;
	pd_data->phy_ops.get_data_role = ops->get_data_role;
	pd_data->phy_ops.get_vconn_source = ops->get_vconn_source;
	pd_data->phy_ops.set_vconn_source = ops->set_vconn_source;
	pd_data->phy_ops.set_check_msg_pass = ops->set_check_msg_pass;
	pd_data->phy_ops.get_status = ops->get_status;
	pd_data->phy_ops.poll_status = ops->poll_status;
	pd_data->phy_ops.driver_reset = ops->driver_reset;
	pd_data->phy_ops.get_short_state = ops->get_short_state;
}

#ifdef CONFIG_OF
static int of_sm5713_usbpd_dt(struct sm5713_usbpd_manager_data *_data)
{
	int ret = 0;
	struct device_node *np =
		of_find_node_by_name(NULL, "pdic-manager");

	if (np == NULL) {
		pr_err("%s np NULL\n", __func__);
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "pdic,max_power",
			&_data->max_power);
	if (ret < 0)
		pr_err("%s error reading max_power %d\n",
				__func__, _data->max_power);

	ret = of_property_read_u32(np, "pdic,op_power",
			&_data->op_power);
	if (ret < 0)
		pr_err("%s error reading op_power %d\n",
				__func__, _data->max_power);

	ret = of_property_read_u32(np, "pdic,max_current",
			&_data->max_current);
	if (ret < 0)
		pr_err("%s error reading max_current %d\n",
				__func__, _data->max_current);

	ret = of_property_read_u32(np, "pdic,min_current",
			&_data->min_current);
	if (ret < 0)
		pr_err("%s error reading min_current %d\n",
				__func__, _data->min_current);

	_data->giveback = of_property_read_bool(np,
						"pdic,giveback");
	_data->usb_com_capable = of_property_read_bool(np,
						"pdic,usb_com_capable");
	_data->no_usb_suspend = of_property_read_bool(np,
						"pdic,no_usb_suspend");

	/* source capability */
	ret = of_property_read_u32(np, "source,max_voltage",
			&_data->source_max_volt);
	ret = of_property_read_u32(np, "source,min_voltage",
			&_data->source_min_volt);
	ret = of_property_read_u32(np, "source,max_power",
			&_data->source_max_power);

	return ret;
}
#endif

static void sm5713_usbpd_init_source_cap_data(struct sm5713_usbpd_manager_data *_data)
{
	msg_header_type *msg_header = &_data->pd_data->source_msg_header;
	data_obj_type *data_obj = &_data->pd_data->source_data_obj;

	msg_header->msg_type = USBPD_Source_Capabilities;
	msg_header->port_data_role = USBPD_DFP;
	msg_header->spec_revision = 1;
	msg_header->port_power_role = USBPD_SOURCE;
	msg_header->num_data_objs = 1;

	data_obj->power_data_obj.max_current = 900 / 10;
	data_obj->power_data_obj.voltage = 5000 / 50;
	data_obj->power_data_obj.supply = POWER_TYPE_FIXED;
	data_obj->power_data_obj.data_role_swap = 1;
	data_obj->power_data_obj.dual_role_power = 1;
	data_obj->power_data_obj.usb_suspend_support = 1;
	data_obj->power_data_obj.usb_comm_capable = 1;
	data_obj->power_data_obj.reserved = 0;
}

static int sm5713_usbpd_manager_init(struct sm5713_usbpd_data *pd_data)
{
	int ret = 0;
	struct sm5713_usbpd_manager_data *manager = &pd_data->manager;

	if (manager == NULL) {
		pr_err("%s, usbpd manager data is error!!\n", __func__);
		return -ENOMEM;
	}
	pr_info("%s\n", __func__);
	ret = of_sm5713_usbpd_dt(manager);
#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	fp_select_pdo = sm5713_select_pdo;
#endif
	manager->pd_data = pd_data;
	manager->power_role_swap = true;
	manager->data_role_swap = true;
	manager->vconn_source_swap = false;
	manager->acc_type = CCIC_DOCK_DETACHED;
	manager->Vendor_ID = 0;
	manager->Product_ID = 0;
	manager->Device_Version = 0;
	manager->alt_sended = 0;
	manager->SVID_0 = 0;
	manager->SVID_1 = 0;
	manager->Standard_Vendor_ID = 0;
	manager->is_samsung_accessory_enter_mode = 0;
	manager->fled_flash_enable = 0;
	manager->fled_torch_enable = 0;
	manager->origin_available_pdo_num = 0;
	manager->uvdm_out_ok = 1;
	manager->uvdm_in_ok = 1;
	manager->dr_swap_cnt = 0;

	init_waitqueue_head(&manager->uvdm_out_wq);
	init_waitqueue_head(&manager->uvdm_in_wq);

	sm5713_usbpd_init_source_cap_data(manager);
	INIT_DELAYED_WORK(&manager->acc_detach_handler,
			sm5713_usbpd_acc_detach_handler);
	INIT_DELAYED_WORK(&manager->new_power_handler,
			sm5713_usbpd_manager_new_power_handler);

	return ret;
}

static void sm5713_usbpd_rx_layer_init(struct sm5713_protocol_data *rx)
{
	int i;

	rx->stored_message_id = USBPD_nMessageIDCount+1;
	rx->msg_header.word = 0;
	rx->state = 0;
	rx->status = DEFAULT_PROTOCOL_NONE;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		rx->data_obj[i].object = 0;
}

static void sm5713_usbpd_tx_layer_init(struct sm5713_protocol_data *tx)
{
	int i;

	tx->stored_message_id = USBPD_nMessageIDCount+1;
	tx->msg_header.word = 0;
	tx->state = 0;
	tx->status = DEFAULT_PROTOCOL_NONE;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		tx->data_obj[i].object = 0;
}

void sm5713_usbpd_tx_request_discard(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_protocol_data *tx = &pd_data->protocol_tx;
	int i;

	tx->msg_header.word = 0;
	for (i = 0; i < USBPD_MAX_COUNT_MSG_OBJECT; i++)
		tx->data_obj[i].object = 0;

	dev_err(pd_data->dev, "%s\n", __func__);
}

void sm5713_usbpd_init_protocol(struct sm5713_usbpd_data *pd_data)
{
	struct sm5713_policy_data *policy = &pd_data->policy;
	struct sm5713_phydrv_data *pdic_data = pd_data->phy_driver_data;

	if (pdic_data->is_jig_case_on) {
		pr_info("%s: Do not protocol reset.\n", __func__);
		return;
	}

	if (policy->state == PE_SRC_Startup ||
			policy->state == PE_SNK_Startup ||
			policy->state == PE_SRC_Send_Soft_Reset ||
			policy->state == PE_SNK_Send_Soft_Reset ||
			policy->state == PE_SRC_Soft_Reset ||
			policy->state == PE_SNK_Soft_Reset) {
		if ((pdic_data->reset_done == 0) && !pdic_data->is_mpsm_exit)
			sm5713_protocol_layer_reset(pd_data);
	}

	sm5713_usbpd_rx_layer_init(&pd_data->protocol_rx);
	sm5713_usbpd_tx_layer_init(&pd_data->protocol_tx);
}

static void sm5713_usbpd_init_counters(struct sm5713_usbpd_data *pd_data)
{
	pr_info("%s: init counter\n", __func__);
	pd_data->counter.retry_counter = 0;
	pd_data->counter.message_id_counter = 0;
	pd_data->counter.caps_counter = 0;
	pd_data->counter.hard_reset_counter = 0;
	pd_data->counter.swap_hard_reset_counter = 0;
	pd_data->counter.discover_identity_counter = 0;
}

void sm5713_usbpd_reinit(struct device *dev)
{
	struct sm5713_usbpd_data *pd_data = dev_get_drvdata(dev);

	sm5713_usbpd_init_counters(pd_data);
	sm5713_usbpd_init_protocol(pd_data);
	sm5713_usbpd_init_policy(pd_data);
	reinit_completion(&pd_data->msg_arrived);
	pd_data->wait_for_msg_arrived = 0;
	complete(&pd_data->msg_arrived);
}

int sm5713_usbpd_init(struct device *dev, void *phy_driver_data)
{
	struct sm5713_usbpd_data *pd_data;

	if (!dev)
		return -EINVAL;

	pd_data = kzalloc(sizeof(struct sm5713_usbpd_data), GFP_KERNEL);

	if (!pd_data)
		return -ENOMEM;

	pd_data->dev = dev;
	pd_data->phy_driver_data = phy_driver_data;
	dev_set_drvdata(dev, pd_data);

#ifdef CONFIG_USB_TYPEC_MANAGER_NOTIFIER
	pd_noti.pusbpd = pd_data;
	pd_noti.sink_status.current_pdo_num = 0;
	pd_noti.sink_status.selected_pdo_num = 0;
#endif
	sm5713_usbpd_init_counters(pd_data);
	sm5713_usbpd_init_protocol(pd_data);
	sm5713_usbpd_init_policy(pd_data);
	sm5713_usbpd_manager_init(pd_data);

	INIT_WORK(&pd_data->worker, sm5713_usbpd_policy_work);
	init_completion(&pd_data->msg_arrived);

	return 0;
}
