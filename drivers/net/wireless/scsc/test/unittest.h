/****************************************************************************
 *
 * Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd. All rights reserved
 *
 ****************************************************************************/

#ifndef __SLSI_UNITTEST_H__
#define __SLSI_UNITTEST_H__

#include "dev.h"

struct slsi_test_dev;
struct slsi_test_bh_work {
	bool                    available;
	struct slsi_test_dev    *uftestdev;
	struct workqueue_struct *workqueue;
	struct work_struct      work;
	struct slsi_spinlock    spinlock;
};

struct slsi_test_data_route {
	bool configured;
	u16  test_device_minor_number; /* index into slsi_test_devices[] */
	u8   mac[ETH_ALEN];
	u16  vif;
	u8   ipsubnet;
	u16  sequence_number;
};

struct slsi_test_dev {
	/* This is used for:
	 * 1) The uf6kunittesthip<n> chardevice number
	 * 2) The uf6kunittest<n> chardevice number
	 * 3) The /procf/devices/unifi<n> number
	 */
	int                         device_minor_number;

	void                        *uf_cdev;
	struct device               *dev;
	struct slsi_dev             *sdev;

	struct workqueue_struct     *attach_detach_work_queue;
	/* a std mutex */
	struct mutex                attach_detach_mutex;
	struct work_struct          attach_work;
	struct work_struct          detach_work;
	bool                        attached;

	u8                          hw_addr[ETH_ALEN];
	struct slsi_test_bh_work    bh_work;

	/* a std spinlock */
	spinlock_t                  route_spinlock;
	struct slsi_test_data_route route[SLSI_AP_PEER_CONNECTIONS_MAX];
};

void slsi_test_dev_attach(struct slsi_test_dev *uftestdev);
void slsi_test_dev_detach(struct slsi_test_dev *uftestdev);
bool slsi_test_process_signal(struct slsi_test_dev *uftestdev, struct sk_buff *skb);

int slsi_test_udi_node_init(struct slsi_test_dev *uftestdev, struct device *parent);
int slsi_test_udi_node_reregister(struct slsi_test_dev *uftestdev);
int slsi_test_udi_node_deinit(struct slsi_test_dev *uftestdev);

int slsi_test_udi_init(void);
int slsi_test_udi_deinit(void);

void slsi_test_bh_work_f(struct work_struct *work);
static inline int slsi_test_bh_init(struct slsi_test_dev *uftestdev)
{
	uftestdev->bh_work.available = false;
	uftestdev->bh_work.uftestdev = uftestdev;
	slsi_spinlock_create(&uftestdev->bh_work.spinlock);
	INIT_WORK(&uftestdev->bh_work.work, slsi_test_bh_work_f);
	uftestdev->bh_work.workqueue = alloc_ordered_workqueue("slsi_wlan_unittest_bh", 0);
	if (!uftestdev->bh_work.workqueue)
		return -ENOMEM;
	uftestdev->bh_work.available = true;
	return 0;
}

static inline void slsi_test_bh_start(struct slsi_test_dev *uftestdev)
{
	slsi_spinlock_lock(&uftestdev->bh_work.spinlock);
	uftestdev->bh_work.available = true;
	slsi_spinlock_unlock(&uftestdev->bh_work.spinlock);
}

static inline void slsi_test_bh_run(struct slsi_test_dev *uftestdev)
{
	slsi_spinlock_lock(&uftestdev->bh_work.spinlock);
	if (!uftestdev->bh_work.available)
		goto exit;
	queue_work(uftestdev->bh_work.workqueue, &uftestdev->bh_work.work);
exit:
	slsi_spinlock_unlock(&uftestdev->bh_work.spinlock);
}

static inline void slsi_test_bh_stop(struct slsi_test_dev *uftestdev)
{
	struct workqueue_struct *workqueue = NULL;

	slsi_spinlock_lock(&uftestdev->bh_work.spinlock);
	uftestdev->bh_work.available = false;
	workqueue = uftestdev->bh_work.workqueue;
	uftestdev->bh_work.workqueue = NULL;
	slsi_spinlock_unlock(&uftestdev->bh_work.spinlock);

	if (workqueue)
		flush_workqueue(workqueue);
}

static inline void slsi_test_bh_deinit(struct slsi_test_dev *uftestdev)
{
	struct workqueue_struct *workqueue = NULL;

	slsi_spinlock_lock(&uftestdev->bh_work.spinlock);
	WARN_ON(uftestdev->bh_work.available);
	uftestdev->bh_work.available = false;
	workqueue = uftestdev->bh_work.workqueue;
	uftestdev->bh_work.workqueue = NULL;
	slsi_spinlock_unlock(&uftestdev->bh_work.spinlock);
	if (workqueue) {
		flush_workqueue(workqueue);
		destroy_workqueue(workqueue);
	}
}

#endif
