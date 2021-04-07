#ifndef __HCI_LOOPBACK_H
#define __HCI_LOOPBACK_H
/**
 * Loopback Protocol (Interface)
 *
 * Bounces anything send straight back.
 */

#include "scsc_mport.h"

/*****************************************************************************/

struct scsc_loopback {
	struct scsc_mport port;
};

/*****************************************************************************/

void scsc_loopback_init(struct scsc_loopback *loopback);
void scsc_loopback_deinit(struct scsc_loopback *loopback);

struct scsc_mport *scsc_loopback_get_port(struct scsc_loopback *loopback);

#endif /* __HCI_LOOPBACK_H */
