/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"

#include "scsc/api/bt_audio.h"
#include "miframman.h"
#include "mifproc.h"

/* Caller should provide locking */
void miframman_init(struct miframman *ram, void *start_dram, size_t size_pool, void *start_region)
{
	mutex_init(&ram->lock);

	SCSC_TAG_INFO(MIF, "MIFRAMMAN_BLOCK_SIZE = %d\n", MIFRAMMAN_BLOCK_SIZE);
	ram->num_blocks = size_pool / MIFRAMMAN_BLOCK_SIZE;

	if (ram->num_blocks == 0) {
		SCSC_TAG_ERR(MIF, "Pool size < BLOCK_SIZE\n");
		return;
	}

	if (ram->num_blocks >= MIFRAMMAN_NUM_BLOCKS) {
		SCSC_TAG_ERR(MIF, "Not enough memory\n");
		return;
	}

	memset(ram->bitmap, BLOCK_FREE, sizeof(ram->bitmap));

	ram->start_region = start_region;  /* For monitoring purposes only */
	ram->start_dram = start_dram;
	ram->size_pool = size_pool;
	ram->free_mem = ram->num_blocks * MIFRAMMAN_BLOCK_SIZE;

	mifproc_create_ramman_proc_dir(ram);
}

void miframabox_init(struct mifabox *mifabox, void *start_aboxram)
{
	/* No locking as not a shared resource */
	mifabox->aboxram = (struct scsc_bt_audio_abox *)start_aboxram;
}

void *__miframman_alloc(struct miframman *ram, size_t nbytes, int tag)
{
	unsigned int index = 0;
	unsigned int mem_itr_index = 0;
	unsigned int available;
	unsigned int i;
	unsigned int min_available_blocks = MIFRAMMAN_NUM_BLOCKS;
	size_t       num_blocks = 0;
	void         *free_mem = NULL;
	bool 		 has_available_blocks = false;

	if (!nbytes || nbytes > ram->free_mem) {
		SCSC_TAG_INFO(MIF, "nbytes > ram->free_mem");
		goto end;
	}

	/* Number of blocks required (rounding up) */
	num_blocks = nbytes / MIFRAMMAN_BLOCK_SIZE +
		     ((nbytes % MIFRAMMAN_BLOCK_SIZE) > 0 ? 1 : 0);

	if (num_blocks > ram->num_blocks) {
		SCSC_TAG_INFO(MIF, "num_blocks > ram->num_blocks\n");
		goto end;
	}

	/* Iterate through whole memory and find shortest contiguous memory region
	 * from where allocation can be made
	 */
	while (mem_itr_index <= (ram->num_blocks - num_blocks)) {
		available = 0;

		/* Search for next consecutive block */
		for (i = 0; i < (ram->num_blocks - mem_itr_index); i++) {
			if (ram->bitmap[i + mem_itr_index] != BLOCK_FREE)
				break;
			available++;
		}

		if (available >= num_blocks) {
			has_available_blocks = true;

			if (available < min_available_blocks) {
				min_available_blocks = available;
				index = mem_itr_index;
			}
		}
		mem_itr_index = mem_itr_index + available + 1;
	}

	/* If we have found the memory we need */
	if (has_available_blocks) {
		free_mem = ram->start_dram +
			MIFRAMMAN_BLOCK_SIZE * index;

		/* Mark the block boundary as used */
		ram->bitmap[index] = BLOCK_BOUND;
		ram->bitmap[index] |= (u8)(tag <<  MIFRAMMAN_BLOCK_OWNER_SHIFT); /* Add owner tack for tracking */
		index++;

		/* Additional blocks in this allocation */
		for (i = 1; i < num_blocks; i++) {
			ram->bitmap[index] = BLOCK_INUSE;
			ram->bitmap[index] |= (u8)(tag <<  MIFRAMMAN_BLOCK_OWNER_SHIFT); /* Add owner tack for tracking */
			index++;
		}

		ram->free_mem -= num_blocks * MIFRAMMAN_BLOCK_SIZE;
		goto exit;
	}
end:
	SCSC_TAG_INFO(MIF, "Not enough shared memory (nbytes %zd, free_mem %u, num_blocks %zd, ram_num_blocks %u)\n",
		nbytes, ram->free_mem, num_blocks, ram->num_blocks);
	return NULL;
exit:
	return free_mem;
}


#define MIFRAMMAN_ALIGN(mem, align) \
	((void *)((((uintptr_t)(mem) + (align + sizeof(void *))) \
		   & (~(uintptr_t)(align - 1)))))

#define MIFRAMMAN_PTR(mem) \
	(*(((void **)((uintptr_t)(mem) & \
		      (~(uintptr_t)(sizeof(void *) - 1)))) - 1))

/*
 * Allocate shared DRAM block
 *
 * Parameters:
 *  ram		- pool identifier
 *  nbytes	- allocation size
 *  align	- allocation alignment
 *  tag		- owner identifier (typically service ID), 4 bits.
 *
 * Returns
 *  Pointer to allocated area, or NULL
 */
void *miframman_alloc(struct miframman *ram, size_t nbytes, size_t align, int tag)
{
	void *mem, *align_mem = NULL;

	mutex_lock(&ram->lock);
	if (!is_power_of_2(align) || nbytes == 0) {
		SCSC_TAG_ERR(MIF, "Failed size/alignment check (nbytes %zd, align %zd)\n", nbytes, align);
		goto end;
	}

	if (align < sizeof(void *))
		align = sizeof(void *);

	mem = __miframman_alloc(ram, nbytes + align + sizeof(void *), tag);
	if (!mem)
		goto end;

	align_mem = MIFRAMMAN_ALIGN(mem, align);

	/* Store allocated pointer */
	MIFRAMMAN_PTR(align_mem) = mem;
end:
	mutex_unlock(&ram->lock);
	return align_mem;
}

/*
 * Free shared DRAM block
 *
 * Parameters:
 *  ram		- pool identifier
 *  mem		- buffer to free
 */
void __miframman_free(struct miframman *ram, void *mem)
{
	unsigned int index, num_blocks = 0;

	if (ram->start_dram == NULL || !mem) {
		SCSC_TAG_ERR(MIF, "Mem is NULL\n");
		return;
	}

	/* Get block index */
	index = (unsigned int)((mem - ram->start_dram)
			       / MIFRAMMAN_BLOCK_SIZE);

	/* Check */
	if (index >= ram->num_blocks) {
		SCSC_TAG_ERR(MIF, "Incorrect index %d\n", index);
		return;
	}

	/* Check it is a Boundary block */
	if ((ram->bitmap[index] & MIFRAMMAN_BLOCK_STATUS_MASK) != BLOCK_BOUND) {
		SCSC_TAG_ERR(MIF, "Incorrect Block descriptor\n");
		return;
	}
	ram->bitmap[index++] = BLOCK_FREE;

	/* Free remaining blocks */
	num_blocks++;
	while (index < ram->num_blocks && (ram->bitmap[index] & MIFRAMMAN_BLOCK_STATUS_MASK) == BLOCK_INUSE) {
		ram->bitmap[index++] = BLOCK_FREE;
		num_blocks++;
	}

	ram->free_mem += num_blocks * MIFRAMMAN_BLOCK_SIZE;
}

void miframman_free(struct miframman *ram, void *mem)
{
	mutex_lock(&ram->lock);
	/* Restore allocated pointer */
	if (mem)
		__miframman_free(ram, MIFRAMMAN_PTR(mem));
	mutex_unlock(&ram->lock);
}

/* Caller should provide locking */
void miframman_deinit(struct miframman *ram)
{
	/* Mark all the blocks as INUSE (by Common) to prevent new allocations */
	memset(ram->bitmap, BLOCK_INUSE, sizeof(ram->bitmap));

	ram->num_blocks = 0;
	ram->start_dram = NULL;
	ram->size_pool = 0;
	ram->free_mem = 0;

	mifproc_remove_ramman_proc_dir(ram);
}

void miframabox_deinit(struct mifabox *mifabox)
{
	/* not dynamic - so just mark as NULL */
	/* Maybe this function should be empty? */
	mifabox->aboxram = NULL;
}

/* Log current allocations in a ramman in proc */
void miframman_log(struct miframman *ram, struct seq_file *fd)
{
	unsigned int b;
	unsigned int i;
	int tag;
	size_t num_blocks = 0;

	if (!ram)
		return;

	seq_printf(fd, "ramman: start_dram %p, size %zd, free_mem %u\n\n",
		ram->start_region, ram->size_pool, ram->free_mem);

	for (b = 0; b < ram->num_blocks; b++) {
		if ((ram->bitmap[b] & MIFRAMMAN_BLOCK_STATUS_MASK) == BLOCK_BOUND) {
			/* Found a boundary allocation */
			num_blocks++;
			tag = (ram->bitmap[b] & MIFRAMMAN_BLOCK_OWNER_MASK) >> MIFRAMMAN_BLOCK_OWNER_SHIFT;

			/* Count subsequent blocks in this group */
			for (i = 1;
			     i < ram->num_blocks && (ram->bitmap[b + i] & MIFRAMMAN_BLOCK_STATUS_MASK) == BLOCK_INUSE;
			     i++) {
				/* Check owner matches boundary block */
				int newtag = (ram->bitmap[b + i] & MIFRAMMAN_BLOCK_OWNER_MASK) >> MIFRAMMAN_BLOCK_OWNER_SHIFT;
				if (newtag != tag) {
					seq_printf(fd, "Allocated block tag %d doesn't match boundary tag %d, index %d, %p\n",
						newtag, tag, b + i,
						ram->start_dram + (b + i) * MIFRAMMAN_BLOCK_SIZE);
				}
				num_blocks++;
			}
			seq_printf(fd, "index %8d, svc %d, bytes %12d, blocks %10d, %p\n",
				b, tag,
				(i * MIFRAMMAN_BLOCK_SIZE),
				i,
				ram->start_dram + (b  * MIFRAMMAN_BLOCK_SIZE));
		}
	}
}
