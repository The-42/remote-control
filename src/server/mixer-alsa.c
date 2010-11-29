#include "remote-control-stub.h"
#include "remote-control.h"

struct mixer_control {
	//snd_mixer_selem_t *element;
};

struct mixer {
	//snd_mixer_t *mixer;

	struct mixer_control *controls[MIXER_CONTROL_MAX];
};

int mixer_create(struct mixer **mixerp)
{
	return -ENOSYS;
}

int mixer_free(struct mixer *mixer)
{
	return -ENOSYS;
}

int mixer_set_volume(struct mixer *mixer, unsigned short control, uint8_t volume)
{
	return -ENOSYS;
}

int mixer_get_volume(struct mixer *mixer, unsigned short control, uint8_t *volumep)
{
	return -ENOSYS;
}

int mixer_set_mute(struct mixer *mixer, unsigned short control, bool mute)
{
	return -ENOSYS;
}

int mixer_is_muted(struct mixer *mixer, unsigned short control, bool *mutep)
{
	return -ENOSYS;
}
