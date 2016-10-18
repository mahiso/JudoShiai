/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h> 

#include "judoshiai.h"

extern void view_on_row_activated (GtkTreeView        *treeview,
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  *col,
                                   gpointer            userdata);


#define NUM_RESULTS 10

struct find_data {
    GtkWidget *name, *dialog;
    GtkWidget *results[NUM_RESULTS];
    gint       index[NUM_RESULTS];
    GtkTreeIter iter[NUM_RESULTS];
    gboolean   valid[NUM_RESULTS];
    void     (*callback)(gint, gpointer);
    void      *args;
} findstruct;

static void search_end(GtkWidget *widget, 
		       GdkEvent *event,
		       gpointer data)
{
    g_free(data);
    gtk_widget_destroy(widget);
}

static gboolean button_pressed(GtkWidget *button, 
			       gpointer userdata)
{
    struct find_data *data = userdata;
    gint i;

    for (i = 0; i < NUM_RESULTS && data->valid[i]; i++) {
        if (data->results[i] == button) {
            if (data->callback) {
                data->callback(data->index[i], data->args);
                return TRUE;
            } else {
                GtkTreeIter iter;
                if (find_iter(&iter, data->index[i])) {
                    GtkTreePath *path = gtk_tree_model_get_path(current_model, &iter); 
                    view_on_row_activated((GtkTreeView *)current_view, path, NULL, NULL);
                    gtk_tree_path_free(path);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

static void lookup(GtkWidget *w, gpointer arg) 
{
    GtkTreeIter iter_cat, iter_j;
    gboolean ok_cat, ok_j;
    struct find_data *data = arg;
    gint n = 0, i;
    gchar buf[80];

    gchar *name = g_utf8_casefold(gtk_entry_get_text(GTK_ENTRY(data->name)), -1);

    for (i = 0; i < NUM_RESULTS; i++) {
        gtk_button_set_label(GTK_BUTTON(data->results[i]), "");
        data->valid[i] = FALSE;
    }

    ok_cat = gtk_tree_model_get_iter_first(current_model, &iter_cat);
    while (ok_cat) {
        ok_j = gtk_tree_model_iter_children(current_model, &iter_j, &iter_cat);
        while (ok_j) {
            gint index;
            gchar *first, *last, *first_u, *last_u;
            gchar *club=NULL, *cat=NULL, *country=NULL;

            gtk_tree_model_get(current_model, &iter_j,
                               COL_INDEX, &index,
                               COL_LAST_NAME, &last,
                               COL_FIRST_NAME, &first,
                               COL_CLUB, &club, 
                               COL_COUNTRY, &country, 
                               COL_CATEGORY, &cat,
                               -1);

            first_u = g_utf8_casefold(first, -1);
            last_u = g_utf8_casefold(last, -1);

            if ((strncmp(name, last_u, strlen(name)) == 0) || strncmp(name, first_u, strlen(name)) == 0) {
		if (country && country[0])
		    snprintf(buf, sizeof(buf), "%s, %s, %s/%s, %s", 
			     last, first, club, country, cat);
		else
		    snprintf(buf, sizeof(buf), "%s, %s, %s, %s", 
			     last, first, club, cat);

                gtk_button_set_label(GTK_BUTTON(data->results[n]), buf);
                data->index[n] = index;
                data->iter[n] = iter_j;
                data->valid[n] = TRUE;

                n++;
                if (n >= NUM_RESULTS)
                    goto out;
            }

            g_free(first);
            g_free(last);
            g_free(club);
            g_free(country);
            g_free(cat);
            g_free(first_u);
            g_free(last_u);
			
            ok_j = gtk_tree_model_iter_next(current_model, &iter_j);
        }

        ok_cat = gtk_tree_model_iter_next(current_model, &iter_cat);
    }
	
out:
    g_free(name);
}

void search_competitor_args(GtkWidget *w, gpointer cb, gpointer args)
{
    GtkWidget *dialog, *tmp;
    gint i;

    struct find_data *data = g_malloc(sizeof(*data));
    memset(data, 0, sizeof(*data));

    data->callback = cb;
    data->args = args;

    dialog = gtk_dialog_new_with_buttons (_("Search"),
                                          GTK_WINDOW(main_window),
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_OK,
                                          NULL);

    data->dialog = dialog;
    data->name = tmp = gtk_entry_new();
    //gtk_entry_set_max_length(GTK_ENTRY(tmp), 30);
    gtk_entry_set_text(GTK_ENTRY(tmp), "");
    g_signal_connect(G_OBJECT(tmp), "changed", G_CALLBACK(lookup), data);
#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       tmp, FALSE, FALSE, 0);
#else
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), tmp);
#endif
    for (i = 0; i < NUM_RESULTS; i++) {
        data->results[i] = tmp = gtk_button_new_with_label("");
        g_object_set(tmp, "xalign", 0.0, NULL);
#if (GTKVER == 3)
        gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                           tmp, FALSE, FALSE, 0);
#else
        gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), tmp);
#endif
        g_signal_connect(G_OBJECT(data->results[i]), 
                         "clicked",
                         G_CALLBACK(button_pressed), 
                         (gpointer)data);
    }

    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(search_end), (gpointer) data);

    gtk_widget_show_all(dialog);
}

void search_competitor(GtkWidget *w, gpointer cb)
{
    search_competitor_args(w, cb, NULL);
}
