/**
 * Loopback Protocol (Implementation)
 */

#include <linux/kernel.h>
#include <linux/module.h>

#include <hydra/trace.h>

#include "scsc_loopback.h"

/*****************************************************************************/

/**
 * Handle data received on port by sending it back.
 */
static void scsc_loopback_port_recv(
	struct scsc_mport   *port,
	const unsigned char *data,
	size_t              count)
{
	os_trace_dbg("%s: @%p, count %zu", __func__, port, count);

	scsc_mport_emit(port, data, count);
}

static const struct scsc_mport_ops scsc_loopback_port_ops = {
	scsc_loopback_port_recv
};

/*****************************************************************************/

void scsc_loopback_init(struct scsc_loopback *loopback)
{
	os_trace_dbg("%s: @%p", __func__, loopback);

	scsc_mport_init(&loopback->port, &scsc_loopback_port_ops);
}

void scsc_loopback_deinit(struct scsc_loopback *loopback)
{
}

struct scsc_mport *scsc_loopback_get_port(
	struct scsc_loopback *loopback)
{
	return &loopback->port;
}
