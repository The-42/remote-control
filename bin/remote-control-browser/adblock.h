#ifndef __ADDBLOCK_H__
#define __ADDBLOCK_H__

G_BEGIN_DECLS

typedef struct
{
	gchar* name;
	GType type;
	gchar* default_value;
	gchar* value;
} MESettingString;

typedef struct
{
	gchar* name;
	GType type;
	gchar** default_value;
	gchar** value;
	gsize default_length;
	gsize length;
} MESettingStringList;

/* ------------------------------------  */

void adblock_activate_cb (WebKitBrowser*   browser);
void adblock_deactivate_cb (WebKitBrowser*   browser);
void adblock_add_tab_cb (WebKitWebView* web_view);
void adblock_remove_tab_cb (WebKitWebView* web_view);

G_END_DECLS

#endif
