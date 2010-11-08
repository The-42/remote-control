#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_lldp_enable(void *priv, bool enable)
{
	int ret = -ENOSYS;
	g_debug("> %s(priv=%p, enable=%d)", __func__, priv, enable);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_lldp_read(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	int ret = -ENOSYS;
	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
