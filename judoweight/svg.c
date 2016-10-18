/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>

#include <cairo.h>
#include <cairo-pdf.h>
#include <librsvg/rsvg.h>

#include "judoweight.h"

#define WRITE2(_s, _l)                                                  \
    do {if (dfile) fwrite(_s, 1, _l, dfile);                            \
        if (!rsvg_handle_write(handle, (guchar *)_s, _l, &err)) {       \
            g_print("\nERROR %s: %s %d\n",                              \
                    err->message, __FUNCTION__, __LINE__); err = NULL; return TRUE; } } while (0)

#define WRITE1(_s, _l)                                                  \
    do { gint _i; for (_i = 0; _i < _l; _i++) {                         \
        if (_s[_i] == '&')                                              \
            WRITE2("&amp;", 5);                                         \
        else if (_s[_i] == '<')                                         \
            WRITE2("&lt;", 4);                                          \
        else if (_s[_i] == '>')                                         \
            WRITE2("&gt;", 4);                                          \
        else if (_s[_i] == '\'')                                        \
            WRITE2("&apos;", 6);                                        \
        else if (_s[_i] == '"')                                         \
            WRITE2("&quot;", 6);                                        \
        else                                                            \
            WRITE2(&_s[_i], 1);                                         \
        }} while (0)

#define WRITE(_a) WRITE1(_a, strlen(_a))

#define IS_LABEL_CHAR(_x) ((_x >= 'a' && _x <= 'z') || _x == 'C' || _x == 'M' || _x == '#' || _x == '=')
#define IS_VALUE_CHAR(_x) (_x >= '0' && _x <= '9')

#define IS_SAME(_a, _b) (!strcmp((char *)_a, (char *)_b))

static FILE *dfile = NULL;

void read_svg_file(void);

gchar *svg_file = NULL;
gboolean nomarg = FALSE;
gboolean shrink = FALSE;
gboolean scale = FALSE;

static gchar *svg_data = NULL;
static gsize  svg_datalen = 0;
static gint   svg_width;
static gint   svg_height;
static gboolean svg_ok = FALSE;

#define CODE_LEN 16

static struct {
    guchar code[CODE_LEN];
    gint codecnt;
    gint value;
} attr[16];
static gint cnt = 0;

static gchar *get_file_name(void)
{
    GtkWidget *dialog;
    static gchar *last_dir = NULL;
    gint response;

    dialog = gtk_file_chooser_dialog_new (_("Choose a file"),
                                          GTK_WINDOW(main_window),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                                          GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                          NULL);

    if (last_dir)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), last_dir);

    response = gtk_dialog_run (GTK_DIALOG (dialog));

    if (response == GTK_RESPONSE_ACCEPT) {
        g_free(svg_file);
        svg_file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        g_free(last_dir);
        last_dir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (dialog));
        g_key_file_set_string(keyfile, "preferences", "svgfile", svg_file);
    } else if (response == GTK_RESPONSE_CLOSE) {
        g_free(svg_file);
        svg_file = NULL;
        g_key_file_set_string(keyfile, "preferences", "svgfile", "");
    }

    gtk_widget_destroy (dialog);

    return svg_file;
}

void set_svg_file(GtkWidget *menu_item, gpointer data)
{
    get_file_name();
    read_svg_file();
}

void read_svg_file(void)
{
    g_free(svg_data);
    svg_data = NULL;
    svg_datalen = 0;
    svg_ok = FALSE;

    if (svg_file == NULL || svg_file[0] == 0)
        return;
    
    if (!g_file_get_contents(svg_file, &svg_data, &svg_datalen, NULL))
        g_print("CANNOT OPEN '%s'\n", svg_file);
    else  {
        RsvgHandle *h = rsvg_handle_new_from_data((guchar *)svg_data, svg_datalen, NULL);
        if (h) {
            RsvgDimensionData dim;
            rsvg_handle_get_dimensions(h, &dim);
            svg_width = dim.width;
            svg_height = dim.height;
            g_object_unref(h);
            svg_ok = TRUE;
        } else {
            g_print("Cannot open SVG file %s\n", svg_file);
        }
    }

    /*
    nomarg = strstr(svg_file, "nomarg") != NULL;
    shrink = strstr(svg_file, "shrink") != NULL;
    scale = strstr(svg_file, "scale") != NULL;
    */
}

gint write_judoka(RsvgHandle *handle, gint start, struct msg_edit_competitor *j)
{
    gint i;
    GError *err = NULL;
    gchar buf[256];

    for (i = start; i < cnt; i++) {
        if (attr[i].code[0] == '\'') {
            // quoted text
            WRITE1((attr[i].code+1), attr[i].codecnt - 1);
        } else if (IS_SAME(attr[i].code, "first")) {
            WRITE(j->first);
        } else if (IS_SAME(attr[i].code, "last")) {
            WRITE(j->last);
        } else if (IS_SAME(attr[i].code, "club")) {
            WRITE(j->club);
        } else if (IS_SAME(attr[i].code, "country")) {
            WRITE(j->country);
        } else if (IS_SAME(attr[i].code, "weight")) {
            snprintf(buf, sizeof(buf), "%d.%02d", j->weight/1000, (j->weight%1000)/10);
            WRITE(buf);
        } else if (IS_SAME(attr[i].code, "s")) {
            WRITE(" ");
        }
    }

    return 0;
}

gint paint_svg(struct paint_data *pd)
{
    GError *err = NULL;

    if (svg_ok == FALSE)
        return FALSE;
    /*
      if (pd->c) {
      cairo_set_source_rgb(pd->c, 1.0, 1.0, 1.0);
      cairo_rectangle(pd->c, 0.0, 0.0, pd->paper_width, pd->paper_height);
      cairo_fill(pd->c);
      }
    */
    RsvgHandle *handle = rsvg_handle_new();
    
    guchar *p = (guchar *)svg_data;

    //g_print("Open SVG debug file\n");
    //dfile = fopen("debug.svg", "w");

    while(*p) {
        if (*p == '%' && IS_LABEL_CHAR(p[1])) {
            memset(attr, 0, sizeof(attr));
            cnt = 0;
            p++;

            while (IS_LABEL_CHAR(*p) || IS_VALUE_CHAR(*p) || *p == '-' || *p == '\'' || *p == '|' || *p == '!') {
                while (IS_LABEL_CHAR(*p))
                    attr[cnt].code[attr[cnt].codecnt++] = *p++;
                
                if (*p == '-') p++;
            
                while (IS_VALUE_CHAR(*p))
                    attr[cnt].value = attr[cnt].value*10 + *p++ - '0';

                if (*p == '-') p++;

                if (*p == '\'' || *p == '|') {
                    cnt++;
                    p++;
                    attr[cnt].code[0] = '\'';
                    attr[cnt].codecnt = 1;
                    while (*p && *p != '\'' && *p != '|') {
                        attr[cnt].code[attr[cnt].codecnt] = *p++;
                        if (attr[cnt].codecnt < CODE_LEN)
                            attr[cnt].codecnt++;
                    }
                    if (*p == '\'' || *p == '|')
                        p++;
                }

                cnt++;

                if (*p == '!') {
                    p++;
                    break;
                }
            } // while IS_LABEL...

#if 0
            g_print("\n");
            gint i;
            for (i = 0; i < cnt; i++)
                g_print("i=%d code='%s' val=%d\n",
                        i, attr[i].code, attr[i].value);
            g_print("\n");
#endif
            if (attr[0].code[0] == 'c') {
                write_judoka(handle, 1, &(pd->msg));
            } else if (attr[0].code[0] == 'd') {
                gchar buf[32];
                time_t t = time(NULL);
                struct tm *tm = localtime(&t);
                snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
                         tm->tm_year+1900,
                         tm->tm_mon+1,
                         tm->tm_mday,
                         tm->tm_hour,
                         tm->tm_min,
                         tm->tm_sec);
                WRITE(buf);
            }
        } else { // *p != %
            WRITE2(p, 1);
            p++;
        }
    } // while
    
    rsvg_handle_close(handle, NULL);
    
    if (scale && pd->c) {
        cairo_save(pd->c);
        cairo_scale(pd->c, pd->paper_width/svg_width, pd->paper_height/svg_height);
        rsvg_handle_render_cairo(handle, pd->c);
        cairo_restore(pd->c);
    } else if (pd->c) {
        rsvg_handle_render_cairo(handle, pd->c);
    }

    g_object_unref(handle);

    if (dfile) {
        fclose(dfile);
        dfile = NULL;
        g_print("SVG debug file closed\n");
    }

    return TRUE;
}

