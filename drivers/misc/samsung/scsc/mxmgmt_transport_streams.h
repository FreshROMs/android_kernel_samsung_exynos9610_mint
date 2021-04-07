/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef MXMGR_TRANSPORT_STREAMS_H__
#define MXMGR_TRANSPORT_STREAMS_H__

/**
 * MIF input/output streams to/from the AP.
 * These are seperated out to allow their use directly from within unit tests.
 */
struct {
	/** from AP */
	mif_stream *istream;
	/** to AP */
	mif_stream *ostream;
} mxmgr_stream_container;

extern mxmgr_stream_container mxmgr_streams;


#endif /* MXMGR_TRANSPORT_STREAMS_H__ */
