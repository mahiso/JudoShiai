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


//static guint destination;
guint selected_judokas[TOTAL_NUM_COMPETITORS];
guint num_selected_judokas;
static gchar *dest_category = NULL;
static gint   dest_category_ix = 0;

static gboolean for_each_row_selected(GtkTreeModel *model,
                                      GtkTreePath *path,
                                      GtkTreeIter *iter,
                                      gpointer data)
{
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(current_view));

    if (gtk_tree_selection_iter_is_selected(selection, iter)) {
        guint index;
        gboolean visible;

        gtk_tree_model_get(model, iter,
                           COL_INDEX, &index,
                           COL_VISIBLE, &visible,
                           -1);

        if (visible) {
            if (num_selected_judokas < TOTAL_NUM_COMPETITORS - 1)
                selected_judokas[num_selected_judokas++] = index;
        } else {
            GtkTreeIter tmp_iter;
            gboolean ok;

            ok = gtk_tree_model_iter_children(model, &tmp_iter, iter);
            while (ok) {
                guint index2;

                gtk_tree_model_get(model, &tmp_iter,
                                   COL_INDEX, &index2,
                                   -1);

                if (num_selected_judokas < TOTAL_NUM_COMPETITORS - 1)
                    selected_judokas[num_selected_judokas++] = index2;

                ok = gtk_tree_model_iter_next(model, &tmp_iter);
            }
        }
    }

    return FALSE;
}

static void view_popup_menu_move_judoka(GtkWidget *menuitem, gpointer userdata)
{
    int i;

    GtkTreeView *treeview = GTK_TREE_VIEW(current_view);
    GtkTreeModel *model = current_model;
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    if (!dest_category)
        return;
#if 0
    if (db_category_match_status(dest_category_ix) & REAL_MATCH_EXISTS) {
        SHOW_MESSAGE("%s: %s.", dest_category, _("Remove drawing first"));
        return;
    }
#endif
    //destination = (guint)userdata;
    num_selected_judokas = 0;
    gtk_tree_model_foreach(model, for_each_row_selected, userdata);
    gtk_tree_selection_unselect_all(selection);

    for (i = 0; i < num_selected_judokas; i++) {
        struct judoka *j = get_data(selected_judokas[i]);
        if (!j)
            continue;

        if (db_competitor_match_status(j->index) & MATCH_EXISTS) {
            SHOW_MESSAGE("%s %s: %s.", 
                         j->first, j->last, _("Remove drawing first"));
        } else {
            if (j->category)
                g_free((void *)j->category);
            j->category = strdup(dest_category);
            display_one_judoka(j);
            db_update_judoka(j->index, j);
        }
        free_judoka(j);
    }
}

static void view_popup_menu_copy_judoka(GtkWidget *menuitem, gpointer userdata)
{
    int i;

    GtkTreeView *treeview = GTK_TREE_VIEW(current_view);
    GtkTreeModel *model = current_model;
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    num_selected_judokas = 0;
    gtk_tree_model_foreach(model, for_each_row_selected, userdata);
    gtk_tree_selection_unselect_all(selection);

    for (i = 0; i < num_selected_judokas; i++) {
        struct judoka *j = get_data(selected_judokas[i]);
        if (!j)
            continue;

        if (j->category)
            g_free((void *)j->category);

        j->index = current_index++;
        j->category = g_strdup("?");
        db_add_judoka(j->index, j);
        display_one_judoka(j);

        free_judoka(j);
    }
}

void view_popup_menu_print_cards(GtkWidget *menuitem, gpointer userdata)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(current_view);
    GtkTreeModel *model = current_model;
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    num_selected_judokas = 0;
    gtk_tree_model_foreach(model, for_each_row_selected, userdata);
    gtk_tree_selection_unselect_all(selection);

    print_accreditation_cards(FALSE);
}

extern void print_weight_notes_to_default_printer(GtkWidget *menuitem, gpointer userdata);

void view_popup_menu_print_cards_to_default(GtkWidget *menuitem, gpointer userdata)
{
    GtkTreeView *treeview = GTK_TREE_VIEW(current_view);
    GtkTreeModel *model = current_model;
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    num_selected_judokas = 0;
    gtk_tree_model_foreach(model, for_each_row_selected, userdata);
    gtk_tree_selection_unselect_all(selection);

    print_weight_notes_to_default_printer(NULL, NULL);
}

static void view_popup_menu_change_category(GtkWidget *menuitem, gpointer userdata)
{
    struct judoka *j = get_data(ptr_to_gint(userdata));
    if (!j)
        return;

    if (dest_category)
        g_free(dest_category);

    dest_category = strdup(j->last);
    dest_category_ix = j->index;
    free_judoka(j);

    view_popup_menu_move_judoka(NULL, userdata);
}

static void view_popup_menu_remove_draw(GtkWidget *menuitem, gpointer userdata)
{
    if (db_category_match_status(ptr_to_gint(userdata)) & MATCH_MATCHED) {
        SHOW_MESSAGE(_("Matches matched. Clear the results first."));
        return;
    }

    struct compsys cs;
    cs = get_cat_system(ptr_to_gint(userdata));
    cs.system = cs.numcomp = cs.table = 0; // leave wishsys as is

    db_set_system(ptr_to_gint(userdata), cs);
    db_remove_matches(ptr_to_gint(userdata));
    update_category_status_info(ptr_to_gint(userdata));
    matches_refresh();
    refresh_window();
}

static void change_display(GtkWidget *menuitem, gpointer userdata)
{
    if (userdata)
        gtk_tree_view_collapse_all(GTK_TREE_VIEW(current_view));
    else
        gtk_tree_view_expand_all(GTK_TREE_VIEW(current_view));
}

#if 0
static void do_nothing(GtkWidget *menuitem, gpointer userdata)
{

}
#endif

extern struct cat_def category_definitions[];
extern gint find_age_index_by_age(gint age, gint gender);

static void create_new_category(GtkWidget *menuitem, gpointer userdata)
{
    int i;
    gint age = 0, weight = 0;
    gboolean male = FALSE, female = FALSE;
    gchar cbuf[20];

    GtkTreeView *treeview = GTK_TREE_VIEW(current_view);
    GtkTreeModel *model = current_model;
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    num_selected_judokas = 0;
    gtk_tree_model_foreach(model, for_each_row_selected, userdata);
    gtk_tree_selection_unselect_all(selection);

    for (i = 0; i < num_selected_judokas; i++) {
        struct judoka *j = get_data(selected_judokas[i]);
        if (!j)
            continue;

        if (j->birthyear) {
            if ((current_year - j->birthyear) > age)
                age = current_year - j->birthyear;
        }

        if (j->regcategory && j->regcategory[0]) {
            gint age1 = 0;
            gint n = find_age_index(j->regcategory);
            if (n >= 0) {
                age1 = category_definitions[n].age;
                if (category_definitions[n].gender & IS_FEMALE)
                    female = TRUE;
                else
                    male = TRUE;
            }

            if (j->birthyear == 0 && age1 > age)
                age = age1;
        } else {
            if (j->deleted & GENDER_MALE)
                male = TRUE;
            else if (j->deleted & GENDER_FEMALE)
                female = TRUE;
        }

        if (j->weight > weight)
            weight = j->weight;

        free_judoka(j);
    }

    gint k = 0;

    // Finnish speciality removed
    if (0 && draw_system == DRAW_FINNISH) {
        if (age > 19 && male)
            k += sprintf(cbuf+k, "M");
        if (age > 19 && female)
            k += sprintf(cbuf+k, "N");
        if (age >= 0 && age <= 19) {
            k += sprintf(cbuf+k, "%c", 
                         "JIIHHGGFFEEDDCCBBAAA"[age]);
            if (male)
                k += sprintf(cbuf+k, "P"); 
            if (female)
                k += sprintf(cbuf+k, "T");
        }
    } else {
        gint n = find_age_index_by_age(age, male ? IS_MALE : (female ? IS_FEMALE : IS_MALE));
        if (n >= 0)
            k += sprintf(cbuf+k, "%s", category_definitions[n].agetext);
        else
            k += sprintf(cbuf+k, "U%d", age+1);
    }

    k += sprintf(cbuf+k, "-%d,%d", 
                 weight/1000, 
                 (weight%1000)/100);


    GtkTreeIter tmp_iter;
    if (find_iter_category(&tmp_iter, cbuf)) {
        SHOW_MESSAGE("%s %s!", cbuf, _("already exists"));
        return;
    }
	

    for (i = 0; i < num_selected_judokas; i++) {
        struct judoka *j = get_data(selected_judokas[i]);
        
        if (!j)
            continue;

        if (db_competitor_match_status(j->index) & MATCH_EXISTS) {
            SHOW_MESSAGE("%s %s: %s.", 
                         j->first, j->last, _("Remove drawing first"));
        } else {
            if (j->category)
                g_free((void *)j->category);
            j->category = strdup(cbuf);
            gint ret = display_one_judoka(j);
            if (ret >= 0) {
                /* new category */
                struct judoka e;
                e.index = ret;
                e.last = cbuf;
                e.belt = 0;
                e.deleted = 0;
                e.birthyear = 0;
                db_add_category(e.index, &e);
            }

            db_update_judoka(j->index, j);
        }
        free_judoka(j);
    }
}

static void view_popup_menu_draw_and_print_category(GtkWidget *menuitem, gpointer userdata)
{
    view_popup_menu_draw_category(menuitem, userdata);
    print_doc(menuitem, gint_to_ptr(ptr_to_gint(userdata) | PRINT_SHEET | PRINT_TO_PRINTER));
}

static void show_category_window(GtkWidget *menuitem, gpointer userdata)
{
    category_window(ptr_to_gint(userdata));
}

void view_popup_menu(GtkWidget *treeview, 
                     GdkEventButton *event, 
                     gpointer userdata,
                     gchar *regcategory,
                     gboolean visible)
{
    gboolean team = FALSE;
    GtkWidget *menu, *menuitem;
    gint matched = db_category_match_status(ptr_to_gint(userdata));
    //db_matched_matches_exist(ptr_to_gint(userdata));

    if (dest_category)
        g_free(dest_category);

    dest_category = strdup(regcategory);
    dest_category_ix = ptr_to_gint(userdata);

    struct category_data *catdata = avl_get_category(dest_category_ix);
    if (catdata && (catdata->deleted & TEAM))
        team = TRUE;

    menu = gtk_menu_new();

    menuitem = gtk_menu_item_new_with_label(_("Expand All"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) change_display, (gpointer)0);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	
    menuitem = gtk_menu_item_new_with_label(_("Collapse All"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) change_display, (gpointer)1);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	
    GtkWidget *submenu;
    GtkTreeIter iter;
    gboolean ok;

    menuitem = gtk_menu_item_new_with_label(_("Move Competitors to Category..."));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

    ok = gtk_tree_model_get_iter_first(current_model, &iter);
    while (ok) {
        gint index;
        gchar *cat = NULL;
        gtk_tree_model_get(current_model, &iter,
                           COL_INDEX, &index,
                           COL_LAST_NAME, &cat,
                           -1);
        menuitem = gtk_menu_item_new_with_label(cat);
        g_signal_connect(menuitem, "activate",
                         (GCallback) view_popup_menu_change_category, 
                         gint_to_ptr(index));
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), menuitem);
        g_free(cat);
        
        ok = gtk_tree_model_iter_next(current_model, &iter);
    }

    if ((matched & SYSTEM_DEFINED /*REAL_MATCH_EXISTS*/) == 0) {
        menuitem = gtk_menu_item_new_with_label(_("Move Competitors Here"));
        g_signal_connect(menuitem, "activate",
                         (GCallback) view_popup_menu_move_judoka, userdata);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        if (!team) {
            menuitem = gtk_menu_item_new_with_label(_("Compose Unofficial Category"));
            g_signal_connect(menuitem, "activate",
                             (GCallback) create_new_category, userdata);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

            menuitem = gtk_menu_item_new_with_label(_("Draw Selected"));
            g_signal_connect(menuitem, "activate",
                             (GCallback) view_popup_menu_draw_category, userdata);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

            menuitem = gtk_menu_item_new_with_label(_("Draw and Print Selected"));
            g_signal_connect(menuitem, "activate",
                             (GCallback) view_popup_menu_draw_and_print_category, userdata);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

            menuitem = gtk_menu_item_new_with_label(_("Draw Manually"));
            g_signal_connect(menuitem, "activate",
                             (GCallback) view_popup_menu_draw_category_manually, userdata);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        }

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    } else if ((matched & MATCH_MATCHED) == 0) {
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        if (!team) {
            menuitem = gtk_menu_item_new_with_label(_("Remove Drawing"));
            g_signal_connect(menuitem, "activate",
                             (GCallback) view_popup_menu_remove_draw, userdata);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

            menuitem = gtk_menu_item_new_with_label(_("Edit Drawing"));
            g_signal_connect(menuitem, "activate",
                             (GCallback) view_popup_menu_draw_category_manually, 
                             gint_to_ptr(ptr_to_gint(userdata) | 0x01000000));
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

            gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
        }
    }

    menuitem = gtk_menu_item_new_with_label(_("Copy Competitors"));
    g_signal_connect(menuitem, "activate",
		     (GCallback) view_popup_menu_copy_judoka, userdata);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    if ((matched & SYSTEM_DEFINED /*REAL_MATCH_EXISTS*/) == 0) {
        menuitem = gtk_menu_item_new_with_label(_("Remove Competitors"));
        g_signal_connect(menuitem, "activate",
                         (GCallback) remove_competitors, userdata);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    }

    if (!team) {
        menuitem = gtk_menu_item_new_with_label(_("Show Sheet"));
        g_signal_connect(menuitem, "activate",
                         (GCallback) show_category_window, userdata);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        menuitem = gtk_menu_item_new_with_label(_("Print Selected Sheets (Printer)"));
        g_signal_connect(menuitem, "activate",
                         (GCallback) print_doc, gint_to_ptr(ptr_to_gint(userdata) | PRINT_SHEET | PRINT_TO_PRINTER));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        menuitem = gtk_menu_item_new_with_label(_("Print Selected Sheets (PDF)"));
        g_signal_connect(menuitem, "activate",
                         (GCallback) print_doc, gint_to_ptr(ptr_to_gint(userdata) | PRINT_SHEET | PRINT_TO_PDF));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    }

    menuitem = gtk_menu_item_new_with_label(_("Print Selected Accreditation Cards"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) view_popup_menu_print_cards, userdata);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label(_("Print Selected Accreditation Cards To Default Printer"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) view_popup_menu_print_cards_to_default, userdata);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    gtk_widget_show_all(menu);

    /* Note: event can be NULL here when called from view_onPopupMenu;
     *  gdk_event_get_time() accepts a NULL argument */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   (event != NULL) ? event->button : 0,
                   gdk_event_get_time((GdkEvent*)event));
}

