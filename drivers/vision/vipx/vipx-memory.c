/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <asm/cacheflush.h>
#include <linux/ion_exynos.h>
#include <linux/exynos_iovmm.h>

#include "vipx-log.h"
#include "vipx-mailbox.h"
#include "vipx-system.h"
#include "vipx-memory.h"

static int vipx_memory_map_dmabuf(struct vipx_memory *mem,
		struct vipx_buffer *buf)
{
	int ret;
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t dvaddr;
	void *kvaddr;

	vipx_enter();
	if (buf->m.fd <= 0) {
		ret = -EINVAL;
		vipx_err("fd(%d) is invalid\n", buf->m.fd);
		goto p_err;
	}

	dbuf = dma_buf_get(buf->m.fd);
	if (IS_ERR(dbuf)) {
		ret = PTR_ERR(dbuf);
		vipx_err("dma_buf is invalid (%d/%d)\n", buf->m.fd, ret);
		goto p_err;
	}
	buf->dbuf = dbuf;

	if (buf->size + buf->offset > dbuf->size) {
		ret = -EINVAL;
		vipx_err("size is invalid (%zu/%u/%zu)\n",
				buf->size, buf->offset, dbuf->size);
		goto p_err_size;
	}
	buf->dbuf_size = dbuf->size;

	attachment = dma_buf_attach(dbuf, mem->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		vipx_err("failed to attach dma-buf (%d)\n", ret);
		goto p_err_attach;
	}
	buf->attachment = attachment;

	sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		vipx_err("failed to map attachment (%d)\n", ret);
		goto p_err_map_attach;
	}
	buf->sgt = sgt;

	dvaddr = ion_iovmm_map(attachment, 0, buf->dbuf_size,
			DMA_BIDIRECTIONAL, 0);
	if (IS_ERR_VALUE(dvaddr)) {
		ret = (int)dvaddr;
		vipx_err("failed to map iova (%d)\n", ret);
		goto p_err_iova;
	}
	buf->dvaddr = dvaddr;

	if (buf->mem_attr == VIPX_COMMON_CACHEABLE) {
		kvaddr = dma_buf_vmap(dbuf);
		if (IS_ERR(kvaddr)) {
			ret = PTR_ERR(kvaddr);
			vipx_err("failed to map kvaddr (%d)\n", ret);
			goto p_err_kva;
		}
		buf->kvaddr = kvaddr;
	} else {
		buf->kvaddr = NULL;
	}

	vipx_leave();
	return 0;
p_err_kva:
	ion_iovmm_unmap(attachment, dvaddr);
p_err_iova:
	dma_buf_unmap_attachment(attachment, sgt, DMA_BIDIRECTIONAL);
p_err_map_attach:
	dma_buf_detach(dbuf, attachment);
p_err_attach:
p_err_size:
	dma_buf_put(dbuf);
p_err:
	return ret;
}

static int vipx_memory_unmap_dmabuf(struct vipx_memory *mem,
		struct vipx_buffer *buf)
{
	vipx_enter();
	if (buf->kvaddr)
		dma_buf_vunmap(buf->dbuf, buf->kvaddr);

	ion_iovmm_unmap(buf->attachment, buf->dvaddr);
	dma_buf_unmap_attachment(buf->attachment, buf->sgt, DMA_BIDIRECTIONAL);
	dma_buf_detach(buf->dbuf, buf->attachment);
	dma_buf_put(buf->dbuf);
	vipx_leave();
	return 0;
}

static int vipx_memory_sync_for_device(struct vipx_memory *mem,
		struct vipx_buffer *buf)
{
	vipx_enter();
	if (buf->mem_attr == VIPX_COMMON_NON_CACHEABLE) {
		vipx_warn("It is not required to sync non-cacheable area(%d)\n",
				buf->m.fd);
		return 0;
	}

	if (!buf->kvaddr) {
		vipx_err("kvaddr is required to sync cacheable area(%d)\n",
				buf->m.fd);
		return -EINVAL;
	}

	__dma_map_area(buf->kvaddr + buf->offset, buf->size, buf->direction);

	vipx_leave();
	return 0;
}

static int vipx_memory_sync_for_cpu(struct vipx_memory *mem,
		struct vipx_buffer *buf)
{
	vipx_enter();
	if (buf->mem_attr == VIPX_COMMON_NON_CACHEABLE) {
		vipx_warn("It is not required to sync non-cacheable area(%d)\n",
				buf->m.fd);
		return 0;
	}

	if (!buf->kvaddr) {
		vipx_err("kvaddr is required to sync cacheable area(%d)\n",
				buf->m.fd);
		return -EINVAL;
	}

	__dma_unmap_area(buf->kvaddr + buf->offset, buf->size, buf->direction);

	vipx_leave();
	return 0;
}

const struct vipx_memory_ops vipx_memory_ops = {
	.map_dmabuf		= vipx_memory_map_dmabuf,
	.unmap_dmabuf		= vipx_memory_unmap_dmabuf,
	.sync_for_device	= vipx_memory_sync_for_device,
	.sync_for_cpu		= vipx_memory_sync_for_cpu,
};

static int __vipx_memory_iovmm_map_sg(struct vipx_memory *mem,
		struct vipx_priv_mem *pmem)
{
	size_t size;

	vipx_enter();
	size = iommu_map_sg(mem->domain, pmem->dvaddr, pmem->sgt->sgl,
			pmem->sgt->nents, 0);
	if (!size) {
		vipx_err("Failed to map sg\n");
		return -ENOMEM;
	}

	if (size != pmem->size) {
		vipx_warn("pmem size(%zd) is different from mapped size(%zd)\n",
				pmem->size, size);
		pmem->size = size;
	}

	vipx_leave();
	return 0;
}

extern void exynos_sysmmu_tlb_invalidate(struct iommu_domain *iommu_domain,
		dma_addr_t d_start, size_t size);

static int __vipx_memory_iovmm_unmap(struct vipx_memory *mem,
		struct vipx_priv_mem *pmem)
{
	size_t size;

	vipx_enter();
	size = iommu_unmap(mem->domain, pmem->dvaddr, pmem->size);
	if (size < 0) {
		vipx_err("Failed to unmap iovmm(%zd)\n", size);
		return size;
	}
	exynos_sysmmu_tlb_invalidate(mem->domain, pmem->dvaddr, pmem->size);

	vipx_leave();
	return 0;
}

static int __vipx_memory_alloc(struct vipx_memory *mem,
		struct vipx_priv_mem *pmem)
{
	int ret;
	const char *heap_name = "ion_system_heap";
	struct dma_buf *dbuf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sgt;
	dma_addr_t dvaddr;
	void *kvaddr;

	vipx_enter();
	dbuf = ion_alloc_dmabuf(heap_name, pmem->size, pmem->flags);
	if (IS_ERR(dbuf)) {
		ret = PTR_ERR(dbuf);
		vipx_err("Failed to allocate dma_buf (%d) [%s]\n",
				ret, pmem->name);
		goto p_err_alloc;
	}
	pmem->dbuf = dbuf;
	pmem->dbuf_size = dbuf->size;

	attachment = dma_buf_attach(dbuf, mem->dev);
	if (IS_ERR(attachment)) {
		ret = PTR_ERR(attachment);
		vipx_err("Failed to attach dma_buf (%d) [%s]\n",
				ret, pmem->name);
		goto p_err_attach;
	}
	pmem->attachment = attachment;

	sgt = dma_buf_map_attachment(attachment, pmem->direction);
	if (IS_ERR(sgt)) {
		ret = PTR_ERR(sgt);
		vipx_err("Failed to map attachment (%d) [%s]\n",
				ret, pmem->name);
		goto p_err_map_attachment;
	}
	pmem->sgt = sgt;

	if (pmem->kmap) {
		kvaddr = dma_buf_vmap(dbuf);
		if (IS_ERR(kvaddr)) {
			ret = PTR_ERR(kvaddr);
			vipx_err("Failed to map kvaddr (%d) [%s]\n",
					ret, pmem->name);
			goto p_err_kmap;
		}
		pmem->kvaddr = kvaddr;
	}

	if (pmem->fixed_dvaddr) {
		ret = __vipx_memory_iovmm_map_sg(mem, pmem);
		if (ret)
			goto p_err_map_dva;
	} else {
		dvaddr = ion_iovmm_map(attachment, 0, pmem->size,
				pmem->direction, 0);
		if (IS_ERR_VALUE(dvaddr)) {
			ret = (int)dvaddr;
			vipx_err("Failed to map dvaddr (%d) [%s]\n",
					ret, pmem->name);
			goto p_err_map_dva;
		}
		pmem->dvaddr = dvaddr;
	}

	if (pmem->kmap)
		vipx_info("[%20s] memory is allocated(%#p,%#x,%zuKB)",
				pmem->name, kvaddr, (int)pmem->dvaddr,
				pmem->size / SZ_1K);
	else
		vipx_info("[%20s] memory is allocated(%#x,%zuKB)",
				pmem->name, (int)pmem->dvaddr,
				pmem->size / SZ_1K);

	vipx_leave();
	return 0;
p_err_map_dva:
	if (pmem->kmap)
		dma_buf_vunmap(dbuf, kvaddr);
p_err_kmap:
	dma_buf_unmap_attachment(attachment, sgt, pmem->direction);
p_err_map_attachment:
	dma_buf_detach(dbuf, attachment);
p_err_attach:
	dma_buf_put(dbuf);
p_err_alloc:
	return ret;
}

static void __vipx_memory_free(struct vipx_memory *mem,
		struct vipx_priv_mem *pmem)
{
	vipx_enter();
	if (pmem->fixed_dvaddr)
		__vipx_memory_iovmm_unmap(mem, pmem);
	else
		ion_iovmm_unmap(pmem->attachment, pmem->dvaddr);

	if (pmem->kmap)
		dma_buf_vunmap(pmem->dbuf, pmem->kvaddr);

	dma_buf_unmap_attachment(pmem->attachment, pmem->sgt, pmem->direction);
	dma_buf_detach(pmem->dbuf, pmem->attachment);
	dma_buf_put(pmem->dbuf);
	vipx_leave();
}

int vipx_memory_open(struct vipx_memory *mem)
{
	int ret;

	vipx_enter();
	ret = __vipx_memory_alloc(mem, &mem->fw);
	if (ret)
		goto p_err_map;

	if (mem->mbox.size < sizeof(struct vipx_mailbox_ctrl)) {
		vipx_err("mailbox(%zu) is larger than allocated memory(%zu)\n",
				sizeof(struct vipx_mailbox_ctrl),
				mem->mbox.size);
		goto p_err_mbox;
	}

	ret = __vipx_memory_alloc(mem, &mem->mbox);
	if (ret)
		goto p_err_mbox;

	ret = __vipx_memory_alloc(mem, &mem->heap);
	if (ret)
		goto p_err_heap;

	ret = __vipx_memory_alloc(mem, &mem->log);
	if (ret)
		goto p_err_debug;

	vipx_leave();
	return 0;
p_err_debug:
	__vipx_memory_free(mem, &mem->heap);
p_err_heap:
	__vipx_memory_free(mem, &mem->mbox);
p_err_mbox:
	__vipx_memory_free(mem, &mem->fw);
p_err_map:
	return ret;
}

int vipx_memory_close(struct vipx_memory *mem)
{
	vipx_enter();
	__vipx_memory_free(mem, &mem->log);
	__vipx_memory_free(mem, &mem->heap);
	__vipx_memory_free(mem, &mem->mbox);
	__vipx_memory_free(mem, &mem->fw);
	vipx_leave();
	return 0;
}

int vipx_memory_probe(struct vipx_system *sys)
{
	struct device *dev;
	struct vipx_memory *mem;
	struct vipx_priv_mem *fw;
	struct vipx_priv_mem *mbox;
	struct vipx_priv_mem *heap;
	struct vipx_priv_mem *log;

	vipx_enter();
	dev = sys->dev;
	dma_set_mask(dev, DMA_BIT_MASK(36));

	mem = &sys->memory;
	mem->dev = dev;
	mem->domain = get_domain_from_dev(dev);
	mem->mops = &vipx_memory_ops;

	fw = &mem->fw;
	mbox = &mem->mbox;
	heap = &mem->heap;
	log = &mem->log;

	snprintf(fw->name, VIPX_PRIV_MEM_NAME_LEN, "CC_DRAM_BIN");
	fw->size = PAGE_ALIGN(VIPX_CC_DRAM_BIN_SIZE);
	fw->flags = 0;
	fw->direction = DMA_TO_DEVICE;
	fw->kmap = true;
	fw->dvaddr = VIPX_CC_DRAM_BIN_DVADDR;
	fw->fixed_dvaddr = true;

	snprintf(mbox->name, VIPX_PRIV_MEM_NAME_LEN, "MBOX");
	mbox->size = PAGE_ALIGN(VIPX_MBOX_SIZE);
	mbox->flags = 0;
	mbox->direction = DMA_BIDIRECTIONAL;
	mbox->kmap = true;
	mbox->dvaddr = VIPX_MBOX_DVADDR;
	mbox->fixed_dvaddr = true;

	snprintf(heap->name, VIPX_PRIV_MEM_NAME_LEN, "HEAP");
	heap->size = PAGE_ALIGN(VIPX_HEAP_SIZE);
	heap->flags = 0;
	heap->direction = DMA_FROM_DEVICE;
	heap->dvaddr = VIPX_HEAP_DVADDR;
	heap->fixed_dvaddr = true;

	snprintf(log->name, VIPX_PRIV_MEM_NAME_LEN, "LOG");
	log->size = PAGE_ALIGN(VIPX_LOG_SIZE);
	log->flags = 0;
	log->direction = DMA_BIDIRECTIONAL;
	log->kmap = true;
	log->dvaddr = VIPX_LOG_DVADDR;
	log->fixed_dvaddr = true;

	vipx_leave();
	return 0;
}

void vipx_memory_remove(struct vipx_memory *mem)
{
	vipx_enter();
	vipx_leave();
}
