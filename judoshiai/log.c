/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <gtk/gtk.h>
#include "sqlite3.h"
#include "judoshiai.h"

enum {
    COL_DATE,
    COL_SEVERITY,
    COL_TATAMI,
    COL_TEXT,
    NUM_LOG_COLUMNS
};

gchar logfile_name[200] = {0};

static GtkWidget *log_view = NULL;
static GtkWidget    *log_label;

static GtkTreeModel *create_model(void)
{
    GtkListStore *store;

    store = gtk_list_store_new (NUM_LOG_COLUMNS,
                                G_TYPE_UINT,      /* date     */
                                G_TYPE_UINT,      /* severity */
                                G_TYPE_UINT,      /* tatami   */
                                G_TYPE_STRING);   /* message  */
    return GTK_TREE_MODEL (store);
}
	
static gint sort_iter_compare_func_1(GtkTreeModel *model,
                                     GtkTreeIter  *a,
                                     GtkTreeIter  *b,
                                     gpointer      userdata)
{
    gint sortcol = GPOINTER_TO_INT(userdata);
    guint data1, data2;

    gtk_tree_model_get(model, a, sortcol, &data1, -1);
    gtk_tree_model_get(model, b, sortcol, &data2, -1);

    if (data1 > data2)
        return 1;

    if (data1 < data2)
        return -1;

    return 0;
}

static void date_cell_data_func (GtkTreeViewColumn *col,
                                 GtkCellRenderer   *renderer,
                                 GtkTreeModel      *model,
                                 GtkTreeIter       *iter,
                                 gpointer           user_data)
{
    gchar         buf[32];
    time_t        date;
    struct tm    *tm;

    gtk_tree_model_get(model, iter, COL_DATE, &date, -1);
  
    tm = localtime(&date);

	if (tm)
	    sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d", 
	            tm->tm_year+1900, 
	            tm->tm_mon+1,
	            tm->tm_mday,
	            tm->tm_hour,
	            tm->tm_min,
	            tm->tm_sec);
        
    g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */

    g_object_set(renderer, "text", buf, NULL);
}

static void add_columns(GtkTreeView *view)
{
    GtkCellRenderer     *renderer;
    GtkTreeModel        *model = gtk_tree_view_get_model(view);
    GtkTreeViewColumn   *col;
    gint                 col_offset;


    /* --- Column date --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Time"),
                                                              renderer, "text",
                                                              COL_DATE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, date_cell_data_func, NULL, NULL);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_DATE);

    /* --- Column severity --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Class"),
                                                              renderer, "text",
                                                              COL_SEVERITY,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);

    /* --- Column tatami --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, "Tatami",
                                                              renderer, "text",
                                                              COL_TATAMI,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_TATAMI);

    /* --- Column text --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Message"),
                                                              renderer, "text",
                                                              COL_TEXT,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);

    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_DATE,
                                     sort_iter_compare_func_1,
                                     GINT_TO_POINTER(COL_DATE),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(model),
                                     COL_TATAMI,
                                     sort_iter_compare_func_1,
                                     GINT_TO_POINTER(COL_TATAMI),
                                     NULL);
}

void set_log_page(GtkWidget *notebook)
{
    GtkWidget    *log_scrolled_window;
    GtkWidget    *treeview;
    GtkTreeModel *model;

    log_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(log_scrolled_window), 10);

    model = create_model();
        
    log_view = treeview = gtk_tree_view_new_with_model(model);

    g_object_unref(model);

    add_columns(GTK_TREE_VIEW(treeview));

#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
    gtk_container_add(GTK_CONTAINER(log_scrolled_window), treeview);
#else
    gtk_scrolled_window_add_with_viewport(
        GTK_SCROLLED_WINDOW(log_scrolled_window), treeview);
#endif

    log_label = gtk_label_new (_("Log"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), log_scrolled_window, log_label);
}

void shiai_log(guint severity, guint tatami, gchar *format, ...)
{
    time_t        t;
    GtkTreeModel *model;
    GtkTreeIter   iter;
    static gint   num_log_lines = 0;

    if (!log_view)
        return;

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(log_view));

    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);

    t = time(NULL);

    while (num_log_lines >= 100) {
        gtk_tree_model_get_iter_first(model, &iter);
        gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
        num_log_lines--;
    }

    gtk_list_store_append((GtkListStore *)model, &iter);
        
    gtk_list_store_set((GtkListStore *)model, &iter,
                       COL_DATE,       t,
                       COL_SEVERITY,   severity,
                       COL_TATAMI,     tatami,
                       COL_TEXT,       text ? text : "?",
                       -1);
    num_log_lines++;

    if (logfile_name[0]) {
        FILE *f = fopen(logfile_name, "a");
        if (f) {
            struct tm *tm = localtime(&t);
            gsize x;

            gchar *text_ISO_8859_1 = 
                g_convert(text, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);

			if (tm)
	            fprintf(f, "%04d-%02d-%02d %02d:%02d:%02d %d %d %s\n",
	                    tm->tm_year+1900, 
	                    tm->tm_mon+1,
	                    tm->tm_mday,
	                    tm->tm_hour,
	                    tm->tm_min,
	                    tm->tm_sec,
	                    severity, tatami, text_ISO_8859_1);
            g_free(text_ISO_8859_1);
            fclose(f);
        }
    }

    g_free(text);
}

void set_log_col_titles(void)
{
    GtkTreeViewColumn *col;

    if (!current_view)
        return;

    col = gtk_tree_view_get_column(GTK_TREE_VIEW(log_view), COL_DATE);
    gtk_tree_view_column_set_title(col, _("Time"));

    col = gtk_tree_view_get_column(GTK_TREE_VIEW(log_view), COL_SEVERITY);
    gtk_tree_view_column_set_title(col, _("Class"));

    col = gtk_tree_view_get_column(GTK_TREE_VIEW(log_view), COL_TEXT);
    gtk_tree_view_column_set_title(col, _("Message"));

    if (log_label)
        gtk_label_set_text(GTK_LABEL(log_label), _("Log"));
}
