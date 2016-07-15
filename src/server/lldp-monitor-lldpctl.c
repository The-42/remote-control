/*
 * Copyright (C) 2016 Avionic Design GmbH
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <lldpctl.h>
#include <ctype.h>
#include <glib.h>

#include "remote-control-stub.h"
#include "remote-control.h"

struct lldp_monitor {
	lldpctl_conn_t *conn;
	gchar *iface;
};

int lldp_monitor_free(struct lldp_monitor *monitor)
{
	if (!monitor)
		return -EINVAL;

	lldpctl_release(monitor->conn);
	g_free(monitor->iface);

	g_free(monitor);

	return 0;
}

int lldp_monitor_create(struct lldp_monitor **monitorp, GKeyFile *config)
{
	struct lldp_monitor *monitor;
	gchar *value;

	if (!monitorp)
		return -EINVAL;

	monitor = g_new0(struct lldp_monitor, 1);
	if (!monitor)
		return -ENOMEM;

	value = g_key_file_get_string(config, "lldp", "transport", NULL);
	if (value) {
		monitor->conn = lldpctl_new_name(value, NULL, NULL, NULL);
		g_debug("lldp: requested transport %s", value);
		g_free(value);
	} else {
		monitor->conn = lldpctl_new(NULL, NULL, NULL);
	}
	if (!monitor->conn) {
		lldp_monitor_free(monitor);
		return -ENODEV;
	}

	value = g_key_file_get_string(config, "lldp", "interface", NULL);
	if (value) {
		g_debug("lldp: requested interface %s", value);
		monitor->iface = value;
	}

	*monitorp = monitor;

	return 0;
}

GSource *lldp_monitor_get_source(struct lldp_monitor *monitor)
{
	return NULL;
}

static int lldp_write_tlv(char *data, int type, const void *mem, size_t size,
		int sub)
{
	int sub_len = (type == 1 || type == 2) ? 1 : 0;
	int pos = 0;
	int len;

	if (!mem) /* Nothing to add */
		return 0;

	len = sub_len + size;
	if (len > 0x1FF)
		len = 0x1FF;
	if (!data) /* Only detect needed size */
		return 2 + len;
	data[pos++] = (type << 1) + ((len >> 8) & 1);
	data[pos++] = len & 0xFF;
	if (sub_len)
		data[pos++] = sub;
	memcpy(&data[pos], mem, size);

	return pos + size;
}

const char lldp_header[] = {
	0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E, /* DA */
	0, 0, 0, 0, 0, 0, /* SA */ /* TODO: Do we need to implement this? */
	0x88, 0xCC /* Ethertype */
};

const char lldp_ttl_tlv[] = {
	0, 120 /* Use 120 sec for now */
};

const char lldp_end_tlv[] = {
	0, 0
};

static char *lldp_write_neighbor(lldpctl_atom_t *n, int *len)
{
	size_t port_id_len, port_descr_len, chassis_name_len, chassis_descr_len;
	const uint8_t *chassis_id;
	const char *chassis_descr;
	const char *chassis_name;
	const char *port_descr;
	size_t chassis_id_len;
	const char *port_id;
	int pos = 0;
	char *ret;

	chassis_id = lldpctl_atom_get_buffer(n, lldpctl_k_chassis_id,
			&chassis_id_len);
	port_id = lldpctl_atom_get_str(n, lldpctl_k_port_id);
	port_id_len = strlen(port_id);
	port_descr = lldpctl_atom_get_str(n, lldpctl_k_port_descr);
	port_descr_len = strlen(port_descr);
	chassis_name = lldpctl_atom_get_str(n, lldpctl_k_chassis_name);
	chassis_name_len = strlen(chassis_name);
	chassis_descr = lldpctl_atom_get_str(n, lldpctl_k_chassis_descr);
	chassis_descr_len = strlen(chassis_descr);

	ret = malloc(sizeof(lldp_header) +
			lldp_write_tlv(NULL, 1, chassis_id,
					chassis_id_len, 0) +
			lldp_write_tlv(NULL, 2, port_id,
					port_id_len, 0) +
			lldp_write_tlv(NULL, 3, lldp_ttl_tlv,
					sizeof(lldp_ttl_tlv), 0) +
			lldp_write_tlv(NULL, 4, port_descr,
					port_descr_len, 0) +
			lldp_write_tlv(NULL, 5, chassis_name,
					chassis_name_len, 0) +
			lldp_write_tlv(NULL, 6, chassis_descr,
					chassis_descr_len, 0) +
			sizeof (lldp_end_tlv));
	if (!ret)
		return NULL;

	memcpy(&ret[pos], lldp_header, sizeof(lldp_header));
	pos += sizeof(lldp_header);

	pos += lldp_write_tlv(&ret[pos], 1, chassis_id, chassis_id_len,
			lldpctl_atom_get_int(n, lldpctl_k_chassis_id_subtype));
	pos += lldp_write_tlv(&ret[pos], 2, port_id, port_id_len,
			lldpctl_atom_get_int(n, lldpctl_k_port_id_subtype));
	pos += lldp_write_tlv(&ret[pos], 3, lldp_ttl_tlv, sizeof(lldp_ttl_tlv),
			0);
	pos += lldp_write_tlv(&ret[pos], 4, port_descr, port_descr_len,
			0);
	pos += lldp_write_tlv(&ret[pos], 5, chassis_name, chassis_name_len,
			0);
	pos += lldp_write_tlv(&ret[pos], 6, chassis_descr, chassis_descr_len,
			0);

	memcpy(&ret[pos], lldp_end_tlv, sizeof(lldp_end_tlv));
	pos += sizeof(lldp_end_tlv);

	*len = pos;

	return ret;
}

static lldpctl_atom_t *lldp_monitor_get_neighbors(struct lldp_monitor *monitor)
{
	lldpctl_atom_t *ret = NULL;
	lldpctl_atom_iter_t *iter;
	lldpctl_atom_t *ifaces;
	lldpctl_atom_t *iface;

	ifaces = lldpctl_get_interfaces(monitor->conn);
	if (!ifaces)
		return NULL;

	iter = lldpctl_atom_iter(ifaces);
	while (!ret && iter &&
			(iface = lldpctl_atom_iter_value(ifaces, iter))) {
		const char *name = lldpctl_atom_get_str(iface,
				lldpctl_k_interface_name);
		if (!monitor->iface || !g_strcmp0(monitor->iface, name)) {
			lldpctl_atom_t *port = lldpctl_get_port(iface);
			if (port) {
				ret = lldpctl_atom_get(port,
						lldpctl_k_port_neighbors);
				lldpctl_atom_dec_ref(port);
			}
		}
		iter = lldpctl_atom_iter_next(ifaces, iter);
		lldpctl_atom_dec_ref(iface);
	}

	lldpctl_atom_dec_ref(ifaces);

	return ret;
}

ssize_t lldp_monitor_read(struct lldp_monitor *monitor, void *buffer,
		size_t size)
{
	lldpctl_atom_t *neighbors = NULL;
	lldpctl_atom_t *neighbor = NULL;
	char *data = NULL;
	ssize_t ret = 0;

	if (!monitor || !buffer || !size)
		return -EINVAL;

	neighbors = lldp_monitor_get_neighbors(monitor);
	if (neighbors)
		neighbor = lldpctl_atom_iter_value(neighbors,
				lldpctl_atom_iter(neighbors));
	if (neighbor)
		data = lldp_write_neighbor(neighbor, &ret);

	if (ret > size)
		ret = size;
	if (ret > 0)
		memcpy(buffer, data, ret);

	if (neighbor)
		lldpctl_atom_dec_ref(neighbor);
	if (neighbors)
		lldpctl_atom_dec_ref(neighbors);

	free(data);

	return ret;
}
