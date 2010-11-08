#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_check_io(void *priv, uint8_t *value)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, value=%p)", __func__, priv, value);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_justaboard_set_mask(void *priv, uint32_t mask)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, mask=%#x)", __func__, priv, mask);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_justaboard_get_mask(void *priv, uint32_t *mask)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
