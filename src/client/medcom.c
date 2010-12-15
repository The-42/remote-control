#include <pthread.h>

#include "remote-control.h"

struct medcom_client {
	unsigned int done;
	pthread_t thread;
};

static void *poll_event_thread(void *context)
{
	struct rpc_client *rpc = rpc_client_from_priv(context);
	struct medcom_client *client = context;
	ssize_t ret = 0;

	while (!client->done) {
		ret = rpc_client_poll(rpc, 250);
		if (ret < 0) {
			if (ret != -ETIMEDOUT)
				client->done = 1;

			continue;
		}

		ret = rpc_client_process_events(rpc);
		if (ret < 0)
			continue;
	}

	return NULL;
}

int medcom_init(struct medcom_client **clientp, const char *hostname,
		const char *service)
{
	struct rpc_client *rpc = NULL;
	struct medcom_client *client;
	struct rpc_host host;
	int err;

	if (!clientp)
		return -EINVAL;

	err = rpc_client_create(&rpc, RPC_INTERFACE, sizeof(*client));
	if (err < 0)
		return err;

	if (!hostname)
		hostname = "localhost";

	if (!service)
		service = "7482";

	memset(&host, 0, sizeof(host));
	host.hostname = hostname;
	host.service = service;

	err = rpc_client_connect(rpc, &host);
	if (err < 0) {
		rpc_client_free(rpc);
		return err;
	}

	err = rpc_client_set_event_handler(rpc, rpc_dispatch_event);
	if (err < 0) {
		rpc_client_free(rpc);
		return err;
	}

	client = rpc_client_priv(rpc);

	err = pthread_create(&client->thread, NULL, poll_event_thread, client);
	if (err < 0) {
		rpc_client_free(rpc);
		return err;
	}

	*clientp = client;
	return 0;
}

int medcom_exit(struct medcom_client *client)
{
	return -ENOSYS;
}

int medcom_register_event_handler(struct medcom_client *client,
                enum medcom_event queue, medcom_event_handler_t handler,
                void *data)
{
#if 0
        struct event_handler *eh;
        struct list_head *head;

        if (!client || !handler || (queue < 0) || (queue >= MEDCOM_EVENT_MAX))
                return -EINVAL;

        head = &client->handlers[queue];

        eh = malloc(sizeof(*eh));
        if (!eh)
                return -ENOMEM;

        memset(eh, 0, sizeof(*eh));
        eh->handler = handler;
        list_init(&eh->list);
        eh->data = data;

        list_add_tail(&eh->list, head);

        return 0;
#else
	return -ENOSYS;
#endif
}

int medcom_unregister_event_handler(struct medcom_client *client,
                enum medcom_event queue, medcom_event_handler_t handler)
{
#if 0
        struct list_head *node, *temp;
        struct event_handler *eh;
        struct list_head *head;

        if (!client || (queue < 0) || (queue >= MEDCOM_EVENT_MAX))
                return -EINVAL;

        head = &client->handlers[queue];

        list_for_each_safe(node, temp, head) {
                eh = list_entry(node, struct event_handler, list);
                if (eh->handler == handler) {
                        list_del_init(&eh->list);
                        free(eh);
                }
        }

        return 0;
#else
	return -ENOSYS;
#endif
}
