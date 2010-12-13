#ifndef VOIP__H
#define VOIP__H 1

struct panel_data;

int voip_panel_append(GtkWidget *widget, const gchar* to_append);

inline void on_voip_button_1_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "1");
}

inline void on_voip_button_2_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "2");
}

inline void on_voip_button_3_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "3");
}

inline void on_voip_button_4_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "4");
}

inline void on_voip_button_5_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "5");
}

inline void on_voip_button_6_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "6");
}

inline void on_voip_button_7_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "7");
}

inline void on_voip_button_8_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "8");
}

inline void on_voip_button_9_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "9");
}

inline void on_voip_button_0_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "0");
}

inline void on_voip_button_star_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "*");
}

inline void on_voip_button_hash_clicked(GtkWidget *widget, gpointer data)
{
	voip_panel_append(widget, "#");
}

#endif /* VOIP__H */
