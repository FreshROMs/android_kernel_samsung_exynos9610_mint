/******************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 *****************************************************************************/
#include <linux/string.h>
#include <linux/spinlock.h>
#include "mbulk.h"
#include "hip4_sampler.h"

#include "debug.h"

/* mbulk descriptor is aligned to 64 bytes considering the host processor's
 * cache line size
 */
#define MBULK_ALIGN                              (64)
#define MBULK_IS_ALIGNED(s)                      (((uintptr_t)(s) & (MBULK_ALIGN - 1)) == 0)
#define MBULK_SZ_ROUNDUP(s)                      round_up(s, MBULK_ALIGN)

/* a magic number to allocate the remaining buffer to the bulk buffer
 * in a segment. Used in chained mbulk allocation.
 */
#define MBULK_DAT_BUFSZ_REQ_BEST_MAGIC  ((u32)(-2))

static DEFINE_SPINLOCK(mbulk_pool_lock);

static inline void mbulk_debug(struct mbulk *m)
{
	(void)m; /* may be unused */
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->next_offset %p: %d\n", &m->next_offset, m->next_offset);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->flag %p: %d\n", &m->flag, m->flag);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->clas %p: %d\n",  &m->clas, m->clas);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->pid  %p: %d\n", &m->pid, m->pid);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->refcnt %p: %d\n", &m->refcnt, m->refcnt);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->dat_bufsz %p: %d\n", &m->dat_bufsz, m->dat_bufsz);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->sig_bufsz %p: %d\n", &m->sig_bufsz, m->sig_bufsz);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->len %p: %d\n", &m->len, m->len);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->head %p: %d\n", &m->head, m->head);
	SLSI_DBG1_NODEV(SLSI_MBULK, "m->chain_next_offset %p: %d\n", &m->chain_next_offset, m->chain_next_offset);
}

/* Mbulk tracker - tracks mbulks sent and returned by fw */
struct mbulk_tracker {
	mbulk_colour colour;
};

/* mbulk pool */
struct mbulk_pool {
	bool         valid;                   /** is valid */
	u8           pid;                     /** pool id */
	struct mbulk *free_list;              /** head of free segment list */
	int          free_cnt;                /** current number of free segments */
	int          usage[MBULK_CLASS_MAX];  /** statistics of usage per mbulk clas*/
	char         *base_addr;              /** base address of the pool */
	char         *end_addr;               /** exclusive end address of the pool */
	mbulk_len_t  seg_size;                /** segment size in bytes "excluding" struct mbulk */
	u8           guard;                   /** pool guard **/
	int          tot_seg_num;             /** total number of segments in this pool */
	struct mbulk_tracker *mbulk_tracker;
	char	     shift;
#ifdef CONFIG_SCSC_WLAN_HIP4_PROFILING
	int          minor;
#endif
};

/* get a segment from a pool */
static inline struct mbulk *mbulk_pool_get(struct mbulk_pool *pool, enum mbulk_class clas)
{
	struct mbulk *m;
	u8           guard = pool->guard;

	spin_lock_bh(&mbulk_pool_lock);
	m = pool->free_list;

	if (m == NULL || pool->free_cnt <= guard) { /* guard */
		spin_unlock_bh(&mbulk_pool_lock);
		return NULL;
	}

	pool->free_cnt--;
	pool->usage[clas]++;

	SCSC_HIP4_SAMPLER_MBULK(pool->minor, (pool->free_cnt & 0x100) >> 8, (pool->free_cnt & 0xff), pool->pid);

	if (m->next_offset == 0)
		pool->free_list = NULL;
	else
		pool->free_list = (struct mbulk *)((uintptr_t)pool->free_list + m->next_offset);

	memset(m, 0, sizeof(*m));
	m->pid = pool->pid;
	m->clas = clas;

	spin_unlock_bh(&mbulk_pool_lock);
	return m;
}

/* put a segment to a pool */
static inline void mbulk_pool_put(struct mbulk_pool *pool, struct mbulk *m)
{
	if (m->flag == MBULK_F_FREE)
		return;

	spin_lock_bh(&mbulk_pool_lock);
	pool->usage[m->clas]--;
	pool->free_cnt++;

	SCSC_HIP4_SAMPLER_MBULK(pool->minor, (pool->free_cnt & 0x100) >> 8, (pool->free_cnt & 0xff), pool->pid);
	m->flag = MBULK_F_FREE;
	if (pool->free_list != NULL)
		m->next_offset = (uintptr_t)pool->free_list - (uintptr_t)m;
	else
		m->next_offset = 0;
	pool->free_list = m;
	spin_unlock_bh(&mbulk_pool_lock);
}

/** mbulk pool configuration */
struct mbulk_pool_config {
	mbulk_len_t seg_sz;     /** segment size "excluding" struct mbulk */
	int         seg_num;    /** number of segments. If -1, all remaining space is used */
};

/** mbulk pools */
static struct mbulk_pool mbulk_pools[MBULK_POOL_ID_MAX];

/**
 * allocate a mbulk segment from the pool
 *
 * Note that the refcnt would be zero if \dat_bufsz is zero, as there is no
 * allocated bulk data.
 * If \dat_bufsz is \MBULK_DAT_BUFSZ_REQ_BEST_MAGIC, then this function
 * allocates all remaining buffer space to the bulk buffer.
 *
 */
static struct mbulk *mbulk_seg_generic_alloc(struct mbulk_pool *pool,
					     enum mbulk_class clas, size_t sig_bufsz, size_t dat_bufsz)
{
	struct mbulk *m;

	if (pool == NULL)
		return NULL;

	/* get a segment from the pool */
	m = mbulk_pool_get(pool, clas);
	if (m == NULL)
		return NULL;

	/* signal buffer */
	m->sig_bufsz = (mbulk_len_t)sig_bufsz;
	if (sig_bufsz)
		m->flag = MBULK_F_SIG;

	/* data buffer.
	 * Note that data buffer size can be larger than the requested.
	 */
	m->head = m->sig_bufsz;
	if (dat_bufsz == 0) {
		m->dat_bufsz = 0;
		m->refcnt = 0;
	} else if (dat_bufsz == MBULK_DAT_BUFSZ_REQ_BEST_MAGIC) {
		m->dat_bufsz = pool->seg_size - m->sig_bufsz;
		m->refcnt = 1;
	} else {
		m->dat_bufsz = (mbulk_len_t)dat_bufsz;
		m->refcnt = 1;
	}

	mbulk_debug(m);
	return m;
}

int mbulk_pool_get_free_count(u8 pool_id)
{
	struct mbulk_pool *pool;
	int num_free;

	if (pool_id >= MBULK_POOL_ID_MAX) {
		WARN_ON(pool_id >= MBULK_POOL_ID_MAX);
		return -EIO;
	}

	spin_lock_bh(&mbulk_pool_lock);
	pool = &mbulk_pools[pool_id];

	if (!pool->valid) {
		WARN_ON(!pool->valid);
		spin_unlock_bh(&mbulk_pool_lock);
		return -EIO;
	}

	num_free = pool->free_cnt;
	spin_unlock_bh(&mbulk_pool_lock);

	return num_free;
}

/**
 * Allocate a bulk buffer with an in-lined signal buffer
 *
 * A mbulk segment is allocated from the given the pool, if its size
 * meeting the requested size.
 *
 */
struct mbulk *mbulk_with_signal_alloc_by_pool(u8 pool_id, mbulk_colour colour,
					      enum mbulk_class clas, size_t sig_bufsz_req, size_t dat_bufsz)
{
	struct mbulk_pool *pool;
	size_t            sig_bufsz;
	size_t            tot_bufsz;
	struct mbulk      *m_ret;
	u32		  index;

	/* data buffer should be aligned */
	sig_bufsz = MBULK_SIG_BUFSZ_ROUNDUP(sizeof(struct mbulk) + sig_bufsz_req) - sizeof(struct mbulk);

	if (pool_id >= MBULK_POOL_ID_MAX) {
		WARN_ON(pool_id >= MBULK_POOL_ID_MAX);
		return NULL;
	}

	pool = &mbulk_pools[pool_id];

	if (!pool->valid) {
		WARN_ON(!pool->valid);
		return NULL;
	}

	/* check if this pool meets the size */
	tot_bufsz = sig_bufsz + dat_bufsz;
	if (dat_bufsz != MBULK_DAT_BUFSZ_REQ_BEST_MAGIC &&
	    pool->seg_size < tot_bufsz)
		return NULL;

	m_ret = mbulk_seg_generic_alloc(pool, clas, sig_bufsz, dat_bufsz);

	index = (((uintptr_t)pool->end_addr - (uintptr_t)m_ret) >> pool->shift) - 1;
	if (index >= pool->tot_seg_num)
		return NULL;

	pool->mbulk_tracker[index].colour = colour;

	return m_ret;
}

mbulk_colour mbulk_get_colour(u8 pool_id, struct mbulk *m)
{
	struct mbulk_pool *pool;
	u16 index;

	pool = &mbulk_pools[pool_id];

	if (!pool->valid) {
		WARN_ON(1);
		return 0;
	}

	index = (((uintptr_t)pool->end_addr - (uintptr_t)m) >> pool->shift) - 1;
	if (index >= pool->tot_seg_num)
		return 0;

	return pool->mbulk_tracker[index].colour;
}

#ifdef MBULK_SUPPORT_SG_CHAIN
/**
 * allocate a chained mbulk buffer from a specific mbulk pool
 *
 */
struct mbulk *mbulk_chain_with_signal_alloc_by_pool(u8 pool_id,
						    enum mbulk_class clas, size_t sig_bufsz, size_t dat_bufsz)
{
	size_t       tot_len;
	struct mbulk *m, *head, *pre;

	head = mbulk_with_signal_alloc_by_pool(pool_id, clas, sig_bufsz,
					       MBULK_DAT_BUFSZ_REQ_BEST_MAGIC);
	if (head == NULL || MBULK_SEG_TAILROOM(head) >= dat_bufsz)
		return head;

	head->flag |= (MBULK_F_CHAIN_HEAD | MBULK_F_CHAIN);
	tot_len = MBULK_SEG_TAILROOM(head);
	pre = head;

	while (tot_len < dat_bufsz) {
		m = mbulk_with_signal_alloc_by_pool(pool_id, clas, 0,
						    MBULK_DAT_BUFSZ_REQ_BEST_MAGIC);
		if (m == NULL)
			break;
		/* all mbulk in this chain has an attribue, MBULK_F_CHAIN */
		m->flag |= MBULK_F_CHAIN;
		tot_len += MBULK_SEG_TAILROOM(m);
		pre->chain_next = m;
		pre = m;
	}

	if (tot_len < dat_bufsz) {
		mbulk_chain_free(head);
		return NULL;
	}

	return head;
}

/**
 * free a chained mbulk
 */
void mbulk_chain_free(struct mbulk *sg)
{
	struct mbulk *chain_next, *m;

	/* allow null pointer */
	if (sg == NULL)
		return;

	m = sg;
	while (m != NULL) {
		chain_next = m->chain_next;

		/* is not scatter-gather anymore */
		m->flag &= ~(MBULK_F_CHAIN | MBULK_F_CHAIN_HEAD);
		mbulk_seg_free(m);

		m = chain_next;
	}
}

/**
 * get a tail mbulk in the chain
 *
 */
struct mbulk *mbulk_chain_tail(struct mbulk *m)
{
	while (m->chain_next != NULL)
		m = m->chain_next;
	return m;
}

/**
 * total buffer size in a chanied mbulk
 *
 */
size_t mbulk_chain_bufsz(struct mbulk *m)
{
	size_t tbufsz = 0;

	while (m != NULL) {
		tbufsz += m->dat_bufsz;
		m = m->chain_next;
	}

	return tbufsz;
}

/**
 * total data length in a chanied mbulk
 *
 */
size_t mbulk_chain_tlen(struct mbulk *m)
{
	size_t tlen = 0;

	while (m != NULL) {
		tlen += m->len;
		m = m->chain_next;
	}

	return tlen;
}
#endif /*MBULK_SUPPORT_SG_CHAIN*/

/**
 * add a memory zone to a mbulk pool list
 *
 */
#ifdef CONFIG_SCSC_WLAN_DEBUG
int mbulk_pool_add(u8 pool_id, char *base, char *end, size_t seg_size, u8 guard, int minor)
#else
int mbulk_pool_add(u8 pool_id, char *base, char *end, size_t seg_size, u8 guard)
#endif
{
	struct mbulk_pool *pool;
	struct mbulk      *next;
	size_t            byte_per_block;

	if (pool_id >= MBULK_POOL_ID_MAX) {
		WARN_ON(pool_id >= MBULK_POOL_ID_MAX);
		return -EIO;
	}

	pool = &mbulk_pools[pool_id];

	if (!MBULK_IS_ALIGNED(base)) {
		WARN_ON(!MBULK_IS_ALIGNED(base));
		return -EIO;
	}

	/* total required memory per block */
	byte_per_block = MBULK_SZ_ROUNDUP(sizeof(struct mbulk) + seg_size);

	if (byte_per_block == 0)
		return -EIO;

	/* init pool structure */
	memset(pool, 0, sizeof(*pool));
	pool->pid = pool_id;
	pool->base_addr = base;
	pool->end_addr = end;
	pool->seg_size = (mbulk_len_t)(byte_per_block - sizeof(struct mbulk));
	pool->guard = guard;
	pool->shift = ffs(byte_per_block) - 1;

	/* allocate segments */
	next = (struct mbulk *)base;
	while (((uintptr_t)next + byte_per_block) <= (uintptr_t)end) {
		memset(next, 0, sizeof(struct mbulk));
		next->pid = pool_id;

		/* add to the free list */
		if (pool->free_list == NULL)
			next->next_offset = 0;
		else
			next->next_offset = (uintptr_t)pool->free_list - (uintptr_t)next;
		next->flag = MBULK_F_FREE;
		pool->free_list = next;
		pool->tot_seg_num++;
		pool->free_cnt++;
		next = (struct mbulk *)((uintptr_t)next + byte_per_block);
	}

	pool->valid = (pool->free_cnt) ? true : false;
#ifdef CONFIG_SCSC_WLAN_DEBUG
	pool->minor = minor;
#endif
	/* create a mbulk tracker object */
	pool->mbulk_tracker = (struct mbulk_tracker *)vmalloc(pool->tot_seg_num * sizeof(struct mbulk_tracker));

	if (pool->mbulk_tracker == NULL)
		return -EIO;

	return 0;
}

void mbulk_pool_remove(u8 pool_id)
{
	struct mbulk_pool *pool;

	if (pool_id >= MBULK_POOL_ID_MAX) {
		WARN_ON(pool_id >= MBULK_POOL_ID_MAX);
		return;
	}

	pool = &mbulk_pools[pool_id];

	/* Destroy mbulk tracker */
	vfree(pool->mbulk_tracker);

	pool->mbulk_tracker = NULL;
}

/**
 * add mbulk pools in MIF address space
 */
void mbulk_pool_dump(u8 pool_id, int max_cnt)
{
	struct mbulk_pool *pool;
	struct mbulk      *m;
	int               cnt = max_cnt;

	pool = &mbulk_pools[pool_id];
	m = pool->free_list;
	while (m != NULL && cnt--)
		m = (m->next_offset == 0) ? NULL :
		    (struct mbulk *)(pool->base_addr + m->next_offset);
}

/**
 * free a mbulk in the virtual host
 */
void mbulk_free_virt_host(struct mbulk *m)
{
	u8                pool_id;
	struct mbulk_pool *pool;

	if (m == NULL)
		return;

	pool_id = m->pid & 0x1;

	pool = &mbulk_pools[pool_id];

	if (!pool->valid) {
		WARN_ON(!pool->valid);
		return;
	}

	/* put to the pool */
	mbulk_pool_put(pool, m);
}
