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

#ifndef __GNSS_LINK_DEVICE_SHMEM_H__
#define __GNSS_LINK_DEVICE_SHMEM_H__

#include <linux/mcu_ipc.h>
#include "gnss_link_device_memory.h"

#define IPC_WAKELOCK_TIMEOUT		(HZ)
#define BCMD_WAKELOCK_TIMEOUT		(HZ / 10) /* 100 msec */

struct shmem_circ {
	u32 __iomem *head;
	u32 __iomem *tail;
	u8  __iomem *buff;
	u32          size;
};

struct shmem_ipc_device {
	struct shmem_circ txq;
	struct shmem_circ rxq;
};

struct shmem_ipc_map {
	struct shmem_ipc_device dev;
};

struct shmem_region {
	u8 __iomem *vaddr; /* ioremap base address */
	u32         paddr; /* physical base address */
	u32         size;  /* region size */
};

struct shmem_link_device {
	struct link_device ld;

	struct gnss_mbox *mbx;
	struct gnss_shared_reg **reg;

	/* Reserved memory information */
	struct shmem_region res_mem;

	/* Fault area information */
	struct shmem_region fault_mem;

	/* SHMEM (SHARED MEMORY) address, size, IRQ# */
	struct shmem_region ipc_mem;

	u32 ipc_reg_cnt;

	/* IPC device map */
	struct shmem_ipc_map ipc_map;

	/* Pointers (aliases) to IPC device map */
	struct shmem_ipc_device *dev;

	/* MBOX number & IRQ */
	int int_ap2gnss_ipc_msg;
	int irq_gnss2ap_ipc_msg;

	/* Wakelock for SHMEM device */
	struct wake_lock wlock;
	char wlock_name[GNSS_MAX_NAME_LEN];

	/* for locking TX process */
	spinlock_t tx_lock;

	/* for retransmission under SHMEM flow control after TXQ full state */
	atomic_t res_required;
	//struct completion req_ack_cmpl;

	/* for efficient RX process */
	struct tasklet_struct rx_tsk;
	struct delayed_work msg_rx_dwork;
	struct io_device *iod;

	/* for logging SHMEM status */
	struct mem_status_queue tx_msq;
	struct mem_status_queue rx_msq;

	/* for logging SHMEM dump */
	//struct trace_data_queue trace_list;

	/* to hold/release "cp_wakeup" for PM (power-management) */
	//struct delayed_work cp_sleep_dwork;
	atomic_t ref_cnt;
	//spinlock_t pm_lock;
};

/* converts from struct link_device* to struct xxx_link_device* */
#define to_shmem_link_device(linkdev) \
		container_of(linkdev, struct shmem_link_device, ld)

void gnss_write_reg(struct shmem_link_device *, enum gnss_reg_type, u32);
u32 gnss_read_reg(struct shmem_link_device *, enum gnss_reg_type);

/**
 * get_txq_head
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of a head (in) pointer in a TX queue.
 */
static inline u32 get_txq_head(struct shmem_link_device *shmd)
{
	return gnss_read_reg(shmd, GNSS_REG_TX_HEAD);
}

/**
 * get_txq_tail
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of a tail (out) pointer in a TX queue.
 *
 * It is useless for an AP to read a tail pointer in a TX queue twice to verify
 * whether or not the value in the pointer is valid, because it can already have
 * been updated by a GNSS after the first access from the AP.
 */
static inline u32 get_txq_tail(struct shmem_link_device *shmd)
{
	return gnss_read_reg(shmd, GNSS_REG_TX_TAIL);
}

/**
 * get_txq_buff
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the start address of the buffer in a TXQ.
 */
static inline u8 *get_txq_buff(struct shmem_link_device *shmd)
{
	return shmd->dev->txq.buff;
}

/**
 * get_txq_buff_size
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the size of the buffer in a TXQ.
 */
static inline u32 get_txq_buff_size(struct shmem_link_device *shmd)
{
	return shmd->dev->txq.size;
}

/**
 * get_rxq_head
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of a head (in) pointer in an RX queue.
 *
 * It is useless for an AP to read a head pointer in an RX queue twice to verify
 * whether or not the value in the pointer is valid, because it can already have
 * been updated by a GNSS after the first access from the AP.
 */
static inline u32 get_rxq_head(struct shmem_link_device *shmd)
{
	return gnss_read_reg(shmd, GNSS_REG_RX_HEAD);
}

/**
 * get_rxq_tail
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of a tail (in) pointer in an RX queue.
 */
static inline u32 get_rxq_tail(struct shmem_link_device *shmd)
{
	return gnss_read_reg(shmd, GNSS_REG_RX_TAIL);
}

/**
 * get_rxq_buff
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the start address of the buffer in an RXQ.
 */
static inline u8 *get_rxq_buff(struct shmem_link_device *shmd)
{
	return shmd->dev->rxq.buff;
}

/**
 * get_rxq_buff_size
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the size of the buffer in an RXQ.
 */
static inline u32 get_rxq_buff_size(struct shmem_link_device *shmd)
{
	return shmd->dev->rxq.size;
}

/**
 * set_txq_head
 * @shmd: pointer to an instance of shmem_link_device structure
 * @in: value to be written to the head pointer in a TXQ
 */
static inline void set_txq_head(struct shmem_link_device *shmd, u32 in)
{
	gnss_write_reg(shmd, GNSS_REG_TX_HEAD, in);
}

/**
 * set_txq_tail
 * @shmd: pointer to an instance of shmem_link_device structure
 * @out: value to be written to the tail pointer in a TXQ
 */
static inline void set_txq_tail(struct shmem_link_device *shmd, u32 out)
{
	gnss_write_reg(shmd, GNSS_REG_TX_TAIL, out);
}

/**
 * set_rxq_head
 * @shmd: pointer to an instance of shmem_link_device structure
 * @in: value to be written to the head pointer in an RXQ
 */
static inline void set_rxq_head(struct shmem_link_device *shmd, u32 in)
{
	gnss_write_reg(shmd, GNSS_REG_RX_HEAD, in);
}

/**
 * set_rxq_tail
 * @shmd: pointer to an instance of shmem_link_device structure
 * @out: value to be written to the tail pointer in an RXQ
 */
static inline void set_rxq_tail(struct shmem_link_device *shmd, u32 out)
{
	gnss_write_reg(shmd, GNSS_REG_RX_TAIL, out);
}

/**
 * read_int2gnss
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Returns the value of the AP-to-GNSS interrupt register.
 */
static inline u16 read_int2gnss(struct shmem_link_device *shmd)
{
	return mbox_get_value(MCU_GNSS, shmd->int_ap2gnss_ipc_msg);
}

/**
 * reset_txq_circ
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * Empties a TXQ by resetting the head (in) pointer with the value in the tail
 * (out) pointer.
 */
static inline void reset_txq_circ(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	u32 head = get_txq_head(shmd);
	u32 tail = get_txq_tail(shmd);

	gif_err("%s: %s_TXQ: HEAD[%u] <== TAIL[%u]\n",
		ld->name, "FMT", head, tail);

	set_txq_head(shmd, tail);
}

/**
 * reset_rxq_circ
 * @shmd: pointer to an instance of shmem_link_device structure
 * @dev: IPC device (IPC_FMT, IPC_RAW, etc.)
 *
 * Empties an RXQ by resetting the tail (out) pointer with the value in the head
 * (in) pointer.
 */
static inline void reset_rxq_circ(struct shmem_link_device *shmd)
{
	struct link_device *ld = &shmd->ld;
	u32 head = get_rxq_head(shmd);
	u32 tail = get_rxq_tail(shmd);

	gif_err("%s: %s_RXQ: TAIL[%u] <== HEAD[%u]\n",
		ld->name, "FMT", tail, head);

	set_rxq_tail(shmd, head);
}

/**
 * get_rxq_rcvd
 * @shmd: pointer to an instance of shmem_link_device structure
 * @mst: pointer to an instance of mem_status structure
 * OUT @circ: pointer to an instance of circ_status structure
 *
 * Stores {start address of the buffer in a RXQ, size of the buffer, in & out
 * pointer values, size of received data} into the 'circ' instance.
 *
 * Returns an error code.
 */
static inline int get_rxq_rcvd(struct shmem_link_device *shmd,
			       struct mem_status *mst, struct circ_status *circ)
{
	struct link_device *ld = &shmd->ld;

	circ->buff = get_rxq_buff(shmd);
	circ->qsize = get_rxq_buff_size(shmd);
	circ->in = mst->head[RX];
	circ->out = mst->tail[RX];
	circ->size = circ_get_usage(circ->qsize, circ->in, circ->out);

	if (circ_valid(circ->qsize, circ->in, circ->out)) {
		gif_debug("%s: %s_RXQ qsize[%u] in[%u] out[%u] rcvd[%u]\n",
			ld->name, "FMT", circ->qsize, circ->in,
			circ->out, circ->size);
		return 0;
	} else {
		gif_err("%s: ERR! %s_RXQ invalid (qsize[%d] in[%d] out[%d])\n",
			ld->name, "FMT", circ->qsize, circ->in,
			circ->out);
		return -EIO;
	}
}

/*
 * shmem_purge_rxq
 * @ld: pointer to an instance of the link_device structure
 *
 * Purges pending transfers from the RXQ.
 */
static inline void purge_rxq(struct link_device *ld)
{
	skb_queue_purge(ld->skb_rxq);
}

/**
 * get_txq_space
 * @shmd: pointer to an instance of shmem_link_device structure
 * OUT @circ: pointer to an instance of circ_status structure
 *
 * Stores {start address of the buffer in a TXQ, size of the buffer, in & out
 * pointer values, size of free space} into the 'circ' instance.
 *
 * Returns the size of free space in the buffer or an error code.
 */
static inline int get_txq_space(struct shmem_link_device *shmd,
				struct circ_status *circ)
{
	struct link_device *ld = &shmd->ld;
	int cnt = 0;
	u32 qsize;
	u32 head;
	u32 tail;
	int space;

	while (1) {
		qsize = get_txq_buff_size(shmd);
		head = get_txq_head(shmd);
		tail = get_txq_tail(shmd);
		space = circ_get_space(qsize, head, tail);

		gif_debug("%s: %s_TXQ{qsize:%u in:%u out:%u space:%u}\n",
			ld->name, "FMT", qsize, head, tail, space);

		if (circ_valid(qsize, head, tail))
			break;

		cnt++;
		gif_err("%s: ERR! invalid %s_TXQ{qsize:%d in:%d out:%d space:%d}, count %d\n",
			ld->name, "FMT", qsize, head, tail,
			space, cnt);
		if (cnt >= MAX_RETRY_CNT) {
			space = -EIO;
			break;
		}

		udelay(100);
	}

	circ->buff = get_txq_buff(shmd);
	circ->qsize = qsize;
	circ->in = head;
	circ->out = tail;
	circ->size = space;

	return space;
}

/**
 * shmem_purge_txq
 * @ld: pointer to an instance of the link_device structure
 *
 * Purges pending transfers from the TXQ.
 */
static inline void purge_txq(struct link_device *ld)
{
	struct shmem_link_device *shmd = to_shmem_link_device(ld);
	unsigned long flags;

	spin_lock_irqsave(&shmd->tx_lock, flags);
	skb_queue_purge(ld->skb_txq);
	spin_unlock_irqrestore(&shmd->tx_lock, flags);
}

/**
 * clear_shmem_map
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Clears all pointers in every circular queue.
 */
static inline void clear_shmem_map(struct shmem_link_device *shmd)
{
	set_txq_head(shmd, 0);
	set_txq_tail(shmd, 0);
	set_rxq_head(shmd, 0);
	set_rxq_tail(shmd, 0);

	atomic_set(&shmd->res_required, 0);

	memset(shmd->ipc_mem.vaddr, 0x0, shmd->ipc_mem.size);
}

/**
 * reset_shmem_ipc
 * @shmd: pointer to an instance of shmem_link_device structure
 *
 * Reset SHMEM with IPC map.
 */
static inline void reset_shmem_ipc(struct shmem_link_device *shmd)
{
	clear_shmem_map(shmd);

	atomic_set(&shmd->res_required, 0);

	atomic_set(&shmd->ref_cnt, 0);
}
#endif

