/****************************************************************************
 *
 *       Copyright (c) 2012 - 2016 Samsung Electronics Co., Ltd
 *
 ****************************************************************************/

#include <net/ip.h>

#include "debug.h"
#include "utils.h"
#include "udi.h"
#include "unittest.h"
#include "mgt.h"
#include "scsc/scsc_mx.h"

#define SLSI_TESTDRV_NAME "s5n2560_test"

static int radios = 11;
module_param(radios, int, 0444);
MODULE_PARM_DESC(radios, "Number of simulated radios");

/* spinlock for retaining the (struct slsi_dev) information */
static struct slsi_spinlock slsi_test_devices_lock;
static struct slsi_test_dev *slsi_test_devices[SLSI_UDI_MINOR_NODES];

static struct class         *test_dev_class;
/* Major number of device created by system. */
static dev_t                major_number;

static struct device_driver slsi_test_driver = {
	.name = SLSI_TESTDRV_NAME
};

static void slsi_test_dev_attach_work(struct work_struct *work);
static void slsi_test_dev_detach_work(struct work_struct *work);

static void slsi_test_dev_free(void)
{
	int i;

	for (i = 0; i < SLSI_UDI_MINOR_NODES; i++) {
		struct slsi_test_dev *uftestdev;

		slsi_spinlock_lock(&slsi_test_devices_lock);
		uftestdev = slsi_test_devices[i];
		slsi_test_devices[i] = NULL;
		slsi_spinlock_unlock(&slsi_test_devices_lock);
		if (uftestdev != NULL) {
			SLSI_INFO_NODEV("Free Test Device: %02X:%02X:%02X:%02X:%02X:%02X\n",
					uftestdev->hw_addr[0],
					uftestdev->hw_addr[1],
					uftestdev->hw_addr[2],
					uftestdev->hw_addr[3],
					uftestdev->hw_addr[4],
					uftestdev->hw_addr[5]);

			if (WARN_ON(uftestdev->attached)) {
				slsi_test_bh_deinit(uftestdev);
				flush_workqueue(uftestdev->attach_detach_work_queue);
			}
			destroy_workqueue(uftestdev->attach_detach_work_queue);

			slsi_test_udi_node_deinit(uftestdev);

			device_unregister(uftestdev->dev);
			device_destroy(test_dev_class, uftestdev->dev->devt);
		}
	}

	slsi_test_udi_deinit();

	if (test_dev_class != NULL)
		class_destroy(test_dev_class);

	unregister_chrdev_region(major_number, SLSI_UDI_MINOR_NODES);
}

int slsi_sdio_func_drv_register(void)
{
	int                  i = 0, err = 0, ret = 0;
	struct slsi_test_dev *uftestdev;
	dev_t                devno;

	SLSI_INFO_NODEV("Loading SLSI " SLSI_TESTDRV_NAME " Test Driver for mac80211\n");

	if (radios > SLSI_UDI_MINOR_NODES) {
		SLSI_ERR_NODEV("Loading failed, configure SLSI_UDI_MINOR_NODES to match no. of simulated radios\n");
		return -ENOMEM;
	}

	slsi_spinlock_create(&slsi_test_devices_lock);
	memset(slsi_test_devices, 0x00, sizeof(slsi_test_devices));

	/* Allocate two device numbers for each device. */
	ret = alloc_chrdev_region(&major_number, 0, SLSI_UDI_MINOR_NODES, SLSI_TESTDRV_NAME);
	if (ret) {
		SLSI_ERR_NODEV("Failed to add alloc dev numbers: %d\n", ret);
		unregister_chrdev_region(major_number, SLSI_UDI_MINOR_NODES);
		major_number = 0;
		return -ENOMEM;
	}

	test_dev_class = class_create(THIS_MODULE, SLSI_TESTDRV_NAME);
	if (IS_ERR(test_dev_class))
		return -EAGAIN;

	slsi_test_udi_init();

	for (i = 0; i < radios; i++) {
		uftestdev = kmalloc(sizeof(*uftestdev), GFP_KERNEL);
		memset(uftestdev, 0, sizeof(*uftestdev));
		uftestdev->attach_detach_work_queue = alloc_ordered_workqueue("Test Work", 0);
		INIT_WORK(&uftestdev->attach_work, slsi_test_dev_attach_work);
		INIT_WORK(&uftestdev->detach_work, slsi_test_dev_detach_work);

		devno = MKDEV(MAJOR(major_number), i);

		uftestdev->dev = device_create(test_dev_class, NULL, devno, uftestdev, SLSI_TESTDRV_NAME "_dev%d", i);
		if (IS_ERR(uftestdev->dev)) {
			SLSI_ERR_NODEV("device_create FAILED, returned - (%ld)\n", PTR_ERR(uftestdev->dev));
			err = -ENOMEM;
			goto failed_free_all;
		}

		uftestdev->dev->driver = &slsi_test_driver;

		mutex_init(&uftestdev->attach_detach_mutex);
		slsi_test_bh_init(uftestdev);
		spin_lock_init(&uftestdev->route_spinlock);

		if (slsi_test_udi_node_init(uftestdev, uftestdev->dev) != 0) {
			SLSI_ERR_NODEV("udi <node> init FAILED\n");
			goto failed_dev_unregister;
		}

		/* Using a fixed MAC address instead of slsi_get_hw_mac_address(),
		 * MAC Address format 00:12:FB:00:00:<xx> where xx increments for every PHY
		 * (00:12:FB OUI Samsung Electronics)
		 */

		memset(uftestdev->hw_addr, 0, sizeof(uftestdev->hw_addr));
		uftestdev->hw_addr[1] = 0x12;
		uftestdev->hw_addr[2] = 0xFB;
		/*To randomize the mac address*/
		uftestdev->hw_addr[ETH_ALEN - 1] += (i & (0xff));

		SLSI_INFO_NODEV("Create Test Device: %02X:%02X:%02X:%02X:%02X:%02X\n",
				uftestdev->hw_addr[0],
				uftestdev->hw_addr[1],
				uftestdev->hw_addr[2],
				uftestdev->hw_addr[3],
				uftestdev->hw_addr[4],
				uftestdev->hw_addr[5]);
		slsi_test_devices[uftestdev->device_minor_number] = uftestdev;
	}

	return 0;

failed_dev_unregister:
	device_unregister(uftestdev->dev);
	device_destroy(test_dev_class, uftestdev->dev->devt);
failed_free_all:
	slsi_test_dev_free();

	return -EPERM;
}

void slsi_sdio_func_drv_unregister(void)
{
	SLSI_INFO_NODEV("Unloading UF6K Test Driver for mac80211\n");
	slsi_test_dev_free();
}

void slsi_test_dev_attach(struct slsi_test_dev *uftestdev)
{
	struct slsi_dev            *sdev;
	struct scsc_service_client service_client;

	mutex_lock(&uftestdev->attach_detach_mutex);
	SLSI_INFO_NODEV("UnitTest UDI Attached : %02X:%02X:%02X:%02X:%02X:%02X\n",
			uftestdev->hw_addr[0],
			uftestdev->hw_addr[1],
			uftestdev->hw_addr[2],
			uftestdev->hw_addr[3],
			uftestdev->hw_addr[4],
			uftestdev->hw_addr[5]);

	if (uftestdev->attached) {
		SLSI_ERR_NODEV("attached == true\n");
		goto exit;
	}

	uftestdev->attached = true;
	sdev = slsi_dev_attach(uftestdev->dev, (struct scsc_mx *)uftestdev, &service_client);

	slsi_spinlock_lock(&slsi_test_devices_lock);
	uftestdev->sdev = sdev;
	if (!sdev) {
		SLSI_ERR_NODEV("slsi_dev_attach() Failed\n");
		uftestdev->attached = false;
	} else {
		slsi_test_bh_start(uftestdev);
	}

	slsi_spinlock_unlock(&slsi_test_devices_lock);

exit:
	mutex_unlock(&uftestdev->attach_detach_mutex);
}

void slsi_test_dev_detach(struct slsi_test_dev *uftestdev)
{
	mutex_lock(&uftestdev->attach_detach_mutex);
	SLSI_INFO(uftestdev->sdev, "UnitTest UDI Detached : %02X:%02X:%02X:%02X:%02X:%02X\n",
		  uftestdev->hw_addr[0],
		  uftestdev->hw_addr[1],
		  uftestdev->hw_addr[2],
		  uftestdev->hw_addr[3],
		  uftestdev->hw_addr[4],
		  uftestdev->hw_addr[5]);
	if (!uftestdev->attached) {
		SLSI_ERR(uftestdev->sdev, "attached != true\n");
		goto exit;
	}

	uftestdev->attached = false;
	if (uftestdev->sdev) {
		struct slsi_dev *sdev = uftestdev->sdev;

		slsi_test_bh_stop(uftestdev);

		slsi_dev_detach(sdev);
		slsi_spinlock_lock(&slsi_test_devices_lock);
		uftestdev->sdev = NULL;
		slsi_spinlock_unlock(&slsi_test_devices_lock);
	}

exit:
	mutex_unlock(&uftestdev->attach_detach_mutex);
}

void slsi_init_netdev_mac_addr(struct slsi_dev *sdev)
{
	/* Get mac address from file system. */
	slsi_get_hw_mac_address(sdev, sdev->hw_addr);

	SLSI_ETHER_COPY(sdev->netdev_addresses[SLSI_NET_INDEX_WLAN], sdev->hw_addr);

	SLSI_ETHER_COPY(sdev->netdev_addresses[SLSI_NET_INDEX_P2P],  sdev->hw_addr);
	sdev->netdev_addresses[SLSI_NET_INDEX_P2P][0] |= 0x02; /* Set the local bit */

	SLSI_ETHER_COPY(sdev->netdev_addresses[SLSI_NET_INDEX_P2PX_SWLAN], sdev->hw_addr);
	sdev->netdev_addresses[SLSI_NET_INDEX_P2PX_SWLAN][0] |= 0x02; /* Set the local bit */
	sdev->netdev_addresses[SLSI_NET_INDEX_P2PX_SWLAN][4] ^= 0x80; /* EXOR 5th byte with 0x80 */

	SLSI_ETHER_COPY(sdev->netdev[SLSI_NET_INDEX_WLAN]->dev_addr, sdev->netdev_addresses[SLSI_NET_INDEX_WLAN]);
	SLSI_ETHER_COPY(sdev->netdev[SLSI_NET_INDEX_P2P]->dev_addr, sdev->netdev_addresses[SLSI_NET_INDEX_P2P]);
}

bool slsi_test_process_signal_ip_remap(struct slsi_test_dev *uftestdev, struct sk_buff *skb, struct slsi_test_data_route *route)
{
	int proto = ntohs(skb->protocol);
	u8  *frame = fapi_get_data(skb) + 14;

	switch (proto) {
	case 0x0806:
	{
		/* Arp */
		u8 *sha = &frame[8];
		u8 *spa = &frame[14];
		u8 *tha = &frame[18];
		u8 *tpa = &frame[24];

		SLSI_UNUSED_PARAMETER(sha);
		SLSI_UNUSED_PARAMETER(spa);
		SLSI_UNUSED_PARAMETER(tha);
		SLSI_UNUSED_PARAMETER(tpa);

		SLSI_DBG4(uftestdev->sdev, SLSI_TEST, "ARP: sha:%pM, spa:%d.%d.%d.%d, tha:%pM, tpa:%d.%d.%d.%d\n",
			  sha, spa[0], spa[1], spa[2], spa[3],
			  tha, tpa[0], tpa[1], tpa[2], tpa[3]);
		spa[2] = route->ipsubnet;
		tpa[2] = route->ipsubnet;
		SLSI_DBG4(uftestdev->sdev, SLSI_TEST, "ARP: sha:%pM, spa:%d.%d.%d.%d, tha:%pM, tpa:%d.%d.%d.%d\n",
			  sha, spa[0], spa[1], spa[2], spa[3],
			  tha, tpa[0], tpa[1], tpa[2], tpa[3]);
		return true;
	}
	case 0x0800:
	{
		/* IPv4 */
		struct iphdr *iph = (struct iphdr *)frame;
		u8           *src = (u8 *)&iph->saddr;
		u8           *dst = (u8 *)&iph->daddr;

		SLSI_UNUSED_PARAMETER(src);
		SLSI_UNUSED_PARAMETER(dst);

		SLSI_DBG4(uftestdev->sdev, SLSI_TEST, "PING: src:%d.%d.%d.%d, dst:%d.%d.%d.%d, check:0x%.4X\n",
			  src[0], src[1], src[2], src[3],
			  dst[0], dst[1], dst[2], dst[3],
			  iph->check);
		src[2] = route->ipsubnet;
		dst[2] = route->ipsubnet;

		/* Calculation of IP header checksum */
		iph->check = 0;
		ip_send_check(iph);

		SLSI_DBG4(uftestdev->sdev, SLSI_TEST, "PING: src:%d.%d.%d.%d, dst:%d.%d.%d.%d, check:0x%.4X\n",
			  src[0], src[1], src[2], src[3],
			  dst[0], dst[1], dst[2], dst[3],
			  iph->check);

		return true;
	}
	default:
		SLSI_DBG4(uftestdev->sdev, SLSI_TEST, "Proto:0x%.4X\n", proto);
		break;
	}
	return false;
}

static struct slsi_test_data_route *slsi_test_process_signal_get_route(struct slsi_test_dev *uftestdev, const u8 *mac)
{
	struct slsi_test_data_route *route;
	int                         i;

	if (WARN_ON(!spin_is_locked(&uftestdev->route_spinlock)))
		return NULL;

	for (i = 0; i < SLSI_AP_PEER_CONNECTIONS_MAX; i++) {
		route = &uftestdev->route[i];
		if (route->configured && ether_addr_equal(route->mac, mac))
			return route;
	}

	return NULL;
}

static struct slsi_test_data_route *slsi_test_process_signal_get_free_route(struct slsi_test_dev *uftestdev)
{
	struct slsi_test_data_route *route;
	int                         i;

	if (WARN_ON(!spin_is_locked(&uftestdev->route_spinlock)))
		return NULL;

	for (i = 0; i < SLSI_AP_PEER_CONNECTIONS_MAX; i++) {
		route = &uftestdev->route[i];
		if (!route->configured)
			return route;
	}

	return NULL;
}

void slsi_test_process_signal_set_route(struct slsi_test_dev *uftestdev, struct sk_buff *skb)
{
	struct slsi_test_data_route *route;
	u8                          mac[ETH_ALEN];
	u16                         dest_device_minor_number = 0xFFFF;
	int                         i;

	mac[0] = fapi_get_buff(skb, u.debug_generic_req.debug_words[2]) >> 8;
	mac[1] = fapi_get_buff(skb, u.debug_generic_req.debug_words[2]) & 0xFF;
	mac[2] = fapi_get_buff(skb, u.debug_generic_req.debug_words[3]) >> 8;
	mac[3] = fapi_get_buff(skb, u.debug_generic_req.debug_words[3]) & 0xFF;
	mac[4] = fapi_get_buff(skb, u.debug_generic_req.debug_words[4]) >> 8;
	mac[5] = fapi_get_buff(skb, u.debug_generic_req.debug_words[4]) & 0xFF;

	slsi_spinlock_lock(&slsi_test_devices_lock);
	for (i = 0; i < SLSI_UDI_MINOR_NODES; i++) {
		struct slsi_test_dev *destdev = slsi_test_devices[i];

		if (destdev != NULL && ether_addr_equal(destdev->hw_addr, mac))
			dest_device_minor_number = destdev->device_minor_number;
	}
	slsi_spinlock_unlock(&slsi_test_devices_lock);

	if (dest_device_minor_number == 0xFFFF) {
		SLSI_ERR(uftestdev->sdev, "Setting Route for %pM FAILED. No match found\n", mac);
		return;
	}
	spin_lock(&uftestdev->route_spinlock);
	route = slsi_test_process_signal_get_route(uftestdev, mac);
	if (!route)
		route = slsi_test_process_signal_get_free_route(uftestdev);

	if (route) {
		SLSI_DBG1(uftestdev->sdev, SLSI_TEST, "Setting Route for %pM -> %pM\n", uftestdev->hw_addr, mac);
		route->configured = true;
		route->test_device_minor_number = dest_device_minor_number;
		SLSI_ETHER_COPY(route->mac, mac);
		route->vif      = fapi_get_u16(skb, u.debug_generic_req.debug_words[5]);
		route->ipsubnet = fapi_get_u16(skb, u.debug_generic_req.debug_words[6]) & 0xFF;
		route->sequence_number = 1;
	} else {
		SLSI_ERR(uftestdev->sdev, "Setting Route for %pM FAILED. No Free Route Entry\n", mac);
	}

	spin_unlock(&uftestdev->route_spinlock);
}

void slsi_test_process_signal_clear_route(struct slsi_test_dev *uftestdev, struct sk_buff *skb)
{
	struct slsi_test_data_route *route;
	u8                          mac[ETH_ALEN];

	mac[0] = fapi_get_buff(skb, u.debug_generic_req.debug_words[2]) >> 8;
	mac[1] = fapi_get_buff(skb, u.debug_generic_req.debug_words[2]) & 0xFF;
	mac[2] = fapi_get_buff(skb, u.debug_generic_req.debug_words[3]) >> 8;
	mac[3] = fapi_get_buff(skb, u.debug_generic_req.debug_words[3]) & 0xFF;
	mac[4] = fapi_get_buff(skb, u.debug_generic_req.debug_words[4]) >> 8;
	mac[5] = fapi_get_buff(skb, u.debug_generic_req.debug_words[4]) & 0xFF;

	spin_lock(&uftestdev->route_spinlock);
	SLSI_DBG1(uftestdev->sdev, SLSI_TEST, "Clearing Route for %pM\n", mac);
	route = slsi_test_process_signal_get_route(uftestdev, mac);
	if (route)
		route->configured = false;
	else
		SLSI_ERR(uftestdev->sdev, "Clearing Route for %pM FAILED. No Route Entry Found\n", mac);
	spin_unlock(&uftestdev->route_spinlock);
}

bool slsi_test_process_signal(struct slsi_test_dev *uftestdev, struct sk_buff *skb)
{
	if (fapi_get_sigid(skb) == DEBUG_GENERIC_REQ) {
		SLSI_DBG1(uftestdev->sdev, SLSI_TEST, "fapi_get_u16(skb, u.debug_generic_req.debug_words[0]) = %d\n", fapi_get_u16(skb, u.debug_generic_req.debug_words[0]));
		SLSI_DBG1(uftestdev->sdev, SLSI_TEST, "fapi_get_u16(skb, u.debug_generic_req.debug_words[1]) = %d\n", fapi_get_u16(skb, u.debug_generic_req.debug_words[1]));
		if (fapi_get_u16(skb, u.debug_generic_req.debug_words[0]) == 0x1357) {
			if (fapi_get_u16(skb, u.debug_generic_req.debug_words[1]) == 0)
				queue_work(uftestdev->attach_detach_work_queue, &uftestdev->detach_work);
			else if (fapi_get_u16(skb, u.debug_generic_req.debug_words[1]) == 1)
				queue_work(uftestdev->attach_detach_work_queue, &uftestdev->attach_work);
			else if (fapi_get_u16(skb, u.debug_generic_req.debug_words[1]) == 2)
				slsi_test_process_signal_set_route(uftestdev, skb);
			else if (fapi_get_u16(skb, u.debug_generic_req.debug_words[1]) == 3)
				slsi_test_process_signal_clear_route(uftestdev, skb);
		}
		kfree_skb(skb);
		return true;
	}

	/* Automatically route the packet to the other test device and bypass the */
	if (fapi_get_sigid(skb) == MA_UNITDATA_REQ) {
		struct slsi_test_data_route *route;
		struct ethhdr               *ehdr = (struct ethhdr *)skb->data;

		spin_lock(&uftestdev->route_spinlock);
		route = slsi_test_process_signal_get_route(uftestdev, ehdr->h_dest);
		if (route && slsi_test_process_signal_ip_remap(uftestdev, skb, route)) {
			struct slsi_skb_cb        *cb;
			struct fapi_signal req = *((struct fapi_signal *)skb->data);
			struct fapi_signal *ind;

			/* Convert the MA_UNITDATA_REQ to a MA_UNITDATA_IND */
			WARN_ON(!skb_pull(skb, fapi_sig_size(ma_unitdata_req)));
			ind = (struct fapi_signal *)skb_push(skb, fapi_sig_size(ma_unitdata_ind));
			if (WARN_ON(!ind)) {
				kfree_skb(skb);
				spin_unlock(&uftestdev->route_spinlock);
				return true;
			}

			ind->id = cpu_to_le16(MA_UNITDATA_IND);
			ind->receiver_pid = 0;
			ind->sender_pid = 0;
			fapi_set_u16(skb, u.ma_unitdata_ind.vif, cpu_to_le16(route->vif));
			fapi_set_u16(skb, u.ma_unitdata_ind.sequence_number,             route->sequence_number++);

			cb = slsi_skb_cb_init(skb);
			cb->sig_length = fapi_get_expected_size(skb);
			cb->data_length = skb->len;

			slsi_spinlock_lock(&slsi_test_devices_lock);
			if (slsi_test_devices[route->test_device_minor_number] &&
			    slsi_test_devices[route->test_device_minor_number]->sdev) {
				if (slsi_hip_rx(slsi_test_devices[route->test_device_minor_number]->sdev, skb) != 0)
					kfree_skb(skb);
			} else {
				route->configured = false;
				kfree_skb(skb);
			}
			slsi_spinlock_unlock(&slsi_test_devices_lock);
			spin_unlock(&uftestdev->route_spinlock);
			return true;
		}
		spin_unlock(&uftestdev->route_spinlock);
	}

	return false;
}

static void slsi_test_dev_attach_work(struct work_struct *work)
{
	struct slsi_test_dev *uftestdev = container_of(work, struct slsi_test_dev, attach_work);

	SLSI_INFO_NODEV("UnitTest TEST Attach\n");
	slsi_test_dev_attach(uftestdev);
	slsi_test_udi_node_reregister(uftestdev);
}

static void slsi_test_dev_detach_work(struct work_struct *work)
{
	struct slsi_test_dev *uftestdev = container_of(work, struct slsi_test_dev, detach_work);

	SLSI_INFO(uftestdev->sdev, "UnitTest TEST Detach\n");
	slsi_test_dev_detach(uftestdev);
}

void scsc_wifi_unpause_arp_q_all_vif(struct slsi_dev *sdev)
{
}

void scsc_wifi_pause_arp_q_all_vif(struct slsi_dev *sdev)
{
}
