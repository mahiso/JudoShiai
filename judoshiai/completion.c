/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>

#ifdef WIN32
//#include <glib/gwin32.h>
#endif

#include "judoshiai.h"

static gboolean found;

static gboolean foreach_func (GtkTreeModel *model,
                              GtkTreePath  *path,
                              GtkTreeIter  *iter,
                              gpointer      user_data)
{
    gchar *txt;
    const gchar *find = user_data;

    gtk_tree_model_get(model, iter, 0, &txt, -1);
    found = strcmp(txt, find) == 0;
    g_free(txt);
    return found;
}
void complete_add_if_not_exist(
#if (GTKVER == 3)
                               GtkEntryCompletion *completer,
#else
                               GCompletion *completer, 
#endif
                               const gchar *txt)
{
    if (txt == NULL || txt[0] == 0)
        return;

#if (GTKVER == 3)
    GtkTreeIter iter;
    GtkListStore *model = GTK_LIST_STORE(gtk_entry_completion_get_model(completer));
    if (!model) return;

    found = FALSE;
    gtk_tree_model_foreach(GTK_TREE_MODEL(model), foreach_func, (gpointer)txt);

    if (!found) {
        gtk_list_store_append(model, &iter);
        gtk_list_store_set(model, &iter, 0, txt, -1);
    }
#else
    GList *items;
    GList *completions;

    completions = g_completion_complete_utf8(completer, txt, NULL);
    if (completions)
        return;

    items = NULL;
    items = g_list_append(items, g_strdup(txt));
    g_completion_add_items(completer, items);	
#endif
}

#if (GTKVER == 3)

static gboolean on_match_select(GtkEntryCompletion *widget,
                                GtkTreeModel       *model,
                                GtkTreeIter        *iter,
                                gpointer            user_data)
{  
    /** no usage at the moment
    GValue value = {0, };
    gtk_tree_model_get_value(model, iter, 0, &value);
    g_value_unset(&value);
    **/
    return FALSE;
} 

#else

int complete_cb(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    GCompletion *completer = data;
    GList *completions;
    gchar *prefix = NULL;
    gchar *notselected = NULL;
    gint pos, start, end;
    gunichar firstchar = event->string[0] & 0xff;
    gchar key[10];
    int n;

    if (!g_unichar_isalnum(firstchar) && !g_unichar_isprint(firstchar)) {
        return FALSE;
    }

    n = g_unichar_to_utf8(firstchar, key);
    key[n] = 0;

    pos = gtk_editable_get_position(GTK_EDITABLE(widget));
    gtk_editable_get_selection_bounds(GTK_EDITABLE(widget), &start, &end);

    if (end - start <= 0)
        start = -1;

    notselected = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, start);

    prefix = g_strdup_printf("%s%s", notselected, key);

    completions = g_completion_complete_utf8(completer, prefix, NULL);
    if (completions) {
        gint len = g_utf8_strlen(prefix, -1);
        gtk_entry_set_text(GTK_ENTRY(widget), completions->data);
        gtk_editable_set_position(GTK_EDITABLE(widget), len);
        gtk_editable_select_region(GTK_EDITABLE(widget), len, -1);
        g_free(notselected);
        g_free(prefix);
        return TRUE;
    }

    g_free(notselected);
    g_free(prefix);
    return FALSE;
}

static gint complete_compare(const gchar *s1, const gchar *s2, gsize n)
{
    gchar *u1 = g_utf8_casefold(s1, -1);
    gchar *u2 = g_utf8_casefold(s2, -1);
    gint result = strncmp(u1, u2, n);
    g_free(u1);
    g_free(u2);
    return result;
}

#endif

#if (GTKVER == 3)
GtkEntryCompletion *
#else
GCompletion *
#endif
club_completer_new(void)
{
#if (GTKVER == 3)
    GtkTreeIter iter;
    GtkEntryCompletion *completer = g_object_ref(gtk_entry_completion_new());// ???
    gtk_entry_completion_set_text_column(completer, 0);
    g_signal_connect(G_OBJECT (completer), "match-selected",
                     G_CALLBACK (on_match_select), NULL);    

    GtkListStore *model = gtk_list_store_new(1, G_TYPE_STRING);
#else
    GCompletion *completer;
    GList *items;

    completer = g_completion_new(NULL);
    g_completion_set_compare(completer, complete_compare);

    items = NULL;
#endif

    gchar line[256];
    gchar *file = g_build_filename(installation_dir, "etc", "clubs.txt", NULL);
    FILE *f = fopen(file, "r");
    g_free(file);

    if (!f)
        return completer;

    while (fgets(line, sizeof(line), f)) {
        gchar *p1 = strchr(line, '\r');
        if (p1)	*p1 = 0;
        p1 = strchr(line, '\n');
        if (p1)	*p1 = 0;

        if (strlen(line) < 2)
            continue;

	p1 = strchr(line, '=');
	if (p1) {
	    *p1 = 0;
	    p1++;
	    gchar *p2 = strchr(p1, '>');
	    if (p2) {
		*p2 = 0;
		p2++;
	    }
	    club_name_set(line, p1, p2 ? p2 : "");
	}
		
#if (GTKVER == 3)
        gtk_list_store_append(model, &iter);
        gtk_list_store_set(model, &iter, 0, line, -1);
#else
        items = g_list_append(items, g_strdup(line));
#endif
    }

    fclose(f);

#if (GTKVER == 3)
    gtk_entry_completion_set_model(completer, GTK_TREE_MODEL(model));
#else
    g_completion_add_items(completer, items);	
#endif

    return completer;
}

