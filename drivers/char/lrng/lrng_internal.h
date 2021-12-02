/* SPDX-License-Identifier: GPL-2.0 OR BSD-2-Clause */
/*
 * Copyright (C) 2018 - 2021, Stephan Mueller <smueller@chronox.de>
 */

#ifndef _LRNG_INTERNAL_H
#define _LRNG_INTERNAL_H

#include <crypto/sha.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rwlock.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include <linux/bitops.h>
#include <linux/string.h>
#include <asm/unaligned.h>

#define SHA256_H0	0x6a09e667UL
#define SHA256_H1	0xbb67ae85UL
#define SHA256_H2	0x3c6ef372UL
#define SHA256_H3	0xa54ff53aUL
#define SHA256_H4	0x510e527fUL
#define SHA256_H5	0x9b05688cUL
#define SHA256_H6	0x1f83d9abUL
#define SHA256_H7	0x5be0cd19UL

static const u32 SHA256_K[] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static inline u32 Ch(u32 x, u32 y, u32 z)
{
	return z ^ (x & (y ^ z));
}

static inline u32 Maj(u32 x, u32 y, u32 z)
{
	return (x & y) | (z & (x | y));
}

#define e0(x)	(ror32(x, 2) ^ ror32(x, 13) ^ ror32(x, 22))
#define e1(x)	(ror32(x, 6) ^ ror32(x, 11) ^ ror32(x, 25))
#define s0(x)	(ror32(x, 7) ^ ror32(x, 18) ^ (x >> 3))
#define s1(x)	(ror32(x, 17) ^ ror32(x, 19) ^ (x >> 10))

static inline void LOAD_OP(int I, u32 *W, const u8 *input)
{
	W[I] = get_unaligned_be32((__u32 *)input + I);
}

static inline void BLEND_OP(int I, u32 *W)
{
	W[I] = s1(W[I-2]) + W[I-7] + s0(W[I-15]) + W[I-16];
}

#define SHA256_ROUND(i, a, b, c, d, e, f, g, h) do {		\
	u32 t1, t2;						\
	t1 = h + e1(e) + Ch(e, f, g) + SHA256_K[i] + W[i];	\
	t2 = e0(a) + Maj(a, b, c);				\
	d += t1;						\
	h = t1 + t2;						\
} while (0)

static inline void sha256_transform(u32 *state, const u8 *input, u32 *W)
{
	u32 a, b, c, d, e, f, g, h;
	int i;

	/* load the input */
	for (i = 0; i < 16; i += 8) {
		LOAD_OP(i + 0, W, input);
		LOAD_OP(i + 1, W, input);
		LOAD_OP(i + 2, W, input);
		LOAD_OP(i + 3, W, input);
		LOAD_OP(i + 4, W, input);
		LOAD_OP(i + 5, W, input);
		LOAD_OP(i + 6, W, input);
		LOAD_OP(i + 7, W, input);
	}

	/* now blend */
	for (i = 16; i < 64; i += 8) {
		BLEND_OP(i + 0, W);
		BLEND_OP(i + 1, W);
		BLEND_OP(i + 2, W);
		BLEND_OP(i + 3, W);
		BLEND_OP(i + 4, W);
		BLEND_OP(i + 5, W);
		BLEND_OP(i + 6, W);
		BLEND_OP(i + 7, W);
	}

	/* load the state into our registers */
	a = state[0];  b = state[1];  c = state[2];  d = state[3];
	e = state[4];  f = state[5];  g = state[6];  h = state[7];

	/* now iterate */
	for (i = 0; i < 64; i += 8) {
		SHA256_ROUND(i + 0, a, b, c, d, e, f, g, h);
		SHA256_ROUND(i + 1, h, a, b, c, d, e, f, g);
		SHA256_ROUND(i + 2, g, h, a, b, c, d, e, f);
		SHA256_ROUND(i + 3, f, g, h, a, b, c, d, e);
		SHA256_ROUND(i + 4, e, f, g, h, a, b, c, d);
		SHA256_ROUND(i + 5, d, e, f, g, h, a, b, c);
		SHA256_ROUND(i + 6, c, d, e, f, g, h, a, b);
		SHA256_ROUND(i + 7, b, c, d, e, f, g, h, a);
	}

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static inline void sha256_init(struct sha256_state *sctx)
{
	sctx->state[0] = SHA256_H0;
	sctx->state[1] = SHA256_H1;
	sctx->state[2] = SHA256_H2;
	sctx->state[3] = SHA256_H3;
	sctx->state[4] = SHA256_H4;
	sctx->state[5] = SHA256_H5;
	sctx->state[6] = SHA256_H6;
	sctx->state[7] = SHA256_H7;
	sctx->count = 0;
}

static inline void sha256_update(struct sha256_state *sctx, const u8 *data, unsigned int len)
{
	unsigned int partial, done;
	const u8 *src;
	u32 W[64];

	partial = sctx->count & 0x3f;
	sctx->count += len;
	done = 0;
	src = data;

	if ((partial + len) > 63) {
		if (partial) {
			done = -partial;
			memcpy(sctx->buf + partial, data, done + 64);
			src = sctx->buf;
		}

		do {
			sha256_transform(sctx->state, src, W);
			done += 64;
			src = data + done;
		} while (done + 63 < len);

		memzero_explicit(W, sizeof(W));

		partial = 0;
	}
	memcpy(sctx->buf + partial, src, len - done);
}

static inline void __sha256_final(struct sha256_state *sctx, u8 *out, int digest_words)
{
	__be32 *dst = (__be32 *)out;
	__be64 bits;
	unsigned int index, pad_len;
	int i;
	static const u8 padding[64] = { 0x80, };

	/* Save number of bits */
	bits = cpu_to_be64(sctx->count << 3);

	/* Pad out to 56 mod 64. */
	index = sctx->count & 0x3f;
	pad_len = (index < 56) ? (56 - index) : ((64+56) - index);
	sha256_update(sctx, padding, pad_len);

	/* Append length (before padding) */
	sha256_update(sctx, (const u8 *)&bits, sizeof(bits));

	/* Store state in digest */
	for (i = 0; i < digest_words; i++)
		put_unaligned_be32(sctx->state[i], &dst[i]);

	/* Zeroize sensitive information. */
	memzero_explicit(sctx, sizeof(*sctx));
}

static inline void sha256_final(struct sha256_state *sctx, u8 *out)
{
	__sha256_final(sctx, out, 8);
}

/*************************** General LRNG parameter ***************************/

/* Security strength of LRNG -- this must match DRNG security strength */
#define LRNG_DRNG_SECURITY_STRENGTH_BYTES 32
#define LRNG_DRNG_SECURITY_STRENGTH_BITS (LRNG_DRNG_SECURITY_STRENGTH_BYTES * 8)
#define LRNG_DRNG_BLOCKSIZE 64		/* Maximum of DRNG block sizes */
#define LRNG_DRNG_INIT_SEED_SIZE_BITS (LRNG_DRNG_SECURITY_STRENGTH_BITS +      \
				       CONFIG_LRNG_SEED_BUFFER_INIT_ADD_BITS)
#define LRNG_DRNG_INIT_SEED_SIZE_BYTES (LRNG_DRNG_INIT_SEED_SIZE_BITS >> 3)

/*
 * SP800-90A defines a maximum request size of 1<<16 bytes. The given value is
 * considered a safer margin.
 *
 * This value is allowed to be changed.
 */
#define LRNG_DRNG_MAX_REQSIZE		(1<<12)

/*
 * SP800-90A defines a maximum number of requests between reseeds of 2^48.
 * The given value is considered a much safer margin, balancing requests for
 * frequent reseeds with the need to conserve entropy. This value MUST NOT be
 * larger than INT_MAX because it is used in an atomic_t.
 *
 * This value is allowed to be changed.
 */
#define LRNG_DRNG_RESEED_THRESH		(1<<20)

/*
 * Maximum DRNG generation operations without reseed having full entropy
 * This value defines the absolute maximum value of DRNG generation operations
 * without a reseed holding full entropy. LRNG_DRNG_RESEED_THRESH is the
 * threshold when a new reseed is attempted. But it is possible that this fails
 * to deliver full entropy. In this case the DRNG will continue to provide data
 * even though it was not reseeded with full entropy. To avoid in the extreme
 * case that no reseed is performed for too long, this threshold is enforced.
 * If that absolute low value is reached, the LRNG is marked as not operational.
 *
 * This value is allowed to be changed.
 */
#define LRNG_DRNG_MAX_WITHOUT_RESEED	(1<<30)

/*
 * Min required seed entropy is 128 bits covering the minimum entropy
 * requirement of SP800-131A and the German BSI's TR02102.
 *
 * This value is allowed to be changed.
 */
#define LRNG_FULL_SEED_ENTROPY_BITS	LRNG_DRNG_SECURITY_STRENGTH_BITS
#define LRNG_MIN_SEED_ENTROPY_BITS	128
#define LRNG_INIT_ENTROPY_BITS		32

/*
 * Wakeup value
 *
 * This value is allowed to be changed but must not be larger than the
 * digest size of the hash operation used update the aux_pool.
 */
#ifdef CONFIG_CRYPTO_LIB_SHA256
# define LRNG_ATOMIC_DIGEST_SIZE	SHA256_DIGEST_SIZE
#else
# define LRNG_ATOMIC_DIGEST_SIZE	SHA1_DIGEST_SIZE
#endif
#define LRNG_WRITE_WAKEUP_ENTROPY	LRNG_ATOMIC_DIGEST_SIZE

/*
 * If the switching support is configured, we must provide support up to
 * the largest digest size. Without switching support, we know it is only
 * the built-in digest size.
 */
#ifdef CONFIG_LRNG_DRNG_SWITCH
# define LRNG_MAX_DIGESTSIZE		64
#else
# define LRNG_MAX_DIGESTSIZE		LRNG_ATOMIC_DIGEST_SIZE
#endif

/*
 * Oversampling factor of IRQ events to obtain
 * LRNG_DRNG_SECURITY_STRENGTH_BYTES. This factor is used when a
 * high-resolution time stamp is not available. In this case, jiffies and
 * register contents are used to fill the entropy pool. These noise sources
 * are much less entropic than the high-resolution timer. The entropy content
 * is the entropy content assumed with LRNG_IRQ_ENTROPY_BITS divided by
 * LRNG_IRQ_OVERSAMPLING_FACTOR.
 *
 * This value is allowed to be changed.
 */
#define LRNG_IRQ_OVERSAMPLING_FACTOR	10

/* Alignmask that is intended to be identical to CRYPTO_MINALIGN */
#define LRNG_KCAPI_ALIGN		ARCH_KMALLOC_MINALIGN

/*
 * This definition must provide a buffer that is equal to SHASH_DESC_ON_STACK
 * as it will be casted into a struct shash_desc.
 */
#define LRNG_POOL_SIZE	(sizeof(struct shash_desc) + HASH_MAX_DESCSIZE)

/************************ Default DRNG implementation *************************/

extern struct chacha20_state lrng_chacha20;
extern const struct lrng_crypto_cb lrng_cc20_crypto_cb;
void lrng_cc20_init_state(struct chacha20_state *state);

/********************************** /proc *************************************/

#ifdef CONFIG_SYSCTL
void lrng_pool_inc_numa_node(void);
void lrng_proc_update_max_write_thresh(u32 new_digestsize);
#else
static inline void lrng_pool_inc_numa_node(void) { }
static inline void lrng_proc_update_max_write_thresh(u32 new_digestsize) { }
#endif

/****************************** LRNG interfaces *******************************/

extern u32 lrng_write_wakeup_bits;
extern int lrng_drng_reseed_max_time;

void lrng_writer_wakeup(void);
void lrng_init_wakeup(void);
void lrng_debug_report_seedlevel(const char *name);
void lrng_process_ready_list(void);

/* External interface to use of the switchable DRBG inside the kernel */
void get_random_bytes_full(void *buf, int nbytes);

/************************* Jitter RNG Entropy Source **************************/

#ifdef CONFIG_LRNG_JENT
u32 lrng_get_jent(u8 *outbuf, u32 requested_bits);
u32 lrng_jent_entropylevel(u32 requested_bits);
void lrng_jent_es_state(unsigned char *buf, size_t buflen);
#else /* CONFIG_LRNG_JENT */
static inline u32 lrng_get_jent(u8 *outbuf, u32 requested_bits) { return 0; }
static inline u32 lrng_jent_entropylevel(u32 requested_bits) { return 0; }
static inline void lrng_jent_es_state(unsigned char *buf, size_t buflen) { }
#endif /* CONFIG_LRNG_JENT */

/************************** CPU-based Entropy Source **************************/

static inline u32 lrng_fast_noise_entropylevel(u32 ent_bits, u32 requested_bits)
{
	/* Obtain entropy statement */
	ent_bits = ent_bits * requested_bits / LRNG_DRNG_SECURITY_STRENGTH_BITS;
	/* Cap entropy to buffer size in bits */
	ent_bits = min_t(u32, ent_bits, requested_bits);
	return ent_bits;
}

#ifdef CONFIG_LRNG_CPU
u32 lrng_get_arch(u8 *outbuf, u32 requested_bits);
u32 lrng_archrandom_entropylevel(u32 requested_bits);
void lrng_arch_es_state(unsigned char *buf, size_t buflen);
#else /* CONFIG_LRNG_CPU */
static inline u32 lrng_get_arch(u8 *outbuf, u32 requested_bits) { return 0; }
static inline u32 lrng_archrandom_entropylevel(u32 requested_bits) { return 0; }
static inline void lrng_arch_es_state(unsigned char *buf, size_t buflen) { }
#endif /* CONFIG_LRNG_CPU */

/************************** Interrupt Entropy Source **************************/

#ifdef CONFIG_LRNG_IRQ
void lrng_pcpu_reset(void);
u32 lrng_pcpu_avail_pool_size(void);
u32 lrng_pcpu_avail_entropy(void);
int lrng_pcpu_switch_hash(int node,
			  const struct lrng_crypto_cb *new_cb, void *new_hash,
			  const struct lrng_crypto_cb *old_cb);
u32 lrng_pcpu_pool_hash(u8 *outbuf, u32 requested_bits, bool fully_seeded);
void lrng_pcpu_array_add_u32(u32 data);
u32 lrng_gcd_analyze(u32 *history, size_t nelem);
void lrng_irq_es_state(unsigned char *buf, size_t buflen);
#else /* CONFIG_LRNG_IRQ */
static inline void lrng_pcpu_reset(void) { }
static inline u32 lrng_pcpu_avail_pool_size(void) { return 0; }
static inline u32 lrng_pcpu_avail_entropy(void) { return 0; }
static inline int lrng_pcpu_switch_hash(int node,
			  const struct lrng_crypto_cb *new_cb, void *new_hash,
			  const struct lrng_crypto_cb *old_cb)
{
	return 0;
}
static inline u32 lrng_pcpu_pool_hash(u8 *outbuf, u32 requested_bits,
				      bool fully_seeded)
{
	return 0;
}
static inline void lrng_pcpu_array_add_u32(u32 data) { }
static inline void lrng_irq_es_state(unsigned char *buf, size_t buflen) { }
#endif /* CONFIG_LRNG_IRQ */

/****************************** DRNG processing *******************************/

/* DRNG state handle */
struct lrng_drng {
	void *drng;				/* DRNG handle */
	void *hash;				/* Hash handle */
	const struct lrng_crypto_cb *crypto_cb;	/* Crypto callbacks */
	atomic_t requests;			/* Number of DRNG requests */
	atomic_t requests_since_fully_seeded;	/* Number DRNG requests since
						   last fully seeded */
	unsigned long last_seeded;		/* Last time it was seeded */
	bool fully_seeded;			/* Is DRNG fully seeded? */
	bool force_reseed;			/* Force a reseed */

	/* Lock write operations on DRNG state, DRNG replacement of crypto_cb */
	struct mutex lock;
	spinlock_t spin_lock;
	/* Lock *hash replacement - always take before DRNG lock */
	rwlock_t hash_lock;
};

extern struct mutex lrng_crypto_cb_update;

struct lrng_drng *lrng_drng_init_instance(void);
struct lrng_drng *lrng_drng_atomic_instance(void);

static __always_inline bool lrng_drng_is_atomic(struct lrng_drng *drng)
{
	return (drng->drng == lrng_drng_atomic_instance()->drng);
}

/* Lock the DRNG */
static __always_inline void lrng_drng_lock(struct lrng_drng *drng,
					   unsigned long *flags)
	__acquires(&drng->spin_lock)
{
	/* Use spin lock in case the atomic DRNG context is used */
	if (lrng_drng_is_atomic(drng)) {
		spin_lock_irqsave(&drng->spin_lock, *flags);

		/*
		 * In case a lock transition happened while we were spinning,
		 * catch this case and use the new lock type.
		 */
		if (!lrng_drng_is_atomic(drng)) {
			spin_unlock_irqrestore(&drng->spin_lock, *flags);
			__acquire(&drng->spin_lock);
			mutex_lock(&drng->lock);
		}
	} else {
		__acquire(&drng->spin_lock);
		mutex_lock(&drng->lock);
	}
}

/* Unlock the DRNG */
static __always_inline void lrng_drng_unlock(struct lrng_drng *drng,
					     unsigned long *flags)
	__releases(&drng->spin_lock)
{
	if (lrng_drng_is_atomic(drng)) {
		spin_unlock_irqrestore(&drng->spin_lock, *flags);
	} else {
		mutex_unlock(&drng->lock);
		__release(&drng->spin_lock);
	}
}

void lrng_reset(void);
void lrng_drngs_init_cc20(bool force_seed);
bool lrng_sp80090c_compliant(void);

static inline u32 lrng_compress_osr(void)
{
	return lrng_sp80090c_compliant() ?  CONFIG_LRNG_OVERSAMPLE_ES_BITS : 0;
}

static inline u32 lrng_reduce_by_osr(u32 entropy_bits)
{
	u32 osr_bits = lrng_compress_osr();
	return (entropy_bits >= osr_bits) ? (entropy_bits - osr_bits) : 0;
}

bool lrng_get_available(void);
void lrng_set_available(void);
void lrng_drng_reset(struct lrng_drng *drng);
int lrng_drng_get_atomic(u8 *outbuf, u32 outbuflen);
int lrng_drng_get_sleep(u8 *outbuf, u32 outbuflen);
void lrng_drng_force_reseed(void);
void lrng_drng_seed_work(struct work_struct *dummy);

#ifdef CONFIG_NUMA
struct lrng_drng **lrng_drng_instances(void);
void lrng_drngs_numa_alloc(void);
#else	/* CONFIG_NUMA */
static inline struct lrng_drng **lrng_drng_instances(void) { return NULL; }
static inline void lrng_drngs_numa_alloc(void) { return; }
#endif /* CONFIG_NUMA */

/************************* Entropy sources management *************************/

enum lrng_external_noise_source {
	lrng_noise_source_hw,
	lrng_noise_source_user
};

void lrng_set_entropy_thresh(u32 new);
u32 lrng_avail_entropy(void);
void lrng_reset_state(void);

bool lrng_state_exseed_allow(enum lrng_external_noise_source source);
void lrng_state_exseed_set(enum lrng_external_noise_source source, bool type);
bool lrng_state_min_seeded(void);
bool lrng_state_fully_seeded(void);
bool lrng_state_operational(void);

int lrng_pool_trylock(void);
void lrng_pool_unlock(void);
void lrng_pool_all_numa_nodes_seeded(bool set);
void lrng_pool_add_entropy(void);

struct entropy_buf {
	u8 a[LRNG_DRNG_INIT_SEED_SIZE_BYTES];
	u8 b[LRNG_DRNG_INIT_SEED_SIZE_BYTES];
	u8 c[LRNG_DRNG_INIT_SEED_SIZE_BYTES];
	u8 d[LRNG_DRNG_INIT_SEED_SIZE_BYTES];
	u32 now, a_bits, b_bits, c_bits, d_bits;
};

bool lrng_fully_seeded(bool fully_seeded, struct entropy_buf *eb);
void lrng_unset_fully_seeded(struct lrng_drng *drng);
void lrng_fill_seed_buffer(struct entropy_buf *entropy_buf, u32 requested_bits);
void lrng_init_ops(struct entropy_buf *eb);

/*********************** Auxiliary Pool Entropy Source ************************/

u32 lrng_avail_aux_entropy(void);
void lrng_aux_es_state(unsigned char *buf, size_t buflen);
u32 lrng_get_digestsize(void);
void lrng_pool_set_entropy(u32 entropy_bits);
int lrng_aux_switch_hash(const struct lrng_crypto_cb *new_cb, void *new_hash,
			 const struct lrng_crypto_cb *old_cb);
int lrng_pool_insert_aux(const u8 *inbuf, u32 inbuflen, u32 entropy_bits);
void lrng_get_backtrack_aux(struct entropy_buf *entropy_buf,
			    u32 requested_bits);

/* Obtain the security strength of the LRNG in bits */
static inline u32 lrng_security_strength(void)
{
	/*
	 * We use a hash to read the entropy in the entropy pool. According to
	 * SP800-90B table 1, the entropy can be at most the digest size.
	 * Considering this together with the last sentence in section 3.1.5.1.2
	 * the security strength of a (approved) hash is equal to its output
	 * size. On the other hand the entropy cannot be larger than the
	 * security strength of the used DRBG.
	 */
	return min_t(u32, LRNG_FULL_SEED_ENTROPY_BITS, lrng_get_digestsize());
}

static inline u32 lrng_get_seed_entropy_osr(bool fully_seeded)
{
	u32 requested_bits = lrng_security_strength();

	/* Apply oversampling during initialization according to SP800-90C */
	if (lrng_sp80090c_compliant() && !fully_seeded)
		requested_bits += CONFIG_LRNG_SEED_BUFFER_INIT_ADD_BITS;
	return requested_bits;
}

/************************** Health Test linking code **************************/

enum lrng_health_res {
	lrng_health_pass,		/* Health test passes on time stamp */
	lrng_health_fail_use,		/* Time stamp unhealthy, but mix in */
	lrng_health_fail_drop		/* Time stamp unhealthy, drop it */
};

#ifdef CONFIG_LRNG_HEALTH_TESTS
bool lrng_sp80090b_startup_complete(void);
bool lrng_sp80090b_compliant(void);

enum lrng_health_res lrng_health_test(u32 now_time);
void lrng_health_disable(void);

#else	/* CONFIG_LRNG_HEALTH_TESTS */
static inline bool lrng_sp80090b_startup_complete(void) { return true; }
static inline bool lrng_sp80090b_compliant(void) { return false; }

static inline enum lrng_health_res
lrng_health_test(u32 now_time) { return lrng_health_pass; }
static inline void lrng_health_disable(void) { }
#endif	/* CONFIG_LRNG_HEALTH_TESTS */

/****************************** Helper code ***********************************/

static inline u32 atomic_read_u32(atomic_t *v)
{
	return (u32)atomic_read(v);
}

/******************** Crypto Primitive Switching Support **********************/

#ifdef CONFIG_LRNG_DRNG_SWITCH
static inline void lrng_hash_lock(struct lrng_drng *drng, unsigned long *flags)
{
	read_lock_irqsave(&drng->hash_lock, *flags);
}

static inline void lrng_hash_unlock(struct lrng_drng *drng, unsigned long flags)
{
	read_unlock_irqrestore(&drng->hash_lock, flags);
}
#else /* CONFIG_LRNG_DRNG_SWITCH */
static inline void lrng_hash_lock(struct lrng_drng *drng, unsigned long *flags)
{ }

static inline void lrng_hash_unlock(struct lrng_drng *drng, unsigned long flags)
{ }
#endif /* CONFIG_LRNG_DRNG_SWITCH */

/*************************** Auxiliary functions ******************************/

void invalidate_batched_entropy(void);

/***************************** Testing code ***********************************/

#ifdef CONFIG_LRNG_RAW_HIRES_ENTROPY
bool lrng_raw_hires_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_HIRES_ENTROPY */
static inline bool lrng_raw_hires_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_HIRES_ENTROPY */

#ifdef CONFIG_LRNG_RAW_JIFFIES_ENTROPY
bool lrng_raw_jiffies_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_JIFFIES_ENTROPY */
static inline bool lrng_raw_jiffies_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_JIFFIES_ENTROPY */

#ifdef CONFIG_LRNG_RAW_IRQ_ENTROPY
bool lrng_raw_irq_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_IRQ_ENTROPY */
static inline bool lrng_raw_irq_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_IRQ_ENTROPY */

#ifdef CONFIG_LRNG_RAW_IRQFLAGS_ENTROPY
bool lrng_raw_irqflags_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_IRQFLAGS_ENTROPY */
static inline bool lrng_raw_irqflags_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_IRQFLAGS_ENTROPY */

#ifdef CONFIG_LRNG_RAW_RETIP_ENTROPY
bool lrng_raw_retip_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_RETIP_ENTROPY */
static inline bool lrng_raw_retip_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_RETIP_ENTROPY */

#ifdef CONFIG_LRNG_RAW_REGS_ENTROPY
bool lrng_raw_regs_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_REGS_ENTROPY */
static inline bool lrng_raw_regs_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_REGS_ENTROPY */

#ifdef CONFIG_LRNG_RAW_ARRAY
bool lrng_raw_array_entropy_store(u32 value);
#else	/* CONFIG_LRNG_RAW_ARRAY */
static inline bool lrng_raw_array_entropy_store(u32 value) { return false; }
#endif	/* CONFIG_LRNG_RAW_ARRAY */

#ifdef CONFIG_LRNG_IRQ_PERF
bool lrng_perf_time(u32 start);
#else /* CONFIG_LRNG_IRQ_PERF */
static inline bool lrng_perf_time(u32 start) { return false; }
#endif /*CONFIG_LRNG_IRQ_PERF */

#endif /* _LRNG_INTERNAL_H */
