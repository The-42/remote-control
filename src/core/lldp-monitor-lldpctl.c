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
	port_id_len = port_id ? strlen(port_id) : 0;
	port_descr = lldpctl_atom_get_str(n, lldpctl_k_port_descr);
	port_descr_len = port_descr ? strlen(port_descr) : 0;
	chassis_name = lldpctl_atom_get_str(n, lldpctl_k_chassis_name);
	chassis_name_len = chassis_name ? strlen(chassis_name) : 0;
	chassis_descr = lldpctl_atom_get_str(n, lldpctl_k_chassis_descr);
	chassis_descr_len = chassis_descr ? strlen(chassis_descr) : 0;

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

#ifdef DEBUG_INFO
static void lldp_monitor_dump_info(struct lldp_monitor *monitor)
{
	GList *keys, *key;
	GHashTable *info;
	int err;

	if ((err = lldp_monitor_read_info(monitor, &info)) < 0) {
		printf("lldp_monitor_dump_info failed: %d\n", err);
		return;
	}

	printf("lldp_monitor_dump_info:\n");
	keys = g_hash_table_get_keys(info);
	keys = g_list_sort(keys, (GCompareFunc)g_strcmp0);
	key = g_list_first(keys);
	while (key) {
		char *val = g_hash_table_lookup(info, key->data);
		if (val)
			printf("%s:%s\n", (char *)key->data, val);
		key = g_list_next(key);
	}
	g_list_free(keys);
	g_hash_table_unref(info);
}
#endif

ssize_t lldp_monitor_read(struct lldp_monitor *monitor, void *buffer,
		size_t size)
{
	lldpctl_atom_t *neighbors = NULL;
	lldpctl_atom_t *neighbor = NULL;
	char *data = NULL;
	ssize_t ret = 0;

#ifdef DEBUG_INFO
	lldp_monitor_dump_info(monitor);
#endif

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

#define G_HASH_TABLE_ADD_STR(c,p,d,n,k,v) { \
	const char *s = lldpctl_atom_get_str(n, v); \
	if (lldpctl_last_error(c) == LLDPCTL_NO_ERROR && s) \
		g_hash_table_insert(d, \
				g_strdup_printf("%s%s", p, k), \
				g_strdup(s)); \
}
#define G_HASH_TABLE_ADD_INT(c,p,d,n,k,v) { \
	long int i = lldpctl_atom_get_int(n, v); \
	if (lldpctl_last_error(c) == LLDPCTL_NO_ERROR) \
		g_hash_table_insert(d, \
				g_strdup_printf("%s%s", p, k), \
				g_strdup_printf("%ld", i)); \
}
#define LLDPCTR_ATOM_LIST_FOR_EACH(a,i,v) \
	for (i = lldpctl_atom_iter(a); \
	     i && (v = lldpctl_atom_iter_value(a, i)); \
	     i = lldpctl_atom_iter_next(a, i), \
		 lldpctl_atom_dec_ref(v))

static void lldp_add_neighbor(GHashTable *data, lldpctl_conn_t *conn,
		lldpctl_atom_t *neighbor, int index)
{
	gchar *prefix = g_strdup_printf("neighbor%d.", index);
	lldpctl_atom_iter_t *iter;
	lldpctl_atom_t *subs;
	lldpctl_atom_t *sub;
	int pos;

	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "port.protocol",
			lldpctl_k_port_protocol);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor, "port.age",
			lldpctl_k_port_age);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "port.id_subtype",
			lldpctl_k_port_id_subtype);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor, "port.id_subtype_num",
			lldpctl_k_port_id_subtype);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "port.id",
			lldpctl_k_port_id);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "port.descr",
			lldpctl_k_port_descr);

	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor, "port.dot3.mfs",
			lldpctl_k_port_dot3_mfs);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor, "port.dot3.aggregid",
			lldpctl_k_port_dot3_aggregid);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor,
			"port.dot3.autoneg.support",
			lldpctl_k_port_dot3_autoneg_support);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor,
			"port.dot3.autoneg.enabled",
			lldpctl_k_port_dot3_autoneg_enabled);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor,
			"port.dot3.autoneg.advertised",
			lldpctl_k_port_dot3_autoneg_advertised);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "port.dot3.mautype",
			lldpctl_k_port_dot3_mautype);

	sub = lldpctl_atom_get(neighbor, lldpctl_k_port_dot3_power);
	if (sub) {
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.devicetype",
				lldpctl_k_dot3_power_devicetype);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.supported",
				lldpctl_k_dot3_power_supported);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.enabled",
				lldpctl_k_dot3_power_enabled);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.paircontrol",
				lldpctl_k_dot3_power_paircontrol);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.pairs",
				lldpctl_k_dot3_power_pairs);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.class",
				lldpctl_k_dot3_power_class);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.type",
				lldpctl_k_dot3_power_type);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.source",
				lldpctl_k_dot3_power_source);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.priority",
				lldpctl_k_dot3_power_priority);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.allocated",
				lldpctl_k_dot3_power_allocated);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.dot3.power.requested",
				lldpctl_k_dot3_power_requested);
		lldpctl_atom_dec_ref(sub);
	}
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor, "port.pvid",
			lldpctl_k_port_vlan_pvid);
	pos = 0;
	subs = lldpctl_atom_get(neighbor, lldpctl_k_port_vlans);
	LLDPCTR_ATOM_LIST_FOR_EACH(subs, iter, sub) {
		gchar *sub_prefix = g_strdup_printf("%sport.vlan%d.", prefix,
				pos++);

		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "id",
				lldpctl_k_vlan_id);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "name",
				lldpctl_k_vlan_name);
		g_free(sub_prefix);
	}
	lldpctl_atom_dec_ref(subs);

	pos = 0;
	subs = lldpctl_atom_get(neighbor, lldpctl_k_port_ppvids);
	LLDPCTR_ATOM_LIST_FOR_EACH(subs, iter, sub) {
		gchar *sub_prefix = g_strdup_printf("%sport.ppvid%d.", prefix,
				pos++);

		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "status",
				lldpctl_k_ppvid_status);
		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "id",
				lldpctl_k_ppvid_id);
		g_free(sub_prefix);
	}
	lldpctl_atom_dec_ref(subs);

	pos = 0;
	subs = lldpctl_atom_get(neighbor, lldpctl_k_port_pis);
	LLDPCTR_ATOM_LIST_FOR_EACH(subs, iter, sub) {
		gchar *sub_prefix = g_strdup_printf("%sport.pid%d.", prefix,
				pos++);

		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "id",
				lldpctl_k_pi_id);
		g_free(sub_prefix);
	}
	lldpctl_atom_dec_ref(subs);

	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor, "chassis.index",
			lldpctl_k_chassis_index);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "chassis.id_subtype",
			lldpctl_k_chassis_id_subtype);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor, "chassis.id_subtype_num",
			lldpctl_k_chassis_id_subtype);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "chassis.id",
			lldpctl_k_chassis_id);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "chassis.name",
			lldpctl_k_chassis_name);
	G_HASH_TABLE_ADD_STR(conn, prefix, data, neighbor, "chassis.descr",
			lldpctl_k_chassis_descr);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor,
			"chassis.cap.available",
			lldpctl_k_chassis_cap_available);
	G_HASH_TABLE_ADD_INT(conn, prefix, data, neighbor,
			"chassis.cap.enabled", lldpctl_k_chassis_cap_enabled);
	pos = 0;
	subs = lldpctl_atom_get(neighbor, lldpctl_k_chassis_mgmt);
	LLDPCTR_ATOM_LIST_FOR_EACH(subs, iter, sub) {
		gchar *sub_prefix = g_strdup_printf("%schassis.mgmt%d.",
				prefix, pos++);

		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "ip",
				lldpctl_k_mgmt_ip);
		g_free(sub_prefix);
	}
	lldpctl_atom_dec_ref(subs);

	pos = 0;
	subs = lldpctl_atom_get(neighbor, lldpctl_k_port_med_policies);
	LLDPCTR_ATOM_LIST_FOR_EACH(subs, iter, sub) {
		gchar *sub_prefix;

		if (lldpctl_atom_get_int(sub, lldpctl_k_med_policy_type) <= 0)
			continue;

		sub_prefix = g_strdup_printf("%sport.med.policy%d.", prefix,
				pos++);

		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "type",
				lldpctl_k_med_policy_type);
		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "unknown",
				lldpctl_k_med_policy_unknown);
		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "tagging",
				lldpctl_k_med_policy_tagged);
		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "vid",
				lldpctl_k_med_policy_vid);
		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "priority",
				lldpctl_k_med_policy_priority);
		G_HASH_TABLE_ADD_INT(conn, sub_prefix, data, sub, "dscp",
				lldpctl_k_med_policy_dscp);
		g_free(sub_prefix);
	}
	lldpctl_atom_dec_ref(subs);

	pos = 0;
	subs = lldpctl_atom_get(neighbor, lldpctl_k_port_med_locations);
	LLDPCTR_ATOM_LIST_FOR_EACH(subs, iter, sub) {
		gchar *sub_prefix;

		if (lldpctl_atom_get_int(sub,
				lldpctl_k_med_location_format) <= 0)
			continue;

		sub_prefix = g_strdup_printf("%sport.med.location%d.",
				prefix, pos++);

		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "format",
				lldpctl_k_med_location_format);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "geoid",
				lldpctl_k_med_location_geoid);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "latitude",
				lldpctl_k_med_location_latitude);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "longitude",
				lldpctl_k_med_location_longitude);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "altitude",
				lldpctl_k_med_location_altitude);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub,
				"altitude_unit",
				lldpctl_k_med_location_altitude_unit);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "country",
				lldpctl_k_med_location_country);
		G_HASH_TABLE_ADD_STR(conn, sub_prefix, data, sub, "elin",
				lldpctl_k_med_location_elin);
		/*TODO: Add list of med_location_ca_elements (med_civicaddress_type, med_civicaddress_value */
		g_free(sub_prefix);
	}
	lldpctl_atom_dec_ref(subs);

	sub = lldpctl_atom_get(neighbor, lldpctl_k_port_med_power);
	if (sub) {
		G_HASH_TABLE_ADD_STR(conn, prefix, data, sub,
				"port.med.power.type",
				lldpctl_k_med_power_type);
		G_HASH_TABLE_ADD_STR(conn, prefix, data, sub,
				"port.med.power.source",
				lldpctl_k_med_power_source);
		G_HASH_TABLE_ADD_STR(conn, prefix, data, sub,
				"port.med.power.priority",
				lldpctl_k_med_power_priority);
		G_HASH_TABLE_ADD_INT(conn, prefix, data, sub,
				"port.med.power.val", lldpctl_k_med_power_val);
	}
	g_free(prefix);
}

int lldp_monitor_read_info(struct lldp_monitor *monitor, GHashTable **data)
{
	lldpctl_atom_t *neighbors = NULL;
	lldpctl_atom_t *neighbor = NULL;
	lldpctl_atom_iter_t *iter;
	int i = 0;

	if (!monitor || !data)
		return -EINVAL;

	*data = g_hash_table_new_full(NULL, NULL, g_free, g_free);
	if (!*data)
		return -ENOMEM;

	neighbors = lldp_monitor_get_neighbors(monitor);
	LLDPCTR_ATOM_LIST_FOR_EACH(neighbors, iter, neighbor) {
		lldp_add_neighbor(*data, monitor->conn, neighbor, i++);
	}
	lldpctl_atom_dec_ref(neighbors);

	return 0;
}
