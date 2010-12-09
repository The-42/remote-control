#include <glib.h>

#include "remote-control.h"

int32_t medcom_voip_login(void *priv, struct medcom_voip_account *account)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_voip_login_stub(rpc, &ret, account);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_voip_logout(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_voip_logout_stub(rpc, &ret);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_voip_connect_to(void *priv, const char *uri)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_voip_connect_to_stub(rpc, &ret, uri);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_voip_accept_incoming(void *priv, char **uri)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_voip_accept_incoming_stub(rpc, &ret, uri);
	if (err < 0)
		return err;

	return ret;
}

int32_t medcom_voip_disconnect(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err;

	err = medcom_voip_disconnect_stub(rpc, &ret);
	if (err < 0)
		return err;

	return ret;
}
