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
#include "sqlite3.h"
#include "judoshiai.h"

struct treedata {
    GtkTreeStore  *treestore;
    GtkTreeIter    toplevel, child;
};

struct model_iter {
    GtkTreeModel *model;
    GtkTreeIter   iter;
};

#define NUM_IX_MAPPED 1024

struct ix_map {
    gint foreign;
    gint our;
}; 

struct shiai_map {
    gint foreign_cat;
    gint our_cat;
    gint ix_num;
    struct ix_map ixmap[NUM_IX_MAPPED];
};

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    /* If you return FALSE in the "delete_event" signal handler,
     * GTK will emit the "destroy" signal. Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit?'
     * type dialogs. */

    //g_print ("delete event occurred\n");

    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete_event". */

    return FALSE;
}

static void destroy_event( GtkWidget *widget,
			   gpointer   data )
{
    //g_print("destroy\n");
}

static gint set_one_category(GtkTreeModel *model, GtkTreeIter *iter, guint index, 
			     const gchar *category, guint tatami, guint group, struct compsys sys)
{
    struct judoka j;

    if (find_iter_category_model(iter, category, model)) {
        /* category exists */
        return -1;
    }

    /* new category */
    gtk_tree_model_get_iter_first(model, iter);
    gtk_tree_store_append((GtkTreeStore *)model, iter, NULL);

    j.index = index; // ? index : current_category_index++;
    j.last = category;
    j.first = "";
    j.birthyear = group;
    j.club = "";
    j.country = "";
    j.regcategory = "";
    j.belt = tatami;
    j.weight = compress_system(sys);
    j.visible = FALSE;
    j.category = "";
    j.deleted = 0;
    j.id = "";
    j.comment = "";
    j.coachid = "";

    put_data_by_iter_model(&j, iter, model);

    return j.index;
}

static gint display_one_competitor(GtkTreeModel *model, struct judoka *j)
{

    GtkTreeIter iter, parent, child;
    struct judoka *parent_data;
    guint ret = -1;

    if (j->visible == FALSE) {
        /* category row */
        if (find_iter_model(&iter, j->index, model)) {
            /* existing category */
            put_data_by_iter_model(j, &iter, model);
            return -1;
        }

        return set_one_category(model, &parent, j->index, (gchar *)j->last, 
                                j->belt, j->birthyear, uncompress_system(j->weight));
    }

    if (find_iter_model(&iter, j->index, model)) {
        /* existing judoka */

        if (gtk_tree_model_iter_parent((GtkTreeModel *)model, &parent, &iter) == FALSE) {
            /* illegal situation */
            g_print("ILLEGAL %s:%d\n", __FILE__, __LINE__);
            ret = set_one_category(model, &parent, 0, 
                                   (gchar *)j->category, 
                                   0, 0, (struct compsys){0,0,0,0});
        } 

        parent_data = get_data_by_iter_model(&parent, model);

        if (strcmp(parent_data->last, j->category)) {
            /* category has changed */
            gtk_tree_store_remove((GtkTreeStore *)model, &iter);
            ret = set_one_category(model, &parent, 0, (gchar *)j->category, 0, 0, (struct compsys){0,0,0,0});
            gtk_tree_store_append((GtkTreeStore *)model, &iter, &parent);
            put_data_by_iter_model(j, &iter, model);
        } else {
            /* current row is ok */
            put_data_by_iter_model(j, &iter, model);
        }

        free_judoka(parent_data);
    } else {
        /* new judoka */
        ret = set_one_category(model, &parent, 0, (gchar *)j->category, 0, 0, (struct compsys){0,0,0,0});

        gtk_tree_store_append((GtkTreeStore *)model, &child, &parent);
        put_data_by_iter_model(j, &child, model);
    }

    return ret;
}

#define IS(x) (!strcmp(azColName[i], #x))

static int db_competitor_callback(void *data, int argc, char **argv, char **azColName)
{
    int i;
    struct judoka j;
    GtkTreeModel *model = data;

    for(i = 0; i < argc; i++){
        //g_print("  %s=%s", azColName[i], argv[i] ? argv[i] : "(NULL)");
        if (IS(index))
            j.index = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(last))
            j.last = argv[i] ? argv[i] : "?";
        else if (IS(first))
            j.first = argv[i] ? argv[i] : "?";
        else if (IS(birthyear))
            j.birthyear = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(club))
            j.club = argv[i] ? argv[i] : "?";
        else if (IS(country))
            j.country = argv[i] ? argv[i] : "";
        else if (IS(regcategory) || IS(wclass))
            j.regcategory = argv[i] ? argv[i] : "?";
        else if (IS(belt))
            j.belt = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(weight))
            j.weight = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(visible))
            j.visible = argv[i] ? atoi(argv[i]) : 1;
        else if (IS(category))
            j.category = argv[i] ? argv[i] : "?";
        else if (IS(deleted))
            j.deleted = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(id))
            j.id = argv[i] ? argv[i] : "";
        else if (IS(seeding))
            j.seeding = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(clubseeding))
            j.clubseeding = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(comment))
            j.comment = argv[i] ? argv[i] : "";
        else if (IS(coachid))
            j.coachid = argv[i] ? argv[i] : "";
    }

    if ((j.deleted & DELETED))
        return 0;

    //j.index = current_index++;
    display_one_competitor(model, &j);

    return 0;
}

static int db_category_callback(void *data, int argc, char **argv, char **azColName)
{
    int i;
    struct judoka j;
    GtkTreeModel *model = data;
    struct compsys cs;

    BZERO(cs);
    memset(&j, 0, sizeof(j));

    for (i = 0; i < argc; i++) {
        //g_print(" %s=%s", azColName[i], argv[i]);
        if (IS(index))
            j.index = atoi(argv[i]);
        else if (IS(category))
            j.last = argv[i];
        else if (IS(tatami))
            j.belt = atoi(argv[i]);
        else if (IS(deleted))
            j.deleted = atoi(argv[i]);
        else if (IS(group))
            j.birthyear = atoi(argv[i]);
        else if (IS(system))
            cs.system = atoi(argv[i]);
        else if (IS(numcomp))
            cs.numcomp = atoi(argv[i]);
        else if (IS(table))
            cs.table = atoi(argv[i]);
        else if (IS(wishsys))
            cs.wishsys = atoi(argv[i]);
    }

    j.weight = compress_system(cs);

    if ((j.deleted & DELETED))
        return 0;

    //j.index = current_category_index++;
    display_one_competitor(model, &j);

    return 0;
}


static gint find_our_ix(struct shiai_map *mp, gint foreign)
{
    gint i;

    if (foreign < COMPETITOR)
        return foreign;

    for (i = 0; i < mp->ix_num; i++) {
        if (mp->ixmap[i].foreign == foreign)
            return mp->ixmap[i].our;
    }
    g_print("ERROR %s:%d: Foreign index=%d\n", __FILE__, __LINE__, foreign);
    return -1;
}

static int db_match_callback(void *data, int argc, char **argv, char **azColName)
{
    guint i;
    gint val;
    struct match m;
    struct shiai_map *mp = data;

    for (i = 0; i < argc; i++) {
        val =  argv[i] ? atoi(argv[i]) : 0;
		
        if (IS(category))
            m.category = val;
        else if (IS(number))
            m.number = val;
        else if (IS(blue))
            m.blue = val;
        else if (IS(white))
            m.white = val;
        else if (IS(blue_score))
            m.blue_score = val;
        else if (IS(white_score))
            m.white_score = val;
        else if (IS(blue_points))
            m.blue_points = val;
        else if (IS(white_points))
            m.white_points = val;
        else if (IS(time))
            m.match_time = val;
        else if (IS(comment))
            m.comment = val;
        else if (IS(date))
            m.date = val;
        else if (IS(legend))
            m.legend = val;
        else if (IS(deleted)) {
            m.deleted = val;
        }
    }

    m.category = mp->our_cat;
    m.blue = find_our_ix(mp, m.blue);
    m.white = find_our_ix(mp, m.white);

    if ((m.deleted & DELETED) == 0)
        db_set_match(&m);

    return 0;
}

static GtkTreeModel *create_and_fill_model(gchar *dbname)
{
    GtkTreeSortable *sortable;
    GtkTreeModel *model;
    GtkTreeStore *treestore;

    treestore = gtk_tree_store_new(NUM_COLS,
                                   G_TYPE_UINT,
                                   G_TYPE_STRING, /* last */
                                   G_TYPE_STRING, /* first */
                                   G_TYPE_UINT,   /* birthyear */
                                   G_TYPE_UINT,   /* belt */
                                   G_TYPE_STRING, /* club */
                                   G_TYPE_STRING, /* country */
                                   G_TYPE_STRING, /* regcategory */
                                   G_TYPE_INT,    /* weight */
                                   G_TYPE_BOOLEAN,/* visible */
                                   G_TYPE_STRING, /* category */
                                   G_TYPE_UINT,   /* deleted */
                                   G_TYPE_STRING, /* id */
                                   G_TYPE_INT,    /* seeding */
                                   G_TYPE_INT,    /* clubseeding */
                                   G_TYPE_STRING, /* comment */
                                   G_TYPE_STRING  /* coachid */
        );

    model = GTK_TREE_MODEL(treestore);
    sortable = GTK_TREE_SORTABLE(model);
    /* set initial sort order */
    gtk_tree_sortable_set_sort_column_id(sortable, SORTID_NAME, GTK_SORT_ASCENDING);

    /* read data from db */

    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

    rc = sqlite3_open(dbname, &db);
    if (rc) {
        g_print("Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return model;
    }

    rc = sqlite3_exec(db, 
                      "SELECT * FROM categories WHERE \"deleted\"&1=0 ORDER BY \"category\" ASC", 
                      db_category_callback, model, &zErrMsg);
    if (rc != SQLITE_OK && zErrMsg) {
        g_print("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    rc = sqlite3_exec(db, 
                      "SELECT * FROM competitors WHERE \"deleted\"&1=0 ORDER BY \"last\" ASC", 
                      db_competitor_callback, model, &zErrMsg);
    if (rc != SQLITE_OK && zErrMsg) {
        g_print("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    sqlite3_close(db);

    return model;
}

static void read_foreign_matches(gchar *dbname, struct shiai_map *mp)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    gchar cmd[200];

    rc = sqlite3_open(dbname, &db);
    if (rc) {
        g_print("Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    sprintf(cmd, 
            "SELECT * FROM matches WHERE \"deleted\"&1=0 AND \"category\"=%d",
            mp->foreign_cat);
	
    rc = sqlite3_exec(db, cmd, db_match_callback, mp, &zErrMsg);
    if (rc != SQLITE_OK && zErrMsg) {
        g_print("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
    }

    sqlite3_close(db);
}

extern void view_on_row_activated (GtkTreeView        *treeview,
                                   GtkTreePath        *path,
                                   GtkTreeViewColumn  *col,
                                   gpointer            userdata);

void row_activated(GtkTreeView        *treeview,
		   GtkTreePath        *path,
		   GtkTreeViewColumn  *col,
		   gpointer            userdata)
{
    GtkTreeModel *model;
    GtkTreeIter   iter, iter2;
    struct judoka *j;
    struct shiai_map *mapped = NULL;

    if (!treeview)
        return;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        j = get_data_by_iter_model(&iter, model);
    } else
        return;

    if (j->visible) { // competitor
        gint ret;

        if (find_iter_name_2(&iter2, j->last, j->first, j->club, j->regcategory)) {
            show_message(_("Competitor already exists!"));
            goto out;
        }

        j->index = current_index++;
        g_free((void *)j->category);
        j->category = g_strdup("?");
        db_add_judoka(j->index, j);
        ret = display_one_judoka(j);
        ret = ret; // make compiler happy while this value is not used
#if 0
        if (ret >= 0) {
            /* new category */
            struct judoka e;
            e.index = ret;
            e.last = j->category;
            e.belt = 0;
            e.deleted = 0;
            e.birthyear = 0;
            db_add_category(e.index, &e);
        }
#endif
        update_competitors_categories(j->index);
        matches_refresh();
        show_note("%s %s %s (%s) %s", _("Competitor"), j->first, j->last, j->regcategory, _("copied"));

        if (find_iter(&iter, j->index)) {
            GtkTreePath *path = gtk_tree_model_get_path(current_model, &iter);
            view_on_row_activated(GTK_TREE_VIEW(current_view), path, NULL, NULL);
            gtk_tree_path_free(path);
        }
    } else { // category
        gboolean ok;
        GtkTreeIter tmp_iter;
        gint ret;

        if (find_iter_category(&iter2, j->last)) {
            show_message(_("Category already exists!"));
            goto out;
        }
        mapped = malloc(sizeof(*mapped));
        if (!mapped)
            g_print("ERROR %s:%d\n", __FILE__, __LINE__);

        mapped->ix_num = 0;
        mapped->foreign_cat = j->index;

        j->index = 0;
        ret = display_one_judoka(j);
        if (ret >= 0) {
            j->index = ret;
            mapped->our_cat = j->index;
            db_add_category(j->index, j);
            db_set_system(j->index, uncompress_system(j->weight));
        } else
            g_print("ERROR %s:%d\n", __FILE__,__LINE__);

        ok = gtk_tree_model_iter_children(model, &tmp_iter, &iter);
        while (ok) {
            struct judoka *j2 = get_data_by_iter_model(&tmp_iter, model);
            mapped->ixmap[mapped->ix_num].foreign = j2->index;
            j2->index = current_index++;
            mapped->ixmap[mapped->ix_num].our = j2->index;
            if (mapped->ix_num < NUM_IX_MAPPED-1)
                mapped->ix_num++;

            db_add_judoka(j2->index, j2);
            ret = display_one_judoka(j2);
            if (ret >= 0)
                g_print("ERROR %s:%d\n", __FILE__,__LINE__);
            free_judoka(j2);
            ok = gtk_tree_model_iter_next(model, &tmp_iter);
        }

        read_foreign_matches(userdata, mapped);
        matches_refresh();
        update_category_status_info(j->index);

        show_note("%s %s %s", 
                  _("Category"), j->last, _("has been copied with competitors and matches"));
    }

out:
    free_judoka(j);
    if (mapped)
        free(mapped);
}

static GtkWidget *create_view_and_model(gchar *dbname)
{
    GtkTreeViewColumn   *col;
    GtkCellRenderer     *renderer;
    GtkWidget           *view;
    GtkTreeModel        *model;
    //GtkTreeSelection    *selection;

    gint col_offset;

    view = gtk_tree_view_new();

    /* --- Column last name --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "weight", PANGO_WEIGHT_BOLD,
                 "weight-set", TRUE,
                 "xalign", 0.0,
                 NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Last Name"),
                                                              renderer, "text",
                                                              COL_LAST_NAME,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, last_name_cell_data_func, (void *)1, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_LAST_NAME);

    /* --- Column first name --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("First Name"),
                                                              renderer, "text",
                                                              COL_FIRST_NAME,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);


    /* --- Column birthyear name --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Year of Birth"),
                                                              renderer, "text",
                                                              COL_BIRTHYEAR,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);


    /* --- Column belt --- */

    renderer = gtk_cell_renderer_text_new();
    //g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Grade"),
                                                              renderer, "text",
                                                              COL_BELT,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, belt_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_BELT);

    /* --- Column club --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Club"),
                                                              renderer, "text",
                                                              COL_CLUB,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_CLUB);

    /* --- Column country --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Country"),
                                                              renderer, "text",
                                                              COL_COUNTRY,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_COUNTRY);

    /* --- Column regcategory --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Reg. Category"),
                                                              renderer, "text",
                                                              COL_WCLASS,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_WCLASS);

    /* --- Column weight --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Weight"),
                                                              renderer, "text",
                                                              COL_WEIGHT,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, weight_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_WEIGHT);

    /* --- Column id --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Id"),
                                                              renderer, "text",
                                                              COL_ID,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_ID);

    /*****/

    model = create_and_fill_model(dbname);
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);
    /*
     * gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
     *			    GTK_SELECTION_MULTIPLE);
     */		    
    g_object_unref(model); /* destroy model automatically with view */

    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
                                GTK_SELECTION_SINGLE);

    g_signal_connect(view, "row-activated", (GCallback) row_activated, dbname);
    //g_signal_connect(view, "button-press-event", (GCallback) view_onButtonPressed, NULL);
    //g_signal_connect(view, "popup-menu", (GCallback) view_onPopupMenu, NULL);

    //selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    //gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_LAST_NAME,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_NAME),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_WEIGHT,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_WEIGHT),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_CLUB,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_CLUB),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_WCLASS,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_WCLASS),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_BELT,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_BELT),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_COUNTRY,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_COUNTRY),
                                     NULL);

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), COL_LAST_NAME);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);


    return view;
}

/*
 * Page init
 */

void set_old_shiai_display(GtkWidget *w, gpointer data)
{
    GtkWidget *judokas_scrolled_window;
    GtkWidget *view;

    /* Open shiai file */

    GtkWidget *dialog;
    GtkFileFilter *filter = gtk_file_filter_new();
    gchar *dbname = NULL;

    gtk_file_filter_add_pattern(filter, "*.shi");
    gtk_file_filter_set_name(filter, _("Tournaments"));

    dialog = gtk_file_chooser_dialog_new (_("Tournament"),
                                          NULL,
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    if (database_name[0] == 0) {
        if (current_directory[0] != '.')
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), current_directory);
        else
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), g_get_home_dir());
    } else {
        gchar *dirname = g_path_get_dirname(database_name);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dirname);
        g_free(dirname);
    }

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)
        dbname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));

    gtk_widget_destroy (dialog);

    if (!dbname)
        goto out;

    valid_ascii_string(dbname);

    /* Create window */

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), dbname);
    gtk_widget_set_size_request(window, FRAME_WIDTH, FRAME_HEIGHT);

    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event), NULL);
    
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (destroy_event), NULL);

    judokas_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_set_border_width(GTK_CONTAINER(judokas_scrolled_window), 10);

	/* Create view and model */

        view = create_view_and_model(dbname);

    	/* pack the table into the scrolled window */
#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
    gtk_container_add(GTK_CONTAINER(judokas_scrolled_window), view);
#else
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(judokas_scrolled_window), view);
#endif
    gtk_container_add (GTK_CONTAINER (window), judokas_scrolled_window);

    gtk_widget_show_all(window);


out:
	/* release resources */
	
	/* intentional memory leak */
	//g_free(dbname); /* shiai database name */
	return;
}
