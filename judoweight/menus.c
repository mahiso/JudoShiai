/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
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

#include "judoweight.h"
#include "language.h"

void start_help(GtkWidget *w, gpointer data);
extern void set_serial_dialog(GtkWidget *w, gpointer data);
extern void serial_set_device(gchar *dev);
extern void serial_set_baudrate(gint baud);
extern void serial_set_type(gint type);
extern void set_svg_file(GtkWidget *menu_item, gpointer data);

static GtkWidget *menubar, *preferences, *help, *preferencesmenu, *helpmenu;
static GtkWidget *quit, *manual;
static GtkWidget *node_ip, *my_ip, *preference_serial, *about;
static GtkWidget *light, *menu_light, *lang_menu_item;
static GtkWidget *m_password_protected, *m_automatic_send, *password,
    *m_print_label, *m_nomarg, *m_scale, *svgfile;
//static GtkTooltips *menu_tips;

static void about_judoinfo( GtkWidget *w,
			    gpointer   data )
{
    gtk_show_about_dialog (NULL,
                           "name", "JudoWeight",
                           "title", _("About JudoWeight"),
                           "copyright", "Copyright 2006-2015 Hannu Jokinen",
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

static GtkWidget *create_check_menu_item(GtkWidget *menu, void *cb, gint param)
{
    GtkWidget *w = gtk_check_menu_item_new_with_label("");
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

    //    menu_tips = gtk_tooltips_new ();
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
#if (GTKVER != 3)
    gtk_menu_item_set_right_justified(GTK_MENU_ITEM(menu_light), TRUE);
#endif
    g_signal_connect(G_OBJECT(menu_light), "button_press_event",
                     G_CALLBACK(ask_node_ip_address), (gpointer)NULL);


    /* Create the Preferences menu content. */
    node_ip = create_menu_item(preferencesmenu, ask_node_ip_address, 0);
    my_ip   = create_menu_item(preferencesmenu, show_my_ip_addresses, 0);
    preference_serial = create_menu_item(preferencesmenu, set_serial_dialog, 0);

    create_separator(preferencesmenu);
    m_password_protected = create_check_menu_item(preferencesmenu, set_password_protected, 0);
    m_automatic_send = create_check_menu_item(preferencesmenu, set_automatic_send, 0);
    password = create_menu_item(preferencesmenu, set_password_dialog, 0);

    create_separator(preferencesmenu);
    m_print_label = create_check_menu_item(preferencesmenu, set_print_label, 0);
    m_nomarg = create_check_menu_item(preferencesmenu, set_nomarg, 0);
    m_scale = create_check_menu_item(preferencesmenu, set_scale, 0);
    svgfile = create_menu_item(preferencesmenu, set_svg_file, 0);

    create_separator(preferencesmenu);
    quit    = create_menu_item(preferencesmenu, destroy, 0);

    /* Create the Help menu content. */
    manual = create_menu_item(helpmenu, start_help, 0);
    about  = create_menu_item(helpmenu, about_judoinfo, 0);

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
    gint    i, x1;

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
    str = g_key_file_get_string(keyfile, "preferences", "serialdevice", &error);
    if (!error) {
        serial_set_device(str);
        g_free(str);
    }

    error = NULL;
    x1 = g_key_file_get_integer(keyfile, "preferences", "serialbaudrate", &error);
    if (!error)
        serial_set_baudrate(x1);

    error = NULL;
    x1 = g_key_file_get_integer(keyfile, "preferences", "serialtype", &error);
    if (!error)
        serial_set_type(x1);

    weightpwcrc32 = 0; // password is temporarily 0 to enable checkbox editing
    error = NULL;
    if (g_key_file_get_boolean(keyfile, "preferences", "pwprotected", &error)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(m_password_protected));
    }

    error = NULL;
    if (g_key_file_get_boolean(keyfile, "preferences", "autosend", &error)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(m_automatic_send));
    }

    error = NULL;
    if (g_key_file_get_boolean(keyfile, "preferences", "printlabel", &error)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(m_print_label));
    }

    error = NULL;
    if (g_key_file_get_boolean(keyfile, "preferences", "nomarg", &error)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(m_nomarg));
    }

    error = NULL;
    if (g_key_file_get_boolean(keyfile, "preferences", "scale", &error)) {
        gtk_menu_item_activate(GTK_MENU_ITEM(m_scale));
    }

    error = NULL;
    x1 = g_key_file_get_integer(keyfile, "preferences", "password", &error);
    if (!error)
        weightpwcrc32 = x1;

    error = NULL;
    str = g_key_file_get_string(keyfile, "preferences", "svgfile", &error);
    if (!error) {
        svg_file = str;
        read_svg_file();
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
    change_menu_label(preference_serial, _("Scale Serial Interface..."));
    change_menu_label(m_password_protected, _("No manual edit"));
    change_menu_label(m_automatic_send, _("Automatic send"));
    change_menu_label(password,     _("Password"));
    change_menu_label(m_print_label, _("Print label"));
    change_menu_label(m_nomarg,      _("No margins"));
    change_menu_label(m_scale,       _("Fit page"));
    change_menu_label(svgfile,       _("SVG Templates"));

    change_menu_label(manual,       _("Manual"));
    change_menu_label(about,        _("About"));

    change_language_1();

    g_key_file_set_integer(keyfile, "preferences", "language", language);

    return TRUE;
}
