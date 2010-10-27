#include "remote-control.h"

int32_t medcom_voip_get_version(void *priv, uint32_t *version)
{
	return -ENOSYS;
}

int32_t medcom_voip_get_status(void *priv, uint32_t *status)
{
	return -ENOSYS;
}

int32_t medcom_voip_get_status_string(void *priv, uint32_t status, char **buffer)
{
	return -ENOSYS;
}

int32_t medcom_voip_login(void *priv, struct medcom_voip_account *account)
{
	return -ENOSYS;
}

int32_t medcom_voip_logout(void *priv)
{
	return -ENOSYS;
}

int32_t medcom_voip_still_logged_in(void *priv, bool *status)
{
	return -ENOSYS;
}

int32_t medcom_voip_connect_to(void *priv, const char *uri)
{
	return -ENOSYS;
}

int32_t medcom_voip_accept_incoming(void *priv, char **uri)
{
	return -ENOSYS;
}

int32_t medcom_voip_disconnect(void *priv)
{
	return -ENOSYS;
}

int32_t medcom_voip_is_connected(void *priv, bool *state)
{
	return -ENOSYS;
}

int32_t medcom_voip_is_calling(void *priv, bool *state)
{
	return -ENOSYS;
}

int32_t medcom_voip_get_last_contact(void *priv, char **contact)
{
	return -ENOSYS;
}
