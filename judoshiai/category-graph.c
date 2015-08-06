/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "judoshiai.h"

extern gboolean mirror_display;

static gboolean mouse_click(GtkWidget *sheet_page, 
			    GdkEventButton *event, 
			    gpointer userdata);
static gboolean motion_notify(GtkWidget *sheet_page, 
                              GdkEventMotion *event, 
			      gpointer userdata);
static gboolean release_notify(GtkWidget *sheet_page, 
			       GdkEventButton *event, 
			       gpointer userdata);
static gboolean cannot_move_category(gint cat);
static cairo_surface_t *get_image(const gchar *name);


#define THIN_LINE     (paper_width < 700.0 ? 1.0 : paper_width/700.0)
#define THICK_LINE    (2*THIN_LINE)

#define W(_w) ((_w)*paper_width)
#define H(_h) ((_h)*paper_height)

#define BOX_HEIGHT (1.4*extents.height > n*mt/30.0 ? 1.4*extents.height : n*mt/30.0)

#define NUM_RECTANGLES 1000

static struct {
    double x1, y1, x2, y2;
    gint   index;
    gint   tatami;
    gint   group;
} point_click_areas[NUM_RECTANGLES];	

static gint num_rectangles;
static GdkCursor *hand_cursor = NULL;

static gboolean button_drag = FALSE;
static gint     dragged_cat = 0;
static gchar    dragged_text[32];
static gint     start_box = 0;
static gdouble  dragged_x, dragged_y;

#define NO_SCROLL   0
#define SCROLL_DOWN 1
#define SCROLL_UP   2
static gint     scroll_up_down = 0;
static cairo_surface_t *anchor = NULL;
static GtkWidget *cat_graph_label = NULL;

static struct win_collection {
    GtkWidget *scrolled_window;
    GtkWidget *darea;
} w;

static void refresh_darea(void)
{
    gtk_widget_queue_draw(w.darea);
    //gtk_widget_queue_draw_area(w.darea, 0, 0, 600, 2000);
}
#define refresh_window refresh_darea

void draw_gategory_graph(void)
{
    refresh_darea();
}

static void paint(cairo_t *c, gdouble paper_width, gdouble paper_height, gpointer userdata)
{
    gint i, matches_left, matches_time;
    cairo_text_extents_t extents;
    gdouble       y_pos = 0.0, colwidth = W(1.0/(number_of_tatamis + 1));
    gint          max_group;
    struct tm    *tm;
    time_t        secs;
    cairo_surface_t *cs = userdata;

    if (!anchor)
        anchor = get_image("anchor.png");

    cairo_set_font_size(c, 12);
    cairo_text_extents(c, "Hj:000", &extents);

    if (cs) {
        cairo_set_source_surface(c, cs, 0, 0);
        cairo_paint(c);
        goto drag;
    }

    num_rectangles = 0;

    cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
    cairo_rectangle(c, 0.0, 0.0, paper_width, paper_height);
    cairo_fill(c);

    cairo_set_line_width(c, THIN_LINE);
    cairo_set_source_rgb(c, 0.0, 0.0, 0.0);

    y_pos = extents.height*1.4;

    for (i = 0; i <= number_of_tatamis; i++) {
        gdouble left = (i)*colwidth;
        gdouble right = (i+1)*colwidth;
        gchar buf[30];
        gint old_group = 0;
        struct category_data *catdata = category_queue[i].next;

        if (mirror_display && i) {
            left = (number_of_tatamis - i + 1)*colwidth;
            right = (number_of_tatamis - i + 2)*colwidth;
        }
		
        matches_left = 0;
        matches_time = 0;
        max_group = 0;

        y_pos = extents.height*1.4;

        cairo_set_line_width(c, THIN_LINE);
        cairo_set_source_rgb(c, 0.0, 0.0, 0.0);

        if (catdata)
            old_group = catdata->group;

        while (catdata) {
            gint n = 1;
            gdouble x;

            gint mt = get_category_match_time(catdata->category);
            if (mt < 180)
                mt = 180;

            if (catdata->group != old_group) {
                cairo_save(c);
                cairo_set_line_width(c, THICK_LINE);
                cairo_set_source_rgb(c, 0.0, 0.0, 1.0);
                cairo_move_to(c, left, y_pos);
                cairo_line_to(c, right, y_pos);
                cairo_stroke(c);

                if (matches_left) {
                    /* write time */
                    cairo_rectangle(c, right - extents.width, 
                                    y_pos - extents.height, 
                                    extents.width, extents.height);
                    cairo_fill(c);

                    secs = time(NULL) + matches_time;
                    tm = localtime(&secs);
				
                    cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
                    sprintf(buf, "%02d:%02d", tm->tm_hour, tm->tm_min);
                    cairo_move_to(c, right - extents.width, y_pos);
                    cairo_show_text(c, buf);
                }

                cairo_restore(c);

                y_pos += 1.0;
                old_group = catdata->group;
            }

            if (catdata->group > max_group)
                max_group = catdata->group;

            x = n = 0;

            if (catdata->match_count > 0) {
                x = catdata->matched_matches_count*colwidth/catdata->match_count;
                //n = catdata->match_count - catdata->matched_matches_count;
            }

            GtkTreeIter tmp_iter;
            if (find_iter(&tmp_iter, catdata->index)) {
                gint k = gtk_tree_model_iter_n_children(current_model, &tmp_iter);
                n = num_matches_left(catdata->index, k);
            }

            if (n == 0)
                goto loop;

            cairo_save(c);
            cairo_set_source_rgb(c, 0.0, 1.0, 0.0);
            cairo_rectangle(c, left, y_pos, x, BOX_HEIGHT);
            cairo_fill(c);

            if (catdata->match_count == 0)
                cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
            else if (catdata->matched_matches_count == 0)
                cairo_set_source_rgb(c, 1.0, 1.0, 0.0);
            else
                cairo_set_source_rgb(c, 1.0, 0.65, 0.0);

            cairo_rectangle(c, left+x, y_pos, colwidth - x, BOX_HEIGHT);
            cairo_fill(c);

            if (cannot_move_category(catdata->index)) {
                cairo_set_source_surface(c, anchor, left + colwidth/2, y_pos);
                cairo_paint(c);
            }

            cairo_restore(c);

            gint mul = 1;
            if (catdata->deleted & TEAM_EVENT) {
                mul = find_num_weight_classes(catdata->category);
                if (mul == 0) mul = 1;
            }

            matches_left += n*mul;//catdata->match_count - catdata->matched_matches_count;
            matches_time += n*mt*mul;

            struct judoka *j = get_data(catdata->index);
            if (j) {
                cairo_move_to(c, left+5, y_pos+extents.height);
                cairo_show_text(c, j->last);
                free_judoka(j);
            }

            point_click_areas[num_rectangles].index = catdata->index;
            point_click_areas[num_rectangles].group = catdata->group;
            point_click_areas[num_rectangles].tatami = i;
            point_click_areas[num_rectangles].x1 = left;
            point_click_areas[num_rectangles].y1 = y_pos;
            point_click_areas[num_rectangles].x2 = right;
            y_pos += BOX_HEIGHT;
            point_click_areas[num_rectangles].y2 = y_pos;
            if (num_rectangles < NUM_RECTANGLES-1)
                num_rectangles++;

            cairo_move_to(c, left, y_pos);
            cairo_line_to(c, right, y_pos);
            cairo_stroke(c);


        loop:
            catdata = catdata->next;
        } // while cat queue

        cairo_save(c);
        cairo_set_line_width(c, THICK_LINE);
        cairo_set_source_rgb(c, 0.0, 0.0, 1.0);
        cairo_move_to(c, left, y_pos);
        cairo_line_to(c, right, y_pos);
        cairo_stroke(c);

        if (matches_left) {
            /* write time */
            cairo_rectangle(c, right - extents.width, 
                            y_pos - extents.height, 
                            extents.width, extents.height);
            cairo_fill(c);

            secs = time(NULL) + matches_time;
            tm = localtime(&secs);
				
            cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
            sprintf(buf, "%02d:%02d", tm->tm_hour, tm->tm_min);
            cairo_move_to(c, right - extents.width, y_pos);
            cairo_show_text(c, buf);
        }

        cairo_restore(c);
        y_pos += 1.0;

        cairo_move_to(c, 10 + left, extents.height);
        if (i == 0)
            sprintf(buf, "%s  [%d]", _("Unlocated"), matches_left);
        else
            sprintf(buf, "Tatami %d  [%d]", i, matches_left);
        cairo_show_text(c, buf);

        point_click_areas[num_rectangles].index = 0;
        point_click_areas[num_rectangles].group = max_group+1;
        point_click_areas[num_rectangles].tatami = i;
        point_click_areas[num_rectangles].x1 = left;
        point_click_areas[num_rectangles].y1 = y_pos;
        point_click_areas[num_rectangles].x2 = right;
        point_click_areas[num_rectangles].y2 = H(1.0);
        if (num_rectangles < NUM_RECTANGLES-1)
            num_rectangles++;

        if (i) {
            cairo_save(c);
            cairo_set_line_width(c, THICK_LINE);
            cairo_set_source_rgb(c, 0.0, 0.0, 0.0);
            cairo_move_to(c, left, 0);
            cairo_line_to(c, left, H(1.0));
            cairo_stroke(c);
            cairo_restore(c);
        }
    } // for tatamis

    cairo_save(c);
    cairo_set_line_width(c, THICK_LINE);
    cairo_set_source_rgb(c, 0.0, 0.0, 0.0);
    cairo_move_to(c, 0, extents.height*1.4);
    cairo_line_to(c, W(1.0), extents.height*1.4);
    cairo_stroke(c);
    cairo_restore(c);

 drag:	
    if (button_drag) {
        cairo_set_line_width(c, THIN_LINE);
        cairo_text_extents(c, dragged_text, &extents);
        cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
        cairo_rectangle(c, dragged_x - extents.width/2.0, dragged_y - extents.height,
                        extents.width + 4, extents.height + 4);
        cairo_fill(c);
        cairo_set_source_rgb(c, 0, 0, 0);
        cairo_rectangle(c, dragged_x - extents.width/2.0 - 1, dragged_y - extents.height - 1,
                        extents.width + 4, extents.height + 4);
        cairo_stroke(c);
        cairo_move_to(c, dragged_x - extents.width/2.0, dragged_y);
        cairo_show_text(c, dragged_text);
    }
}

static gboolean expose_scrolled(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
    static time_t last = 0;
    time_t now = time(NULL);
    if (now > last) 
        gtk_widget_queue_draw(w.darea);
    last = now;
    return FALSE;
}

/* This is called when we need to draw the windows contents */
static gboolean expose(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
    //static gint cnt = 0; 
    //g_print("CATEGORY GRAPH: expose %d\n", cnt++);
#if (GTKVER == 3)
    cairo_t *c = (cairo_t *)event;
    paint(c, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget), NULL);
    return FALSE;
#else
    cairo_t *c = gdk_cairo_create(widget->window);
#endif
    static cairo_surface_t *cs = NULL;
    static gint oldw = 0, oldh = 0;

#if (GTKVER == 3)
    if (cs && (oldw != gtk_widget_get_allocated_width(widget) || 
               oldh != gtk_widget_get_allocated_height(widget))) {
#else
    if (cs && (oldw != widget->allocation.width || oldh != widget->allocation.height)) {
#endif
        cairo_surface_destroy(cs);
        cs = NULL;
    }

    if (!cs) {
#if (GTKVER == 3)
        cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                        gtk_widget_get_allocated_width(widget), 
                                        gtk_widget_get_allocated_height(widget));
        oldw = gtk_widget_get_allocated_width(widget);
        oldh = gtk_widget_get_allocated_height(widget);
#else
        cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                        widget->allocation.width, widget->allocation.height);
        oldw = widget->allocation.width;
        oldh = widget->allocation.height;
#endif
    }

    if (button_drag) {
#if (GTKVER == 3)
        paint(c, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget), cs);
#else
        paint(c, widget->allocation.width, widget->allocation.height, cs);
#endif
    } else {
        cairo_t *c1 = cairo_create(cs);
#if (GTKVER == 3)
        paint(c1, gtk_widget_get_allocated_width(widget), gtk_widget_get_allocated_height(widget), NULL);
#else
        paint(c1, widget->allocation.width, widget->allocation.height, NULL);
#endif
        cairo_set_source_surface(c, cs, 0, 0);
        cairo_paint(c);
        cairo_destroy(c1);
    }

    cairo_show_page(c);
    cairo_destroy(c);
    //cairo_surface_destroy(cs);

    return FALSE;
}

static gint scroll_callback(gpointer userdata)
{
    struct win_collection *w = userdata;

    if (button_drag == FALSE || scroll_up_down == NO_SCROLL)
        return TRUE;

    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(w->scrolled_window));
    gdouble adjnow = gtk_adjustment_get_value(adj);
	
    if (scroll_up_down == SCROLL_DOWN)
        gtk_adjustment_set_value(adj, adjnow + 50.0);
    else if (scroll_up_down == SCROLL_UP)
        gtk_adjustment_set_value(adj, adjnow - 50.0);
	
    return TRUE;
}


void set_category_graph_page(GtkWidget *notebook)
{
    wait_cursor = gdk_cursor_new(GDK_HAND1);

    w.scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(w.scrolled_window), 10);

    w.darea = gtk_drawing_area_new();
    gtk_widget_set_size_request(w.darea, 600, 6000);

#if (GTKVER != 3)
    GTK_WIDGET_SET_FLAGS(w.darea, GTK_CAN_FOCUS);
#endif
    gtk_widget_add_events(w.darea, 
                          GDK_BUTTON_PRESS_MASK | 
                          GDK_BUTTON_RELEASE_MASK |
                          /*GDK_POINTER_MOTION_MASK |*/
                          GDK_POINTER_MOTION_HINT_MASK |
                          GDK_BUTTON_MOTION_MASK);
	

    /* pack the table into the scrolled window */
#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
    gtk_container_add(GTK_CONTAINER(w.scrolled_window), w.darea);
#else
    gtk_scrolled_window_add_with_viewport (
        GTK_SCROLLED_WINDOW(w.scrolled_window), w.darea);
#endif

    cat_graph_label = gtk_label_new (_("Categories"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), w.scrolled_window, cat_graph_label);

    gtk_widget_show(w.darea);

#if (GTKVER == 3)
    g_signal_connect(G_OBJECT(w.scrolled_window), 
                     "draw", G_CALLBACK(expose_scrolled), NULL);
    g_signal_connect(G_OBJECT(w.darea), 
                     "draw", G_CALLBACK(expose), w.darea);
#else
    g_signal_connect(G_OBJECT(w.darea), 
                     "expose-event", G_CALLBACK(expose), w.darea);
#endif
    g_signal_connect(G_OBJECT(w.darea), 
                     "button-press-event", G_CALLBACK(mouse_click), NULL);
#if 0
    g_signal_connect(G_OBJECT(w.darea), 
                     "screen-changed", G_CALLBACK(screen_changed), NULL)
#endif

        g_signal_connect(G_OBJECT(w.darea), 
			 "button-release-event", G_CALLBACK(release_notify), NULL);
    g_signal_connect(G_OBJECT(w.darea), 
                     "motion-notify-event", G_CALLBACK(motion_notify), &w);

    g_timeout_add(500, scroll_callback, &w);
}

static gint find_box(gdouble x, gdouble y)
{
    gint t;

    for (t = 0; t < num_rectangles; t++) {
        if (x >= point_click_areas[t].x1 &&
            x <= point_click_areas[t].x2 &&
            y >= point_click_areas[t].y1 &&
            y <= point_click_areas[t].y2) {
            return t;
        }
    }
    return -1;
}

static gboolean mouse_click(GtkWidget *sheet_page, 
			    GdkEventButton *event, 
			    gpointer userdata)
{
#if (GTKVER == 3)
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), hand_cursor);
#else
    gdk_window_set_cursor(GTK_WIDGET(main_window)->window, hand_cursor);
#endif
    button_drag = FALSE;

    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_PRESS &&  
        (event->button == 1 || event->button == 3)) {
        gdouble x = event->x, y = event->y; 
        gint t;

        dragged_x = x;
        dragged_y = y;

        t = find_box(x, y);
        if (t < 0)
            return FALSE;

        if (cannot_move_category(point_click_areas[t].index))
            return FALSE;

        button_drag = TRUE;
        dragged_cat = point_click_areas[t].index;
        if (dragged_cat < 10000)
            button_drag = FALSE;
        start_box = t;

        struct category_data *catdata = avl_get_category(dragged_cat);
        if (catdata)
            snprintf(dragged_text, sizeof(dragged_text), "%s -> ?",
                     catdata->category);

        refresh_window();

        return TRUE;

    }
    return FALSE;
}

static gboolean motion_notify(GtkWidget *sheet_page, 
                              GdkEventMotion *event, 
                              gpointer userdata)
{
    struct win_collection *w = userdata;

    if (button_drag == FALSE)
        return FALSE;

    gdouble x = event->x, y = event->y; 
    gint t;
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(w->scrolled_window));
    gdouble adjnow = gtk_adjustment_get_value(adj);
	
#if (GTKVER == 3)
    if (y - adjnow > gtk_widget_get_allocated_height(w->scrolled_window) - 50) {
#else
    if (y - adjnow > w->scrolled_window->allocation.height - 50) {
#endif
        scroll_up_down = SCROLL_DOWN;
    } else if (y - adjnow < 20.0) {
        scroll_up_down = SCROLL_UP;
    } else
        scroll_up_down = NO_SCROLL;

    dragged_x = x;
    dragged_y = y;

    t = find_box(x, y);
    if (t < 0)
        return FALSE;

    struct category_data *catdata = avl_get_category(dragged_cat);
    if (catdata && t >= 0)
        snprintf(dragged_text, sizeof(dragged_text), "%s -> T%d:%d",
                 catdata->category, point_click_areas[t].tatami, 
                 point_click_areas[t].group);
    refresh_window();

    return FALSE;
}

static gboolean release_notify(GtkWidget *sheet_page, 
			       GdkEventButton *event, 
			       gpointer userdata)
{
#if (GTKVER == 3)
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), NULL);
#else
    gdk_window_set_cursor(GTK_WIDGET(main_window)->window, NULL);
#endif
    if (!button_drag)
        return FALSE;

    button_drag = FALSE;

    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_RELEASE &&  
        (event->button == 1 || event->button == 3)) {
        gdouble x = event->x, y = event->y; 
        gint t;

        t = find_box(x, y);
        if (t < 0)
            return FALSE;

        struct judoka *j = get_data(dragged_cat);
        if (!j)
            return FALSE;

        j->index = dragged_cat;
        j->belt = point_click_areas[t].tatami;
        j->birthyear = point_click_areas[t].group;
        db_update_category(j->index, j);
        if (display_one_judoka(j) >= 0)
            g_print("Error: %s %d\n", __FUNCTION__, __LINE__);

        category_refresh(j->index);
        update_category_status_info_all();

        free_judoka(j);

        refresh_window();

        draw_match_graph();

        return TRUE;
    }

    return FALSE;
}

static gboolean cannot_move_category(gint cat) {
    gint i;

    for (i = 1; i <= NUM_TATAMIS; i++) {
        struct match *m = get_cached_next_matches(i);
        if ((m[0].category & MATCH_CATEGORY_MASK) == cat && m[0].number < 1000 &&
            (m[0].blue >= COMPETITOR || m[0].white >= COMPETITOR))
            return TRUE;
    }
    return FALSE;
}

static cairo_surface_t *get_image(const gchar *name)
{
    cairo_surface_t *pic;
    gchar *file = g_build_filename(installation_dir, "etc", name, NULL);
    pic = cairo_image_surface_create_from_png(file);
    g_free(file);
    return pic;
}

void set_cat_graph_titles(void)
{
    if (cat_graph_label)
        gtk_label_set_text(GTK_LABEL(cat_graph_label), _("Categories"));
}
