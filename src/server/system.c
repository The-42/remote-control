#include <glib.h>

#include "remote-control.h"

int32_t medcom_reset(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
