/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/**
 * MIF stream (Interface)
 *
 * Provides a one-way communication mechanism between two points. The consumer side
 * will be notified via an interrupt when the producer side writes data to the
 * stream, and likewise the producer will be notified when the consumer has read
 * data from the stream.
 *
 * It is expected that the data sent across the stream consists of fixed-size
 * packets, and that the underlying storage mechanism is initialised to use a packet size
 * that is at least as large as the largest message size. If this is not the case,
 * callers are responsible for handling reading of partial messages from the stream
 * in multiples of the packet size.
 */

#ifndef MIFSTREAM_H__
#define MIFSTREAM_H__

/* Uses */

#include "cpacket_buffer.h"
#include "mifintrbit.h"
#include "scsc_logring_common.h"

/* Public Types */

enum MIF_STREAM_PEER {
	MIF_STREAM_PEER_R4,
	MIF_STREAM_PEER_M4,
};

enum MIF_STREAM_DIRECTION {
	MIF_STREAM_DIRECTION_IN,
	MIF_STREAM_DIRECTION_OUT,
};

/**
 * Defines for the MIF Stream interrupt bits
 *
 * MIF_STREAM_INTRBIT_TYPE_RESERVED: the bits are reserved
 * at initialization and are assigned to GDB transport channels.
 * It is for purpose of forcing Panics from either MX manager or GDB
 *
 * MIF_STREAM_INTRBIT_TYPE_ALLOC: the bits are allocated dynamically
 * when a stream is initialized
 */
enum MIF_STREAM_INTRBIT_TYPE {
	MIF_STREAM_INTRBIT_TYPE_RESERVED,
	MIF_STREAM_INTRBIT_TYPE_ALLOC,
};

/* Forward Decls */

struct mif_stream;

/* Public Functions */

/**
 * Initialises MIF Stream state.
 */
int mif_stream_init(struct mif_stream *stream, enum scsc_mif_abs_target target, enum MIF_STREAM_DIRECTION direction, uint32_t num_packets, uint32_t packet_size,
		    struct scsc_mx *mx, enum MIF_STREAM_INTRBIT_TYPE intrbit, mifintrbit_handler tohost_irq_handler, void *data);
/**
 * Initialises MIF Output Stream state.
 */
void mif_stream_release(struct mif_stream *stream);
/**
 * Reads the given number of bytes from the MIF stream, copying them
 * to the provided address. This removes the read data from the stream.
 *
 * Returns the number of bytes read.
 */
uint32_t mif_stream_read(struct mif_stream *stream, void *buf, uint32_t num_bytes);

/**
 * Returns a pointer to the next packet of data within the stream, without
 * removing it. This can be used to processss data in place without needing to
 * copy it first.
 *
 * If multiple packets are present these can be read in turn by setting the value
 * of current_packet to the returned value from the previous call to mif_stream_peek.
 * Each time the returned pointer will advance in the stream by mif_stream_block_size()
 * bytes.
 *
 * Callers cannot assume that multiple calls to mif_stream_peek() will return
 * consecutive addresses.
 *
 * mif_stream_peek_complete must be called to remove the packet(s) from the stream.
 *
 * Returns a pointer to the beginning of the packet to read, or NULL if there is no
 * packet to process.
 *
 * Example use:
 *   // Get the first data packet
 *   void *current_packet = mif_stream_peek( buffer, NULL );
 *   void *last_packet = NULL;
 *   while( current_packet != NULL )
 *   {
 *      // Process data packet
 *      ...
 *
 *      // Get the next data packet
 *      last_packet = current_packet;
 *      current_packet = mif_stream_peek( buffer, current_packet );
 *   }
 *
 *   // Remove all processed packets from the stream
 *   if( last_packet != NULL )
 *   {
 *      mif_stream_peek( buffer, last_packet );
 *   }
 */
const void *mif_stream_peek(struct mif_stream *stream, const void *current_packet);

/**
 * Removes all packets from the stream up to and including the given
 * packet.
 *
 * This must be called after using mif_stream_peek to indicate that packet(s)
 * can be removed from the stream.
 */
void mif_stream_peek_complete(struct mif_stream *stream, const void *packet);

/**
 * Writes the given number of bytes to the MIF stream.
 *
 * Returns true if the block was written, false if there is not enough
 * free space in the buffer for the data.
 */
bool mif_stream_write(struct mif_stream *stream, const void *buf, uint32_t num_bytes);

/**
 * Writes a set of non-contiguous data blocks to the MIF stream
 * as a contiguous set.
 *
 * Returns true if the blocks were written, false if there is not enough
 * free space in the buffer for the block.
 */
bool mif_stream_write_gather(struct mif_stream *stream, const void **bufs, uint32_t *lengths, uint32_t num_bufs);

/**
 * Returns the size in bytes of each individual block within the stream.
 *
 * When reading data from the stream using mif_stream_read or mif_stream_peek
 * this value is the amount of data
 */
uint32_t mif_stream_block_size(struct mif_stream *stream);

/**
 * Returns the interrupt number that will be triggered by reads from the stream
 */
uint8_t mif_stream_read_interrupt(struct mif_stream *stream);

/**
 * Returns the interrupt number that will be triggered by writes to the stream
 */
uint8_t mif_stream_write_interrupt(struct mif_stream *stream);

/*
 * Initialises the stream's part of the configuration area
 */
void mif_stream_config_serialise(struct mif_stream *stream, struct mxstreamconf *stream_conf);

/**
 * Log the state of this stream at the specified log_level.
 */
void mif_stream_log(const struct mif_stream *stream, enum scsc_log_level log_level);

/**
 * MIF Packet Stream Descriptor.
 */
struct mif_stream {
	struct scsc_mx       *mx;
	struct cpacketbuffer buffer;

	/** MIF stream peer, R4 or M4? */
	enum MIF_STREAM_PEER peer;

	/** MIF interrupt bit index, one in each direction */
	uint8_t              read_bit_idx;
	uint8_t              write_bit_idx;
	enum MIF_STREAM_DIRECTION direction;
};

#endif /* MIFSTREAM_H__ */
