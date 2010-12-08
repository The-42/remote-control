#include <glib.h>

#include "remote-control.h"

int32_t medcom_voip_login(void *priv, struct medcom_voip_account *account)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	g_debug("> %s(priv=%p, account=%p)", __func__, priv, (void *)account);

	err = medcom_voip_login_stub(rpc, &ret, account);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_logout(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	g_debug("> %s(priv=%p)", __func__, priv);

	err = medcom_voip_logout_stub(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_connect_to(void *priv, const char *uri)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	g_debug("> %s(priv=%p, uri=%s)", __func__, priv, uri);

	err = medcom_voip_connect_to_stub(rpc, &ret, uri);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_accept_incoming(void *priv, char **uri)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	g_debug("> %s(priv=%p, uri=%p)", __func__, priv, uri);

	err = medcom_voip_accept_incoming_stub(rpc, &ret, uri);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_voip_disconnect(void *priv)
{
	struct rpc_client *rpc = rpc_client_from_priv(priv);
	int32_t ret = 0;
	int err = 0;

	g_debug("> %s(priv=%p)", __func__, priv);

	err = medcom_voip_disconnect_stub(rpc, &ret);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
