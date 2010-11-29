#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <glib.h>

#ifdef HAVE_LINUX_GPIODEV_H
#include <linux/gpiodev.h>
#endif

#include "remote-control-stub.h"
#include "remote-control.h"

enum gpio {
	GPIO_HANDSET,
	GPIO_TOUCHSCREEN,
	GPIO_SMARTCARD,
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define BIT(x) (1 << (x))

enum gpio gpio_list[] = {
	GPIO_HANDSET,
	GPIO_SMARTCARD,
};

struct event_manager {
	struct rpc_server *server;
	pthread_t thread;
	bool done;
	int fd;

	uint32_t irq_status;

	enum event_smartcard_state smartcard_state;
};

#ifdef HAVE_LINUX_GPIODEV_H
static void *event_thread(void *context)
{
	struct event_manager *manager = context;

	g_debug("> %s(context=%p)", __func__, context);

	while (!manager->done) {
		struct gpio_event gpio;
		struct timeval timeout;
		struct event event;
		fd_set rfds;
		int err;

		memset(&timeout, 0, sizeof(timeout));
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;

		FD_ZERO(&rfds);
		FD_SET(manager->fd, &rfds);

		err = select(manager->fd + 1, &rfds, NULL, NULL, &timeout);
		if (err < 0) {
			g_error("select(): %s", strerror(errno));
			break;
		}

		if (err == 0)
			continue;

		err = read(manager->fd, &gpio, sizeof(gpio));
		if (err < 0) {
			g_error("read(): %s", strerror(errno));
			break;
		}

		switch (gpio.gpio) {
		case GPIO_HANDSET:
			event.source = EVENT_SOURCE_HANDSET;
			if (gpio.value) {
				g_debug("  HANDSET transitioned to HOOK_OFF state");
				event.handset.state = EVENT_HANDSET_STATE_HOOK_OFF;
			} else {
				g_debug("  HANDSET transitioned to HOOK_ON state");
				event.handset.state = EVENT_HANDSET_STATE_HOOK_ON;
			}
			break;

		case GPIO_SMARTCARD:
			event.source = EVENT_SOURCE_SMARTCARD;
			if (gpio.value) {
				g_debug("  SMARTCARD transitioned to REMOVED state");
				event.smartcard.state = EVENT_SMARTCARD_STATE_REMOVED;
			} else {
				g_debug("  SMARTCARD transitioned to INSERTED state");
				event.smartcard.state = EVENT_SMARTCARD_STATE_INSERTED;
			}
			break;

		default:
			g_debug("  unknown GPIO %u transitioned to %u",
					gpio.gpio, gpio.value);
			continue;
		}

		event_manager_report(manager, &event);
	}

	g_debug("< %s()", __func__);
	return NULL;
}
#endif

int event_manager_create(struct event_manager **managerp, struct rpc_server *server)
{
	struct event_manager *manager;
#ifdef HAVE_LINUX_GPIODEV_H
	unsigned int i;
#endif
	int err = 0;

	g_debug("> %s(managerp=%p)", __func__, managerp);

	if (!managerp) {
		err = -EINVAL;
		goto out;
	}

	manager = malloc(sizeof(*manager));
	if (!manager) {
		err = -ENOMEM;
		goto out;
	}

	memset(manager, 0, sizeof(*manager));
	manager->server = server;
	manager->done = false;
	manager->fd = -1;
	manager->irq_status = 0;

#ifdef HAVE_LINUX_GPIODEV_H
	manager->fd = open("/dev/gpio-0", O_RDWR);
	if (manager->fd < 0) {
		err = -errno;
		event_manager_free(manager);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(gpio_list); i++) {
		struct gpio_event enable;

		enable.gpio = gpio_list[i];
		enable.value = 1;

		err = ioctl(manager->fd, GPIO_IOC_ENABLE_IRQ, &enable);
		if (err < 0) {
			err = -errno;
			event_manager_free(manager);
			goto out;
		}
	}

	err = pthread_create(&manager->thread, NULL, event_thread, manager);
	if (err) {
		err = -err;
		event_manager_free(manager);
		goto out;
	}
#endif /* HAVE_LINUX_GPIODEV_H */

	*managerp = manager;

out:
	g_debug("< %s() = %d", __func__, err);
	return err;
}

int event_manager_free(struct event_manager *manager)
{
	int ret = 0;

	g_debug("> %s(manager=%p)", __func__, manager);

	if (!manager) {
		ret = -EINVAL;
		goto out;
	}

	manager->done = true;

	if (manager->thread) {
		g_debug("  waiting for thread to finish ...");
		pthread_join(manager->thread, NULL);
		g_debug("  done");
	}

	if (manager->fd >= 0)
		close(manager->fd);

	free(manager);

out:
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int event_manager_report(struct event_manager *manager, struct event *event)
{
	uint32_t irq_status = 0;
	int ret = 0;

	g_debug("> %s(manager=%p, event=%p)", __func__, manager, event);

	switch (event->source) {
	case EVENT_SOURCE_MODEM:
		irq_status |= BIT(EVENT_SOURCE_MODEM);
		break;

	case EVENT_SOURCE_IO:
		irq_status |= BIT(EVENT_SOURCE_IO);
		break;

	case EVENT_SOURCE_VOIP:
		irq_status |= BIT(EVENT_SOURCE_VOIP);
		break;

	case EVENT_SOURCE_SMARTCARD:
		g_debug("  SMARTCARD interrupt");
		manager->smartcard_state = event->smartcard.state;
		irq_status |= BIT(EVENT_SOURCE_SMARTCARD);
		break;

	case EVENT_SOURCE_HANDSET:
		irq_status |= BIT(EVENT_SOURCE_HANDSET);
		break;

	default:
		ret = -ENXIO;
		break;
	}

	g_debug("  IRQ status: %08x", irq_status);

	if (irq_status != manager->irq_status) {
		ret = medcom_irq_event_stub(manager->server, 0);
		if (ret < 0)
			g_debug("  medcom_irq_event_stub(): %d", ret);
		else
			ret = 0;

		manager->irq_status = irq_status;
	}

	g_debug("  IRQ status: %08x", manager->irq_status);
	g_debug("< %s() = %d", __func__, ret);
	return ret;
}

int event_manager_get_status(struct event_manager *manager, uint32_t *statusp)
{
	if (!manager || !statusp)
		return -EINVAL;

	*statusp = manager->irq_status;
	return 0;
}

int event_manager_get_source_state(struct event_manager *manager, struct event *event)
{
	int err = 0;

	if (!manager || !event)
		return -EINVAL;

	switch (event->source) {
	case EVENT_SOURCE_SMARTCARD:
		manager->irq_status &= ~BIT(EVENT_SOURCE_SMARTCARD);
		event->smartcard.state = manager->smartcard_state;
		break;

	default:
		err = -ENOSYS;
		break;
	}

	return err;
}
