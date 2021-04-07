/****************************************************************************
*
* Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
*
* Maxwell Software Mailbox Emulation shared data definitions.
*
* Ref: SC-506707-DD - Structure version 2
*
****************************************************************************/
/**
 * Circular buffer backed packet stream (Implementation)
 *
 * To allow easy distinction between full and empty buffers, there
 * is one slot reserved in the available space. This means that the following
 * conditions can be used to easily test the buffer's status without tracking
 * the used size explicitly:
 * - read_index == write_index : Buffer is empty
 * - write_index + 1 == read_index : Buffer is full
 * Otherwise if read_index == write_index then the buffer could be either
 * empty or full.
 */
/* Implements */
#include "cpacket_buffer.h"

/* Uses */
#include <scsc/scsc_logring.h>
#include <linux/module.h>
#include "miframman.h"


/**
 * Advances the read/write index by the given amount, wrapping around if this
 * exceeds the buffer length.
 */
static inline void cpacketbuffer_advance_index(uint32_t *idx, uint32_t amount, uint32_t buffer_size)
{
	*idx = (*idx + amount) % buffer_size;
}

/**
 * Converts a buffer address to a read/write index.
 *
 * The address must be at the start of a packet.
 */
static inline uint32_t cpacketbuffer_address_to_index(struct cpacketbuffer *buffer, const uint8_t *address)
{
	ptrdiff_t offset = address - (uint8_t *)buffer->buffer;

	return (offset / buffer->packet_size) % buffer->num_packets;
}

/**
 * Converts a buffer read/write index to an address.
 */
static inline uint8_t *cpacketbuffer_index_to_address(struct cpacketbuffer *buffer, uint32_t *idx)
{
	return (uint8_t *)buffer->buffer + (*idx % buffer->num_packets) * buffer->packet_size;
}

/** Returns the current read index of the buffer */
static inline uint32_t cpacketbuffer_read_index(const struct cpacketbuffer *buffer)
{
	return *buffer->read_index;
}

/** Returns the current write index of the buffer */
static inline uint32_t cpacketbuffer_write_index(const struct cpacketbuffer *buffer)
{
	return *buffer->write_index;
}

/** Writes a set of whole packets to the buffer */
static bool cpacketbuffer_write_block(struct cpacketbuffer *buffer, const void *buf, uint32_t num_bytes)
{
	uint32_t num_packets = (num_bytes + buffer->packet_size - 1) / buffer->packet_size;
	const uint8_t *source_data;
	uint32_t start_write_index;
	uint32_t end_write_index;

	if (num_packets > cpacketbuffer_free_space(buffer))
		/* Not enough free packets to write this block */
		return false;

	source_data = (const uint8_t *)buf;
	start_write_index = cpacketbuffer_write_index(buffer);
	end_write_index = start_write_index;
	cpacketbuffer_advance_index(&end_write_index, num_packets - 1, buffer->num_packets);

	if (end_write_index < start_write_index) {
		/* Writes wrap around the buffer, split the write in two */
		uint32_t initial_write_size = (buffer->num_packets - start_write_index) * buffer->packet_size;

		memcpy(cpacketbuffer_index_to_address(buffer, buffer->write_index), source_data, initial_write_size);
		memcpy(buffer->buffer, source_data + initial_write_size, num_bytes - initial_write_size);
	} else
		memcpy(cpacketbuffer_index_to_address(buffer, buffer->write_index), source_data, num_bytes);

	/* CPU memory barrier */
	wmb();
	cpacketbuffer_advance_index(buffer->write_index, num_packets, buffer->num_packets);

	return true;
}

/** Log buffer configuration at DEBUG level */
static void mxcbufconf_print(const struct mxcbufconf *buf_conf)
{
	SCSC_TAG_DBG4(CPKTBUFF, "mxcbufconf\n\tbuffer_loc: 0x%x\n\tnum_packets: %d\n\tpacket_size: %d\n\treadix: 0x%x\n\twriteix: 0x%x\n",
		      buf_conf->buffer_loc,
		      buf_conf->num_packets,
		      buf_conf->packet_size,
		      buf_conf->read_index_loc,
		      buf_conf->write_index_loc
		      );
}

/** Externally visible functions */

int cpacketbuffer_init(struct cpacketbuffer *buffer, uint32_t num_packets, uint32_t packet_size, struct scsc_mx *mx)
{
	struct miframman *miframman;
	uint32_t *ridx;
	uint32_t *widx;
	void *mem;

	buffer->mx = mx;

	miframman = scsc_mx_get_ramman(mx);
	mem = miframman_alloc(miframman, num_packets * packet_size, 4, MIFRAMMAN_OWNER_COMMON);
	if (!mem)
		return -ENOMEM;

	ridx = miframman_alloc(miframman, sizeof(uint32_t), 4, MIFRAMMAN_OWNER_COMMON);
	if (!ridx) {
		miframman_free(miframman, mem);
		return -ENOMEM;
	}

	widx = miframman_alloc(miframman, sizeof(uint32_t), 4, MIFRAMMAN_OWNER_COMMON);
	if (!widx) {
		miframman_free(miframman, ridx);
		miframman_free(miframman, mem);
		return -ENOMEM;
	}

	buffer->buffer = mem;
	buffer->num_packets = num_packets;
	buffer->packet_size = packet_size;
	buffer->read_index = ridx;
	buffer->write_index = widx;

	*buffer->read_index = 0;
	*buffer->write_index = 0;
	return 0;
}

void cpacketbuffer_release(struct cpacketbuffer *buffer)
{
	struct miframman *miframman;

	miframman = scsc_mx_get_ramman(buffer->mx);

	miframman_free(miframman, buffer->read_index);
	miframman_free(miframman, buffer->write_index);
	miframman_free(miframman, buffer->buffer);
}

bool cpacketbuffer_write(struct cpacketbuffer *buffer, const void *buf, uint32_t num_bytes)
{
	uint32_t start_write_index;

	if (buf == NULL || num_bytes == 0)
		return false;

	SCSC_TAG_DBG4(CPKTBUFF, "Before: *buffer->read_index=0x%x *buffer->write_index=0x%x\n",
		      *buffer->read_index, *buffer->write_index);

	start_write_index = cpacketbuffer_write_index(buffer);
	if (!cpacketbuffer_write_block(buffer, buf, num_bytes))
		return false;

	/* CPU memory barrier */
	wmb();

	SCSC_TAG_DBG4(CPKTBUFF, "After: *buffer->read_index=0x%x *buffer->write_index=0x%x\n",
		      *buffer->read_index, *buffer->write_index);

	return true;
}

bool cpacketbuffer_write_gather(struct cpacketbuffer *buffer, const void **bufs, uint32_t *num_bytes, uint32_t num_bufs)
{
	uint32_t start_write_index;
	uint32_t i;

	if (bufs == NULL || num_bytes == 0 || num_bufs == 0)
		return false;

	start_write_index = cpacketbuffer_write_index(buffer);
	for (i = 0; i < num_bufs; ++i) {
		/* Write all the whole packets from this buffer */
		uint32_t partial_packet_len = num_bytes[i] % buffer->packet_size;
		uint32_t whole_packet_len = num_bytes[i] - partial_packet_len;

		if (whole_packet_len > 0 &&
		    !cpacketbuffer_write_block(buffer, bufs[i], whole_packet_len))
			return false;

		if (partial_packet_len != 0) {
			/* Partial packet present - write this and enough from the next data block(s) to fill this packet
			 * before continuing */
			uint32_t needed_bytes;
			uint8_t  *write_ptr = cpacketbuffer_index_to_address(buffer, buffer->write_index);

			memcpy(write_ptr, (const uint8_t *)bufs[i] + whole_packet_len, partial_packet_len);
			write_ptr += partial_packet_len;
			needed_bytes = buffer->packet_size - partial_packet_len;
			while (i + 1 < num_bufs && needed_bytes > 0) {
				uint32_t num_bytes_to_take = num_bytes[i + 1] >= needed_bytes ? needed_bytes : num_bytes[i + 1];

				memcpy(write_ptr, bufs[i + 1], num_bytes_to_take);
				bufs[i + 1] = (const uint8_t *)bufs[i + 1] + num_bytes_to_take;
				num_bytes[i + 1] -= num_bytes_to_take;
				write_ptr += num_bytes_to_take;
				needed_bytes -= num_bytes_to_take;

				if (num_bytes[i + 1] == 0)
					/* This buffer has been consumed entirely, move to the next */
					++i;
			}

			/* CPU memory barrier */
			wmb();
			cpacketbuffer_advance_index(buffer->write_index, 1, buffer->num_packets);
		}
	}

	/* CPU memory barrier */
	wmb();

	return true;
}

uint32_t cpacketbuffer_read(struct cpacketbuffer *buffer, void *buf, uint32_t num_bytes)
{
	uint8_t  *read_start;
	uint32_t num_packets;
	uint32_t num_available_packets;

	if (buf == NULL || cpacketbuffer_is_empty(buffer))
		return 0;

	/* Work out where we're reading from */
	read_start = cpacketbuffer_index_to_address(buffer, buffer->read_index);
	num_packets = num_bytes / buffer->packet_size;
	if (num_bytes % buffer->packet_size != 0)
		/* Partial data packet read requested, this means we remove the whole thing */
		++num_packets;

	/* Ensure we have enough actual data to satisfy the read request, otherwise
	 * truncate the read request to the amount of data available. */
	num_available_packets = buffer->num_packets - (cpacketbuffer_free_space(buffer) + 1);
	if (num_packets > num_available_packets) {
		num_packets = num_available_packets;
		num_bytes = num_packets * buffer->packet_size;
	}

	if (cpacketbuffer_read_index(buffer) + num_packets > buffer->num_packets) {
		/* The read wraps around the end of the buffer, do it in two parts */
		uint32_t initial_read_size = (buffer->num_packets - cpacketbuffer_read_index(buffer)) * buffer->packet_size;

		memcpy(buf, read_start, initial_read_size);
		memcpy((uint8_t *)buf + initial_read_size, buffer->buffer, num_bytes - initial_read_size);
	} else
		memcpy(buf, read_start, num_bytes);

	/* CPU memory barrier */
	wmb();
	/* Update the read index with how many packets we pulled out of the stream */
	cpacketbuffer_advance_index(buffer->read_index, num_packets, buffer->num_packets);
	/* CPU memory barrier */
	wmb();

	return num_bytes;
}

const void *cpacketbuffer_peek(struct cpacketbuffer *buffer, const void *current_packet)
{
	uint32_t next_packet_index;

	SCSC_TAG_DBG4(CPKTBUFF, "*buffer->read_index=0x%x *buffer->write_index=0x%x\n",
		      *buffer->read_index, *buffer->write_index);
	if (current_packet == NULL)
		/* Reading the first available packet */
		next_packet_index = cpacketbuffer_read_index(buffer);
	else
		/* Reading the next available packet past the current value of current_packet */
		next_packet_index = cpacketbuffer_address_to_index(buffer,
								   (const uint8_t *)current_packet + buffer->packet_size);

	if (next_packet_index == cpacketbuffer_write_index(buffer))
		/* No more packets available */
		return NULL;

	return cpacketbuffer_index_to_address(buffer, &next_packet_index);
}

void cpacketbuffer_peek_complete(struct cpacketbuffer *buffer, const void *current_packet)
{
	if (current_packet == NULL)
		return;

	/* The address we're given is the last packet read, so the new read index is for the next one */
	*buffer->read_index = cpacketbuffer_address_to_index(buffer,
							     (const uint8_t *)current_packet + buffer->packet_size);
	/* CPU memory barrier */
	wmb();
}

bool cpacketbuffer_is_empty(const struct cpacketbuffer *buffer)
{
	return cpacketbuffer_read_index(buffer) == cpacketbuffer_write_index(buffer);
}

bool cpacketbuffer_is_full(const struct cpacketbuffer *buffer)
{
	return (cpacketbuffer_write_index(buffer) + 1) % buffer->num_packets == cpacketbuffer_read_index(buffer);
}

uint32_t cpacketbuffer_free_space(const struct cpacketbuffer *buffer)
{
	uint32_t base_free_space = cpacketbuffer_write_index(buffer) >= cpacketbuffer_read_index(buffer) ?
				   cpacketbuffer_read_index(buffer) + buffer->num_packets - cpacketbuffer_write_index(buffer) :
				   cpacketbuffer_read_index(buffer) - cpacketbuffer_write_index(buffer);

	/* Subtract the full/empty identification reserved slot from the free space */
	return base_free_space - 1;
}

uint32_t cpacketbuffer_packet_size(const struct cpacketbuffer *buffer)
{
	return buffer->packet_size;
}

void cpacketbuffer_config_serialise(const struct cpacketbuffer *buffer, struct mxcbufconf *buf_conf)
{
	scsc_mifram_ref mifram_ref;
	struct scsc_mif_abs *mif;

	mif = scsc_mx_get_mif_abs(buffer->mx);

	mif->get_mifram_ref(mif, buffer->buffer, &mifram_ref);
	buf_conf->buffer_loc = mifram_ref;
	buf_conf->num_packets = buffer->num_packets;
	buf_conf->packet_size = buffer->packet_size;
	mif->get_mifram_ref(mif, buffer->read_index, &mifram_ref);
	buf_conf->read_index_loc = mifram_ref;
	mif->get_mifram_ref(mif, buffer->write_index, &mifram_ref);
	buf_conf->write_index_loc = mifram_ref;

	mxcbufconf_print(buf_conf);
}

void cpacketbuffer_log(const struct cpacketbuffer *buffer, enum scsc_log_level log_level)
{
	const uint8_t *read_start = cpacketbuffer_index_to_address((struct cpacketbuffer *)buffer, buffer->read_index);

	SCSC_TAG_LVL((CPKTBUFF), log_level,
		"read_index=0x%x write_index=0x%x, read_start[0]=0x%08x\n",
		*buffer->read_index, *buffer->write_index,
		*(uint32_t *)read_start);
}

