/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "sqlite3.h"
#include "judoshiai.h"

#define SIZEX 600
#define SIZEY 400


static gboolean possibly_same_strings(const gchar *s, const gchar *t);

static gboolean delete_event_validate( GtkWidget *widget,
                                       GdkEvent  *event,
                                       gpointer   data )
{
    return FALSE;
}

static void destroy_validate( GtkWidget *widget,
                              gpointer   data )
{
}

#define VALIDATE_ERR_OK     0
#define VALIDATE_ERR_UTF8   1
#define VALIDATE_ERR_SPACES 2

static gint validate_word(const gchar *word)
{
    gint len;
    gchar last;

    if (!word)
        return VALIDATE_ERR_OK;

    len = strlen(word);
    if (len == 0)
        return VALIDATE_ERR_OK;
        
    if (!g_utf8_validate(word, -1, NULL))
        return VALIDATE_ERR_UTF8;

    last = word[len-1];

    if (word[0] == ' ' || word[0] == '\t' ||
        last == ' ' || last == '\t')
        return VALIDATE_ERR_SPACES;

    return VALIDATE_ERR_OK;
}

static void insert_tag(GtkTextBuffer *buffer, gchar *txt, gchar *tag, gint lines)
{
    GtkTextIter start, end;
    gtk_text_buffer_insert_at_cursor(buffer, txt, -1);    
    gint linecnt = gtk_text_buffer_get_line_count(buffer);
    gtk_text_buffer_get_iter_at_line (buffer, &start, linecnt-1-lines);
    gtk_text_buffer_get_iter_at_line (buffer, &end, linecnt-1);                                       
    gtk_text_buffer_apply_tag_by_name (buffer, tag, &start, &end);
}

void db_validation(GtkWidget *w, gpointer data)
{
    gint warnings = 0;
    GtkTextBuffer *buffer;
    GtkWidget *vbox, *result, *ok;
    GtkWindow *window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(GTK_WINDOW(window), _("Database Validation"));
    gtk_widget_set_size_request(GTK_WIDGET(window), SIZEX, SIZEY);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);

#if (GTKVER == 3)
    vbox = gtk_grid_new();
#else
    vbox = gtk_vbox_new(FALSE, 1);
#endif
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 1);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    result = gtk_text_view_new();
    ok = gtk_button_new_with_label(_("OK"));

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 10);
#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
    gtk_container_add(GTK_CONTAINER(scrolled_window), result);
#else
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), result);
#endif

#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(vbox), scrolled_window, 0, 0, 1, 1);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_grid_attach(GTK_GRID(vbox), ok,              0, 1, 1, 1);
#else
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), ok, FALSE, TRUE, 0);
#endif
    gtk_widget_show_all(GTK_WIDGET(window));
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(result));

    PangoFontDescription *font_desc = pango_font_description_from_string("Courier 10");
#if (GTKVER == 3)
    gtk_widget_override_font(GTK_WIDGET(result), font_desc);
#else
    gtk_widget_modify_font(GTK_WIDGET(result), font_desc);
#endif
    pango_font_description_free (font_desc);

    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event_validate), NULL);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (destroy_validate), NULL);

    g_signal_connect_swapped (ok, "clicked",
			      G_CALLBACK (gtk_widget_destroy),
                              window);

    gtk_text_view_set_editable(GTK_TEXT_VIEW(result), FALSE);

    gtk_text_buffer_set_text(buffer, "", -1);
    gtk_text_buffer_create_tag (buffer, "bold", 
                                "weight", PANGO_WEIGHT_BOLD, NULL); 
    gtk_text_buffer_create_tag (buffer, "red", 
                                "foreground", "red", NULL); 

    // database queries
    gint row, rows;

    // find double seedings
    rows = db_get_table("select category,seeding,count(*) from competitors "
                        "where deleted&1=0 "
                        "group by category,seeding having seeding>0 and count(*)>1");

    if (rows > 0) {
        insert_tag(buffer, _("Duplicate seedings:\n"), "bold", 1);
        for (row = 0; row < rows; row++) {
            gchar *cat = db_get_data(row, "category");
            gchar *sed = db_get_data(row, "seeding");
            gchar *cnt = db_get_data(row, "count(*)");
            gchar *txt = g_strdup_printf("  %s %s: %s %s %s %s %s.\n", 
                                         _("Category"), cat,
                                         _("seeding"), sed, _("defined"), cnt, _("times"));

            gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
            g_free(txt);
            warnings++;
        }
    }
    if (rows >= 0)
        db_close_table();
        
    // find double club seedings
    rows = db_get_table("select club,clubseeding,count(*) from competitors "
                        "where deleted&1=0 "
                        "group by club,clubseeding having clubseeding>0 and count(*)>1");

    if (rows > 0) {
        insert_tag(buffer, _("Duplicate club seedings:\n"), "bold", 1);
        for (row = 0; row < rows; row++) {
            gchar *club = db_get_data(row, "club");
            gchar *sed = db_get_data(row, "clubseeding");
            gchar *cnt = db_get_data(row, "count(*)");
            gchar *txt = g_strdup_printf("  %s %s: %s %s %s %s %s.\n", 
                                         _("Club"), club,
                                         _("seeding"), sed, _("defined"), cnt, _("times"));

            gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
            g_free(txt);
            warnings++;
        }
    }
    if (rows >= 0)
        db_close_table();
        
    // not defined categories
    rows = db_get_table("select category from categories "
                        "where not exists (select agetext || weighttext as wclass from catdef "
                        "where categories.category=wclass)");

    if (rows > 0) {
        insert_tag(buffer, _("Missing category definitions:\n"), "bold", 1);
        for (row = 0; row < rows; row++) {
            gchar *cat = db_get_data(row, "category");
            gchar *txt = g_strdup_printf("  %s %s %s %s.\n", 
                                         _("Category"), cat, _("is not"), _("defined"));

            gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
            g_free(txt);
            warnings++;
        }
    }
    if (rows >= 0)
        db_close_table();
        
    // badly written names
    rows = db_get_table("select * from competitors where deleted&1=0");

    if (rows > 0) {
        gboolean hdr_printed = FALSE;
        for (row = 0; row < rows; row++) {
            gint col = 0;
            gchar *word;
            while ((word = db_get_row_col_data(row, col))) {
                gint val = validate_word(word);
                if (val) {
                    warnings++;
                    if (!hdr_printed) {
                        insert_tag(buffer, _("Typing errors:\n"), "bold", 1);
                        hdr_printed = TRUE;
                    }

                    gchar *txt = g_strdup_printf("  %s (%s): ",
                                                 val == VALIDATE_ERR_UTF8 ? _("UTF-8 errors") : _("Extra spaces"), 
                                                 db_get_row_col_data(-1, col));
                    gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
                    g_free(txt);

                    col = 0;
                    while ((word = db_get_row_col_data(row, col))) {
                        gtk_text_buffer_insert_at_cursor(buffer, "'", -1);
                        if (g_utf8_validate(word, -1, NULL))
                            gtk_text_buffer_insert_at_cursor(buffer, word, -1);
                        else
                            gtk_text_buffer_insert_at_cursor(buffer, "!*!*!", -1);
                        gtk_text_buffer_insert_at_cursor(buffer, "' ", -1);
                        col++;
                    }
                    gtk_text_buffer_insert_at_cursor(buffer, "\n", -1);

                    if (val == VALIDATE_ERR_UTF8)
                        insert_tag(buffer, "", "red", 1);
                    break;
                }
                col++;
            }
        }

        // check for duplicates
        hdr_printed = FALSE;
        for (row = 0; row < (rows-1); row++) {
            gchar *last = db_get_data(row, "last");
            gchar *first = db_get_data(row, "first");
            gint row2;
            for (row2 = row+1; row2 < rows; row2++) {
                gchar *last2 = db_get_data(row2, "last");
                gchar *first2 = db_get_data(row2, "first");
                if (possibly_same_strings(last, last2) &&
                    possibly_same_strings(first, first2)) {
                    gchar *cat = db_get_data(row, "category");
                    gchar *cat2 = db_get_data(row2, "category");
                    gchar *txt = g_strdup_printf("  * %s: '%s' '%s'\n    %s: '%s' '%s'\n", 
                                                 cat, last, first, cat2, last2, first2);
                    if (!hdr_printed) {
                        insert_tag(buffer, _("Possible duplicate competitors:\n"), "bold", 1);
                        hdr_printed = TRUE;
                    }
                    if (cat && cat[0] != '?' && cat2 && cat2[0] != '?' && 
                        strcmp(cat, cat2) == 0)
                        insert_tag(buffer, txt, "red", 2);
                    else
                        gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
                    g_free(txt);
                    warnings++;
                }
            }
        }
    }
    if (rows >= 0)
        db_close_table();

    // differently written club names
    rows = db_get_table("select distinct club from competitors where deleted&1=0");

    if (rows > 1) {
        gboolean hdr_printed = FALSE;
        for (row = 0; row < (rows-1); row++) {
            gint row2;
            gchar *club = db_get_data(row, "club");
            for (row2 = row+1; row2 < rows; row2++) {
                gchar *club2 = db_get_data(row2, "club");
                if (possibly_same_strings(club, club2)) {
                    gchar *txt = g_strdup_printf("  '%s' = '%s'?\n", club, club2);
                    if (!hdr_printed) {
                        insert_tag(buffer, _("Possible duplicate club names:\n"), "bold", 1);
                        hdr_printed = TRUE;
                    }
                    gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
                    g_free(txt);
                    warnings++;
                }
            }
        }
    }
    if (rows >= 0)
        db_close_table();

    // find suspicious weights
    rows = db_get_table("select last,first,category,regcategory,weight from competitors "
                        "where weight>0 and "
                        "( weight<10000 or weight>200000 or "
                        "  ( getweight(category)=0 and getweight(regcategory)>0 and "
                        "    ( ( regcategory not like '%+%' and "
                        "        weight>getweight(regcategory)) or "
                        "        weight<getweight(regcategory)-10000)) or "
                        "  ( getweight(category)>0 and "
                        "    ( ( category not like '%+%' and "
                        "        weight>getweight(category)) or "
                        "      weight<getweight(category)-10000))) order by last, first");

    if (rows > 0) {
        insert_tag(buffer, _("Suspicious weights:\n"), "bold", 1);
        for (row = 0; row < rows; row++) {
            gchar *cat = db_get_data(row, "category");
            gchar *rcat = db_get_data(row, "regcategory");
            gchar *last = db_get_data(row, "last");
            gchar *first = db_get_data(row, "first");
            gint   weight = atoi(db_get_data(row, "weight"));
            gchar *txt = g_strdup_printf("  %s %s:  %s=%s  %s=%s  %s=%d.%02d kg\n", 
                                         last, first, _("Category"), cat, _("Reg. Category"), rcat, 
                                         _("Weight"), weight/1000, (weight%1000)/10);

            gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
            g_free(txt);
            warnings++;
        }
    }
    if (rows >= 0)
        db_close_table();

    // find corrupted competitors
    rows = db_get_table("select * from competitors where \"belt\"<0 or \"index\">=10000 or "
                        "\"birthyear\"<0 or \"weight\"<0 or \"visible\"<>1 or \"seeding\"<0 or "
                        "\"clubseeding\"<0");

    if (rows > 0) {
        insert_tag(buffer, _("Corrupted competitor data:\n"), "bold", 1);
        for (row = 0; row < rows; row++) {
            gchar *club = db_get_data(row, "club");
            gchar *first = db_get_data(row, "first");
            gchar *last = db_get_data(row, "last");
            gchar *txt = g_strdup_printf("  %s, %s (%s).\n", last, first, club );

            gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
            g_free(txt);
            warnings++;
        }
    }
    if (rows >= 0)
        db_close_table();

    // find corrupted categories
    rows = db_get_table("select * from competitors where 'index'<10000 or \"tatami\"<0 or "
                        "\"group\"<0 or \"system\"<0 or "
                        "\"numcomp\"<0 or \"table\"<0 or \"wishsys\"<0 or "
                        "\"pos1\"<0 or \"pos2\"<0 or \"pos3\"<0 or \"pos4\"<0 or "
                        "\"pos5\"<0 or \"pos6\"<0 or \"pos7\"<0 or \"pos8\"<0");

    if (rows > 0) {
        insert_tag(buffer, _("Corrupted category data:\n"), "bold", 1);
        for (row = 0; row < rows; row++) {
            gchar *cat = db_get_data(row, "category");
            gchar *txt = g_strdup_printf("  %s.\n", cat);

            gtk_text_buffer_insert_at_cursor(buffer, txt, -1);
            g_free(txt);
            warnings++;
        }
    }
    if (rows >= 0)
        db_close_table();


    gchar *txt = g_strdup_printf("\n* %d %s. *\n", warnings, _("warnings")); 
    insert_tag(buffer, txt, "bold", 1);
    g_free(txt);
}


static gint min3(gint a, gint b, gint c)
{
    gint mi = a;
    if (b < mi) mi = b;
    if (c < mi) mi = c;
    return mi;
}

static gint *get_cell_pointer(gint *pOrigin, gint col, gint row, gint nCols)
{
    return pOrigin + col + (row * (nCols + 1));
}

static gint get_at(gint *pOrigin, gint col, gint row, gint nCols)
{
    gint *pCell = get_cell_pointer (pOrigin, col, row, nCols);
    return *pCell;
}

static void put_at(gint *pOrigin, gint col, gint row, gint nCols, gint x)
{
    gint *pCell = get_cell_pointer (pOrigin, col, row, nCols);
    *pCell = x;
}

static gboolean possibly_same_strings(const gchar *s1, const gchar *s2) 
{
     /* Compute Levenshtein distance */
    gchar *s, *t, *n1, *n2;
    gint *d; // pointer to matrix
    gint n; // length of s
    gint m; // length of t
    gint i; // iterates through s
    gint j; // iterates through t
    gunichar s_i; // ith character of s
    gunichar t_j; // jth character of t
    gint cost; // cost
    gint result; // result
    gint cell; // contents of target cell
    gint above; // contents of cell immediately above
    gint left; // contents of cell immediately to left
    gint diag; // contents of cell immediately above and to left
    gint sz; // number of cells in matrix

    s = n1 = g_utf8_casefold(s1, -1);
    t = n2 = g_utf8_casefold(s2, -1);
    n = g_utf8_strlen(s, -1);
    m = g_utf8_strlen(t, -1);

    if (n == 0) {
        g_free(n1); g_free(n2);
        return (m == 0);
    }
    if (m == 0) {
        g_free(n1); g_free(n2);
        return (n == 0);
    }
    sz = (n+1) * (m+1) * sizeof (gint);
    d = (gint *) g_malloc (sz);

    for (i = 0; i <= n; i++)
        put_at (d, i, 0, n, i);

    for (j = 0; j <= m; j++)
        put_at (d, 0, j, n, j);

    for (i = 1; i <= n; i++) {
        s_i = g_utf8_get_char_validated(s, -1);
        s = g_utf8_find_next_char(s, NULL);
        t = n2;
        for (j = 1; j <= m; j++) {
            t_j = g_utf8_get_char_validated(t, -1);
            t = g_utf8_find_next_char(t, NULL);

            if (s_i == t_j)
                cost = 0;
            else
                cost = 1;

            above = get_at (d,i-1,j, n);
            left = get_at (d,i, j-1, n);
            diag = get_at (d, i-1,j-1, n);
            cell = min3(above + 1, left + 1, diag + cost);
            put_at (d, i, j, n, cell);
        }
    }

    result = get_at (d, n, m, n);
    g_free(d);
    g_free(n1); g_free(n2);

    if (m == n && m <= 3) // country names can differ by a character
        return (result == 0);
    
    if ((m+n) <= 10)
        return (result <= 1);

    return (result <= 2);
}
