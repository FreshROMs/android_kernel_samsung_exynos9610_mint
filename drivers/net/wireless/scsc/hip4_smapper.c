/******************************************************************************
 *
 * Copyright (c) 2014 - 2019 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/

#include <scsc/scsc_mx.h>
#include <scsc/scsc_mifram.h>
#include <scsc/scsc_logring.h>
#include <linux/ratelimit.h>

#include "debug.h"
#include "dev.h"
#include "hip4.h"
#include "hip4_smapper.h"

#define SMAPPER_GRANULARITY	(4 * 1024)

static void hip4_smapper_refill_isr(int irq, void *data);

static int hip4_smapper_alloc_bank(struct slsi_dev *sdev, struct hip4_priv *priv, enum smapper_banks bank_name, u32 entry_size, bool is_large)
{
	u16 i;
	struct hip4_smapper_bank *bank = &(priv)->smapper_banks[bank_name];
	struct hip4_smapper_control *control = &(priv)->smapper_control;
	int err;

	SLSI_DBG4_NODEV(SLSI_SMAPPER, "Init bank %d entry_size %d is_large %d\n", bank_name, entry_size, is_large);
	bank->entry_size = entry_size;

	/* function returns negative number if an error occurs, otherwise returns the bank number */
	err = scsc_service_mifsmapper_alloc_bank(sdev->service, is_large, bank->entry_size, &bank->entries);
	if (err < 0) {
		SLSI_DBG4_NODEV(SLSI_SMAPPER, "Error allocating bank %d\n", err);
		return -ENOMEM;
	}

	bank->bank = (u32)err;
	if (bank->bank >= HIP4_SMAPPER_TOTAL_BANKS) {
		scsc_service_mifsmapper_free_bank(sdev->service, bank->bank);
		SLSI_DBG4_NODEV(SLSI_SMAPPER, "Incorrect bank_num %d\n", bank->bank);
		return -ENOMEM;
	}

	bank->skbuff = kmalloc_array(bank->entries, sizeof(struct sk_buff *),
					GFP_KERNEL);
	bank->skbuff_dma = kmalloc_array(bank->entries, sizeof(dma_addr_t),
					GFP_KERNEL);
	if (!bank->skbuff || !bank->skbuff_dma) {
		kfree(bank->skbuff_dma);
		kfree(bank->skbuff);
		return -ENOMEM;
	}

	for (i = 0; i < bank->entries; i++)
		bank->skbuff[i] = NULL;

	bank->align = scsc_service_get_alignment(sdev->service);
	bank->in_use = true;

	/* update the mapping with BANK# in WLAN with PHY BANK#*/
	control->lookuptable[bank->bank] = bank_name;

	return 0;
}

static int hip4_smapper_allocate_skb_buffer_entry(struct slsi_dev *sdev, struct hip4_smapper_bank *bank, int idx)
{
	struct sk_buff *skb;
	int err;

	skb = alloc_skb(bank->entry_size, GFP_ATOMIC);
	if (!skb) {
		SLSI_DBG4_NODEV(SLSI_SMAPPER, "Not enough memory\n");
		return -ENOMEM;
	}

	slsi_skb_cb_init(skb);
	SLSI_DBG4_NODEV(SLSI_SMAPPER, "SKB allocated: 0x%p at bank %d entry %d\n", skb, bank->bank, idx);
	bank->skbuff_dma[idx] = dma_map_single(sdev->dev, skb->data,
					     bank->entry_size, DMA_FROM_DEVICE);
	err = dma_mapping_error(sdev->dev, bank->skbuff_dma[idx]);
	if (err) {
		SLSI_DBG4_NODEV(SLSI_SMAPPER, "Error mapping SKB: 0x%p at bank %d entry %d\n", skb, bank->bank, idx);
		kfree_skb(skb);
		return err;
	}

	/* Check alignment */
	if (!IS_ALIGNED(bank->skbuff_dma[idx], bank->align)) {
		SLSI_DBG4_NODEV(SLSI_SMAPPER, "Phys address: 0x%x not %d aligned. Unmap memory and return error\n",
				bank->skbuff_dma[idx], bank->align);
		dma_unmap_single(sdev->dev, bank->skbuff_dma[idx], bank->entry_size, DMA_FROM_DEVICE);
		kfree_skb(skb);
		bank->skbuff_dma[idx] = 0;
		return -ENOMEM;
	}
	bank->skbuff[idx] = skb;
	return 0;
}

/* Pre-Allocate the skbs for the RX entries */
static int hip4_smapper_allocate_skb_buffers(struct slsi_dev *sdev, struct hip4_smapper_bank *bank)
{
	unsigned int i;
	unsigned int n;
	int res;

	if (!bank)
		return -EINVAL;

	n = bank->entries;
	for (i = 0; i < n; i++) {
		if (!bank->skbuff[i]) {
			res = hip4_smapper_allocate_skb_buffer_entry(sdev, bank, i);
			if (res != 0)
				return res;
		}
	}

	return 0;
}

static int hip4_smapper_free_skb_buffers(struct slsi_dev *sdev, struct hip4_smapper_bank *bank)
{
	unsigned int i;
	unsigned int n;

	if (!bank)
		return -EINVAL;

	n = bank->entries;
	for (i = 0; i < n; i++) {
		if (bank->skbuff[i]) {
			SLSI_DBG4_NODEV(SLSI_SMAPPER, "SKB free: 0x%p at bank %d entry %d\n", bank->skbuff[i], bank->bank, i);
			dma_unmap_single(sdev->dev, bank->skbuff_dma[i], bank->entry_size, DMA_FROM_DEVICE);
			bank->skbuff_dma[i] = 0;
			kfree_skb(bank->skbuff[i]);
			bank->skbuff[i] = NULL;
		}
	}

	return 0;
}

static int hip4_smapper_program(struct slsi_dev *sdev, struct hip4_smapper_bank *bank)
{
	unsigned int n;

	if (!bank)
		return -EINVAL;

	n = bank->entries;

	SLSI_DBG4_NODEV(SLSI_SMAPPER, "Programming Bank %d\n", bank->bank);

	return scsc_service_mifsmapper_write_sram(sdev->service, bank->bank, n, 0, bank->skbuff_dma);
}

/* refill ISR. FW signals the Host whenever it wants to refill the smapper buffers */
/* Only the Host Owned Buffers should be refilled  */
static void hip4_smapper_refill_isr(int irq, void *data)
{
	struct slsi_hip4	*hip = (struct slsi_hip4 *)data;
	struct slsi_dev 	*sdev = container_of(hip, struct slsi_dev, hip4_inst);
	struct hip4_smapper_control *control;
	struct hip4_smapper_bank *bank;
	enum smapper_banks i;
	unsigned long flags;
	/* Temporary removed
	 * static DEFINE_RATELIMIT_STATE(ratelimit, 1 * HZ, 1);
	 */

	control = &(hip->hip_priv->smapper_control);
#ifdef CONFIG_SCSC_QOS
	/* Ignore request if TPUT is low or platform is in suspend */
	if (hip->hip_priv->pm_qos_state == SCSC_QOS_DISABLED ||
		atomic_read(&hip->hip_priv->in_suspend) ||
		*control->mbox_ptr == 0x0) {
#else
	/* Ignore if platform is in suspend */
	if (atomic_read(&hip->hip_priv->in_suspend) ||
		*control->mbox_ptr == 0x0) {
#endif
	/*
	 * Temporary removed
	 * if (__ratelimit(&ratelimit))
	 *	SLSI_DBG1_NODEV(SLSI_SMAPPER, "Ignore SMAPPER request. Invalid state.\n");
	 */
		/* Clear interrupt */
		scsc_service_mifintrbit_bit_clear(sdev->service, control->th_req);
		return;
	}

	spin_lock_irqsave(&control->smapper_lock, flags);
	/* Check if FW has requested a BANK configuration */
	if (HIP4_SMAPPER_BANKS_CHECK_CONFIGURE(*control->mbox_ptr)) {
		/* Temporary removed
		* SLSI_DBG4_NODEV(SLSI_SMAPPER, "Trigger SMAPPER configuration\n");
		*/
		scsc_service_mifsmapper_configure(sdev->service, SMAPPER_GRANULARITY);
		HIP4_SMAPPER_BANKS_CONFIGURE_DONE(*control->mbox_ptr);
	}
	/* Read the first RX bank and check whether needs to be reprogrammed */
	for (i = RX_0; i < END_RX_BANKS; i++) {
		bank = &hip->hip_priv->smapper_banks[i];

		if (!bank->in_use)
			continue;

		if (HIP4_SMAPPER_GET_BANK_OWNER(bank->bank, *control->mbox_ptr) == HIP_SMAPPER_OWNER_HOST) {
			/* Temporary removed
			 * SLSI_DBG4_NODEV(SLSI_SMAPPER, "SKB allocation at bank %d\n", i);
			 */
			if (hip4_smapper_allocate_skb_buffers(sdev, bank)) {
				/* Temporary removed
				 * SLSI_DBG4_NODEV(SLSI_SMAPPER, "Error Allocating skb buffers at bank %d. Setting owner to FW\n", i);
				 */
				HIP4_SMAPPER_SET_BANK_OWNER(bank->bank, *control->mbox_ptr, HIP_SMAPPER_OWNER_FW);
				continue;
			}
			if (hip4_smapper_program(sdev, bank)) {
				/* Temporary removed
				 * SLSI_DBG4_NODEV(SLSI_SMAPPER, "Error Programming bank %d. Setting owner to FW\n", i);
				 */
				HIP4_SMAPPER_SET_BANK_OWNER(bank->bank, *control->mbox_ptr, HIP_SMAPPER_OWNER_FW);
				continue;
			}
			HIP4_SMAPPER_SET_BANK_STATE(bank->bank, *control->mbox_ptr, HIP_SMAPPER_STATUS_MAPPED);
			HIP4_SMAPPER_SET_BANK_OWNER(bank->bank, *control->mbox_ptr, HIP_SMAPPER_OWNER_FW);
		}
	}
	/* Inform FW that entries have been programmed */
	scsc_service_mifintrbit_bit_set(sdev->service, control->fh_ind, SCSC_MIFINTR_TARGET_R4);

	/* Clear interrupt */
	scsc_service_mifintrbit_bit_clear(sdev->service, control->th_req);

	spin_unlock_irqrestore(&control->smapper_lock, flags);
}

int hip4_smapper_consume_entry(struct slsi_dev *sdev, struct slsi_hip4 *hip, struct sk_buff *skb_fapi)
{
	struct sk_buff *skb;
	struct sk_buff *skb_big = NULL;
	struct hip4_smapper_bank *bank;
	u8 i;
	u8 bank_num;
	u8 entry;
	u8 num_entries;
	u16 len;
	u16 headroom;
	struct hip4_smapper_descriptor *desc;
	struct hip4_smapper_control *control;
	struct slsi_skb_cb *cb = slsi_skb_cb_get(skb_fapi);

	control = &(hip->hip_priv->smapper_control);

	desc = (struct hip4_smapper_descriptor *)skb_fapi->data;

	bank_num = desc->bank_num;
	entry = desc->entry_num;
	len = desc->entry_size;
	headroom = desc->headroom;


	if (bank_num >= HIP4_SMAPPER_TOTAL_BANKS) {
		SLSI_WARN_NODEV("Incorrect bank_num %d\n", bank_num);
		goto error;
	}

	/* Transform PHY BANK# with BANK# in Wlan service*/
	bank_num = control->lookuptable[bank_num];

	bank = &hip->hip_priv->smapper_banks[bank_num];

	if (entry > bank->entries) {
		SLSI_WARN_NODEV("Incorrect entry number %d\n", entry);
		goto error;
	}

	if (len > bank->entry_size) {

		/* If len > entry_size, we assume FW is using > 1 entry */
		num_entries = DIV_ROUND_UP(len, bank->entry_size);

		if ((entry + num_entries) > bank->entries) {
			SLSI_WARN_NODEV("Incorrect entry number %d num_entries %d\n", entry, num_entries);
			goto error;
		}

		/* allocate a skb to copy the multiple bank entries */
		skb_big = alloc_skb(len, GFP_ATOMIC);
		if (!skb_big) {
			SLSI_WARN_NODEV("big SKB allocation failed len:%d\n", len);
			goto error;
		}
		goto multi;
	}

	skb = bank->skbuff[entry];
	if (!skb) {
		SLSI_WARN_NODEV("SKB is NULL at bank %d entry %d\n", bank_num, entry);
		goto error;
	}

	bank->skbuff[entry] = NULL;
	dma_unmap_single(sdev->dev, bank->skbuff_dma[entry], bank->entry_size, DMA_FROM_DEVICE);
	bank->skbuff_dma[entry] = 0;

	hip4_smapper_allocate_skb_buffer_entry(sdev, bank, entry);

	skb_reserve(skb, headroom);
	skb_put(skb, len);
	cb->skb_addr = skb;
	SLSI_DBG4_NODEV(SLSI_SMAPPER, "Consumed Bank %d Entry %d Len %d SKB smapper: 0x%p, SKB fapi %p\n", bank_num, entry, len, skb, skb_fapi);
	return 0;
multi:
	for (i = 0; i < num_entries; i++, entry++) {
		u16 bytes;

		skb = bank->skbuff[entry];
		if (!skb) {
			SLSI_WARN_NODEV("SKB IS NULL at bank %d entry %d\n", bank_num, entry);
			goto error;
		}

		bank->skbuff[entry] = NULL;
		dma_unmap_single(sdev->dev, bank->skbuff_dma[entry], bank->entry_size, DMA_FROM_DEVICE);
		bank->skbuff_dma[entry] = 0;

		hip4_smapper_allocate_skb_buffer_entry(sdev, bank, entry);

		if (len > bank->entry_size)
			bytes = bank->entry_size - headroom;
		else
			bytes = len;

		/* jump to the offset where payload starts; only applicable for 1st entry */
		if (i == 0)
			skb_reserve(skb, headroom);

		/* do the memcpy to big SKB */
		memcpy(skb_put(skb_big, bytes), skb->data, bytes);

		/* Free the skb */
		kfree_skb(skb);
		len -= bytes;
	}
	cb->skb_addr = skb_big;
	SLSI_DBG4_NODEV(SLSI_SMAPPER, "Consumed Bank %d Entry %d Len %d SKB smapper: 0x%p, SKB fapi %p\n", bank_num, entry, len, skb, skb_fapi);
	return 0;
error:
	/* RX is broken.....*/
	if (skb_big)
		kfree_skb(skb_big);
	return -EIO;
}

void *hip4_smapper_get_skb_data(struct slsi_dev *sdev, struct slsi_hip4 *hip, struct sk_buff *skb_fapi)
{
	struct sk_buff *skb;
	struct slsi_skb_cb *cb = slsi_skb_cb_get(skb_fapi);
	struct hip4_smapper_control *control;

	control = &(hip->hip_priv->smapper_control);

	skb = (struct sk_buff *)cb->skb_addr;

	if (!skb) {
		SLSI_DBG4_NODEV(SLSI_SMAPPER, "NULL SKB smapper\n");
		return NULL;
	}

	SLSI_DBG4_NODEV(SLSI_SMAPPER, "Get SKB smapper: 0x%p, SKB fapi 0x%p\n", skb, skb_fapi);
	return skb->data;
}

struct sk_buff *hip4_smapper_get_skb(struct slsi_dev *sdev, struct slsi_hip4 *hip, struct sk_buff *skb_fapi)
{
	struct sk_buff *skb;
	struct slsi_skb_cb *cb = slsi_skb_cb_get(skb_fapi);
	struct hip4_smapper_control *control;

	control = &(hip->hip_priv->smapper_control);

	skb = (struct sk_buff *)cb->skb_addr;

	SLSI_DBG4_NODEV(SLSI_SMAPPER, "Get SKB smapper: 0x%p, SKB fapi 0x%p\n", skb, skb_fapi);
	cb->free_ma_unitdat = true;
	kfree_skb(skb_fapi);

	return skb;
}

void hip4_smapper_free_mapped_skb(struct sk_buff *skb)
{
	struct slsi_skb_cb *cb;

	if (!skb)
		return;

	cb = (struct slsi_skb_cb *)skb->cb;

	if (cb && !cb->free_ma_unitdat && cb->skb_addr) {
		kfree_skb(cb->skb_addr);
		cb->skb_addr = NULL;
	}
}

int hip4_smapper_init(struct slsi_dev *sdev, struct slsi_hip4 *hip)
{
	u8 i;
	struct hip4_smapper_control *control;

	SLSI_DBG4_NODEV(SLSI_SMAPPER, "SMAPPER init\n");

	control = &(hip->hip_priv->smapper_control);

	spin_lock_init(&control->smapper_lock);

	if (dma_set_mask_and_coherent(sdev->dev, DMA_BIT_MASK(64)) != 0)
		return -EIO;

	if (!scsc_mx_service_alloc_mboxes(sdev->service, 1, &control->mbox_scb)) {
		SLSI_DBG4_NODEV(SLSI_SMAPPER, "Unable to allocate mbox\n");
		return -ENODEV;
	}

	/* Claim the RX buffers */
	hip4_smapper_alloc_bank(sdev, hip->hip_priv, RX_0, SMAPPER_GRANULARITY, HIP4_SMAPPER_BANK_LARGE);
	hip4_smapper_alloc_bank(sdev, hip->hip_priv, RX_1, SMAPPER_GRANULARITY, HIP4_SMAPPER_BANK_LARGE);
	hip4_smapper_alloc_bank(sdev, hip->hip_priv, RX_2, SMAPPER_GRANULARITY, HIP4_SMAPPER_BANK_LARGE);
	hip4_smapper_alloc_bank(sdev, hip->hip_priv, RX_3, SMAPPER_GRANULARITY, HIP4_SMAPPER_BANK_LARGE);
	/*Pre-allocate buffers */
	hip4_smapper_allocate_skb_buffers(sdev, &hip->hip_priv->smapper_banks[RX_0]);
	hip4_smapper_allocate_skb_buffers(sdev, &hip->hip_priv->smapper_banks[RX_1]);
	hip4_smapper_allocate_skb_buffers(sdev, &hip->hip_priv->smapper_banks[RX_2]);
	hip4_smapper_allocate_skb_buffers(sdev, &hip->hip_priv->smapper_banks[RX_3]);

	/* Allocate Maxwell resources */
	control->th_req =
		scsc_service_mifintrbit_register_tohost(sdev->service, hip4_smapper_refill_isr, hip);
	control->fh_ind =
		scsc_service_mifintrbit_alloc_fromhost(sdev->service, SCSC_MIFINTR_TARGET_R4);

	control->mbox_ptr =
		scsc_mx_service_get_mbox_ptr(sdev->service, control->mbox_scb);

	/* All banks to REMAP and FW owner*/
	*control->mbox_ptr = 0x0;

	/* Update hip4 config table */
	hip->hip_control->config_v4.smapper_th_req =
		control->th_req;
	hip->hip_control->config_v4.smapper_fh_ind =
		control->fh_ind;
	hip->hip_control->config_v4.smapper_mbox_scb =
		(u8)control->mbox_scb;

	for (i = RX_0; i < END_RX_BANKS; i++) {
		u8 has_entries;
		u8 bank;

		has_entries = hip->hip_priv->smapper_banks[i].entries;
		if (has_entries) {
			/* Get the bank index */
			bank = hip->hip_priv->smapper_banks[i].bank;
			hip->hip_control->config_v4.smapper_bank_addr[bank] = scsc_service_mifsmapper_get_bank_base_address(sdev->service, bank);
			hip->hip_control->config_v4.smapper_entries_banks[bank] = has_entries;
			hip->hip_control->config_v4.smapper_pow_sz[bank] = 12; /* 4kB */
		}
	}
	return 0;
}

void hip4_smapper_deinit(struct slsi_dev *sdev, struct slsi_hip4 *hip)
{
	struct hip4_smapper_bank *bank;
	struct hip4_smapper_control *control;
	unsigned long flags;
	u8 i;

	SLSI_DBG4_NODEV(SLSI_SMAPPER, "SMAPPER deinit\n");
	control = &(hip->hip_priv->smapper_control);

	spin_lock_irqsave(&control->smapper_lock, flags);
	for (i = RX_0; i < END_RX_BANKS; i++) {
		bank = &hip->hip_priv->smapper_banks[i];
		bank->in_use = false;
		hip4_smapper_free_skb_buffers(sdev, bank);
		kfree(bank->skbuff_dma);
		kfree(bank->skbuff);
		scsc_service_mifsmapper_free_bank(sdev->service, bank->bank);
	}
	spin_unlock_irqrestore(&control->smapper_lock, flags);

	scsc_service_mifintrbit_unregister_tohost(sdev->service, control->th_req);
	scsc_service_mifintrbit_free_fromhost(sdev->service, control->fh_ind, SCSC_MIFINTR_TARGET_R4);
	scsc_service_free_mboxes(sdev->service, 1, control->mbox_scb);

}
