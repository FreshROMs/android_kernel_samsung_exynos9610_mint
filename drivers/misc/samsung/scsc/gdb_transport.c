/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

/** Implements */
#include "gdb_transport.h"

/** Uses */
#include <linux/module.h>
#include <linux/slab.h>
#include <scsc/scsc_logring.h>
#include "mifintrbit.h"

struct clients_node {
	struct list_head            list;
	struct gdb_transport_client *gdb_client;
};

struct gdb_transport_node {
	struct list_head     list;
	struct gdb_transport *gdb_transport;
};

static struct gdb_transport_module {
	struct list_head clients_list;
	struct list_head gdb_transport_list;
} gdb_transport_module = {
	.clients_list = LIST_HEAD_INIT(gdb_transport_module.clients_list),
	.gdb_transport_list = LIST_HEAD_INIT(gdb_transport_module.gdb_transport_list)
};

/** Handle incoming packets and pass to handler */
static void gdb_input_irq_handler(int irq, void *data)
{
	struct gdb_transport *gdb_transport = (struct gdb_transport *)data;
	struct scsc_mif_abs  *mif_abs;
	u32                  num_bytes;
	u32                  alloc_bytes;
	char                 *buf;

	SCSC_TAG_DEBUG(GDB_TRANS, "Handling write signal.\n");

	/* 1st length */
	/* Clear the interrupt first to ensure we can't possibly miss one */
	mif_abs = scsc_mx_get_mif_abs(gdb_transport->mx);
	mif_abs->irq_bit_clear(mif_abs, irq);

	while (mif_stream_read(&gdb_transport->mif_istream, &num_bytes, sizeof(uint32_t))) {
		SCSC_TAG_DEBUG(GDB_TRANS, "Transferring %d byte payload to handler.\n", num_bytes);
		if (num_bytes > 0 && num_bytes
		    < (GDB_TRANSPORT_BUF_LENGTH - sizeof(uint32_t))) {
			alloc_bytes = sizeof(char) * num_bytes;
			/* This is called in atomic context so must use kmalloc with GFP_ATOMIC flag */
			buf = kmalloc(alloc_bytes, GFP_ATOMIC);
			/* 2nd payload (msg) */
			mif_stream_read(&gdb_transport->mif_istream, buf, num_bytes);
			gdb_transport->channel_handler_fn(buf, num_bytes, gdb_transport->channel_handler_data);
			kfree(buf);
		} else {
			SCSC_TAG_ERR(GDB_TRANS, "Incorrect num_bytes: 0x%08x\n", num_bytes);
			mif_stream_log(&gdb_transport->mif_istream, SCSC_ERR);
		}
	}
}


/** MIF Interrupt handler for acknowledging reads made by the AP */
static void gdb_output_irq_handler(int irq, void *data)
{
	struct scsc_mif_abs  *mif_abs;
	struct gdb_transport *gdb_transport = (struct gdb_transport *)data;

	SCSC_TAG_DEBUG(GDB_TRANS, "Ignoring read signal.\n");

	/* Clear the interrupt first to ensure we can't possibly miss one */
	/* The FW read some data from the output stream.
	 * Currently we do not care, so just clear the interrupt. */
	mif_abs = scsc_mx_get_mif_abs(gdb_transport->mx);
	mif_abs->irq_bit_clear(mif_abs, irq);
}

static void gdb_transport_probe_registered_clients(struct gdb_transport *gdb_transport)
{
	bool                client_registered = false;
	struct clients_node *gdb_client_node, *gdb_client_next;
	struct scsc_mif_abs *mif_abs;
	char                *dev_uid;

	/* Traverse Linked List for each mif_driver node */
	list_for_each_entry_safe(gdb_client_node, gdb_client_next, &gdb_transport_module.clients_list, list) {
		/* Get UID */
		mif_abs = scsc_mx_get_mif_abs(gdb_transport->mx);
		dev_uid = mif_abs->get_uid(mif_abs);
		gdb_client_node->gdb_client->probe(gdb_client_node->gdb_client, gdb_transport, dev_uid);
		client_registered = true;
	}
	if (client_registered == false)
		SCSC_TAG_INFO(GDB_TRANS, "No clients registered\n");
}

void gdb_transport_release(struct gdb_transport *gdb_transport)
{
	struct clients_node       *gdb_client_node, *gdb_client_next;
	struct gdb_transport_node *gdb_transport_node, *gdb_transport_node_next;
	bool                      match = false;

	list_for_each_entry_safe(gdb_transport_node, gdb_transport_node_next, &gdb_transport_module.gdb_transport_list, list) {
		if (gdb_transport_node->gdb_transport == gdb_transport) {
			match = true;
			SCSC_TAG_INFO(GDB_TRANS, "release client\n");
			/* Wait for client to close */
			mutex_lock(&gdb_transport->channel_open_mutex);
			/* Need to notify clients using the transport has been released */
			list_for_each_entry_safe(gdb_client_node, gdb_client_next, &gdb_transport_module.clients_list, list) {
				gdb_client_node->gdb_client->remove(gdb_client_node->gdb_client, gdb_transport);
			}
			mutex_unlock(&gdb_transport->channel_open_mutex);
			list_del(&gdb_transport_node->list);
			kfree(gdb_transport_node);
		}
	}
	if (match == false)
		SCSC_TAG_INFO(GDB_TRANS, "No match for given scsc_mif_abs\n");

	mif_stream_release(&gdb_transport->mif_istream);
	mif_stream_release(&gdb_transport->mif_ostream);
}

void gdb_transport_config_serialise(struct gdb_transport *gdb_transport,
				    struct mxtransconf   *trans_conf)
{
	mif_stream_config_serialise(&gdb_transport->mif_istream, &trans_conf->to_ap_stream_conf);
	mif_stream_config_serialise(&gdb_transport->mif_ostream, &trans_conf->from_ap_stream_conf);
}


/** Public functions */
int gdb_transport_init(struct gdb_transport *gdb_transport, struct scsc_mx *mx, enum gdb_transport_enum type)
{
	int                       r;
	uint32_t                  mem_length = GDB_TRANSPORT_BUF_LENGTH;
	uint32_t                  packet_size = 4;
	uint32_t                  num_packets;
	struct gdb_transport_node *gdb_transport_node;

	gdb_transport_node = kzalloc(sizeof(*gdb_transport_node), GFP_KERNEL);
	if (!gdb_transport_node)
		return -EIO;

	memset(gdb_transport, 0, sizeof(struct gdb_transport));
	num_packets = mem_length / packet_size;
	mutex_init(&gdb_transport->channel_handler_mutex);
	mutex_init(&gdb_transport->channel_open_mutex);
	gdb_transport->mx = mx;

	if (type == GDB_TRANSPORT_M4)
		r = mif_stream_init(&gdb_transport->mif_istream, SCSC_MIF_ABS_TARGET_M4, MIF_STREAM_DIRECTION_IN, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_ALLOC, gdb_input_irq_handler, gdb_transport);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	else if (type == GDB_TRANSPORT_M4_1)
		r = mif_stream_init(&gdb_transport->mif_istream, SCSC_MIF_ABS_TARGET_M4_1, MIF_STREAM_DIRECTION_IN, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_ALLOC, gdb_input_irq_handler, gdb_transport);
#endif
	else
		r = mif_stream_init(&gdb_transport->mif_istream, SCSC_MIF_ABS_TARGET_R4, MIF_STREAM_DIRECTION_IN, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_ALLOC, gdb_input_irq_handler, gdb_transport);
	if (r) {
		kfree(gdb_transport_node);
		return r;
	}

	if (type == GDB_TRANSPORT_M4)
		r = mif_stream_init(&gdb_transport->mif_ostream, SCSC_MIF_ABS_TARGET_M4, MIF_STREAM_DIRECTION_OUT, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_RESERVED, gdb_output_irq_handler, gdb_transport);
#ifdef CONFIG_SCSC_MX450_GDB_SUPPORT
	else if (type == GDB_TRANSPORT_M4_1)
		r = mif_stream_init(&gdb_transport->mif_ostream, SCSC_MIF_ABS_TARGET_M4_1, MIF_STREAM_DIRECTION_OUT, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_RESERVED, gdb_output_irq_handler, gdb_transport);
#endif
	else
		r = mif_stream_init(&gdb_transport->mif_ostream, SCSC_MIF_ABS_TARGET_R4, MIF_STREAM_DIRECTION_OUT, num_packets, packet_size, mx, MIF_STREAM_INTRBIT_TYPE_RESERVED, gdb_output_irq_handler, gdb_transport);
	if (r) {
		mif_stream_release(&gdb_transport->mif_istream);
		kfree(gdb_transport_node);
		return r;
	}

	gdb_transport->channel_handler_fn = NULL;
	gdb_transport->channel_handler_data = NULL;

	gdb_transport_node->gdb_transport = gdb_transport;
	/* Add gdb_transport node */
	list_add_tail(&gdb_transport_node->list, &gdb_transport_module.gdb_transport_list);
	gdb_transport->type = type;
	gdb_transport_probe_registered_clients(gdb_transport);
	return 0;
}

void gdb_transport_send(struct gdb_transport *gdb_transport, void *message, uint32_t message_length)
{
	char msg[300];

	if (message_length > sizeof(msg))
		return;

	memcpy(msg, message, message_length);

	mutex_lock(&gdb_transport->channel_handler_mutex);
	/* 1st length */
	mif_stream_write(&gdb_transport->mif_ostream, &message_length, sizeof(uint32_t));
	/* 2nd payload (msg) */
	mif_stream_write(&gdb_transport->mif_ostream, message, message_length);
	mutex_unlock(&gdb_transport->channel_handler_mutex);
}
EXPORT_SYMBOL(gdb_transport_send);

void gdb_transport_register_channel_handler(struct gdb_transport *gdb_transport,
					    gdb_channel_handler handler, void *data)
{
	mutex_lock(&gdb_transport->channel_handler_mutex);
	gdb_transport->channel_handler_fn = handler;
	gdb_transport->channel_handler_data = (void *)data;
	mutex_unlock(&gdb_transport->channel_handler_mutex);
}
EXPORT_SYMBOL(gdb_transport_register_channel_handler);

int gdb_transport_register_client(struct gdb_transport_client *gdb_client)
{
	struct clients_node       *gdb_client_node;
	struct gdb_transport_node *gdb_transport_node;
	struct scsc_mif_abs       *mif_abs;
	char                      *dev_uid;

	/* Add node in modules linked list */
	gdb_client_node = kzalloc(sizeof(*gdb_client_node), GFP_KERNEL);
	if (!gdb_client_node)
		return -ENOMEM;

	gdb_client_node->gdb_client = gdb_client;
	list_add_tail(&gdb_client_node->list, &gdb_transport_module.clients_list);


	/* Traverse Linked List for transport registered */
	list_for_each_entry(gdb_transport_node, &gdb_transport_module.gdb_transport_list, list) {
		/* Get UID */
		mif_abs = scsc_mx_get_mif_abs(gdb_transport_node->gdb_transport->mx);
		dev_uid = mif_abs->get_uid(mif_abs);
		gdb_client->probe(gdb_client, gdb_transport_node->gdb_transport, dev_uid);
	}
	return 0;
}
EXPORT_SYMBOL(gdb_transport_register_client);

void gdb_transport_unregister_client(struct gdb_transport_client *gdb_client)
{
	struct clients_node *gdb_client_node, *gdb_client_next;

	/* Traverse Linked List for each client_list  */
	list_for_each_entry_safe(gdb_client_node, gdb_client_next, &gdb_transport_module.clients_list, list) {
		if (gdb_client_node->gdb_client == gdb_client) {
			list_del(&gdb_client_node->list);
			kfree(gdb_client_node);
		}
	}
}
EXPORT_SYMBOL(gdb_transport_unregister_client);
