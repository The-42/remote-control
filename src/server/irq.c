#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

#define BIT(x) (1 << (x))

enum {
	IRQ_HANDSET,
	IRQ_SMARTCARD,
	IRQ_VOIP,
	IRQ_IO,
	IRQ_MODEM,
};

int32_t medcom_irq_enable(void *priv, uint8_t virtkey)
{
	int32_t ret = -ENOSYS;
	g_debug("> %s(priv=%p, virtkey=%#x)", __func__, priv, virtkey);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_irq_get_mask(void *priv, uint32_t *mask)
{
	struct remote_control *rc = priv;
	uint32_t status = 0;
	int32_t ret;

	g_debug("> %s(priv=%p, mask=%p)", __func__, priv, mask);

	if (!priv || !mask) {
		ret = -EINVAL;
		goto out;
	}

	ret = event_manager_get_status(rc->event_manager, &status);
	g_debug("  event_manager_get_status(): %d", ret);
	g_debug("  status: %08x", status);

	if (status & BIT(EVENT_SOURCE_SMARTCARD))
		*mask |= BIT(IRQ_SMARTCARD);

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int32_t medcom_irq_get_info(void *priv, enum medcom_irq_source source, uint32_t *info)
{
	struct remote_control *rc = priv;
	struct event event;
	int32_t ret = 0;
	int err;

	g_debug("> %s(priv=%p, source=%d, info=%p)", __func__, priv, source, info);

	memset(&event, 0, sizeof(event));

	switch (source) {
	case MEDCOM_IRQ_SOURCE_UNKNOWN:
		g_debug("  MEDCOM_IRQ_SOURCE_UNKNOWN");
		break;

	case MEDCOM_IRQ_SOURCE_HOOK:
		g_debug("  MEDCOM_IRQ_SOURCE_HOOK");
		break;

	case MEDCOM_IRQ_SOURCE_CARD:
		g_debug("  MEDCOM_IRQ_SOURCE_CARD");
		memset(&event, 0, sizeof(event));
		event.source = EVENT_SOURCE_SMARTCARD;

		err = event_manager_get_source_state(rc->event_manager, &event);
		if (err < 0) {
			ret = err;
			break;
		}

		switch (event.smartcard.state) {
		case EVENT_SMARTCARD_STATE_INSERTED:
			*info = 1;
			break;

		case EVENT_SMARTCARD_STATE_REMOVED:
			*info = 0;
			break;

		default:
			ret = -ENXIO;
			break;
		}
		break;

	case MEDCOM_IRQ_SOURCE_RING:
		g_debug("  MEDCOM_IRQ_SOURCE_RING");
		break;

	case MEDCOM_IRQ_SOURCE_IO:
		g_debug("  MEDCOM_IRQ_SOURCE_IO");
		break;

	case MEDCOM_IRQ_SOURCE_RDP:
		g_debug("  MEDCOM_IRQ_SOURCE_RDP");
		break;

	default:
		ret = -EINVAL;
		break;
	}

	g_debug("< %s(info=%x) = %d", __func__, *info, ret);
	return ret;
}
