/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h> 

#include "judoshiai.h"

gint webpwcrc32;

void set_webpassword_dialog(GtkWidget *w, gpointer data )
{
    GtkWidget *dialog, *label, *password, *hbox;

    dialog = gtk_dialog_new_with_buttons (_("Password"),
                                          NULL,
                                          GTK_DIALOG_MODAL,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          NULL);

    label = gtk_label_new(_("Password:"));
    password = gtk_entry_new();
#if (GTKVER == 3)
    hbox = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox), label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), password, 1, 0, 1, 1);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox, FALSE, FALSE, 0);
#else
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), label);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), password);

    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox);
#endif
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
        const gchar *pw = gtk_entry_get_text(GTK_ENTRY(password));
        webpwcrc32 = pwcrc32((guchar *)pw, strlen(pw));
        g_key_file_set_integer(keyfile, "preferences", "webpassword", webpwcrc32);
    }

    gtk_widget_destroy(dialog);
}
