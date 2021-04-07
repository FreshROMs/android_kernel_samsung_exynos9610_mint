/****************************************************************************
 *
 * Copyright (c) 2014 - 2018 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __HIP4_SMAPPER_H__
#define __HIP4_SMAPPER_H__

struct slsi_dev;
struct slsi_hip4;

enum smapper_type {
	TX_5G,
	TX_2G,
	RX
};

#define HIP4_SMAPPER_TOTAL_BANKS	10

#define HIP4_SMAPPER_BANK_SMALL		false
#define HIP4_SMAPPER_BANK_LARGE		true

#define HIP_SMAPPER_OWNER_FW		0
#define HIP_SMAPPER_OWNER_HOST		1

#define HIP_SMAPPER_STATUS_REFILL	0
#define HIP_SMAPPER_STATUS_MAPPED	1

#define HIP4_SMAPPER_OTHER_CPU		0
#define HIP4_SMAPPER_OWN_CPU		1

#define HIP4_SMAPPER_STATE_OUT		0
#define HIP4_SMAPPER_STATE_WANT		1
#define HIP4_SMAPPER_STATE_CLAIM	2

#define HIP4_SMAPPER_BANKS_CHECK_CONFIGURE(reg) (((reg) >> 30) == 0 ? 1 : 0)
#define HIP4_SMAPPER_BANKS_CONFIGURE_DONE(reg) ((reg) = (reg) | 0xc0000000)

#define HIP4_SMAPPER_GET_BANK_STATE(b, reg)    (((0x1 << ((b) * 2)) & (reg)) > 0 ? 1 : 0)
#define HIP4_SMAPPER_GET_BANK_OWNER(b, reg)    (((0x2 << ((b) * 2)) & (reg)) > 0 ? 1 : 0)

#define HIP4_SMAPPER_SET_BANK_STATE(b, reg, val)  ((reg) = ((reg) & ~(0x1 << ((b) * 2))) | \
					((val) << ((b) * 2)))
#define HIP4_SMAPPER_SET_BANK_OWNER(b, reg, val)  ((reg) = (reg & ~(0x2 << ((b) * 2))) | \
					(((val) << 1) << ((b) * 2)))


struct hip4_smapper_descriptor {
	u8 bank_num;
	u8 entry_num;
	u16 entry_size;
	u16 headroom;
};

/* There should be an agreement between host and FW about bank mapping */
/* TODO : think about this agreement */
enum smapper_banks {
	RX_0,
	RX_1,
	RX_2,
	RX_3,
	END_RX_BANKS
};

struct hip4_smapper_control {
	u32 emul_loc;   /* Smapper emulator location in MIF_ADDR */
	u32 emul_sz;	/* Smapper emulator size */
	u8  th_req;     /* TH smapper request interrupt bit position */
	u8  fh_ind;     /* FH smapper ind interrupt bit position */
	u32  mbox_scb;   /* SMAPPER MBOX scoreboard location */
	u32 *mbox_ptr;   /* Mbox pointer */
	spinlock_t   smapper_lock;
	/* Lookup table to map the virtual bank mapping in wlan with the phy mapping in HW */
	/* Currently is safe to use this indexing as only WIFI is using smapper */
	u8 lookuptable[HIP4_SMAPPER_TOTAL_BANKS];
};

struct hip4_smapper_bank {
	enum smapper_type       type;
	u16			entries;
	bool			in_use;
	u8                      bank;
	u8                      cur;
	u32			entry_size;
	struct sk_buff          **skbuff;
	dma_addr_t		*skbuff_dma;
	struct hip4_smapper_control_entry *entry;
	u16 align;
};

int hip4_smapper_init(struct slsi_dev *sdev, struct slsi_hip4 *hip);
void hip4_smapper_deinit(struct slsi_dev *sdev, struct slsi_hip4 *hip);

struct mbulk *hip4_smapper_send(struct slsi_hip4 *hip, struct sk_buff *skb, int *val);
int hip4_smapper_consume_entry(struct slsi_dev *sdev, struct slsi_hip4 *hip, struct sk_buff *skb_fapi);
void *hip4_smapper_get_skb_data(struct slsi_dev *sdev, struct slsi_hip4 *hip, struct sk_buff *skb_fapi);
struct sk_buff *hip4_smapper_get_skb(struct slsi_dev *sdev, struct slsi_hip4 *hip, struct sk_buff *skb_fapi);
void hip4_smapper_free_mapped_skb(struct sk_buff *skb);
#endif
