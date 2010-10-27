#include "remote-control.h"

int32_t medcom_audio_set_state(void *priv, enum medcom_audio_state state, bool force)
{
	return -ENOSYS;
}

int32_t medcom_audio_get_state(void *priv, enum medcom_audio_state *state)
{
	return -ENOSYS;
}

int32_t medcom_audio_set_volume(void *priv, uint8_t volume)
{
	return -ENOSYS;
}

int32_t medcom_audio_get_volume(void *priv, uint8_t *volume)
{
	return -ENOSYS;
}

int32_t medcom_audio_enable_speakers(void *priv, bool enable)
{
	return -ENOSYS;
}

int32_t medcom_audio_speakers_enabled(void *priv, bool *enable)
{
	return -ENOSYS;
}

int32_t medcom_audio_call_function(void *priv, uint32_t opcode, uint16_t param)
{
	return -ENOSYS;
}
