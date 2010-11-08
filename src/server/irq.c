#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_irq_enable(void *priv, uint8_t virtkey)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, virtkey=%#x)", __func__, priv, virtkey);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_irq_get_mask(void *priv, uint32_t *mask)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_irq_get_info(void *priv, enum medcom_irq_source source, uint32_t *info)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, source=%d, info=%p)", __func__, priv, source, info);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
