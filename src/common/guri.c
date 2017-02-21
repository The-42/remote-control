#include <stdlib.h>
#include <string.h>

#include "guri.h"

G_DEFINE_TYPE(GURI, g_uri, G_TYPE_OBJECT);

#define G_URI_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), G_TYPE_URI, GURIPrivate))

struct _GURIPrivate
{
	gchar *scheme;
	gchar *user;
	gchar *password;
	gchar *host;
	guint port;
	gchar *path;
	gchar *query;
};

static inline void safe_free(gchar **ptr)
{
	if (ptr) {
		g_free(*ptr);
		*ptr = NULL;
	}
}

static void g_uri_init(GURI *uri)
{
}

static void g_uri_finalize(GObject *object)
{
	GURI *uri = G_URI(object);
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);

	g_free(priv->query);
	g_free(priv->path);
	g_free(priv->host);
	g_free(priv->password);
	g_free(priv->user);
	g_free(priv->scheme);

	G_OBJECT_CLASS(g_uri_parent_class)->finalize(object);
}

static void g_uri_get_property(GObject *object, guint prop_id, GValue *value,
		GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void g_uri_set_property(GObject *object, guint prop_id,
		const GValue *value, GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void g_uri_class_init(GURIClass *class)
{
	GObjectClass *object = G_OBJECT_CLASS(class);

	g_type_class_add_private(class, sizeof(GURIPrivate));

	object->finalize = g_uri_finalize;
	object->get_property = g_uri_get_property;
	object->set_property = g_uri_set_property;
}

static inline gboolean g_ascii_scheme(gchar c)
{
	return g_ascii_isalnum(c) || (c == '.') || (c == '+') || (c == '-');
}

static void g_uri_parse(GURI *uri, const gchar *string)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	const gchar *colon;
	const gchar *path;
	const gchar *ptr;
	const gchar *end;
	const gchar *tmp;

	safe_free(&priv->scheme);
	safe_free(&priv->user);
	safe_free(&priv->password);
	safe_free(&priv->host);
	safe_free(&priv->path);
	safe_free(&priv->query);

	ptr = end = string;

	while (*end && g_ascii_scheme(*end))
		end++;

	if ((end != ptr) && (*end == ':')) {
		priv->scheme = g_strndup(ptr, end - ptr);
		ptr = end + 1;
		ptr += 2;
	}

	path = ptr + strcspn(ptr, "/?#");

	tmp = strchr(ptr, '@');
	if (tmp && (tmp < path)) {
		colon = strchr(ptr, ':');
		if (colon && (colon < tmp))
			priv->password = g_strndup(colon + 1, tmp - colon - 1);
		else
			colon = tmp;

		priv->user = g_strndup(ptr, colon - ptr);
		ptr = tmp + 1;
	}

	colon = strchr(ptr, ':');
	if (colon && (colon < path)) {
		char *portend = NULL;
		priv->port = strtoul(colon + 1, &portend, 10);
	} else {
		colon = path;
	}

	priv->host = g_strndup(ptr, colon - ptr);
	ptr = end = path;

	tmp = strchr(ptr, '?');
	if (tmp) {
		priv->path = g_strndup(path, tmp - path);
		priv->query = g_strdup(tmp + 1);
	} else {
		priv->path = g_strdup(path);
	}
}

GURI *g_uri_new(const gchar *string)
{
	GURI *uri;

	uri = g_object_new(G_TYPE_URI, NULL);
	if (!uri)
		return NULL;

	g_uri_parse(uri, string);
	return uri;
}

const gchar *g_uri_get_scheme(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	return priv->scheme;
}

void g_uri_set_scheme(GURI *uri, const gchar *scheme)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);

	if (priv->scheme)
		g_free(priv->scheme);

	priv->scheme = g_strdup(scheme);
}

const gchar *g_uri_get_user(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	return priv->user;
}

void g_uri_set_user(GURI *uri, const gchar *user)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);

	if (priv->user)
		g_free(priv->user);

	priv->user = g_strdup(user);
}

const gchar *g_uri_get_password(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	return priv->password;
}

void g_uri_set_password(GURI *uri, const gchar *password)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);

	if (priv->password)
		g_free(priv->password);

	priv->password = g_strdup(password);
}

const gchar *g_uri_get_host(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	return priv->host;
}

void g_uri_set_host(GURI *uri, const gchar *host)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);

	if (priv->host)
		g_free(priv->host);

	priv->host = g_strdup(host);
}

guint g_uri_get_port(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	return priv->port;
}

void g_uri_set_port(GURI *uri, guint port)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	priv->port = port;
}

const gchar *g_uri_get_path(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	return priv->path;
}

void g_uri_set_path(GURI *uri, const gchar *path)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);

	if (priv->path)
		g_free(priv->path);

	priv->path = g_strdup(path);
}

const gchar *g_uri_get_query(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	return priv->query;
}

void g_uri_set_query(GURI *uri, const gchar *query)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);

	if (priv->query)
		g_free(priv->query);

	priv->query = g_strdup(query);
}

gchar *g_uri_to_string(const GURI *uri)
{
	GURIPrivate *priv = G_URI_GET_PRIVATE(uri);
	GString *s;

	s = g_string_new(NULL);
	if (!s)
		return NULL;

	if (priv->scheme)
		g_string_append_printf(s, "%s://", priv->scheme);

	if (priv->user) {
		g_string_append_printf(s, "%s", priv->user);

		if (priv->password)
			g_string_append_printf(s, ":%s", priv->password);

		g_string_append_c(s, '@');
	}

	g_string_append_printf(s, "%s", priv->host);

	if (priv->port)
		g_string_append_printf(s, ":%u", priv->port);

	if (priv->path)
		g_string_append_printf(s, "%s", priv->path);

	if (priv->query)
		g_string_append_printf(s, "?%s", priv->query);

	return g_string_free(s, FALSE);
}

gboolean g_uri_same_origin(const GURI *a, const GURI *b)
{
	return (g_strcmp0(g_uri_get_scheme(a), g_uri_get_scheme(b)) == 0) &&
	       (g_strcmp0(g_uri_get_host(a), g_uri_get_host(b)) == 0) &&
	       (g_uri_get_port(a) == g_uri_get_port(b));
}
