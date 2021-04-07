/****************************************************************************
 *
 * Copyright (c) 2014 - 2017 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/slab.h>
#include <linux/module.h>
#include <scsc/scsc_mx.h>
#include <scsc/scsc_release.h>
#include <scsc/scsc_logring.h>
#include "scsc_mif_abs.h"
#include "scsc_mx_impl.h"
#ifdef CONFIG_SCSC_WLBTD
#include "scsc_wlbtd.h"
#endif

#define SCSC_MX_CORE_MODDESC "mx140 Core Driver"

struct clients_node {
	struct list_head             list;
	struct scsc_mx_module_client *module_client;
};

struct mx_node {
	struct list_head list;
	struct scsc_mx   *mx;
};

static struct mx_module {
	struct list_head clients_list;
	struct list_head mx_list;
} mx_module = {
	.clients_list = LIST_HEAD_INIT(mx_module.clients_list),
	.mx_list = LIST_HEAD_INIT(mx_module.mx_list)
};

static void scsc_mx_module_probe_registered_clients(struct scsc_mx *new_mx)
{
	bool                client_registered = false;
	struct clients_node *client_node, *client_next;

	/* Traverse Linked List for each mif_driver node */
	list_for_each_entry_safe(client_node, client_next, &mx_module.clients_list, list) {
		client_node->module_client->probe(client_node->module_client, new_mx, SCSC_MODULE_CLIENT_REASON_HW_PROBE);
		client_registered = true;
	}
	if (client_registered == false)
		SCSC_TAG_INFO(MXMAN, "No clients registered\n");
}

static void scsc_mx_module_probe(struct scsc_mif_abs_driver *abs_driver, struct scsc_mif_abs *mif_abs)
{
	struct scsc_mx *new_mx;
	struct mx_node *mx_node;

	/* Avoid unused parm error */
	(void)abs_driver;

	mx_node = kzalloc(sizeof(*mx_node), GFP_KERNEL);
	if (!mx_node)
		return;
	/* Create new mx instance */
	new_mx = scsc_mx_create(mif_abs);
	if (!new_mx) {
		kfree(mx_node);
		SCSC_TAG_ERR(MXMAN, "Error allocating new_mx\n");
		return;
	}
	/* Add instance in mx_node linked list */
	mx_node->mx = new_mx;

	list_add_tail(&mx_node->list, &mx_module.mx_list);

	scsc_mx_module_probe_registered_clients(new_mx);
}

static void scsc_mx_module_remove(struct scsc_mif_abs *abs)
{
	bool           match = false;
	struct mx_node *mx_node, *next;

	/* Traverse Linked List for each mx node */
	list_for_each_entry_safe(mx_node, next, &mx_module.mx_list, list) {
		/* If there is a match, call destroy  */
		if (scsc_mx_get_mif_abs(mx_node->mx) == abs) {
			match = true;
			scsc_mx_destroy(mx_node->mx);
			list_del(&mx_node->list);
			kfree(mx_node);
		}
	}
	if (match == false)
		SCSC_TAG_ERR(MXMAN, "FATAL, no match for given scsc_mif_abs\n");
}

static struct scsc_mif_abs_driver mx_module_mif_if = {
	.name = "mx140 driver",
	.probe = scsc_mx_module_probe,
	.remove = scsc_mx_module_remove,
};

static int __init scsc_mx_module_init(void)
{
	SCSC_TAG_INFO(MXMAN, SCSC_MX_CORE_MODDESC " scsc_release %d.%d.%d.%d.%d\n",
		SCSC_RELEASE_PRODUCT,
		SCSC_RELEASE_ITERATION,
		SCSC_RELEASE_CANDIDATE,
		SCSC_RELEASE_POINT,
		SCSC_RELEASE_CUSTOMER);

	scsc_mif_abs_register(&mx_module_mif_if);
	return 0;
}

static void __exit scsc_mx_module_exit(void)
{
	struct mx_node *mx_node, *next_mx;

	/* Traverse Linked List for each mx node */
	list_for_each_entry_safe(mx_node, next_mx, &mx_module.mx_list, list) {
		scsc_mx_destroy(mx_node->mx);
		list_del(&mx_node->list);
		kfree(mx_node);
	}

	scsc_mif_abs_unregister(&mx_module_mif_if);

	SCSC_TAG_INFO(MXMAN, SCSC_MX_CORE_MODDESC " unloaded\n");
}

/**
 * Reset all registered service drivers by first calling the remove callback and
 * then the probe callback. This function is used during recovery operations,
 * where the chip has been reset as part of the recovery and the service drivers
 * has to do the same.
 */
int scsc_mx_module_reset(void)
{
	struct clients_node *clients_node;
	struct mx_node      *mx_node, *next_mx;

	/* Traverse Linked List and call registered removed callbacks */
	list_for_each_entry_safe(mx_node, next_mx, &mx_module.mx_list, list)
		list_for_each_entry(clients_node, &mx_module.clients_list, list)
			clients_node->module_client->remove(clients_node->module_client, mx_node->mx, SCSC_MODULE_CLIENT_REASON_RECOVERY);

	/* Traverse Linked List and call registered probed callbacks */
	list_for_each_entry_safe(mx_node, next_mx, &mx_module.mx_list, list)
		list_for_each_entry(clients_node, &mx_module.clients_list, list)
			clients_node->module_client->probe(clients_node->module_client, mx_node->mx, SCSC_MODULE_CLIENT_REASON_RECOVERY);

	return 0;
}
EXPORT_SYMBOL(scsc_mx_module_reset);

int scsc_mx_module_register_client_module(struct scsc_mx_module_client *module_client)
{
	struct clients_node *module_client_node;
	struct mx_node      *mx_node;

	/* Add node in modules linked list */
	module_client_node = kzalloc(sizeof(*module_client_node), GFP_KERNEL);
	if (!module_client_node)
		return -ENOMEM;

	module_client_node->module_client = module_client;
	list_add_tail(&module_client_node->list, &mx_module.clients_list);

	/* Traverse Linked List for each mx node */
	list_for_each_entry(mx_node, &mx_module.mx_list, list) {
		module_client->probe(module_client, mx_node->mx, SCSC_MODULE_CLIENT_REASON_HW_PROBE);
	}
	return 0;
}
EXPORT_SYMBOL(scsc_mx_module_register_client_module);

void scsc_mx_module_unregister_client_module(struct scsc_mx_module_client *module_client)
{
	struct clients_node *client_node, *client_next;
	struct mx_node      *mx_node, *next_mx;

	/* Traverse Linked List for each client_list  */
	list_for_each_entry_safe(client_node, client_next, &mx_module.clients_list, list) {
		if (client_node->module_client == module_client) {
			list_for_each_entry_safe(mx_node, next_mx, &mx_module.mx_list, list) {
				module_client->remove(module_client, mx_node->mx, SCSC_MODULE_CLIENT_REASON_HW_REMOVE);
			}
			list_del(&client_node->list);
			kfree(client_node);
		}
	}
}
EXPORT_SYMBOL(scsc_mx_module_unregister_client_module);

module_init(scsc_mx_module_init);
module_exit(scsc_mx_module_exit);

MODULE_DESCRIPTION(SCSC_MX_CORE_MODDESC);
MODULE_AUTHOR("SCSC");
MODULE_LICENSE("GPL");
