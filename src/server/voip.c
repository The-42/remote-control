#include <glib.h>

#include "remote-control.h"

int32_t medcom_voip_get_version(void *priv, uint32_t *version)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, version=%p)", __func__, priv, version);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_get_status(void *priv, uint32_t *status)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, status=%p)", __func__, priv, status);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_get_status_string(void *priv, uint32_t status, char **buffer)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, status=%#x, buffer=%p)", __func__, priv, status,
			buffer);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_login(void *priv, struct medcom_voip_account *account)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, account=%p)", __func__, priv, account);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_logout(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_still_logged_in(void *priv, bool *status)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, status=%p)", __func__, priv, status);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_connect_to(void *priv, const char *uri)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, uri=%p)", __func__, priv, uri);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_accept_incoming(void *priv, char **uri)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, uri=%p)", __func__, priv, uri);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_disconnect(void *priv)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p)", __func__, priv);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_is_connected(void *priv, bool *state)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%p)", __func__, priv, state);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_is_calling(void *priv, bool *state)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%p)", __func__, priv, state);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_get_last_contact(void *priv, char **contact)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, contact=%p)", __func__, priv, contact);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
