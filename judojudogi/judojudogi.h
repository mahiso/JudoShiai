/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#ifndef _JUDOJUDOGI_H_
#define _JUDOJUDOGI_H_

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

#define FRAME_WIDTH  600
#define FRAME_HEIGHT 400

#define BLUE  1
#define WHITE 2
#define CMASK 3
#define GIVE_POINTS 4

#define OSAEKOMI_DSP_NO      0
#define OSAEKOMI_DSP_YES     1
#define OSAEKOMI_DSP_BLUE    2
#define OSAEKOMI_DSP_WHITE   3
#define OSAEKOMI_DSP_UNKNOWN 4

#define CLEAR_SELECTION   0
#define HANTEI_BLUE       1
#define HANTEI_WHITE      2
#define HANSOKUMAKE_BLUE  3
#define HANSOKUMAKE_WHITE 4

#define NUM_TATAMIS  10
#define NUM_LINES   10

#define NORMAL_DISPLAY     0
#define SMALL_DISPLAY      1
#define HORIZONTAL_DISPLAY 2

#define JUDOGI_OK     0x20
#define JUDOGI_NOK    0x40

struct name_data {
    gint   index;
    gchar *last;
    gchar *first;
    gchar *club;
};

struct match {
    gint   category;
    gint   number;
    gint   blue;
    gint   white;
    gint   flags;
    time_t rest_end;
};

extern GTimer *timer;
extern gchar *installation_dir;
extern gulong my_ip_address, node_ip_addr;
extern GKeyFile *keyfile;
extern gboolean show_tatami[NUM_TATAMIS];
extern struct match match_list[NUM_TATAMIS][NUM_LINES];
extern gint language;

extern gboolean this_is_shiai(void);
extern void msg_to_queue(struct message *msg);
extern struct message *get_rec_msg(void);
extern void destroy( GtkWidget *widget, gpointer   data );
extern void set_preferences(void);
extern gulong host2net(gulong a);

extern GtkWidget *get_menubar_menu(GtkWidget  *window);
extern gpointer client_thread(gpointer args);
extern gboolean change_language(GtkWidget *eventbox, GdkEventButton *event, void *param);
extern void ask_node_ip_address( GtkWidget *w, gpointer data);
extern void show_my_ip_addresses( GtkWidget *w, gpointer data);
extern gint number_of_tatamis(void);
extern void init_trees(void);
extern struct name_data *avl_get_data(gint index);
extern void avl_set_data(gint index, gchar *first, gchar *last, gchar *club);
extern void init_trees(void);
extern void refresh_window(void);
extern gint timeout_ask_for_data(gpointer data);
extern void write_matches(void);
extern void set_display(struct msg_edit_competitor *msg);


#endif
