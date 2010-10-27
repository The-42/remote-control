#include "remote-control.h"

int32_t medcom_net_config(void *priv, uint32_t port, uint32_t timeout, uint32_t repeat, const char *host)
{
	return -ENOSYS;
}

int32_t medcom_net_read(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	return -ENOSYS;
}

int32_t medcom_net_write(void *priv, uint32_t mode, struct rpc_buffer *buffer)
{
	return -ENOSYS;
}
