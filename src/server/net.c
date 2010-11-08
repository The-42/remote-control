#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_net_config(void *priv, uint32_t port, uint32_t timeout, uint32_t repeat, const char *host)
{
	int ret = -ENOSYS;
	g_debug("> %s(priv=%p, port=%u, timeout=%u, repeat=%u, host=%s)",
			__func__, priv, port, timeout, repeat, host);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_net_read(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	int ret = -ENOSYS;
	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_net_write(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	int ret = -ENOSYS;
	g_debug("> %s(priv=%p, mode=%#x, buffer=%p)", __func__, priv, mode,
			buffer);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
