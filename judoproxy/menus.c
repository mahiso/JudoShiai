/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <gtk/gtk.h>

#if (GTKVER == 3)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#include "judoproxy.h"
#include "language.h"

void start_help(GtkWidget *w, gpointer data);


static GtkWidget *menubar, *preferences, *help, *preferencesmenu, *helpmenu;
static GtkWidget *quit, *manual;
static GtkWidget *node_ip, *my_ip, *about;
static GtkWidget *light, *menu_light;
static GtkWidget *writefile, *lang_menu_item, *advertise, *video;

gboolean show_tatami[NUM_TATAMIS];

extern void toggle_full_screen(GtkWidget *menu_item, gpointer data);
extern void toggle_small_display(GtkWidget *menu_item, gpointer data);
extern void toggle_mirror(GtkWidget *menu_item, gpointer data);
extern void toggle_whitefirst(GtkWidget *menu_item, gpointer data);
extern void toggle_redbackground(GtkWidget *menu_item, gpointer data);
extern void set_write_file(GtkWidget *menu_item, gpointer data);

static void about_judoproxy( GtkWidget *w,
                             gpointer   data )
{
    gtk_show_about_dialog (NULL,
                           "name", "JudoProxy",
                           "title", _("About JudoProxy"),
                           "copyright", "Copyright 2006-2016 Hannu Jokinen",
                           "version", SHIAI_VERSION,
                           "website", "http://sourceforge.net/projects/judoshiai/",
                           NULL);
}

static void change_menu_label(GtkWidget *item, const gchar *new_text)
{
    GtkWidget *menu_label = gtk_bin_get_child(GTK_BIN(item));
    gtk_label_set_text(GTK_LABEL(menu_label), new_text);
}

static GtkWidget *get_picture(const gchar *name)
{
    gchar *file = g_build_filename(installation_dir, "etc", name, NULL);
    GtkWidget *pic = gtk_image_new_from_file(file);
    g_free(file);
    return pic;
}

static void set_menu_item_picture(GtkImageMenuItem *menu_item, gchar *name)
{
    GtkImage *image = GTK_IMAGE(gtk_image_menu_item_get_image(GTK_IMAGE_MENU_ITEM(menu_item)));
    gchar *file = g_build_filename(installation_dir, "etc", name, NULL);
    gtk_image_set_from_file(image, file);
    g_free(file);
}

static gint light_callback(gpointer data)
{
    extern gboolean connection_ok;
    static gboolean last_ok = FALSE;
#if 0
    extern time_t traffic_last_rec_time;
    static gboolean yellow_set = FALSE;

    if (yellow_set == FALSE && connection_ok && time(NULL) > traffic_last_rec_time + 6) {
        set_menu_item_picture(GTK_IMAGE_MENU_ITEM(menu_light), "yellowlight.png");
        yellow_set = TRUE;
        return TRUE;
    } else if (yellow_set && connection_ok && time(NULL) < traffic_last_rec_time + 6) {
        yellow_set = FALSE;
        last_ok = !connection_ok;
    }
#endif

    if (connection_ok == last_ok)
        return TRUE;

    last_ok = connection_ok;

    if (connection_ok)
        set_menu_item_picture(GTK_IMAGE_MENU_ITEM(menu_light), "greenlight.png");
    else
        set_menu_item_picture(GTK_IMAGE_MENU_ITEM(menu_light), "redlight.png");

    return TRUE;
}

static GtkWidget *create_menu_item(GtkWidget *menu, void *cb, gint param)
{
    GtkWidget *w = gtk_menu_item_new_with_label("");
    gtk_menu_shell_append (GTK_MENU_SHELL(menu), w);
    g_signal_connect(G_OBJECT(w), "activate", G_CALLBACK(cb), gint_to_ptr(param));
    return w;
}

static void create_separator(GtkWidget *menu)
{
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
}

/* Returns a menubar widget made from the above menu */
GtkWidget *get_menubar_menu(GtkWidget  *window)
{
    GtkAccelGroup *group;

    group = gtk_accel_group_new ();
    menubar = gtk_menu_bar_new ();

    preferences = gtk_menu_item_new_with_label (_("Preferences"));
    help        = gtk_menu_item_new_with_label (_("Help"));
    lang_menu_item = get_language_menu(window, change_language);

    light      = get_picture("redlight.png");
    menu_light = gtk_image_menu_item_new();
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_light), light);
    gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(menu_light), TRUE);

    preferencesmenu  = gtk_menu_new ();
    helpmenu         = gtk_menu_new ();

    gtk_menu_item_set_submenu (GTK_MENU_ITEM (preferences), preferencesmenu);
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (help), helpmenu);

    gtk_menu_shell_append (GTK_MENU_SHELL (menubar), preferences);
    gtk_menu_shell_append (GTK_MENU_SHELL (menubar), help);
    gtk_menu_shell_append (GTK_MENU_SHELL (menubar), lang_menu_item);

    gtk_menu_shell_append (GTK_MENU_SHELL (menubar), menu_light);
    gtk_menu_item_set_right_justified(GTK_MENU_ITEM(menu_light), TRUE);
    g_signal_connect(G_OBJECT(menu_light), "button_press_event",
                     G_CALLBACK(ask_node_ip_address), (gpointer)NULL);


    /* Create the Preferences menu content. */
    node_ip = create_menu_item(preferencesmenu, ask_node_ip_address, 0);
    my_ip   = create_menu_item(preferencesmenu, show_my_ip_addresses, 0);
    create_separator(preferencesmenu);

    advertise = gtk_check_menu_item_new_with_label("Advertise addresses");
    gtk_menu_shell_append(GTK_MENU_SHELL(preferencesmenu), advertise);
    g_signal_connect(G_OBJECT(advertise), "activate",
		     G_CALLBACK(toggle_advertise), NULL);

    video = gtk_check_menu_item_new_with_label("Show video");
    gtk_menu_shell_append(GTK_MENU_SHELL(preferencesmenu), video);
    g_signal_connect(G_OBJECT(video), "activate",
		     G_CALLBACK(toggle_video), NULL);

    //create_separator(preferencesmenu);
    writefile = gtk_menu_item_new_with_label("");
    //gtk_menu_shell_append(GTK_MENU_SHELL(preferencesmenu), writefile);
    g_signal_connect(G_OBJECT(writefile), "activate",
                     G_CALLBACK(set_write_file), 0);

    create_separator(preferencesmenu);
    quit    = create_menu_item(preferencesmenu, destroy, 0);

    /* Create the Help menu content. */
    manual = create_menu_item(helpmenu, start_help, 0);
    about  = create_menu_item(helpmenu, about_judoproxy, 0);

    /* Attach the new accelerator group to the window. */
    gtk_widget_add_accelerator(quit, "activate", group, GDK_Q, GDK_CONTROL_MASK,
                               GTK_ACCEL_VISIBLE);
    gtk_window_add_accel_group (GTK_WINDOW (window), group);

    g_timeout_add(1000, light_callback, NULL);

    /* Finally, return the actual menu bar created by the item factory. */
    return menubar;
}

void set_preferences(void)
{
    GError *error = NULL;
    gchar  *str;
    gint    i;

    error = NULL;
    str = g_key_file_get_string(keyfile, "preferences", "nodeipaddress", &error);
    if (!error) {
        gulong a,b,c,d;
        sscanf(str, "%ld.%ld.%ld.%ld", &a, &b, &c, &d);
        node_ip_addr = host2net((a << 24) | (b << 16) | (c << 8) | d);
        g_free(str);
    }

    error = NULL;
    i = g_key_file_get_integer(keyfile, "preferences", "language", &error);
    if (!error)
        language = i;
    else
        language = LANG_FI;

    error = NULL;
    if (g_key_file_get_boolean(keyfile, "preferences", "advertise", &error) || error) {
        gtk_menu_item_activate(GTK_MENU_ITEM(advertise));
    }
}

gboolean change_language(GtkWidget *eventbox, GdkEventButton *event, void *param)
{
    language = ptr_to_gint(param);
    set_gui_language(language);

    change_menu_label(preferences,  _("Preferences"));
    change_menu_label(help,         _("Help"));

    change_menu_label(quit,         _("Quit"));

    change_menu_label(node_ip,      _("Communication node"));
    change_menu_label(my_ip,        _("Own IP addresses"));

    change_menu_label(advertise,    "Advertise addresses");
    change_menu_label(video,        "Show video");

    change_menu_label(manual,       _("Manual"));
    change_menu_label(about,        _("About"));

    g_key_file_set_integer(keyfile, "preferences", "language", language);

    return TRUE;
}
