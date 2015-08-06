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

#include "comm.h"
#include "language.h"

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

extern gchar *installation_dir;
extern gint language;

struct button_data {
    GtkWidget *dialog;
    cb_t cb;
    GtkWidget *buttons[NUM_LANGS];
};

static GtkWidget *lang_menu, *flags[NUM_LANGS];

static const gchar *flags_files[NUM_LANGS] = {
    "finland.png", "sweden.png", "uk.png", "spain.png", "estonia.png", "ukraine.png", "iceland.png", 
    "norway.png", "poland.png", "slovakia.png", "netherlands.png", "czech.png", "germany.png",
    "russia.png"
};

static const gchar *lang_names[NUM_LANGS] = {
    "fi", "sv", "en", "es", "et", "uk", "is", "nb", "pl", "sk", "nl", "cs", "de", "ru"
};

static const gchar *lang_names_in_own_language[NUM_LANGS] = {
    "Suomi", "Svensk", "English", "Español", "Eesti", "Українська", "Íslenska", "Norsk", "Polski",
    "Slovenčina", "Nederlands", "Čeština", "Deutsch", "Русский язык"
};

static GtkWidget *get_picture(const gchar *name)
{
    gchar *file = g_build_filename(installation_dir, "etc", name, NULL);
    GtkWidget *pic = gtk_image_new_from_file(file);
    g_free(file);
    return pic;
}

void set_gui_language(gint l)
{
    static gchar envbuf[32]; // this must be static for the putenv() function

    sprintf(envbuf, "LANGUAGE=%s", lang_names[l]);
    putenv(envbuf);
    setlocale(LC_ALL, lang_names[l]);

    gchar *dirname = g_build_filename(installation_dir, "share", "locale", NULL);
    bindtextdomain ("judoshiai", dirname);
    g_free(dirname);
    bind_textdomain_codeset ("judoshiai", "UTF-8");
    textdomain ("judoshiai");

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(lang_menu), flags[l]);
}

void lang_dialog_cb(GtkWidget *w, void *param)
{
    struct button_data *data = param;
    gint i;

    for (i = 0; i < NUM_LANGS; i++) {
        if (w == data->buttons[i]) {
            data->cb(NULL, NULL, gint_to_ptr(i));
            //gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(lang_menu), flags[i]);        
            gtk_widget_destroy(data->dialog);
            g_free(data);
            return;
        }
    }
    g_print("ERROR %s\n", __FUNCTION__);
    g_free(data);
    gtk_widget_destroy(data->dialog);
}

static gboolean lang_dialog(GtkWidget *eventbox, GdkEventButton *event, void *param)
{
    struct button_data *data = g_malloc(sizeof(*data));
    GtkWidget *dialog;
#if (GTKVER == 3)
    GtkWidget *table = gtk_grid_new();
#else
    GtkWidget *table = gtk_table_new((NUM_LANGS+1)/2, 4, TRUE);
#endif
    gint i;

    //gtk_table_set_row_spacings(table, 5);
    //gtk_table_set_col_spacings(table, 5);

    dialog = gtk_dialog_new();

    data->cb = param;
    data->dialog = dialog;

    for (i = 0; i < NUM_LANGS; i++) {
        gint col = (i&1) ? 2 : 0;
        gint row = i/2;
        GtkWidget *button = data->buttons[i] = gtk_button_new_with_label(lang_names_in_own_language[i]);
        gtk_button_set_image(GTK_BUTTON(button), get_picture(flags_files[i])/*flags[i]*/);
        gtk_button_set_image_position(GTK_BUTTON(button), GTK_POS_LEFT);
        //gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5); XXXXXX
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), button, col, row, 1, 1);
        g_signal_connect(G_OBJECT(button), "clicked",
                         G_CALLBACK(lang_dialog_cb), data);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), button, 
                                  col, col+1, row, row+1);
        gtk_signal_connect(GTK_OBJECT(button), "clicked",
                           GTK_SIGNAL_FUNC(lang_dialog_cb), data);
#endif
    }

#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
 		      table, TRUE, TRUE, 0);
#else
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), table);
#endif
    gtk_widget_show_all(dialog);

    return TRUE;
}

GtkWidget *get_language_menu(GtkWidget *window, cb_t cb)
{
    //GtkAccelGroup *group;
    gint i;

    //group = gtk_accel_group_new ();
    lang_menu = gtk_image_menu_item_new();

    for (i = 0; i < NUM_LANGS; i++) {
        flags[i] = get_picture(flags_files[i]);
        g_object_ref(flags[i]);
    }

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(lang_menu), flags[language]);        
    gtk_image_menu_item_set_always_show_image(GTK_IMAGE_MENU_ITEM(lang_menu), TRUE);

    g_signal_connect(G_OBJECT(lang_menu), "button_press_event",
      G_CALLBACK(lang_dialog), cb);

    set_gui_language(language);

    g_object_set(gtk_settings_get_default(), "gtk-button-images", TRUE, NULL);

    return lang_menu;
}

const gchar *timer_help_file_names[NUM_LANGS] = {
    "judotimer-fi.pdf", "judotimer-en.pdf", "judotimer-en.pdf", "judotimer-es.pdf", "judotimer-en.pdf",
    "judotimer-uk.pdf", "judotimer-en.pdf", "judotimer-nb.pdf", "judotimer-en.pdf", "judotimer-en.pdf",
    "judotimer-en.pdf", "judotimer-en.pdf", "judotimer-en.pdf", "judotimer-en.pdf"
};


static const gchar *help_file_names[NUM_LANGS] = {
    "judoshiai-en.pdf", // fi
    "judoshiai-en.pdf", // se
    "judoshiai-en.pdf", // en
    "judoshiai-es.pdf", // es
    "judoshiai-en.pdf", // et
    "judoshiai-uk.pdf", // uk
    "judoshiai-en.pdf", // is
    "judoshiai-en.pdf", // no
    "judoshiai-en.pdf", // pl
    "judoshiai-en.pdf", // sk
    "judoshiai-nl.pdf", // nl
    "judoshiai-en.pdf", // cs
    "judoshiai-en.pdf", // de
    "judoshiai-en.pdf"  // ru
};

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"
#include "shellapi.h"
#endif /* WIN32 */

void start_help(GtkWidget *w, gpointer data)
{
    gchar *docfile = g_build_filename(installation_dir, "doc", 
                                      help_file_names[language], NULL);
#ifdef WIN32
    ShellExecute(NULL, TEXT("open"), docfile, NULL, ".\\", SW_SHOWMAXIMIZED);
#else /* ! WIN32 */
    gchar *cmd;
    cmd = g_strdup_printf("if which acroread ; then PDFR=acroread ; "
                          "elif which evince ; then PDFR=evince ; "
                          "else PDFR=xpdf ; fi ; $PDFR \"%s\" &", docfile);
    system(cmd);
    g_free(cmd);
#endif /* ! WIN32 */
    g_free(docfile);
}
