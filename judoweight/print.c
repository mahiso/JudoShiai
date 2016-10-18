/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gtk/gtk.h>

#include <cairo.h>
#include <cairo-pdf.h>

#include "judoweight.h"

#define MY_FONT "Arial"
#define TEXT_HEIGHT 14

static void draw_page(GtkPrintOperation *operation,
                      GtkPrintContext   *context,
                      gint               page_nr,
                      gpointer           user_data)
{
    gint i;
    gchar **texts = user_data;
    GtkPageSetup *setup = gtk_print_context_get_page_setup(context);
    cairo_t *c = gtk_print_context_get_cairo_context(context);
    /*
    gtk_page_setup_set_top_margin(setup, 0.0, GTK_UNIT_POINTS);
    gtk_page_setup_set_bottom_margin(setup, 0.0, GTK_UNIT_POINTS);
    gtk_page_setup_set_left_margin(setup, 0.0, GTK_UNIT_POINTS);
    gtk_page_setup_set_right_margin(setup, 0.0, GTK_UNIT_POINTS);
    */
    gdouble top = gtk_page_setup_get_top_margin(setup, GTK_UNIT_POINTS);
    gdouble left = gtk_page_setup_get_left_margin(setup, GTK_UNIT_POINTS);
    //gdouble bot = gtk_page_setup_get_bottom_margin(setup, GTK_UNIT_POINTS);
    //gdouble right = gtk_page_setup_get_right_margin(setup, GTK_UNIT_POINTS);

    /*
    gdouble paper_width = gtk_print_context_get_width(context);
    gdouble paper_height = gtk_print_context_get_height(context);

    gdouble paper_width_mm = gtk_page_setup_get_paper_width(setup, GTK_UNIT_MM) -
        gtk_page_setup_get_left_margin(setup, GTK_UNIT_MM) -
        gtk_page_setup_get_right_margin(setup, GTK_UNIT_MM);

    gdouble paper_height_mm = gtk_page_setup_get_paper_height(setup, GTK_UNIT_MM) -
        gtk_page_setup_get_top_margin(setup, GTK_UNIT_MM) -
        gtk_page_setup_get_bottom_margin(setup, GTK_UNIT_MM);
    */
    cairo_text_extents_t extents;
    gdouble ypos = top;

    cairo_select_font_face(c, MY_FONT,
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(c, TEXT_HEIGHT);
    cairo_set_source_rgb(c, 0.0, 0.0, 0.0);

    for (i = 0; texts[i]; i++) {
        cairo_text_extents(c, texts[i], &extents);
        cairo_move_to(c, left, ypos);
        cairo_show_text(c, texts[i]);
        ypos += extents.height + 5;
    }
}

static void draw_page_svg(GtkPrintOperation *operation,
                          GtkPrintContext   *context,
                          gint               page_nr,
                          gpointer           user_data)
{
    struct paint_data *pd = user_data;
    GtkPageSetup *setup = gtk_print_context_get_page_setup(context);
    
    if (nomarg) {
        gtk_page_setup_set_top_margin(setup, 0.0, GTK_UNIT_POINTS);
        gtk_page_setup_set_bottom_margin(setup, 0.0, GTK_UNIT_POINTS);
        gtk_page_setup_set_left_margin(setup, 0.0, GTK_UNIT_POINTS);
        gtk_page_setup_set_right_margin(setup, 0.0, GTK_UNIT_POINTS);
    }
#if 0
    gdouble top = gtk_page_setup_get_top_margin(setup, GTK_UNIT_POINTS);
    gdouble left = gtk_page_setup_get_left_margin(setup, GTK_UNIT_POINTS);
    gdouble bot = gtk_page_setup_get_bottom_margin(setup, GTK_UNIT_POINTS);
    gdouble right = gtk_page_setup_get_right_margin(setup, GTK_UNIT_POINTS);
#endif
    pd->c = gtk_print_context_get_cairo_context(context);
    pd->paper_width = gtk_print_context_get_width(context);
    pd->paper_height = gtk_print_context_get_height(context);
#if 0
    g_print("SVG DRAW nomarg=%d scale=%d page=%fx%f Margins=%f-%f/%f-%f\n", 
            nomarg, scale, pd->paper_width, pd->paper_height, top,bot, left,right);
#endif
    paint_svg(pd);
}

static void begin_print(GtkPrintOperation *operation,
                        GtkPrintContext   *context,
                        gpointer           user_data)
{
    gtk_print_operation_set_n_pages(operation, 1);
}

void do_print(gpointer userdata)
{
    GtkPrintOperation *print;

    print = gtk_print_operation_new();

    g_signal_connect (print, "begin_print", G_CALLBACK (begin_print), userdata);
    g_signal_connect (print, "draw_page", G_CALLBACK (draw_page), userdata);

    gtk_print_operation_set_use_full_page(print, FALSE);
    gtk_print_operation_set_unit(print, GTK_UNIT_POINTS);
    
    gtk_print_operation_run(print, GTK_PRINT_OPERATION_ACTION_PRINT,
                            GTK_WINDOW (main_window), NULL);

    g_object_unref(print);
}

void do_print_svg(struct paint_data *pd)
{
    GtkPrintOperation *print;

    print = gtk_print_operation_new();

    g_signal_connect (print, "begin_print", G_CALLBACK (begin_print), pd);
    g_signal_connect (print, "draw_page", G_CALLBACK (draw_page_svg), pd);

    gtk_print_operation_set_use_full_page(print, FALSE);
    gtk_print_operation_set_unit(print, GTK_UNIT_POINTS);
    
    gtk_print_operation_run(print, GTK_PRINT_OPERATION_ACTION_PRINT,
                            GTK_WINDOW (main_window), NULL);

    g_object_unref(print);
}

