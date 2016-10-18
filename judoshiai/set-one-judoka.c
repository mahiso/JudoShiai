/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "sqlite3.h"
#include "judoshiai.h"


static gint set_category(GtkTreeIter *iter, guint index, 
                         const gchar *category, guint tatami, guint group, gint deleted)
{
    struct judoka j;

    if (find_iter_category(iter, category)) {
        /* category exists */
        return -1;
    }

    /* new category */
    gtk_tree_model_get_iter_first(current_model, iter);
    gtk_tree_store_append((GtkTreeStore *)current_model, 
                          iter, NULL);

    j.index = index ? index : current_category_index++;
    j.last = category;
    j.first = "";
    j.birthyear = group;
    j.club = "";
    j.country = "";
    j.regcategory = "";
    j.belt = tatami;
    j.weight = 0;
    j.visible = FALSE;
    j.category = "";
    j.deleted = deleted;
    j.id = "";
    j.seeding = 0;
    j.clubseeding = 0;
    j.comment = "";
    j.coachid = "";

    put_data_by_iter(&j, iter);

    return j.index;
}

gint display_one_judoka(struct judoka *j)
{

    GtkTreeIter iter, parent, child;
    struct judoka *parent_data;
    guint ret = -1;

    if (j->visible == FALSE) {
        /* category row */
        if (find_iter(&iter, j->index)) {
            /* existing category */
            put_data_by_iter(j, &iter);
            return -1;
        }

        return set_category(&parent, j->index, (gchar *)j->last, 
                            j->belt, j->birthyear, j->deleted);
    }

    if (find_iter(&iter, j->index)) {
        /* existing judoka */

        if (gtk_tree_model_iter_parent((GtkTreeModel *)current_model, &parent, &iter) == FALSE) {
            /* illegal situation */
            g_print("ILLEGAL\n");
            if (j->category)
                ret = set_category(&parent, 0, 
                                   (gchar *)j->category, 
                                   0, 0, 0);
        } 

        parent_data = get_data_by_iter(&parent);

        if (j->category && strcmp(parent_data->last, j->category)) {
            /* category has changed */
            gtk_tree_store_remove((GtkTreeStore *)current_model, &iter);
            ret = set_category(&parent, 0, (gchar *)j->category, 0, 0, 0);
            gtk_tree_store_append((GtkTreeStore *)current_model, 
                                  &iter, &parent);
            put_data_by_iter(j, &iter);
        } else {
            /* current row is ok */
            put_data_by_iter(j, &iter);
        }

        free_judoka(parent_data);
    } else {
        /* new judoka */
        ret = set_category(&parent, 0, (gchar *)j->category, 0, 0, 0);

        gtk_tree_store_append((GtkTreeStore *)current_model, 
                              &child, &parent);
        put_data_by_iter(j, &child);
    }

    return ret;
}
