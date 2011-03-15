#ifndef __G_URI_H__
#define __G_URI_H__ 1

#include <glib-object.h>

G_BEGIN_DECLS

#define G_TYPE_URI            (g_uri_get_type())
#define G_URI(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), G_TYPE_URI, GURI))
#define G_URI_CLASS(class)    (G_TYPE_CHECK_CLASS_CAST((class), G_TYPE_URI, GURIClass))
#define G_IS_URI(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), G_TYPE_URI))
#define G_IS_URI_CLASS(class) (G_TYPE_CHECK_CLASS_TYPE((class), G_TYPE_URI))
#define G_URI_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), G_TYPE_URI, GURIClass)

typedef struct _GURI        GURI;
typedef struct _GURIClass   GURIClass;
typedef struct _GURIPrivate GURIPrivate;

struct _GURIClass
{
	GObjectClass parent_class;

	/*< private >*/
};

struct _GURI
{
	GObject parent_instance;
};

GType g_uri_get_type(void) G_GNUC_CONST;

GURI *g_uri_new(const gchar *string);

const gchar *g_uri_get_scheme(const GURI *uri);
void g_uri_set_scheme(GURI *uri, const gchar *scheme);

const gchar *g_uri_get_user(const GURI *uri);
void g_uri_set_user(GURI *uri, const gchar *user);

const gchar *g_uri_get_password(const GURI *uri);
void g_uri_set_password(GURI *uri, const gchar *password);

const gchar *g_uri_get_host(const GURI *uri);
void g_uri_set_host(GURI *uri, const gchar *host);

guint g_uri_get_port(const GURI *uri);
void g_uri_set_port(GURI *uri, guint port);

const gchar *g_uri_get_path(const GURI *uri);
void g_uri_set_path(GURI *uri, const gchar *path);

const gchar *g_uri_get_query(const GURI *uri);
void g_uri_set_query(GURI *uri, const gchar *query);

gboolean g_uri_same_origin(const GURI *a, const GURI *b);
gchar *g_uri_to_string(const GURI *uri);

G_END_DECLS

#endif /* __G_URI_H__ */
