/****************************************************************************
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/**/

#ifndef _PORTING_IMX__H__
#define _PORTING_IMX__H__
#endif
/**
 * ether_addr_copy - Copy an Ethernet address
 * @dst: Pointer to a six-byte array Ethernet address destination
 * @src: Pointer to a six-byte array Ethernet address source
 *
 * Please note: dst & src must both be aligned to u16.
 */
static inline void ether_addr_copy(u8 *dst, const u8 *src)
{
#if defined(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)
        *(u32 *)dst = *(const u32 *)src;
        *(u16 *)(dst + 4) = *(const u16 *)(src + 4);
#else
        u16 *a = (u16 *)dst;
        const u16 *b = (const u16 *)src;

        a[0] = b[0];
        a[1] = b[1];
        a[2] = b[2];
#endif
}

static inline ktime_t ktime_add_ms(const ktime_t kt, const u64 msec)
{
        return ktime_add_ns(kt, msec * NSEC_PER_MSEC);
}


