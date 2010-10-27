#include "remote-control.h"

int32_t medcom_card_get_type(void *priv, enum medcom_card_type *type)
{
	return -ENOSYS;
}

int32_t medcom_card_read(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	return -ENOSYS;
}

int32_t medcom_card_write(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	return -ENOSYS;
}
