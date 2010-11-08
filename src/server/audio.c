#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

int32_t medcom_audio_set_state(void *priv, enum medcom_audio_state state,
		bool force)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%d, force=%d)", __func__, priv, state,
			force);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_audio_get_state(void *priv, enum medcom_audio_state *state)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, state=%p)", __func__, priv, state);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_audio_set_volume(void *priv, uint8_t volume)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, volume=%u)", __func__, priv, volume);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_audio_get_volume(void *priv, uint8_t *volume)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, volume=%p)", __func__, priv, volume);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_audio_enable_speakers(void *priv, bool enable)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, enable=%d)", __func__, priv, enable);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_audio_speakers_enabled(void *priv, bool *enable)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, enable=%p)", __func__, priv, enable);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_audio_call_function(void *priv, uint32_t opcode, uint16_t param)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, opcode=%#x, param=%#x)", __func__, priv,
			opcode, param);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}
