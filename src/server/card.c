#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_card_get_type(void *priv, enum medcom_card_type *type)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, type=%p)", __func__, priv, type);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_card_read(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_card_write(void *priv, off_t offset, struct rpc_buffer *buffer)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, offset=%ld, buffer=%p)", __func__, priv, offset,
			buffer);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
