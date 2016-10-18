/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#ifndef _JUDOWEIGHT_H_
#define _JUDOWEIGHT_H_

#include "comm.h"

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else /* NLS is disabled */
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain) 
#define bind_textdomain_codeset(Domain,Codeset) (Codeset) 
#endif /* ENABLE_NLS */

#define FRAME_WIDTH  500
#define FRAME_HEIGHT 300

#define JUDOGI_OK     0x20
#define JUDOGI_NOK    0x40

struct paint_data {
    cairo_t *c;
    gdouble  paper_width;
    gdouble  paper_height;
    gdouble  paper_width_mm;
    gdouble  paper_height_mm;
    struct msg_edit_competitor msg;
};

extern GtkWidget *main_window;
extern GTimer *timer;
extern gchar *installation_dir;
extern gulong my_ip_address, node_ip_addr;
extern GKeyFile *keyfile;
extern gint language;
extern GtkWidget *weight_entry;
extern gint my_address;
extern gint weightpwcrc32;
extern gboolean password_protected, automatic_send;
extern gchar *svg_file;
extern gboolean nomarg;
extern gboolean scale;

extern gboolean this_is_shiai(void);
extern void msg_to_queue(struct message *msg);
extern struct message *get_rec_msg(void);
extern void destroy( GtkWidget *widget, gpointer   data );
extern void set_preferences(void);
extern gulong host2net(gulong a);

extern GtkWidget *get_menubar_menu(GtkWidget  *window);
extern gpointer client_thread(gpointer args);
extern gpointer serial_thread(gpointer args);
extern gboolean change_language(GtkWidget *eventbox, GdkEventButton *event, void *param);
extern void ask_node_ip_address( GtkWidget *w, gpointer data);
extern void show_my_ip_addresses( GtkWidget *w, gpointer data);

extern void set_weight_entry(gint weight);
extern void set_display(struct msg_edit_competitor *msg);
extern void judoweight_log(gchar *format, ...);
extern void change_language_1(void);
extern void set_password_dialog(GtkWidget *w, gpointer data );
extern gboolean ask_password_dialog(void);
extern void set_password_protected(GtkWidget *w, gpointer data);
extern void set_automatic_send(GtkWidget *w, gpointer data);
extern void set_print_label(GtkWidget *menu_item, gpointer data);
extern void set_nomarg(GtkWidget *menu_item, gpointer data);
extern void set_scale(GtkWidget *menu_item, gpointer data);
extern void do_print(gpointer userdata);
extern void do_print_svg(struct paint_data *pd);
extern void read_svg_file(void);
extern gint paint_svg(struct paint_data *pd);

#endif
