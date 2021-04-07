/****************************************************************************
 *
 * Copyright (c) 2014 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#include <linux/module.h>
#include "slsi_kic_internal.h"

static DEFINE_MUTEX(kic_lock);
static struct slsi_kic_pdata *pdata;

#define SLSI_MAX_NUM_KIC_OPS 7
#define SLSI_MAX_NUM_MULTICAST_GROUP 1

static struct genl_ops slsi_kic_ops[SLSI_MAX_NUM_KIC_OPS];

static int slsi_kic_pre_doit(const struct genl_ops *ops, struct sk_buff *skb,
			     struct genl_info *info)
{
	SCSC_TAG_ERR(KIC_COMMON, "%s Handle CMD %d, seq %d\n",
		     __func__, ops->cmd, info->snd_seq);

	OS_UNUSED_PARAMETER(skb);

	/* Called BEFORE the command cb - do filtering here */

	/* Consider doing some check for "test_mode" primitives here:
	 * It could be a way to prevent test primitives (which can be
	 * powerful) to run unless test_mode has been configured. */

	return 0;
}

static void slsi_kic_post_doit(const struct genl_ops *ops, struct sk_buff *skb,
			       struct genl_info *info)
{
	OS_UNUSED_PARAMETER(ops);
	OS_UNUSED_PARAMETER(skb);
	OS_UNUSED_PARAMETER(info);

	/* Called AFTER the command cb - could do something here */
}

static const struct genl_multicast_group slsi_kic_general_system_mcgrp[SLSI_MAX_NUM_MULTICAST_GROUP] = {
	{ .name = "general_system", },
};


/* The netlink family */
static struct genl_family slsi_kic_fam = {
	.name = "slsi_kic",     /* Have users key off the name instead */
	.hdrsize = 0,           /* No private header */
	.version = 2,
	.netnsok = true,
	.maxattr = SLSI_KIC_ATTR_MAX,
	.pre_doit = slsi_kic_pre_doit,
	.post_doit = slsi_kic_post_doit,
	.ops = slsi_kic_ops,
	.n_ops = SLSI_MAX_NUM_KIC_OPS,
	.mcgrps = slsi_kic_general_system_mcgrp,
	.n_mcgrps = SLSI_MAX_NUM_MULTICAST_GROUP,
};

/**
 * Message building helpers
 */
static inline void *kic_hdr_put(struct sk_buff *skb, uint32_t portid, uint32_t seq,
				int flags, u8 cmd)
{
	/* Since there is no private header just add the generic one */
	return genlmsg_put(skb, portid, seq, &slsi_kic_fam, flags, cmd);
}

static int kic_build_u32_msg(struct sk_buff *msg, uint32_t portid, uint32_t seq, int flags,
			     enum slsi_kic_commands cmd, int attrtype, uint32_t payload)
{
	void *hdr;

	hdr = kic_hdr_put(msg, portid, seq, flags, cmd);
	if (!hdr)
		return -EFAULT;

	if (nla_put_u32(msg, attrtype, payload))
		goto nla_put_failure;

	genlmsg_end(msg, hdr);

	return 0;

nla_put_failure:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}

static int kic_add_timestamp_attrs(struct sk_buff *msg)
{
	struct timespec ts;

	/**
	 * Use getrawmonotonic instead of getnstimeofday to avoid problems with
	 * NTP updating things, which can make things look weird.
	 */
	getrawmonotonic(&ts);

	if (nla_put_u64_64bit(msg, SLSI_KIC_ATTR_TIMESTAMP_TV_SEC, ts.tv_sec, IFLA_BR_PAD))
		goto nla_put_failure;

	if (nla_put_u64_64bit(msg, SLSI_KIC_ATTR_TIMESTAMP_TV_NSEC, ts.tv_nsec, IFLA_BR_PAD))
		goto nla_put_failure;

	return 0;

nla_put_failure:
	return -EMSGSIZE;
}

static int kic_build_system_event_msg(struct sk_buff *msg, uint32_t portid,
				      uint32_t seq, int flags,
				      uint32_t event_cat, uint32_t event)
{
	void          *hdr;
	struct nlattr *nla;

	hdr = kic_hdr_put(msg, portid, seq, flags, SLSI_KIC_CMD_SYSTEM_EVENT_IND);
	if (!hdr)
		return -EFAULT;

	nla = nla_nest_start(msg, SLSI_KIC_ATTR_TIMESTAMP);
	if (kic_add_timestamp_attrs(msg) < 0)
		nla_nest_cancel(msg, nla);
	else
		nla_nest_end(msg, nla);

	if (nla_put_u32(msg, SLSI_KIC_ATTR_SYSTEM_EVENT_CATEGORY, event_cat))
		goto nla_put_failure;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_SYSTEM_EVENT, event))
		goto nla_put_failure;

	genlmsg_end(msg, hdr);
	
	return 0;

nla_put_failure:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}


static int kic_build_firmware_event_msg(struct sk_buff *msg, uint32_t portid,
					uint32_t seq, int flags,
					uint16_t firmware_event_type,
					enum slsi_kic_technology_type tech_type,
					uint32_t contain_type,
					struct slsi_kic_firmware_event_ccp_host *event)
{
	void          *hdr;
	struct nlattr *nla;

	hdr = kic_hdr_put(msg, portid, seq, flags, SLSI_KIC_CMD_FIRMWARE_EVENT_IND);
	if (!hdr) {
		nlmsg_free(msg);
		return -EFAULT;
	}

	if (nla_put_u16(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_TYPE, firmware_event_type))
		goto nla_put_failure;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_TECHNOLOGY_TYPE, tech_type))
		goto nla_put_failure;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_FIRMWARE_CONTAINER_TYPE, contain_type))
		goto nla_put_failure;

	nla = nla_nest_start(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CONTAINER_CCP_HOST);
	if (nla_put_u32(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_ID, event->id))
		goto nla_put_failure_cancel;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_LEVEL, event->level))
		goto nla_put_failure_cancel;

	if (nla_put_string(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_LEVEL_STRING, event->level_string))
		goto nla_put_failure_cancel;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_TIMESTAMP, event->timestamp))
		goto nla_put_failure_cancel;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_CPU, event->cpu))
		goto nla_put_failure_cancel;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_OCCURENCES, event->occurences))
		goto nla_put_failure_cancel;

	if (nla_put(msg, SLSI_KIC_ATTR_FIRMWARE_EVENT_CCP_HOST_ARG, event->arg_length, event->arg))
		goto nla_put_failure_cancel;
	nla_nest_end(msg, nla);

	genlmsg_end(msg, hdr);
	
	return 0;

nla_put_failure_cancel:
	nla_nest_cancel(msg, nla);

nla_put_failure:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}


static int kic_build_service_info_msg_add_service(struct sk_buff                *msg,
						  enum slsi_kic_technology_type tech,
						  struct slsi_kic_service_info  *info)
{
	struct nlattr *nla = NULL;

	if (!msg || !info)
		goto nla_put_failure;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_TECHNOLOGY_TYPE, tech))
		goto nla_put_failure;

	nla = nla_nest_start(msg, SLSI_KIC_ATTR_SERVICE_INFO);
	if (nla_put_string(msg, SLSI_KIC_ATTR_SERVICE_INFO_VER_STR, info->ver_str))
		goto nla_put_failure;

	if (nla_put_u16(msg, SLSI_KIC_ATTR_SERVICE_INFO_FW_API_MAJOR, info->fw_api_major))
		goto nla_put_failure;

	if (nla_put_u16(msg, SLSI_KIC_ATTR_SERVICE_INFO_FW_API_MINOR, info->fw_api_minor))
		goto nla_put_failure;

	if (nla_put_u16(msg, SLSI_KIC_ATTR_SERVICE_INFO_RELEASE_PRODUCT, info->release_product))
		goto nla_put_failure;

	if (nla_put_u16(msg, SLSI_KIC_ATTR_SERVICE_INFO_HOST_RELEASE_ITERATION, info->host_release_iteration))
		goto nla_put_failure;

	if (nla_put_u16(msg, SLSI_KIC_ATTR_SERVICE_INFO_HOST_RELEASE_CANDIDATE, info->host_release_candidate))
		goto nla_put_failure;

	nla_nest_end(msg, nla);

	return 0;

nla_put_failure:
	if (nla)
		nla_nest_cancel(msg, nla);

	return -EMSGSIZE;
}

static int kic_build_service_info_msg(struct sk_buff *msg, uint32_t portid,
				      uint32_t seq, int flags,
				      enum slsi_kic_technology_type tech,
				      struct slsi_kic_service_info *info)
{
	void *hdr;

	hdr = kic_hdr_put(msg, portid, seq, flags, SLSI_KIC_CMD_SERVICE_INFORMATION_IND);
	if (!hdr)
		return -EFAULT;

	if (kic_build_service_info_msg_add_service(msg, tech, info) < 0)
		goto nla_put_failure;

	genlmsg_end(msg, hdr);

	return 0;

nla_put_failure:
	genlmsg_cancel(msg, hdr);
	return -EMSGSIZE;
}


static int get_snd_pid(struct genl_info *info)
{
	uint32_t snd_pid = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
	snd_pid = info->snd_pid;
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 6, 0)
	snd_pid = info->snd_portid;
#endif

	return snd_pid;
}

struct slsi_kic_pdata *slsi_kic_core_get_context(void)
{
	return pdata;
}

/**
 * Set the record to NULL to free and delete all stored records.
 */
static int service_info_delete_record(struct slsi_kic_service_details *record)
{
	struct slsi_kic_pdata *pdata = slsi_kic_core_get_context();

	if (!pdata)
		return -EINVAL;

	if (down_interruptible(&pdata->chip_details.proxy_service_list_mutex))
		SCSC_TAG_ERR(KIC_COMMON, "Failed to lock service info mutex - continue anyway\n");

	if (record == NULL) {
		struct slsi_kic_service_details *service, *tmp_node;

		list_for_each_entry_safe(service, tmp_node, &pdata->chip_details.proxy_service_list, proxy_q) {
			list_del(&service->proxy_q);
			kfree(service);
		}
	} else {
		list_del(&record->proxy_q);
		kfree(record);
	}
	up(&pdata->chip_details.proxy_service_list_mutex);

	return 0;
}

static struct slsi_kic_service_details *
service_info_find_entry(enum slsi_kic_technology_type tech)
{
	struct slsi_kic_pdata           *pdata = slsi_kic_core_get_context();
	struct slsi_kic_service_details *service, *tmp_node;

	if (!pdata)
		return NULL;

	list_for_each_entry_safe(service, tmp_node, &pdata->chip_details.proxy_service_list, proxy_q) {
		if (service->tech == tech)
			return service;
	}

	return NULL;
}

static int service_info_update_record(enum slsi_kic_technology_type tech,
				      struct slsi_kic_service_info  *info)
{
	struct slsi_kic_pdata                  *pdata = slsi_kic_core_get_context();
	static struct slsi_kic_service_details *record;

	if (!pdata)
		return -EINVAL;

	if (down_interruptible(&pdata->chip_details.proxy_service_list_mutex))
		goto err_out;

	record = service_info_find_entry(tech);
	if (record == NULL) {
		up(&pdata->chip_details.proxy_service_list_mutex);
		goto err_out;
	}

	record->tech = tech;
	memcpy(&record->info, info, sizeof(struct slsi_kic_service_info));
	up(&pdata->chip_details.proxy_service_list_mutex);

	return 0;

err_out:
	SCSC_TAG_ERR(KIC_COMMON, "Failed to update service info record\n");
	return -EFAULT;
}

static int service_info_add(enum slsi_kic_technology_type tech,
			    struct slsi_kic_service_info  *info)
{
	struct slsi_kic_service_details *new_entry;
	struct slsi_kic_pdata           *pdata = slsi_kic_core_get_context();

	if (!pdata)
		return -EINVAL;

	new_entry = kmalloc(sizeof(struct slsi_kic_service_details), GFP_KERNEL);
	if (!new_entry)
		return -ENOMEM;

	new_entry->tech = tech;
	memcpy(&new_entry->info, info, sizeof(struct slsi_kic_service_info));

	if (down_interruptible(&pdata->chip_details.proxy_service_list_mutex))
		goto err_out;

	list_add_tail(&new_entry->proxy_q, &pdata->chip_details.proxy_service_list);
	up(&pdata->chip_details.proxy_service_list_mutex);

	return 0;

err_out:
	SCSC_TAG_ERR(KIC_COMMON, "Failed to add service info record to list\n");
	kfree(new_entry);
	return -EFAULT;
}


/**
 * Command callbacks
 */

/* This function shall not do anything since the direction is
 * kernel->user space for this primitive. We should look into if it's
 * possible to handle this better than having an empty stub function. */
static int slsi_kic_wrong_direction(struct sk_buff *skb, struct genl_info *info)
{
	OS_UNUSED_PARAMETER(skb);

	SCSC_TAG_ERR(KIC_COMMON, "%s Received CMD from pid %u seq %u: Wrong direction only supports kernel->user space\n",
		     __func__, info->snd_seq, get_snd_pid(info));
	return -EINVAL;
}

static int slsi_kic_interface_version_number_req(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	void           *hdr;

	OS_UNUSED_PARAMETER(skb);

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = kic_hdr_put(msg, 0, info->snd_seq, 0, SLSI_KIC_CMD_KIC_INTERFACE_VERSION_NUMBER_REQ);
	if (!hdr)
		goto nl_hdr_failure;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_KIC_VERSION_MAJOR, SLSI_KIC_INTERFACE_VERSION_MAJOR))
		goto nla_put_failure;

	if (nla_put_u32(msg, SLSI_KIC_ATTR_KIC_VERSION_MINOR, SLSI_KIC_INTERFACE_VERSION_MINOR))
		goto nla_put_failure;

	genlmsg_end(msg, hdr);
	return genlmsg_reply(msg, info);

nla_put_failure:
	genlmsg_cancel(msg, hdr);

nl_hdr_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}

static int slsi_kic_echo_req(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	uint32_t       payload = 0;

	OS_UNUSED_PARAMETER(skb);

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	if (info->attrs[SLSI_KIC_ATTR_ECHO])
		payload = nla_get_u32(info->attrs[SLSI_KIC_ATTR_ECHO]);

	if (kic_build_u32_msg(msg, get_snd_pid(info), info->snd_seq, 0,
			      SLSI_KIC_CMD_ECHO_REQ, SLSI_KIC_ATTR_ECHO, payload) < 0) {
		nlmsg_free(msg);
		return -ENOBUFS;
	}

	return genlmsg_reply(msg, info);
}

static int slsi_kic_service_information_req(struct sk_buff *skb, struct genl_info *info)
{
	struct slsi_kic_pdata           *pdata = slsi_kic_core_get_context();
	int                             counter = 0, i;
	struct sk_buff                  *msg;
	struct slsi_kic_service_details *sr;
	void                            *hdr;

	OS_UNUSED_PARAMETER(skb);

	if (!pdata)
		return -EINVAL;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = kic_hdr_put(msg, 0, info->snd_seq, 0, SLSI_KIC_CMD_SERVICE_INFORMATION_REQ);
	if (!hdr)
		goto nla_put_failure;

	if (down_interruptible(&pdata->chip_details.proxy_service_list_mutex))
		goto nla_put_failure;

	/* The request doesn't carry attributes, so no validation required.
	 * Query the list for information for each technology and encode. */
	for (i = 0; i < slsi_kic_technology_type__after_last; i++) {
		sr = service_info_find_entry(i);
		if (sr) {
			counter++;
			if (kic_build_service_info_msg_add_service(msg, i, &sr->info) < 0) {
				up(&pdata->chip_details.proxy_service_list_mutex);
				goto nla_put_failure;
			}
		}
	}
	up(&pdata->chip_details.proxy_service_list_mutex);

	if (nla_put_u32(msg, SLSI_KIC_ATTR_NUMBER_OF_ENCODED_SERVICES, counter))
		goto nla_put_failure;

	genlmsg_end(msg, hdr);
	return genlmsg_reply(msg, info);

nla_put_failure:
	nlmsg_free(msg);
	return -EMSGSIZE;
}

static int slsi_kic_test_trigger_recovery_req(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff                     *msg;
	uint32_t                           technology = 0, recovery_type = 0;
	struct slsi_kic_pdata              *pdata = slsi_kic_core_get_context();
	enum slsi_kic_test_recovery_status status = slsi_kic_test_recovery_status_ok;

	OS_UNUSED_PARAMETER(skb);

	if (info->attrs[SLSI_KIC_ATTR_TECHNOLOGY_TYPE])
		technology = nla_get_u32(info->attrs[SLSI_KIC_ATTR_TECHNOLOGY_TYPE]);

	if (info->attrs[SLSI_KIC_ATTR_TEST_RECOVERY_TYPE])
		recovery_type = nla_get_u32(info->attrs[SLSI_KIC_ATTR_TEST_RECOVERY_TYPE]);

	if (pdata) {
		int err = -EFAULT;

		if (technology == slsi_kic_technology_type_wifi) {
			struct slsi_kic_wifi_ops_tuple *wifi_ops = NULL;

			wifi_ops = &pdata->wifi_ops_tuple;

			mutex_lock(&wifi_ops->ops_mutex);
			if (wifi_ops->wifi_ops.trigger_recovery)
				err = wifi_ops->wifi_ops.trigger_recovery(wifi_ops->priv,
									  (enum slsi_kic_test_recovery_type)recovery_type);
			mutex_unlock(&wifi_ops->ops_mutex);
		} else if (technology == slsi_kic_technology_type_curator) {
			struct slsi_kic_cm_ops_tuple *cm_ops = NULL;

			cm_ops = &pdata->cm_ops_tuple;

			mutex_lock(&cm_ops->ops_mutex);
			if (cm_ops->cm_ops.trigger_recovery)
				err = cm_ops->cm_ops.trigger_recovery(cm_ops->priv,
								      (enum slsi_kic_test_recovery_type)recovery_type);
			mutex_unlock(&cm_ops->ops_mutex);
		} else if (technology == slsi_kic_technology_type_bt) {
			struct slsi_kic_bt_ops_tuple *bt_ops = NULL;

			bt_ops = &pdata->bt_ops_tuple;

			mutex_lock(&bt_ops->ops_mutex);
			if (bt_ops->bt_ops.trigger_recovery)
				err = bt_ops->bt_ops.trigger_recovery(bt_ops->priv,
								      (enum slsi_kic_test_recovery_type)recovery_type);
			mutex_unlock(&bt_ops->ops_mutex);
		} else if (technology == slsi_kic_technology_type_ant) {
			struct slsi_kic_ant_ops_tuple *ant_ops = NULL;

			ant_ops = &pdata->ant_ops_tuple;

			mutex_lock(&ant_ops->ops_mutex);
			if (ant_ops->ant_ops.trigger_recovery)
				err = ant_ops->ant_ops.trigger_recovery(ant_ops->priv,
								      (enum slsi_kic_test_recovery_type)recovery_type);
			mutex_unlock(&ant_ops->ops_mutex);
		}

		if (err < 0)
			status = slsi_kic_test_recovery_status_error_send_msg;
	} else
		status = slsi_kic_test_recovery_status_error_invald_param;

	/* Prepare reply */
	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	if (kic_build_u32_msg(msg, get_snd_pid(info), info->snd_seq, 0,
			      SLSI_KIC_CMD_TEST_TRIGGER_RECOVERY_REQ, SLSI_KIC_ATTR_TRIGGER_RECOVERY_STATUS, status) < 0)
		goto nl_hdr_failure;

	return genlmsg_reply(msg, info);

nl_hdr_failure:
	nlmsg_free(msg);
	return -ENOBUFS;
}


int slsi_kic_service_information_ind(enum slsi_kic_technology_type tech,
				     struct slsi_kic_service_info  *info)
{
	struct sk_buff *msg;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	if (service_info_find_entry(tech) == NULL) {
		if (service_info_add(tech, info) < 0)
			SCSC_TAG_ERR(KIC_COMMON, "%s Failed to add record\n", __func__);
	} else if (service_info_update_record(tech, info) < 0)
		SCSC_TAG_ERR(KIC_COMMON, "%s Failed to update record\n", __func__);

	if (kic_build_service_info_msg(msg, 0, 0, 0, tech, info) < 0)
		goto err;

	return genlmsg_multicast(&slsi_kic_fam, msg, 0, 0, GFP_KERNEL);

err:
	nlmsg_free(msg);
	return -ENOBUFS;
}
EXPORT_SYMBOL(slsi_kic_service_information_ind);


int slsi_kic_system_event_ind(enum slsi_kic_system_event_category event_cat,
			      enum slsi_kic_system_events event, gfp_t flags)
{
	struct sk_buff *msg;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, flags);
	if (!msg)
		return -ENOMEM;

	if (kic_build_system_event_msg(msg, 0, 0, 0, event_cat, event) < 0)
		goto err;

	return genlmsg_multicast(&slsi_kic_fam, msg, 0, 0, flags);

err:
	nlmsg_free(msg);
	return -ENOBUFS;
}
EXPORT_SYMBOL(slsi_kic_system_event_ind);


int slsi_kic_firmware_event_ind(uint16_t firmware_event_type, enum slsi_kic_technology_type tech_type,
				uint32_t contain_type, struct slsi_kic_firmware_event_ccp_host *event)
{
	struct sk_buff *msg;

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	if (kic_build_firmware_event_msg(msg, 0, 0, 0, firmware_event_type, tech_type, contain_type, event) < 0)
		return -ENOBUFS;

	return genlmsg_multicast(&slsi_kic_fam, msg, 0, 0, GFP_KERNEL);
}
EXPORT_SYMBOL(slsi_kic_firmware_event_ind);


static struct genl_ops slsi_kic_ops[SLSI_MAX_NUM_KIC_OPS] = {
	{
		.cmd = SLSI_KIC_CMD_KIC_INTERFACE_VERSION_NUMBER_REQ,
		.doit = slsi_kic_interface_version_number_req,
		.policy = slsi_kic_attr_policy,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SLSI_KIC_CMD_SYSTEM_EVENT_IND,
		.doit = slsi_kic_wrong_direction,
		.policy = slsi_kic_attr_policy,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SLSI_KIC_CMD_SERVICE_INFORMATION_REQ,
		.doit = slsi_kic_service_information_req,
		.policy = slsi_kic_attr_policy,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SLSI_KIC_CMD_SERVICE_INFORMATION_IND,
		.doit = slsi_kic_wrong_direction,
		.policy = slsi_kic_attr_policy,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SLSI_KIC_CMD_FIRMWARE_EVENT_IND,
		.doit = slsi_kic_wrong_direction,
		.policy = slsi_kic_attr_policy,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SLSI_KIC_CMD_ECHO_REQ,
		.doit = slsi_kic_echo_req,
		.policy = slsi_kic_attr_policy,
		.flags = GENL_ADMIN_PERM,
	},
	{
		.cmd = SLSI_KIC_CMD_TEST_TRIGGER_RECOVERY_REQ,
		.doit = slsi_kic_test_trigger_recovery_req,
		.policy = slsi_kic_attr_policy,
		.flags = GENL_ADMIN_PERM,
	},
};

static int __init slsi_kic_init(void)
{
	int err;

	SCSC_TAG_DEBUG(KIC_COMMON, "%s Enter\n", __func__);

	mutex_lock(&kic_lock);

	pdata = kzalloc(sizeof(struct slsi_kic_pdata), GFP_KERNEL);
	if (!pdata) {
		SCSC_TAG_ERR(KIC_COMMON, "%s Exit - no mem\n", __func__);
		mutex_unlock(&kic_lock);
		return -ENOMEM;
	}

	mutex_init(&pdata->wifi_ops_tuple.ops_mutex);
	mutex_init(&pdata->cm_ops_tuple.ops_mutex);
	mutex_init(&pdata->bt_ops_tuple.ops_mutex);
	mutex_init(&pdata->ant_ops_tuple.ops_mutex);

	/* Init chip information proxy list */
	INIT_LIST_HEAD(&pdata->chip_details.proxy_service_list);
	sema_init(&pdata->chip_details.proxy_service_list_mutex, 1);
	pdata->state = idle;

	err = genl_register_family(&slsi_kic_fam);
	if (err != 0)
		goto err_out;

	mutex_unlock(&kic_lock);
	SCSC_TAG_DEBUG(KIC_COMMON, "%s Exit\n", __func__);
	return 0;

err_out:
	genl_unregister_family(&slsi_kic_fam);
	mutex_unlock(&kic_lock);
	SCSC_TAG_ERR(KIC_COMMON, "%s Exit - err %d\n", __func__, err);
	return err;
}

static void __exit slsi_kic_exit(void)
{
	int err;

	SCSC_TAG_DEBUG(KIC_COMMON, "%s Enter\n", __func__);

	BUG_ON(!pdata);
	if (!pdata) {
		SCSC_TAG_ERR(KIC_COMMON, "%s Exit - invalid pdata\n", __func__);
		return;
	}

	mutex_lock(&kic_lock);
	err = genl_unregister_family(&slsi_kic_fam);
	if (err < 0)
		SCSC_TAG_ERR(KIC_COMMON, "%s Failed to unregister family\n", __func__);

	if (service_info_delete_record(NULL) < 0)
		SCSC_TAG_ERR(KIC_COMMON, "%s Deleting service info liste failed\n", __func__);

	mutex_destroy(&pdata->wifi_ops_tuple.ops_mutex);
	mutex_destroy(&pdata->cm_ops_tuple.ops_mutex);
	mutex_destroy(&pdata->bt_ops_tuple.ops_mutex);
	mutex_destroy(&pdata->ant_ops_tuple.ops_mutex);

	kfree(pdata);
	pdata = NULL;
	mutex_unlock(&kic_lock);

	SCSC_TAG_DEBUG(KIC_COMMON, "%s Exit\n", __func__);
}

module_init(slsi_kic_init);
module_exit(slsi_kic_exit);

MODULE_DESCRIPTION("SCSC Kernel Information and Control (KIC) interface");
MODULE_AUTHOR("Samsung Electronics Co., Ltd");
MODULE_LICENSE("GPL and additional rights");
