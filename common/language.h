/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#ifndef _LANGUAGE_H_
#define _LANGUAGE_H_

enum {
    LANG_FI = 0,
    LANG_SW,
    LANG_EN,
    LANG_ES,
    LANG_EE,
    LANG_UK,
    LANG_IS,
    LANG_NO,
    LANG_PL,
    LANG_SK,
    LANG_NL,
    LANG_CS,
    LANG_DE,
    LANG_RU,
    NUM_LANGS
};

extern GtkWidget *TEST;

extern const gchar *timer_help_file_names[NUM_LANGS];

typedef gboolean (*cb_t)(GtkWidget *eventbox, GdkEventButton *event, void *param);

GtkWidget *get_language_menu(GtkWidget *window, cb_t cb);
void set_gui_language(gint l);
void start_help(GtkWidget *w, gpointer data);

#endif
