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

guint french_size[NUM_FRENCH] = {8, 16, 32, 64, 128};
guint estim_num_matches[NUM_COMPETITORS+1] = {
    0, 0, 1, 3, 6, 10, 9, 12,                                         /* 0 - 7 */
    11, 12, 14, 16, 18, 20, 21, 22,                                   /* 8 - 15 */
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 34, 36, 38, 40, 41, 42,   /* 16 - 31 */
    43, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,   /* 32 - 47 */
    60, 62, 63, 65, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,   /* 48 - 63 */
    79, 83, 84, 85, 86, 87, 88, 89, 80, 91, 92, 93, 94, 95, 96, 97,   /* 64 - 79*/
    98, 99, 100,101,102,103,104,105,106,107,108,109,110,111,112,113,  /* 80 - 95 */
    114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,130,  /* 96 - 111 */
    131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,  /* 112 - 127 */
    147 /* 128 */
};//XXXXXTODO


static gboolean for_each_row(GtkTreeModel *model,
                             GtkTreePath  *path,
                             GtkTreeIter  *iter,
                             GList       **rowref_list)
{
    GtkTreeRowReference  *rowref;
    rowref = gtk_tree_row_reference_new(GTK_TREE_MODEL(current_model), path);
    *rowref_list = g_list_append(*rowref_list, rowref);

    return FALSE;
}

void create_categories(GtkWidget *w, gpointer data)
{
    GList *rr_list = NULL;    /* list of GtkTreeRowReferences to remove */
    GList *node;
#if 0
    if (db_matches_exists()) {
        g_print("Otteluita arvottu. Poista arvonnat ensin.\n");
        show_message("Otteluita arvottu. Poista arvonnat ensin.");
        return;
    }
#endif
    gint num_comp, num_weighted;
    db_read_competitor_statistics(&num_comp, &num_weighted);
    gint cnt = 0;

    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), wait_cursor);

    gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                           (GtkTreeModelForeachFunc) for_each_row,
                           &rr_list);

    for ( node = rr_list;  node != NULL;  node = node->next ) {
        GtkTreePath *path;

        path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);

        if (path) {
            GtkTreeIter iter;

            cnt++;
            if (num_comp > 40 && (cnt % 10) == 0)
                progress_show(1.0*cnt/num_comp, "");

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(current_model), &iter, path)) {
                struct judoka *j = get_data_by_iter(&iter);
                struct judoka e;
                gint r;

                if (j && j->visible && j->category && j->category[0] == '?'&& j->category[1] == 0) {
                    g_free((void *)j->category);
                    j->category = NULL;

                    if (j->regcategory == NULL || j->regcategory[0] == 0) {
                        gint gender = 0;

                        if (j->deleted & GENDER_MALE)
                            gender = IS_MALE;
                        else if (j->deleted & GENDER_FEMALE)
                            gender = IS_FEMALE;
                        else
                            gender = find_gender(j->first);

                        j->category = find_correct_category(current_year - j->birthyear, 
                                                            j->weight, 
                                                            gender, 
                                                            NULL, TRUE);
                    } else {
                        j->category = find_correct_category(0, j->weight, 0, j->regcategory, FALSE);
                    }

                    if (j->category == NULL || j->category[0] == 0) {
                        if (j->regcategory && j->regcategory[0])
                            j->category = g_strdup(j->regcategory);
                        else 
                            j->category = g_strdup("?");
                    }

                    if (avl_get_category_status_by_name(j->category) & REAL_MATCH_EXISTS) {
                        SHOW_MESSAGE("%s %s %s -> %s.",
                                     _("Cannot move"),
                                     j->first, j->last, j->category);
                        g_free((void *)j->category);
                        j->category = g_strdup("?");
                    }

                    db_update_judoka(j->index, j);

                    r = display_one_judoka(j);
                    if (r >= 0) {
                        /* new category */
                        e.index = r;
                        e.last = j->category;
                        e.belt = 0;
                        e.deleted = 0;
                        e.birthyear = 0;
                        db_add_category(e.index, &e);
                    }
                }
                free_judoka(j);
            }

            /* FIXME/CHECK: Do we need to free the path here? */
        }
    }

    g_list_foreach(rr_list, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free(rr_list);        

#if (GTKVER == 3)
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), NULL);
#else
    gdk_window_set_cursor(GTK_WIDGET(main_window)->window, NULL);
#endif
    progress_show(0.0, "");
}

void view_popup_menu_draw_category(GtkWidget *menuitem, gpointer userdata)
{
    gint n, num_selected = 0;
    guint index = ptr_to_gint(userdata);
    GtkTreeIter iter;
    gboolean ok;
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(current_view));

    ok = gtk_tree_model_get_iter_first(current_model, &iter);
    while (ok) {
        if (gtk_tree_selection_iter_is_selected(selection, &iter)) {
            gint index2;

            gtk_tree_model_get(current_model, &iter,
                               COL_INDEX, &index2,
                               -1);
			
            num_selected++;
            n = gtk_tree_model_iter_n_children(current_model, &iter);
            if (n >= 1 && n <= NUM_COMPETITORS)
                draw_one_category(&iter, n);
            update_matches(index2, (struct compsys){0, 0, 0,0}, 0);
        }
        ok = gtk_tree_model_iter_next(current_model, &iter);
    }

    if (num_selected)
        return;
        
    if (find_iter(&iter, index) == FALSE)
        return;

    n = gtk_tree_model_iter_n_children(current_model, &iter);
    if (n >= 1 && n <= NUM_COMPETITORS)
        draw_one_category(&iter, n);

    //update_matches(index, (struct compsys){0, 0, 0,0}, 0);
    //matches_refresh();

    SYS_LOG_INFO(_("Category drawn"));
    //refresh_window();
}

void view_popup_menu_draw_category_manually(GtkWidget *menuitem, gpointer userdata)
{
    gint n;
    guint index = (ptr_to_gint(userdata))&0x00ffffff;
    GtkTreeIter iter;
        
    if (find_iter(&iter, index) == FALSE)
        return;

    n = gtk_tree_model_iter_n_children(current_model, &iter);
    if (n >= 2 && n <= NUM_COMPETITORS) {
        if ((ptr_to_gint(userdata))&0x01000000)
            edit_drawing(&iter, n);
        else
            draw_one_category_manually(&iter, n);
    }
#if 0
    matches_refresh();

    SYS_LOG_INFO("Sarja arvottu");
#endif
}

void locate_to_tatamis(GtkWidget *w, gpointer data)
{
    gint n = ptr_to_gint(data);
    GtkTreeIter iter;
    gboolean ok;
    gint t = 1, i = 0;
    gint match_cnt[NUM_TATAMIS] = {0};
    gint group_cnt[NUM_TATAMIS] = {0};
    gint group_num[NUM_TATAMIS] = {1};

    gint num_cats = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(current_model), NULL);
    gint cnt = 0;

    for (i = 0; i < NUM_TATAMIS; i++)
        group_num[i] = 1;

#if (GTKVER == 3)
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), wait_cursor);
#else
    gdk_window_set_cursor(GTK_WIDGET(main_window)->window, wait_cursor);
#endif
    ok = gtk_tree_model_get_iter_first(current_model, &iter);
    while (ok) {
        struct judoka *j;
        gint index;

        cnt++;
        if (num_cats > 4)
            progress_show(1.0*cnt/num_cats, "");

        gtk_tree_model_get(current_model, &iter,
                           COL_INDEX, &index,
                           -1);
		
        gint num_competitors = gtk_tree_model_iter_n_children(current_model, &iter);
        gint num_matches = estim_num_matches[num_competitors <= NUM_COMPETITORS ? num_competitors : NUM_COMPETITORS];

        struct category_data *catdata = avl_get_category(index);
        if (catdata && catdata->match_count)
            num_matches = catdata->match_count - catdata->matched_matches_count;

        j = get_data(index);
        if (j == NULL)
            goto cont;

        if (j->belt == 0) {
            gint mincnt = 10000;
            t = 1;
            for (i = 0; i < n; i++) {
                if (match_cnt[i] < mincnt) {
                    t = i+1;
                    mincnt = match_cnt[i];
                }
            }
			
            j->belt = t;
            j->birthyear = group_num[t-1];

        } else 
            t = j->belt;

        match_cnt[t-1] += num_matches;

        if (++group_cnt[t-1] >= 3) {
            group_cnt[t-1] = 0;
            group_num[t-1]++;
        }

        db_update_category(j->index, j);
        display_one_judoka(j);
        free_judoka(j);

    cont:
        ok = gtk_tree_model_iter_next(current_model, &iter);
    }

    matches_refresh();

    progress_show(0.0, "");

#if (GTKVER == 3)
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), NULL);
#else
    gdk_window_set_cursor(GTK_WIDGET(main_window)->window, NULL);
#endif
    SYS_LOG_INFO(_("All the categories located to the tatamis"));
}

void update_category_status_info(gint category)
{
    db_category_match_status(category);
#if 0
    GtkTreeIter iter;
    gint weight;

    weight = db_category_match_status(category);
    if (find_iter(&iter, category) == FALSE) 
        return;


    gtk_tree_store_set((GtkTreeStore *)current_model, 
                       &iter,
                       COL_WEIGHT, weight,
                       -1);
    if (!categories_tree)
        return;

    struct category_data data, *data1;
    data.index = category;
    if (avl_get_by_key(categories_tree, &data, (void *)&data1) == 0) {
        data1->match_status = weight;
    } /***else
          g_print("Error %s %d\n", __FUNCTION__, __LINE__);***/
#endif
}

void update_category_status_info_all(void)
{
    GtkTreeIter iter;
    gboolean ok;
    gint weight, index, visible;

    ok = gtk_tree_model_get_iter_first(current_model, &iter);
    while (ok) {
        gtk_tree_model_get(current_model, &iter,
                           COL_INDEX, &index,
                           COL_VISIBLE, &visible,
                           -1);

        if (visible == 0) {
            weight = db_category_match_status(index);
#if 0			
            gtk_tree_store_set(GTK_TREE_STORE(current_model), 
                               &iter,
                               COL_WEIGHT, weight,
                               -1);
#endif
            struct category_data data, *data1;
            data.index = index;
            if (avl_get_by_key(categories_tree, &data, (void *)&data1) == 0) {
                data1->match_status = weight;
            } else
                g_print("Error %s %d (%d)\n", __FUNCTION__, __LINE__, index);

            ok = gtk_tree_model_iter_next(current_model, &iter);
        } else
            g_print("ERROR: %s:%d\n", __FUNCTION__, __LINE__);

    }

    refresh_window();
}

struct compsys get_cat_system(gint index)
{
    struct category_data *catdata = avl_get_category(index);
    if (catdata)
        return catdata->system;

    return db_get_system(index & MATCH_CATEGORY_MASK);
}

#define COMPETITORS_MASK   0x0000ffff
#define SYSTEM_MASK        0x000f0000
#define SYSTEM_MASK_SHIFT  16
#define SYSTEM_TABLE_MASK  0x00f00000
#define SYSTEM_TABLE_SHIFT 20
#define SYSTEM_WISH_MASK   0x0f000000
#define SYSTEM_WISH_SHIFT  24

gint compress_system(struct compsys d)
{
    return ((d.system << SYSTEM_MASK_SHIFT) |
            d.numcomp |
            (d.table << SYSTEM_TABLE_SHIFT) |
            (d.wishsys << SYSTEM_WISH_SHIFT));
}

struct compsys uncompress_system(gint system)
{
    struct compsys d;
    d.system = (system & SYSTEM_MASK) >> SYSTEM_MASK_SHIFT;
    d.numcomp = system & COMPETITORS_MASK;
    d.table = (system & SYSTEM_TABLE_MASK) >> SYSTEM_TABLE_SHIFT;
    d.wishsys = (system & SYSTEM_WISH_MASK) >> SYSTEM_WISH_SHIFT;
    return d;
}
