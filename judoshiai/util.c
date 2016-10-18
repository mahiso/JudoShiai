/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <gtk/gtk.h>

#include "sqlite3.h"
#include "judoshiai.h"
#include "language.h"


static GtkTreeIter found_iter;
static gboolean    index_found;

static gboolean traverse_rows(GtkTreeModel *model,
			      GtkTreePath *path,
			      GtkTreeIter *iter,
			      gpointer data)
{
    guint ix = ptr_to_gint(data);
    guint index;

    gtk_tree_model_get(model, iter,
                       COL_INDEX, &index,
                       -1);
    if (index == ix) {
        index_found = TRUE;
        found_iter = *iter;
        return TRUE;
    }
    return FALSE;
}

gboolean find_iter(GtkTreeIter *iter, guint index)
{
    if (!current_model)
        return FALSE;

    index_found = FALSE;
    gtk_tree_model_foreach(current_model, traverse_rows, gint_to_ptr(index));
    if (index_found) {
        *iter = found_iter;
        return TRUE;
    }
    return FALSE;
}

gboolean find_iter_model(GtkTreeIter *iter, guint index, GtkTreeModel *model)
{
    index_found = FALSE;
    gtk_tree_model_foreach(model, traverse_rows, gint_to_ptr(index));
    if (index_found) {
        *iter = found_iter;
        return TRUE;
    }
    return FALSE;
}

static GtkTreeIter found_iter_name;
static gboolean    name_found;

static gboolean traverse_rows_for_name(GtkTreeModel *model,
				       GtkTreePath *path,
				       GtkTreeIter *iter,
				       gpointer data)
{
    gchar **names = data;
    gchar *lastname = NULL, *firstname = NULL, *clubname = NULL, *regcat = NULL, *id = NULL;
    gboolean result = FALSE;
	
    gtk_tree_model_get(model, iter,
                       COL_LAST_NAME, &lastname,
                       COL_FIRST_NAME, &firstname, 
                       COL_CLUB, &clubname, 
                       COL_WCLASS, &regcat,
                       COL_ID, &id,
                       -1);

    if (names[4] && id && id[0] && (strcmp(id, names[4]) == 0))
        goto ok;
    if (names[0] && (lastname == NULL || strcmp(lastname, names[0])))
        goto out;
    if (names[1] && (firstname == NULL || strcmp(firstname, names[1])))
        goto out;
    if (names[2] && (clubname == NULL || strcmp(clubname, names[2])))
        goto out;
    if (names[3] && (regcat == NULL || strcmp(regcat, names[3])))
        goto out;
 ok:
    name_found = TRUE;
    found_iter_name = *iter;
    result = TRUE;
 out:
    g_free(lastname);
    g_free(firstname);
    g_free(clubname);
    g_free(regcat);
    g_free(id);
    return result;
}

gboolean find_iter_name(GtkTreeIter *iter, const gchar *last, const gchar *first, const gchar *club)
{
    const gchar *names[5];

    if (!current_model)
        return FALSE;

    names[0] = last;
    names[1] = first;
    names[2] = club;
    names[3] = names[4] = NULL;
    name_found = FALSE;
    gtk_tree_model_foreach(current_model, traverse_rows_for_name, (gpointer)names);
    if (name_found) {
        *iter = found_iter_name;
        return TRUE;
    }
    return FALSE;
}

gboolean find_iter_name_2(GtkTreeIter *iter, const gchar *last, const gchar *first, 
			  const gchar *club, const gchar *category)
{
    const gchar *names[5];

    if (!current_model)
        return FALSE;

    names[0] = last;
    names[1] = first;
    names[2] = club;
    names[3] = category;
    names[4] = NULL;
    name_found = FALSE;
    gtk_tree_model_foreach(current_model, traverse_rows_for_name, (gpointer)names);
    if (name_found) {
        *iter = found_iter_name;
        return TRUE;
    }
    return FALSE;
}

gboolean find_iter_name_id(GtkTreeIter *iter, const gchar *last, const gchar *first, 
                           const gchar *club, const gchar *category, const gchar *id)
{
    const gchar *names[5];

    if (!current_model)
        return FALSE;

    names[0] = last;
    names[1] = first;
    names[2] = club;
    names[3] = category;
    names[4] = id;
    name_found = FALSE;
    gtk_tree_model_foreach(current_model, traverse_rows_for_name, (gpointer)names);
    if (name_found) {
        *iter = found_iter_name;
        return TRUE;
    }
    return FALSE;
}


static GtkTreeIter found_iter_category;
static gboolean    index_found_category;

static gboolean traverse_rows_category(GtkTreeModel *model,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       gpointer data)
{
    gchar *category = (gchar *)data;
    gchar *cat_now;
    gboolean visible;

    gtk_tree_model_get(model, iter,
                       COL_LAST_NAME, &cat_now,
                       COL_VISIBLE, &visible,
                       -1);
    if (visible == FALSE && (!strcmp(category, cat_now))) {
        index_found_category = TRUE;
        found_iter_category = *iter;
        g_free(cat_now);
        return TRUE;
    }
    g_free(cat_now);
    return FALSE;
}

gboolean find_iter_category(GtkTreeIter *iter, const gchar *category)
{
    if (!current_model)
        return FALSE;

    index_found_category = FALSE;
    gtk_tree_model_foreach(current_model, traverse_rows_category, (gpointer)category);
    if (index_found_category) {
        *iter = found_iter_category;
        return TRUE;
    }
    return FALSE;
}

gboolean find_iter_category_model(GtkTreeIter *iter, const gchar *category, GtkTreeModel *model)
{
    index_found_category = FALSE;
    gtk_tree_model_foreach(model, traverse_rows_category, (gpointer)category);
    if (index_found_category) {
        *iter = found_iter_category;
        return TRUE;
    }
    return FALSE;
}


struct judoka *get_data_by_iter(GtkTreeIter *iter)
{
    if (!current_model)
        return NULL;

    struct judoka *j = malloc(sizeof *j);

    gtk_tree_model_get(current_model, iter,
                       COL_INDEX, &j->index,
                       COL_LAST_NAME, &j->last,
                       COL_FIRST_NAME, &j->first, 
                       COL_BIRTHYEAR, &j->birthyear, 
                       COL_CLUB, &j->club, 
                       COL_COUNTRY, &j->country, 
                       COL_WCLASS, &j->regcategory, 
                       COL_BELT, &j->belt, 
                       COL_WEIGHT, &j->weight, 
                       COL_VISIBLE, &j->visible,
                       COL_CATEGORY, &j->category,
                       COL_DELETED, &j->deleted,
                       COL_ID, &j->id, 
                       COL_SEEDING, &j->seeding, 
                       COL_CLUBSEEDING, &j->clubseeding, 
                       COL_COMMENT, &j->comment, 
                       COL_COACHID, &j->coachid, 
                       -1);
    return j;
}

struct judoka *get_data_by_iter_model(GtkTreeIter *iter, GtkTreeModel *model)
{
    struct judoka *j = malloc(sizeof *j);

    gtk_tree_model_get(model, iter,
                       COL_INDEX, &j->index,
                       COL_LAST_NAME, &j->last,
                       COL_FIRST_NAME, &j->first, 
                       COL_BIRTHYEAR, &j->birthyear, 
                       COL_CLUB, &j->club, 
                       COL_COUNTRY, &j->country, 
                       COL_WCLASS, &j->regcategory, 
                       COL_BELT, &j->belt, 
                       COL_WEIGHT, &j->weight, 
                       COL_VISIBLE, &j->visible,
                       COL_CATEGORY, &j->category,
                       COL_DELETED, &j->deleted,
                       COL_ID, &j->id, 
                       COL_SEEDING, &j->seeding, 
                       COL_CLUBSEEDING, &j->clubseeding, 
                       COL_COMMENT, &j->comment, 
                       COL_COACHID, &j->coachid, 
                       -1);
    return j;
}

struct judoka *get_data(guint index)
{
    GtkTreeIter iter;

    if (find_iter(&iter, index & MATCH_CATEGORY_MASK) == FALSE) 
        return NULL;

    return get_data_by_iter(&iter);
}


void put_data_by_iter(struct judoka *j, GtkTreeIter *iter)
{
    if (!current_model)
        return;

    gtk_tree_store_set((GtkTreeStore *)current_model, 
                       iter,
                       COL_INDEX,      j->index,
                       COL_LAST_NAME,  j->last ? j->last : "?",
                       COL_FIRST_NAME, j->first ? j->first : "?",
                       COL_BIRTHYEAR,  j->birthyear,
                       COL_CLUB,       j->club ? j->club : "?",
                       COL_COUNTRY,    j->country ? j->country : "",
                       COL_WCLASS,     j->regcategory ? j->regcategory : "",
                       COL_BELT,       j->belt,
                       COL_WEIGHT,     j->weight,
                       COL_VISIBLE,    j->visible,
                       COL_CATEGORY,   j->category ? j->category : "?",
                       COL_DELETED,    j->deleted,
                       COL_ID,         j->id ? j->id : "",
                       COL_SEEDING,    j->seeding, 
                       COL_CLUBSEEDING,j->clubseeding, 
                       COL_COMMENT,    j->comment ? j->comment : "", 
                       COL_COACHID,    j->coachid ? j->coachid : "", 
                       -1);
}

void put_data_by_iter_model(struct judoka *j, GtkTreeIter *iter, GtkTreeModel *model)
{
    if (!model)
        return;

    if (!j)
        return;

    gtk_tree_store_set((GtkTreeStore *)model, 
                       iter,
                       COL_INDEX,      j->index,
                       COL_LAST_NAME,  j->last ? j->last : "?",
                       COL_FIRST_NAME, j->first ? j->first : "?",
                       COL_BIRTHYEAR,  j->birthyear,
                       COL_CLUB,       j->club ? j->club : "?",
                       COL_COUNTRY,    j->country ? j->country : "",
                       COL_WCLASS,     j->regcategory ? j->regcategory : "",
                       COL_BELT,       j->belt,
                       COL_WEIGHT,     j->weight,
                       COL_VISIBLE,    j->visible,
                       COL_CATEGORY,   j->category ? j->category : "?",
                       COL_DELETED,    j->deleted,
                       COL_ID,         j->id ? j->id : "",
                       COL_SEEDING,    j->seeding, 
                       COL_CLUBSEEDING,j->clubseeding, 
                       COL_COMMENT,    j->comment ? j->comment : "", 
                       COL_COACHID,    j->coachid ? j->coachid : "", 
                       -1);
}

void put_data(struct judoka *j, guint index)
{
    GtkTreeIter iter;

    if (find_iter(&iter, index) == FALSE) 
        return;

    put_data_by_iter(j, &iter);
}

#define FREE(x) do { if (j->x) g_free((void *)j->x); j->x = NULL; } while (0)

void free_judoka(struct judoka *j)
{
    if (j == NULL)
        return;

    FREE(last);
    FREE(first); 
    FREE(club); 
    FREE(regcategory); 
    FREE(category);
    FREE(country);
    FREE(id);
    FREE(comment);
    FREE(coachid);

    g_free(j);
}

void show_message(gchar *msg)
{
    GtkWidget *dialog;
        
    dialog = gtk_message_dialog_new (NULL,
                                     0 /*GTK_DIALOG_DESTROY_WITH_PARENT*/,
                                     GTK_MESSAGE_INFO,
                                     GTK_BUTTONS_OK,
                                     "%s", msg);

    g_signal_connect_swapped (dialog, "response",
                              G_CALLBACK (gtk_widget_destroy),
                              dialog);

    gtk_widget_show(dialog);
}

void show_note(gchar *format, ...)
{
#if 0
    extern GtkWidget *notes;

    if (!notes)
        return;
#endif
    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);

    progress_show(0.0, text);
    //gtk_label_set_text(GTK_LABEL(notes), text);

    g_free(text);
}

gint ask_question(gchar *message)
{
    GtkWidget *dialog, *label;
    gint response;

    dialog = gtk_dialog_new_with_buttons (_("Which one is correct?"),
                                          NULL,
                                          GTK_DIALOG_MODAL,
                                          _("Own database"),        Q_LOCAL,
                                          _("Foreign database"),     Q_REMOTE,
                                          _("Do not change"),        Q_CONTINUE,
                                          _("Stop synchronization"),   Q_QUIT,
                                          NULL);
        
    label = gtk_label_new (message);
#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       label, FALSE, FALSE, 0);
#else
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), label);
#endif
    gtk_widget_show_all(dialog);

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    gtk_widget_destroy (dialog);

    return response;
}

gint weight_grams(const gchar *s)
{
    gint weight = 0, decimal = 0;
        
    while (*s) {
        if (*s == '.' || *s == ',') {
            decimal = 1;
        } else if (*s < '0' || *s > '9') {
        } else if (decimal == 0) {
            weight = 10*weight + 1000*(*s - '0');
        } else {
            switch (decimal) {
            case 1: weight += 100*(*s - '0'); break;
            case 2: weight += 10*(*s - '0'); break;
            case 3: weight += *s - '0'; break;
            }
            decimal++;
        }
        s++;
    }
        
    return weight;
}

void print_time(gchar *fname, gint lineno)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    g_print("%ld.%06ld %s:%d\n", tv.tv_sec, tv.tv_usec, fname, lineno);
}

gint timeval_subtract(GTimeVal *result, GTimeVal *x, GTimeVal *y)
{
    /* Perform the carry for the later subtraction by updating y. */
    if (x->tv_usec < y->tv_usec) {
        gint nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        gint nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    /* Compute the time remaining to wait.
       tv_usec is certainly positive. */
    if (result) {
        result->tv_sec = x->tv_sec - y->tv_sec;
        result->tv_usec = x->tv_usec - y->tv_usec;
    }

    /* Return 1 if result is negative. */
    return x->tv_sec < y->tv_sec;
}

gboolean valid_ascii_string(const gchar *txt)
{
    const gchar *s = txt;

    if (!s)
        return TRUE;

    while (*s) {
        if (*s & 0x80) {
            show_note("%s: \"%s\" %s", _("NOTE"), txt, _("is not a valid ASCII string!"));
            return FALSE;
        }
        s++;
    }

    return TRUE;
}

const gchar *get_name_and_club_text(struct judoka *j, gint flags)
{
    static gchar buffers[4][128];
    static gint n = 0;
    gchar *p;
    
    if (flags & CLUB_TEXT_NO_CLUB) {
        switch (name_layout) {
        case NAME_LAYOUT_N_S_C:
            snprintf(buffers[n], 128, "%s %s", j->first, j->last);
            break;
        case NAME_LAYOUT_S_N_C:
        case NAME_LAYOUT_C_S_N:
            snprintf(buffers[n], 128, "%s %s", j->last, j->first);
            break;
        }
    } else {
        switch (name_layout) {
        case NAME_LAYOUT_N_S_C:
            snprintf(buffers[n], 128, "%s %s, %s", 
                     j->first, j->last, get_club_text(j, flags));
            break;
        case NAME_LAYOUT_S_N_C:
            snprintf(buffers[n], 128, "%s %s, %s", 
                     j->last, j->first, get_club_text(j, flags));
            break;
        case NAME_LAYOUT_C_S_N:
            snprintf(buffers[n], 128, "%s, %s %s", 
                     get_club_text(j, flags), j->last, j->first);
            break;
        }
    }

    p = buffers[n];
    if (++n >= 4)
	n = 0;

    return p;
}

const gchar *get_club_text(struct judoka *j, gint flags)
{
    static gchar buffers[4][64];
    static gint n = 0;
    gchar *p;
    struct club_name_data *data = NULL;

    if (j->club)
	data = club_name_get(j->club);

    if (flags & CLUB_TEXT_ABBREVIATION &&
        club_abbr) {
	if ((club_text & CLUB_TEXT_COUNTRY) && 
	    j->country && j->country[0])
	    return j->country;
	if (data && data->abbreviation && data->abbreviation[0])
	    return data->abbreviation;
	return j->club ? j->club : "";
    }

    if ((flags & CLUB_TEXT_ADDRESS) && 
	(club_text & CLUB_TEXT_COUNTRY) == 0) {
	if (data && data->address && data->address[0]) {
	    snprintf(buffers[n], 64, "%s, %s", 
		     j->club ? j->club : "", 
		     data->address);
	    p = buffers[n];
	    if (++n >= 4)
		n = 0;
	    return p;
	}
	return j->club ? j->club : "";
    }

    if (club_text == CLUB_TEXT_CLUB && j->club)
	return j->club;

    if (club_text == CLUB_TEXT_COUNTRY && j->country)
	return j->country;

    if (j->club && (j->country == NULL || j->country[0] == 0))
	return j->club;

    if (j->country && (j->club == NULL || j->club[0] == 0))
	return j->country;

    snprintf(buffers[n], 64, "%s/%s", j->country, j->club);

    p = buffers[n];
    if (++n >= 4)
	n = 0;

    return p;
}

gboolean firstname_lastname(void)
{
    if (IS_LANG_IS)
        return TRUE;

    switch (name_layout) {
    case NAME_LAYOUT_N_S_C:
        return TRUE;
    case NAME_LAYOUT_S_N_C:
    case NAME_LAYOUT_C_S_N:
        return FALSE;
    }

    return FALSE;
}

const gchar *esc_quote(const gchar *txt)
{
    static gchar buffers[16][64];
    static gint n = 0;
    gchar *p;
    gint i = 0;

    if (!txt)
        return NULL;

    while (*txt && i < 61) {
        if (*txt == '"')
            buffers[n][i++] = *txt;

        buffers[n][i++] = *txt++;
    }

    buffers[n][i] = 0;

    p = buffers[n];
    if (++n >= 16)
	n = 0;

    return p;
}
