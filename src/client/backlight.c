#include "remote-control.h"

int32_t medcom_backlight_enable(void *priv, uint32_t flags)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	printf("> %s(priv=%p, flags=%x)\n", __func__, priv, flags);

	err = medcom_backlight_enable_stub(client, &ret, flags);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	printf("< %s() = %d\n", __func__, ret);
	return ret;
}

int32_t medcom_backlight_get(void *priv, uint8_t *brightness)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	printf("> %s(priv=%p, brightness=%p)\n", __func__, priv, brightness);

	err = medcom_backlight_get_stub(client, &ret, brightness);
	if (err < 0) {
		printf("medcom_backlight_get_stub(): %d\n", err);
		ret = err;
		goto out;
	}

out:
	printf("< %s() = %d\n", __func__, ret);
	return ret;
}

int32_t medcom_backlight_set(void *priv, uint8_t brightness)
{
	struct rpc_client *client = rpc_client_from_priv(priv);
	int32_t ret;
	int err;

	printf("> %s(priv=%p, brightness=%02x)\n", __func__, priv, brightness);

	err = medcom_backlight_set_stub(client, &ret, brightness);
	if (err < 0) {
		ret = err;
		goto out;
	}

out:
	printf("< %s() = %d\n", __func__, ret);
	return ret;
}
