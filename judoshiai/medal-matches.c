/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <cairo.h>
#include <cairo-pdf.h>

#include "judoshiai.h"

enum {
    MATCH_TYPE_GOLD = 0,
    MATCH_TYPE_BRONZE1,
    MATCH_TYPE_BRONZE2,
    MATCH_TYPE_POOLS_LAST,
    NUM_MATCH_TYPES
};

static struct {
    gint tatami;
    gboolean delay;
} medal_matches_types[NUM_MATCH_TYPES];

void update_medal_matches(gint category)
{
    gint j, num;
    struct category_data *catdata = avl_get_category(category);    

    for (j = 0; j < NUM_MATCH_TYPES; j++) {
        num = 0;

        if (catdata->system.system == SYSTEM_POOL ||
            catdata->system.system == SYSTEM_BEST_OF_3 ||
            catdata->system.system == SYSTEM_DPOOL2 ||
            catdata->system.system == SYSTEM_DPOOL3) {
            if (j != MATCH_TYPE_POOLS_LAST)
                continue;

            num = get_abs_matchnum_by_pos(catdata->system, 1, 1);
        } else {
            if (j < MATCH_TYPE_GOLD || j > MATCH_TYPE_BRONZE2)
                continue;

            num = get_abs_matchnum_by_pos(catdata->system,
                                          j == MATCH_TYPE_GOLD ? 1 : 3, 
                                          j == MATCH_TYPE_BRONZE1 ? 1 : 2);
        }

        if (num) {
            db_set_forced_tatami_number_delay(catdata->index, num, 
                                              medal_matches_types[j].tatami, 0, 
                                              medal_matches_types[j].delay);
            //db_set_comment(catdata->index, num, medal_matches_types[j].delay ? COMMENT_WAIT : COMMENT_EMPTY);        
        }
    } // for j
}

void move_medal_matches(GtkWidget *menuitem, gpointer userdata)
{
    GtkWidget *dialog;
#if (GTKVER == 3)
    GtkWidget *table = gtk_grid_new();
#else
    GtkWidget *table = gtk_table_new(3, 5, FALSE);
#endif
    GtkWidget *to_tatami[NUM_MATCH_TYPES], *opt_delay[NUM_MATCH_TYPES];
    gint i, j;
    gint *saved_tatami, new_tatamis[NUM_MATCH_TYPES];
    gboolean *saved_delay, new_delays[NUM_MATCH_TYPES];
    GError  *error = NULL;
    gsize tlen, dlen;

    saved_tatami = g_key_file_get_integer_list(keyfile, "preferences", "medalmatchtatamis", &tlen, &error);
    error = NULL;
    saved_delay = g_key_file_get_boolean_list(keyfile, "preferences", "medalmatchdelays", &dlen, &error);

    dialog = gtk_dialog_new_with_buttons (_("Move Medal Matches"),
                                          GTK_WINDOW(main_window),
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                          NULL);

#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Move match ")),      0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("to tatami and/or")), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_(" delay")),           2, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Final")),            0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Bronze1")),          0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Bronze2")),          0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Pool's last")),      0, 4, 1, 1);
#else
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Move match ")), 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("to tatami and/or")), 1, 2, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_(" delay")), 2, 3, 0, 1);

    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Final")), 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Bronze1")), 0, 1, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Bronze2")), 0, 1, 3, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Pool's last")), 0, 1, 4, 5);
#endif
    for (i = 0; i < NUM_MATCH_TYPES; i++) {
#if (GTKVER == 3)
        GtkWidget *tmp = gtk_combo_box_text_new();
        to_tatami[i] = tmp;
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, _("No move"));
        for (j = 0; j < NUM_TATAMIS; j++) {
            char buf[10];
            sprintf(buf, "%d", j+1);
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, buf);
        }
        gtk_grid_attach(GTK_GRID(table), tmp, 1, i+1, 1, 1);
        if (saved_tatami && tlen > i)
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), saved_tatami[i]);
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 0);

        tmp = gtk_check_button_new();
        opt_delay[i] = tmp;
        gtk_grid_attach(GTK_GRID(table), tmp, 2, i+1, 1, 1);
        if (saved_delay && dlen > i)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), saved_delay[i]);
#else
        GtkWidget *tmp = gtk_combo_box_new_text();
        to_tatami[i] = tmp;
        gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), _("No move"));
        for (j = 0; j < NUM_TATAMIS; j++) {
            char buf[10];
            sprintf(buf, "%d", j+1);
            gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), buf);
        }
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, i+1, i+2);
        if (saved_tatami && tlen > i)
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), saved_tatami[i]);
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 0);

        tmp = gtk_check_button_new();
        opt_delay[i] = tmp;
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 2, 3, i+1, i+2);
        if (saved_delay && dlen > i)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), saved_delay[i]);
#endif
    }

    gtk_widget_show_all(table);
#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       table, FALSE, FALSE, 0);
#else
    gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), table);
#endif
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        for (i = 0; i < NUM_MATCH_TYPES; i++) {
            medal_matches_types[i].tatami = new_tatamis[i] = gtk_combo_box_get_active(GTK_COMBO_BOX(to_tatami[i]));
            medal_matches_types[i].delay = new_delays[i] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_delay[i]));
        }

        g_key_file_set_integer_list(keyfile, "preferences", "medalmatchtatamis", new_tatamis, NUM_MATCH_TYPES);
        g_key_file_set_boolean_list(keyfile, "preferences", "medalmatchdelays", new_delays, NUM_MATCH_TYPES);

        for (i = 0; i <= number_of_tatamis; i++) {
            gchar text[32];
            struct category_data *catdata = category_queue[i].next;

            snprintf(text, sizeof(text), "Tatami %d", i);
            progress_show((gdouble)i/(gdouble)NUM_TATAMIS, text);

            while (catdata) {
                update_medal_matches(catdata->index);
                if (catdata->system.numcomp == 2)
                    db_force_match_number(catdata->index);
                catdata = catdata->next;
            }

            if (i)
                update_matches(0, (struct compsys){0,0,0,0}, i);
        }

        progress_show(0.0, "");
    }

    gtk_widget_destroy(dialog);
}
