/*
 * drivers/soc/samsung/exynos-hdcp/exynos-hdcp2-tx-session.c
 *
 * Copyright (c) 2016 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/slab.h>

#include "exynos-hdcp2.h"
#include "exynos-hdcp2-log.h"

/**
 * generate data for a session data
 */

struct hdcp_session_data *hdcp_session_data_create(void)
{
	struct hdcp_session_data *new_ss_data = NULL;
	static int count = 0; /* TODO change session id creation method */

	new_ss_data = (struct hdcp_session_data *)kzalloc(sizeof(struct hdcp_session_data), GFP_KERNEL);
	if (!new_ss_data) {
		return NULL;
	}

	/* init session data */
	new_ss_data->id = count++;

	new_ss_data->wrap_skey_store = WRAP_SKEY_EMPTY;
	memset(new_ss_data->riv, 0x00, sizeof(new_ss_data->riv));
	hdcp_link_list_init(&(new_ss_data->ln));

	new_ss_data->state = SESS_ST_INIT;

	return new_ss_data;
}

/**
 * destroy a session data
 */
void hdcp_session_data_destroy(struct hdcp_session_data **ss_data)
{
        if (!(*ss_data) || !ss_data)
                return;

        /* clear session key and riv */
        memset((*ss_data)->wrap_skey, 0x00, sizeof((*ss_data)->wrap_skey));
        memset((*ss_data)->riv, 0x00, sizeof((*ss_data)->riv));
        /* clear link list */
        hdcp_link_list_destroy(&((*ss_data)->ln));

        if (*ss_data) {
                kfree(*ss_data);
                *ss_data = NULL;
        }
}

/**
 * generate data for a link data
 */
struct hdcp_link_data *hdcp_link_data_create(void)
{
	struct hdcp_link_data *new_lk_data = NULL;
	static int count = 0; /* TODO: change link id creation method */

	new_lk_data = (struct hdcp_link_data *)kzalloc(sizeof(struct hdcp_link_data), GFP_KERNEL);
	if (!new_lk_data) {
		return NULL;
	}

	/* init link data */
	new_lk_data->id = count++;
	new_lk_data->state = LINK_ST_INIT;

	/* set HDCP version */
#ifdef HDCP_TX_VERSION_2_2
	new_lk_data->tx_ctx.version = HDCP_VERSION_2_2;
#endif

	return new_lk_data;
}

/**
 * destroy a link data
 */
void hdcp_link_data_destroy(struct hdcp_link_data **lk_data)
{
	if (!(*lk_data) || !lk_data)
		return;

	(*lk_data)->ss_ptr = NULL;

	if (*lk_data) {
		kfree(*lk_data);
		*lk_data = NULL;
	}
}

/**
 * init a Session list
 * @ss_list: list head to add it after
 */
void hdcp_session_list_init(struct hdcp_session_list *ss_list)
{
        struct hdcp_session_node *ss_head;

        if (!ss_list)
                return;

	/* hdcp session list mutex init */
	mutex_init(&ss_list->ss_mutex);

        ss_head = &(ss_list->hdcp_session_head);
        ss_head->next = ss_head;
        ss_head->prev = ss_head;
        ss_head->ss_data = NULL;
}

/**
 * add a new entry to the Session list
 * @new_ent: new entry to be added
 * @ss_list: list head to add it after
 *
 * Insert a new entry after the specified head
 */
void hdcp_session_list_add(struct hdcp_session_node *new_ent, struct hdcp_session_list *ss_list)
{
	struct hdcp_session_node *ss_head;

	if (!new_ent || !ss_list)
		return;

	mutex_lock(&(ss_list->ss_mutex));
	ss_head = &(ss_list->hdcp_session_head);

	ss_head->next->prev = new_ent;
	new_ent->next = ss_head->next;

	new_ent->prev = ss_head;
	ss_head->next = new_ent;
	mutex_unlock(&(ss_list->ss_mutex));

	return;

}

/**
 * delete a entry form the Session list
 * @del_ent: a entry to be deleted
 * @ss_list: session list to remove the session node
 */
void hdcp_session_list_del(struct hdcp_session_node *del_ent, struct hdcp_session_list *ss_list)
{
	if (!del_ent || !ss_list)
		return;

	mutex_lock(&ss_list->ss_mutex);
	del_ent->prev->next = del_ent->next;
	del_ent->next->prev = del_ent->prev;
	mutex_unlock(&ss_list->ss_mutex);
}

/**
 * print all entries in the Session list
 * @ss_list: session list to print all nodes
 */
void hdcp_session_list_print_all(struct hdcp_session_list *ss_list)
{
	struct hdcp_session_node *pos;
	struct hdcp_session_node *ss_head;

	if (!ss_list)
		return;

	mutex_lock(&ss_list->ss_mutex);
	ss_head = &(ss_list->hdcp_session_head);
	for (pos = ss_head->next; pos != ss_head && pos != NULL; pos = pos->next)
		hdcp_info("SessionID: %d\n", pos->ss_data->id);
	mutex_unlock(&ss_list->ss_mutex);
}

/**
 * Find an entry from the Session list
 * @id: session id to find session node
 * @ss_list: session list contain the session node
 */
struct hdcp_session_node *hdcp_session_list_find(uint32_t id, struct hdcp_session_list *ss_list)
{
	struct hdcp_session_node *pos;
	struct hdcp_session_node *ss_head;


	if (!ss_list)
		return NULL;

	mutex_lock(&ss_list->ss_mutex);
	ss_head = &ss_list->hdcp_session_head;
	for (pos = ss_head->next; pos != ss_head && pos != NULL; pos = pos->next) {
		if (pos->ss_data->id == id) {
			mutex_unlock(&ss_list->ss_mutex);
			return pos;
		}
	}
	mutex_unlock(&ss_list->ss_mutex);

	return NULL;
}

/**
 * close all links in the session and remove all session nodes
 * @ss_list: session list to remove all
 */
void hdcp_session_list_destroy(struct hdcp_session_list *ss_list)
{
        struct hdcp_session_node *pos;
        struct hdcp_session_node *ss_head;

        if (!ss_list)
                return;

        mutex_lock(&ss_list->ss_mutex);
        ss_head = &ss_list->hdcp_session_head;
        for (pos = ss_head->next; pos != ss_head && pos != NULL;) {
                if (pos) {
                        /* remove session node from the list*/
                        pos->prev->next = pos->next;
                        pos->next->prev = pos->prev;

                        /* remove session data */
                        /* TODO : remove all links */
                        hdcp_session_data_destroy(&pos->ss_data);
                        if (pos)
                                kfree(pos);
                        pos = ss_head->next;
                }
        }
        ss_head->next = ss_head;
        ss_head->prev = ss_head;
        mutex_unlock(&ss_list->ss_mutex);
}

/**
 * init a Link list
 * @lk_list: list head to add it after
 */
void hdcp_link_list_init(struct hdcp_link_list *lk_list)
{
	struct hdcp_link_node *lk_head;

	if (!lk_list)
		return;

	/* initialize link list mutex */
	mutex_init(&lk_list->lk_mutex);

	mutex_lock(&lk_list->lk_mutex);
	lk_head = &(lk_list->hdcp_link_head);
	lk_head->next = lk_head;
	lk_head->prev = lk_head;
	lk_head->lk_data = NULL;
	mutex_unlock(&lk_list->lk_mutex);
}

/**
 * add a new entry to the Link list
 * @new_ent: new entry to be added
 * @ss_list: list head to add it after
 *
 * Insert a new entry after the specified head
 */
void hdcp_link_list_add(struct hdcp_link_node *new_ent, struct hdcp_link_list *lk_list)
{
	struct hdcp_link_node *lk_head;

	if (!new_ent || !lk_list)
		return;

	mutex_lock(&lk_list->lk_mutex);
	lk_head = &(lk_list->hdcp_link_head);
	lk_head->next->prev = new_ent;
	new_ent->next = lk_head->next;
	new_ent->prev = lk_head;
	lk_head->next = new_ent;
	mutex_unlock(&lk_list->lk_mutex);
}

/**
 * delete a entry form the Link list
 * @del_ent: a entry to be deleted
 * @ss_list: session list to remove the session node
 */
void hdcp_link_list_del(struct hdcp_link_node *del_ent, struct hdcp_link_list *lk_list)
{
	if (!del_ent || !lk_list)
		return;

	mutex_lock(&lk_list->lk_mutex);
	del_ent->prev->next = del_ent->next;
	del_ent->next->prev = del_ent->prev;
	mutex_unlock(&lk_list->lk_mutex);
}

/**
 * print all entries in the Link list
 * @ss_list: session list to print all nodes
 */
void hdcp_link_list_print_all(struct hdcp_link_list *lk_list)
{
	struct hdcp_link_node *pos;
	struct hdcp_link_node *lk_head;

	if (!lk_list)
		return;

	mutex_lock(&lk_list->lk_mutex);
	lk_head = &lk_list->hdcp_link_head;
	for (pos = lk_head->next; pos != lk_head && pos != NULL; pos = pos->next)
		hdcp_info("Link: %d\n", pos->lk_data->id);
	mutex_unlock(&lk_list->lk_mutex);
}

/**
 * Find an entry from the Link list
 * @id: Link handle to find link node
 * @lk_list: link list contain the link node
 */
struct hdcp_link_node *hdcp_link_list_find(uint32_t id, struct hdcp_link_list *lk_list)
{
	struct hdcp_link_node *pos;
	struct hdcp_link_node *lk_head;

	if (!lk_list)
		return NULL;

	mutex_lock(&lk_list->lk_mutex);
	lk_head = &lk_list->hdcp_link_head;
	for (pos = lk_head->next; pos != lk_head && pos != NULL; pos = pos->next) {
		if (pos->lk_data->id == id) {
			mutex_unlock(&lk_list->lk_mutex);
			return pos;
		}
	}
	mutex_unlock(&lk_list->lk_mutex);
	return NULL;
}

/**
 * close all Links and remove all Link nodes
 * @ss_list: session list to remove all
 */
void hdcp_link_list_destroy(struct hdcp_link_list *lk_list)
{
	struct hdcp_link_node *pos;
	struct hdcp_link_node *lk_head;

	if (!lk_list)
		return;

	mutex_lock(&lk_list->lk_mutex);
	lk_head = &(lk_list->hdcp_link_head);
	for (pos = lk_head->next; pos != lk_head && pos != NULL;) {
		/* remove link node from the list*/
		pos->prev->next = pos->next;
		pos->next->prev = pos->prev;

		/* remove link data */
		/* TODO : remove all data */
		if (pos)
			kfree(pos);
		pos = lk_head->next;
	}
	lk_head->next = lk_head;
	lk_head->prev = lk_head;
	mutex_unlock(&lk_list->lk_mutex);
}
