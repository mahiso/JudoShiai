/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "judotimer.h"

#include <ao/ao.h>
#include <mpg123.h>

#define BITS 8

G_LOCK_DEFINE(sound);

static mpg123_handle *mh = NULL;
static ao_device *dev = NULL;
static int driver;
static char *buffer;
static size_t buffer_size;
static gboolean play = FALSE;
static gchar *sound_file = NULL;

static void init_dev(void)
{
    int channels, encoding;
    long rate;
    ao_sample_format format;

    if (dev) ao_close(dev);

    mpg123_open(mh, sound_file);
    mpg123_getformat(mh, &rate, &channels, &encoding);

    /* set the output format and open the output device */
    format.bits = mpg123_encsize(encoding) * BITS;
    format.rate = rate;
    format.channels = channels;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    dev = ao_open_live(driver, &format, NULL);
}

void open_sound(void)
{
    int err;

    ao_initialize();
    driver = ao_default_driver_id();
    mpg123_init();
    mh = mpg123_new(NULL, &err);
    buffer_size = mpg123_outblock(mh);
    buffer = malloc(buffer_size * sizeof(char));

    GError *error = NULL;
    gchar  *str;
    if ((str = g_key_file_get_string(keyfile, "preferences", "soundfile", &error))) {
        if (str[0]) {
            sound_file = str;
            init_dev();
        } else
            g_free(str);
    }
}

void close_sound(void)
{
    free(buffer);
    ao_close(dev);
    mpg123_close(mh);
    mpg123_delete(mh);
    mpg123_exit();
    ao_shutdown();
}

static void start_sound(gpointer ok)

{
    size_t done;

    G_LOCK(sound);

    if (mh == NULL || dev == NULL || sound_file == NULL) {
        G_UNLOCK(sound);
        return;
    }

    mpg123_seek(mh, 0, SEEK_SET);

    while (play && (*((gboolean *)ok)) &&
           mpg123_read(mh, (unsigned char *)buffer, buffer_size, &done) == MPG123_OK)
        ao_play(dev, buffer, done);

    G_UNLOCK(sound);
}

void play_sound(void)
{
    play = TRUE;
}

void select_sound(GtkWidget *menu_item, gpointer data)
{
    GtkWidget *dialog;
    GtkFileFilter *filter, *filter_all;

    play = FALSE;

    dialog = gtk_file_chooser_dialog_new(_("Choose a file"),
                                         NULL,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         "Stop Sound", 1000,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);

    filter = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter, "*.[mM][pP][1-3]");
    gtk_file_filter_add_pattern(filter, "*.[wW][aA][vV]");
    gtk_file_filter_add_pattern(filter, "*.[oO][gG][gG]");
    gtk_file_filter_set_name(filter, _("Sound Files"));
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    filter_all = gtk_file_filter_new();
    gtk_file_filter_add_pattern(filter_all, "*");
    gtk_file_filter_set_name(filter_all, _("All Files"));
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter_all);

    gchar *dir = g_build_filename(installation_dir, "etc", NULL);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);
    g_free(dir);

    gint r = gtk_dialog_run(GTK_DIALOG(dialog));

    if (play) { // can start during dialog
        gtk_widget_destroy(dialog);
        return;
    }

    if (r == 1000) { // stop sound
        G_LOCK(sound);

        g_free(sound_file);
        sound_file = NULL;

        if (dev) ao_close(dev);
        dev = NULL;

        g_key_file_set_string(keyfile, "preferences", "soundfile", "");
        gtk_widget_destroy(dialog);

        G_UNLOCK(sound);
        return;
    }

    if (r != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        return;
    }
               
    G_LOCK(sound);

    g_free(sound_file);
    sound_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    g_key_file_set_string(keyfile, "preferences", "soundfile", sound_file);

    init_dev();

    G_UNLOCK(sound);

    gtk_widget_destroy(dialog);
}

gpointer sound_thread(gpointer args)
{
    open_sound();

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        if (play) {
            start_sound(args);
            play = FALSE;
        } else {
            g_usleep(100000);
        }
    }

    close_sound();
    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}
