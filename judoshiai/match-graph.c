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

static void init_display(void);
static gint find_box(gdouble x, gdouble y);
static gboolean mouse_click(GtkWidget *sheet_page, 
			    GdkEventButton *event, 
			    gpointer userdata);
static gboolean motion_notify(GtkWidget *sheet_page, 
                              GdkEventMotion *event, 
			      gpointer userdata);
static gboolean release_notify(GtkWidget *sheet_page, 
			       GdkEventButton *event, 
			       gpointer userdata);

#define AREA_SHIFT        4
#define ARG_MASK        0xf

#define MY_FONT "Arial"

#define THIN_LINE     (paper_width < 700.0 ? 1.0 : paper_width/700.0)
#define THICK_LINE    (2*THIN_LINE)

#define W(_w) ((_w)*paper_width)
#define H(_h) ((_h)*paper_height)

#define BOX_HEIGHT (1.4*extents.height)

#define NUM_RECTANGLES 1000

static struct {
    double x1, y1, x2, y2;
    gint   tatami;
    gint   position;
    gint   group;
    gint   category;
    gint   number;
} point_click_areas[NUM_RECTANGLES], dragged_match;

static gint num_rectangles;
static GdkCursor *hand_cursor = NULL;

static gboolean button_drag = FALSE;
static gchar    dragged_text[32];
static gint     start_box = 0;
static gdouble  dragged_x, dragged_y;
static gint     dragged_t;

#define NO_SCROLL   0
#define SCROLL_DOWN 1
#define SCROLL_UP   2
static gint     scroll_up_down = 0;

static struct {
    gint cat;
    gint num;
} last_wins[NUM_TATAMIS];

static time_t rest_times[NUM_TATAMIS];
static gint   rest_flags[NUM_TATAMIS];

static GtkWidget *match_graph_label = NULL;
static gboolean pending_timeout = FALSE;

gboolean mirror_display = FALSE;

static struct win_collection {
    GtkWidget *scrolled_window;
    GtkWidget *darea;
} w;

void draw_match_graph(void)
{
    init_display();
}

void set_graph_rest_time(gint tatami, time_t rest_end, gint flags)
{
    rest_times[tatami-1] = rest_end;
    rest_flags[tatami-1] = flags;
}

static void refresh_darea(void)
{
    gtk_widget_queue_draw(w.darea);
    //gtk_widget_queue_draw_area(w.darea, 0, 0, 600, 2000);
}
#define refresh_window refresh_darea

static gboolean refresh_graph(gpointer data)
{
    pending_timeout = FALSE;
    init_display();
    return FALSE;
}

static void paint(cairo_t *c, gdouble paper_width, gdouble paper_height, gpointer userdata)
{
    gint i;
    cairo_text_extents_t extents;
    gdouble y_pos = 0.0, colwidth = W(1.0/(number_of_tatamis + 1));
    gboolean update_later = FALSE;
    cairo_surface_t *cs = userdata;

    cairo_set_font_size(c, 12);
    cairo_text_extents(c, "Hj", &extents);

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

    y_pos = BOX_HEIGHT;

    cairo_set_line_width(c, THIN_LINE);
    cairo_set_source_rgb(c, 0.0, 0.0, 0.0);

    struct match *mw = db_matches_waiting();

    for (i = 0; i <= number_of_tatamis; i++) {
        gdouble left = (i)*colwidth;
        gdouble right = (i+1)*colwidth;
        gchar buf[30];
        struct match *nm;
        gint k;

        if (mirror_display && i) {
            left = (number_of_tatamis - i + 1)*colwidth;
            right = (number_of_tatamis - i + 2)*colwidth;
        }

        point_click_areas[num_rectangles].tatami = i;
        point_click_areas[num_rectangles].position = 0;

        if (i == 0)
            nm = mw;
        else
            nm = get_cached_next_matches(i);

        y_pos = BOX_HEIGHT;

        cairo_set_line_width(c, THIN_LINE);
        cairo_set_source_rgb(c, 0.0, 0.0, 0.0);

        if (i) {
            cairo_save(c);

            point_click_areas[num_rectangles].category = 0;
            point_click_areas[num_rectangles].number = 0;
            point_click_areas[num_rectangles].group = 0;
            point_click_areas[num_rectangles].x1 = left;
            point_click_areas[num_rectangles].y1 = y_pos;
            point_click_areas[num_rectangles].x2 = right;

            if (last_wins[i-1].cat == next_matches_info[i-1][0].won_catnum &&
                last_wins[i-1].num == next_matches_info[i-1][0].won_matchnum) 
                cairo_set_source_rgb(c, 0.7, 1.0, 0.7);
            else
                cairo_set_source_rgb(c, 1.0, 1.0, 0.0);

            cairo_rectangle(c, left, y_pos, colwidth, 3*BOX_HEIGHT);
            cairo_fill(c);
            cairo_set_source_rgb(c, 0.0, 0.0, 0.0);
            cairo_move_to(c, left+5, y_pos+extents.height);
            cairo_show_text(c, _("Prev. winner:"));
            y_pos += BOX_HEIGHT;
            cairo_move_to(c, left+5, y_pos+extents.height);
            cairo_show_text(c, next_matches_info[i-1][0].won_cat);

            gchar *txt = get_match_number_text(next_matches_info[i-1][0].won_catnum, 
                                               next_matches_info[i-1][0].won_matchnum);
            if (txt) {
                cairo_move_to(c, left+5+colwidth/2, y_pos+extents.height);
                cairo_show_text(c, txt);
            }

            y_pos += BOX_HEIGHT;
            cairo_move_to(c, left+5, y_pos+extents.height);
            cairo_show_text(c, next_matches_info[i-1][0].won_first);
            cairo_show_text(c, " ");
            cairo_show_text(c, next_matches_info[i-1][0].won_last);
            y_pos += BOX_HEIGHT;

            point_click_areas[num_rectangles].y2 = y_pos;

            if (num_rectangles < NUM_RECTANGLES-1)
                num_rectangles++;

            cairo_set_line_width(c, THICK_LINE);
            cairo_move_to(c, left, y_pos);
            cairo_line_to(c, right, y_pos);
            cairo_stroke(c);

            cairo_restore(c);
        }

        for (k = 0; k < (i ? NEXT_MATCH_NUM : WAITING_MATCH_NUM); k++) {
            struct match *m = &nm[k];
            gchar buf[40];
			
            point_click_areas[num_rectangles].position = k + 1;

            if (m->number >= 1000)
                break;

            struct category_data *catdata = avl_get_category(m->category);

            if ((catdata->match_status & MATCH_UNMATCHED) == 0)
                continue;

            cairo_save(c);
            if (m->forcedtatami)
                cairo_set_source_rgb(c, 1.0, 1.0, 0.6);
            else
                cairo_set_source_rgb(c, 1.0, 1.0, 1.0);

            if (i)
                cairo_rectangle(c, left, y_pos, colwidth, 4*BOX_HEIGHT);
            else
                cairo_rectangle(c, left, y_pos, colwidth, BOX_HEIGHT);

            cairo_fill(c);
            cairo_restore(c);
			
            cairo_save(c);
            cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_BOLD);
            cairo_move_to(c, left+5, y_pos+extents.height);

            if (catdata) {
                if (catdata->deleted & TEAM_EVENT) {
                    gint ageix = find_age_index(catdata->category);
                    if (ageix >= 0 && m->number > 0 && 
                        category_definitions[ageix].weights[m->number-1].weighttext[0]) {
                        snprintf(buf, sizeof(buf), "%s #%d%s", catdata->category, 
                                 m->category >> MATCH_CATEGORY_SUB_SHIFT,
                                 category_definitions[ageix].weights[m->number-1].weighttext);
                    } else
                        snprintf(buf, sizeof(buf), "%s", catdata->category);
                } else 
                    snprintf(buf, sizeof(buf), "%s #%d", catdata->category, m->number);
            } else
                snprintf(buf, sizeof(buf), "?");

            cairo_show_text(c, buf);
            //cairo_show_text(c, catdata ? catdata->category : "?");

            gchar *txt = get_match_number_text(m->category, m->number);
            if (txt || m->forcedtatami || i == 0) {
                cairo_move_to(c, left+5+colwidth/2, y_pos+extents.height);
                if (i == 0) {
                    if (txt && m->forcedtatami)
                        snprintf(buf, sizeof(buf), "T%d:%s", m->forcedtatami, txt);
                    else if (txt)
                        snprintf(buf, sizeof(buf), "T%d:%s", 
                                 catdata ? catdata->tatami : 0, txt);
                    else
                        snprintf(buf, sizeof(buf), "T%d", 
                                 catdata ? catdata->tatami : 0);
                } else if (txt && m->forcedtatami)
                    snprintf(buf, sizeof(buf), "T%d:%s", 
                             catdata ? catdata->tatami : 0,
                             txt);
                else if (m->forcedtatami)
                    snprintf(buf, sizeof(buf), "T%d", catdata ? catdata->tatami : 0);
                else if (txt)
                    snprintf(buf, sizeof(buf), "%s", txt);
                cairo_show_text(c, buf);
            }

            gint rt = (gint)rest_times[i-1] - (gint)time(NULL);
            if (k == 0 && i && rt > 0) {
                if (rest_flags[i-1] & MATCH_FLAG_BLUE_REST)
                    sprintf(buf, "<= %d:%02d ==", rt/60, rt%60);
                else
                    sprintf(buf, "== %d:%02d =>", rt/60, rt%60);
                cairo_set_source_rgb(c, 0.8, 0.0, 0.0);
                cairo_move_to(c, left+5+colwidth/2, y_pos+extents.height);
                cairo_show_text(c, buf);
                update_later = TRUE;
            }
            cairo_restore(c);

            if (i) {
                struct judoka *j = get_data(m->blue);
                if (j) {
                    cairo_save(c);

                    if (m->flags & MATCH_FLAG_BLUE_DELAYED) {
                        if (m->flags & MATCH_FLAG_BLUE_REST)
                            cairo_set_source_rgb(c, 0.5, 0.5, 1.0);
                        else
                            cairo_set_source_rgb(c, 1.0, 0.5, 0.5);
                    } else if (m->forcedtatami)
                        cairo_set_source_rgb(c, 1.0, 1.0, 0.6);
                    else
                        cairo_set_source_rgb(c, 1.0, 1.0, 1.0);

                    cairo_rectangle(c, left, y_pos+BOX_HEIGHT, colwidth/2, 3*BOX_HEIGHT);
                    cairo_fill(c);

                    cairo_set_source_rgb(c, 0, 0, 0);

                    cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_move_to(c, left+5, y_pos+2*BOX_HEIGHT+extents.height);
                    cairo_show_text(c, j->last);
                    cairo_restore(c);

                    cairo_move_to(c, left+5, y_pos+BOX_HEIGHT+extents.height);
                    cairo_show_text(c, j->first);

                    cairo_move_to(c, left+5, y_pos+3*BOX_HEIGHT+extents.height);
                    cairo_show_text(c, get_club_text(j, 0));
                    free_judoka(j);
                }
                j = get_data(m->white);
                if (j) {
                    cairo_save(c);

                    if (m->flags & MATCH_FLAG_WHITE_DELAYED) {
                        if (m->flags & MATCH_FLAG_WHITE_REST)
                            cairo_set_source_rgb(c, 0.5, 0.5, 1.0);
                        else
                            cairo_set_source_rgb(c, 1.0, 0.5, 0.5);
                    } else if (m->forcedtatami)
                        cairo_set_source_rgb(c, 1.0, 1.0, 0.6);
                    else
                        cairo_set_source_rgb(c, 1.0, 1.0, 1.0);

                    cairo_rectangle(c, left+colwidth/2, y_pos+BOX_HEIGHT, colwidth/2, 3*BOX_HEIGHT);
                    cairo_fill(c);

                    cairo_set_source_rgb(c, 0, 0, 0);

                    cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_move_to(c, left+5+colwidth/2, y_pos+2*BOX_HEIGHT+extents.height);
                    cairo_show_text(c, j->last);
                    cairo_restore(c);

                    cairo_move_to(c, left+5+colwidth/2, y_pos+BOX_HEIGHT+extents.height);
                    cairo_show_text(c, j->first);

                    cairo_move_to(c, left+5+colwidth/2, y_pos+3*BOX_HEIGHT+extents.height);
                    cairo_show_text(c, get_club_text(j, 0));
                    free_judoka(j);
                }
            }

            point_click_areas[num_rectangles].category = m->category;
            point_click_areas[num_rectangles].number = m->number;
            point_click_areas[num_rectangles].group = catdata ? catdata->group : 0;
            point_click_areas[num_rectangles].tatami = i;
            point_click_areas[num_rectangles].x1 = left;
            point_click_areas[num_rectangles].y1 = y_pos;
            point_click_areas[num_rectangles].x2 = right;
            if (i)
                y_pos += 4*BOX_HEIGHT;
            else
                y_pos += BOX_HEIGHT;
            point_click_areas[num_rectangles].y2 = y_pos;
            if (num_rectangles < NUM_RECTANGLES-1)
                num_rectangles++;

            cairo_move_to(c, left, y_pos);
            cairo_line_to(c, right, y_pos);
            cairo_stroke(c);
        }

        cairo_move_to(c, 10 + left, extents.height);
        if (i == 0)
            sprintf(buf, _("Delayed"));
        else
            sprintf(buf, "Tatami %d", i);
        cairo_show_text(c, buf);

        point_click_areas[num_rectangles].category = 0;
        point_click_areas[num_rectangles].number = 0;
        point_click_areas[num_rectangles].group = 0;
        point_click_areas[num_rectangles].tatami = i;
        point_click_areas[num_rectangles].position = k + 1;
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
    }

	
    cairo_save(c);
    cairo_set_line_width(c, THICK_LINE);
    cairo_set_source_rgb(c, 0.0, 0.0, 0.0);
    cairo_move_to(c, 0, BOX_HEIGHT);
    cairo_line_to(c, W(1.0), BOX_HEIGHT);
    cairo_stroke(c);
    cairo_restore(c);

 drag:
#if 0
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

        gint t = dragged_t; //find_box(dragged_x, dragged_y);
        if (t >= 0) {
            gdouble snap_y;
            if (point_click_areas[t].y2 - dragged_y < 
                dragged_y - point_click_areas[t].y1)
                snap_y = point_click_areas[t].y2;
            else
                snap_y = point_click_areas[t].y1;

            cairo_save(c);
            cairo_set_source_rgb(c, 0.0, 0.0, 1.0);
            cairo_set_line_width(c, THICK_LINE);
            cairo_move_to(c, point_click_areas[t].x1, snap_y);
            cairo_line_to(c, point_click_areas[t].x2, snap_y);
            cairo_stroke(c);
            cairo_restore(c);
        }
    }
#endif
    if (update_later && pending_timeout == FALSE) {
        pending_timeout = TRUE;
        g_timeout_add(5000, refresh_graph, NULL);
    }
}

static cairo_surface_t *surface = NULL;
static gdouble paper_width, paper_height;

static void init_display(void)
{
    if (!surface) return;
    cairo_t *c = cairo_create(surface);
    paint(c, paper_width, paper_height, NULL);
    cairo_show_page(c);
    cairo_destroy(c);
    refresh_darea();
}

static gboolean configure_event_cb(GtkWidget         *widget,
                                   GdkEventConfigure *event,
                                   gpointer           data)
{
    if (surface)
        cairo_surface_destroy(surface);

    paper_width = gtk_widget_get_allocated_width(widget);
    paper_height = gtk_widget_get_allocated_height(widget);

    surface = gdk_window_create_similar_surface(gtk_widget_get_window(widget),
                                                CAIRO_CONTENT_COLOR, paper_width, paper_height);

    init_display();

    /* We've handled the configure event, no need for further processing. */
    return TRUE;
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
    //g_print("MATCH-GRAPH: expose %d\n", cnt++);
#if (GTKVER == 3)
    cairo_t *c = (cairo_t *)event;
    cairo_set_source_surface(c, surface, 0, 0);
    cairo_paint(c);

    if (button_drag) {
        cairo_text_extents_t extents;
    
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

        gint t = dragged_t; //find_box(dragged_x, dragged_y);
        if (t >= 0) {
            gdouble snap_y;
            if (point_click_areas[t].y2 - dragged_y < 
                dragged_y - point_click_areas[t].y1)
                snap_y = point_click_areas[t].y2;
            else
                snap_y = point_click_areas[t].y1;

            cairo_save(c);
            cairo_set_source_rgb(c, 0.0, 0.0, 1.0);
            cairo_set_line_width(c, THICK_LINE);
            cairo_move_to(c, point_click_areas[t].x1, snap_y);
            cairo_line_to(c, point_click_areas[t].x2, snap_y);
            cairo_stroke(c);
            cairo_restore(c);
        }
    }

    return FALSE;
#else
    static cairo_surface_t *cs = NULL;
    static gint oldw = 0, oldh = 0;
    cairo_t *c = gdk_cairo_create(widget->window);
    gint allocw = widget->allocation.width;
    gint alloch = widget->allocation.height;

    if (cs && (oldw != allocw || oldh != alloch)) {
        cairo_surface_destroy(cs);
        cs = NULL;
    }

    if (!cs) {
        cs = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, allocw, alloch);
        oldw = allocw;
        oldh = alloch;
    }

    if (button_drag) {
        paint(c, allocw, alloch, cs);
    } else {
        cairo_t *c1 = cairo_create(cs);
        paint(c1, allocw, alloch, NULL);

        cairo_set_source_surface(c, cs, 0, 0);
        cairo_paint(c);
        cairo_destroy(c1);
    }

    cairo_show_page(c);
    cairo_destroy(c);
#endif

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

static gboolean query_tooltip (GtkWidget  *widget, gint x, gint y, gboolean keyboard_mode,
                               GtkTooltip *tooltip, gpointer user_data)
{
    gchar buf[128];
    gint n = 0;
    gint t = find_box(x, y);
    if (t < 0)
        return FALSE;

    if (point_click_areas[t].tatami)
        return FALSE;

    struct match *m = db_get_match_data(point_click_areas[t].category, point_click_areas[t].number);
    if (!m)
        return FALSE;

    buf[0] = 0;
    struct judoka *j = get_data(m->blue);
    if (j) {
        n = snprintf(buf, sizeof(buf), "%s %s, %s", j->last, j->first, get_club_text(j, 0));
        free_judoka(j);
    } else
        n = snprintf(buf, sizeof(buf), "???");

    j = get_data(m->white);
    if (j) {
        n += snprintf(buf+n, sizeof(buf)-n, "\n%s %s, %s", j->last, j->first, get_club_text(j, 0));
        free_judoka(j);
    } else
        n += snprintf(buf+n, sizeof(buf)-n, "\n???");

    gtk_tooltip_set_text(tooltip, buf);
    return TRUE;
}

void set_match_graph_page(GtkWidget *notebook)
{
    w.scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(w.scrolled_window), 10);

    w.darea = gtk_drawing_area_new();
    gtk_widget_set_size_request(w.darea, 600, 2000);

#if (GTKVER == 3)
    gtk_widget_set_events(w.darea, gtk_widget_get_events(w.darea) |
                          GDK_BUTTON_PRESS_MASK | 
                          GDK_BUTTON_RELEASE_MASK |
                          /*GDK_POINTER_MOTION_MASK |*/
                          GDK_POINTER_MOTION_HINT_MASK |
                          GDK_BUTTON_MOTION_MASK);
#else
    GTK_WIDGET_SET_FLAGS(w.darea, GTK_CAN_FOCUS);
    gtk_widget_add_events(w.darea, 
                          GDK_BUTTON_PRESS_MASK | 
                          GDK_BUTTON_RELEASE_MASK |
                          /*GDK_POINTER_MOTION_MASK |*/
                          GDK_POINTER_MOTION_HINT_MASK |
                          GDK_BUTTON_MOTION_MASK);
#endif
	

    /* pack the table into the scrolled window */
#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
    //gtk_widget_set_hexpand(w.darea, TRUE);
    //gtk_widget_set_vexpand(w.darea, TRUE);    
    //gtk_widget_set_hexpand(w.scrolled_window, TRUE);
    //gtk_widget_set_vexpand(w.scrolled_window, TRUE);    
    //GtkWidget *viewport = gtk_viewport_new(NULL,NULL);
    //gtk_container_add(GTK_CONTAINER(viewport), w.darea);
    gtk_container_add(GTK_CONTAINER(w.scrolled_window), w.darea);
#else
    gtk_scrolled_window_add_with_viewport (
        GTK_SCROLLED_WINDOW(w.scrolled_window), w.darea);
#endif

    //gtk_widget_show(w.darea);
    gtk_widget_show_all(w.scrolled_window);

    match_graph_label = gtk_label_new (_("Matches"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), w.scrolled_window, match_graph_label);

#if (GTKVER == 3)
    g_signal_connect(G_OBJECT(w.scrolled_window), 
                     "draw", G_CALLBACK(expose_scrolled), NULL);
    g_signal_connect(G_OBJECT(w.darea), 
                     "draw", G_CALLBACK(expose), w.darea);
    g_signal_connect(G_OBJECT(w.darea),"configure-event",
                     G_CALLBACK(configure_event_cb), NULL);
#else
    g_signal_connect(G_OBJECT(w.darea), 
                     "expose-event", G_CALLBACK(expose), w.darea);
#endif
    g_signal_connect(G_OBJECT(w.darea), 
                     "button-press-event", G_CALLBACK(mouse_click), NULL);
    g_signal_connect(G_OBJECT(w.darea), 
                     "button-release-event", G_CALLBACK(release_notify), NULL);
    g_signal_connect(G_OBJECT(w.darea), 
                     "motion-notify-event", G_CALLBACK(motion_notify), &w);
    g_timeout_add(500, scroll_callback, &w);

    gtk_widget_set_has_tooltip(w.darea, TRUE);
    g_signal_connect (w.darea, "query-tooltip", G_CALLBACK(query_tooltip), NULL);
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

static void change_comment(GtkWidget *menuitem, gpointer userdata)
{
    gint t = (ptr_to_gint(userdata)) >> 4;
    gint cmd = (ptr_to_gint(userdata)) & 0x0f;
    struct compsys sys = db_get_system(point_click_areas[t].category);

    db_set_comment(point_click_areas[t].category, point_click_areas[t].number, cmd);
    update_matches(0, (struct compsys){0,0,0,0}, db_find_match_tatami(point_click_areas[t].category, point_click_areas[t].number));

    init_display();

    /* send comment to net */
    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_SET_COMMENT;
    msg.u.set_comment.data = ptr_to_gint(userdata);
    msg.u.set_comment.category = point_click_areas[t].category;
    msg.u.set_comment.number = point_click_areas[t].number;
    msg.u.set_comment.cmd = cmd;
    msg.u.set_comment.sys = sys.system;
    send_packet(&msg);
}

static void freeze(GtkWidget *menuitem, gpointer userdata)
{
    gint t = (ptr_to_gint(userdata)) >> AREA_SHIFT;
    gint arg = (ptr_to_gint(userdata)) & ARG_MASK;

    db_freeze_matches(point_click_areas[t].tatami, 
                      point_click_areas[t].category, 
                      point_click_areas[t].number,
                      arg);
    init_display();
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
        (event->button == 1)) {
        gdouble x = event->x, y = event->y; 
        gint t;
		    
        dragged_x = x;
        dragged_y = y;

        t = find_box(x, y);
        if (t < 0)
            return FALSE;

        dragged_match = point_click_areas[t];
        start_box = t;

        if (point_click_areas[t].category == 0) {
            gint tatami = point_click_areas[t].tatami;
            if (tatami > 0 && tatami <= NUM_TATAMIS) {
                last_wins[tatami-1].cat = next_matches_info[tatami-1][0].won_catnum;
                last_wins[tatami-1].num = next_matches_info[tatami-1][0].won_matchnum;
            }
        } else {
            struct category_data *catdata = avl_get_category(dragged_match.category);
            if (catdata)
                snprintf(dragged_text, sizeof(dragged_text), "%s:%d",
                         catdata->category, dragged_match.number);
        }

        button_drag = TRUE;
        refresh_window();
        return TRUE;
		    
    } else if (event->type == GDK_BUTTON_PRESS &&  
               (event->button == 3)) {
        GtkWidget *menu, *menuitem;
        gdouble x = event->x, y = event->y; 
        gint t;
        gpointer p;
		    
        t = find_box(x, y);
        if (t < 0)
            return FALSE;

        menu = gtk_menu_new();

        menuitem = gtk_menu_item_new_with_label(_("Next match"));
        p = gint_to_ptr(COMMENT_MATCH_1 | (t << 4));
        g_signal_connect(menuitem, "activate",
                         (GCallback) change_comment, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        menuitem = gtk_menu_item_new_with_label(_("Preparing"));
        p = gint_to_ptr(COMMENT_MATCH_2 | (t << 4));
        g_signal_connect(menuitem, "activate",
                         (GCallback) change_comment, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        menuitem = gtk_menu_item_new_with_label(_("Delay the match"));
        p = gint_to_ptr(COMMENT_WAIT | (t << 4));
        g_signal_connect(menuitem, "activate",
                         (GCallback) change_comment, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        menuitem = gtk_menu_item_new_with_label(_("Remove delay"));
        p = gint_to_ptr(COMMENT_EMPTY | (t << 4));
        g_signal_connect(menuitem, "activate",
                         (GCallback) change_comment, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

        menuitem = gtk_menu_item_new_with_label(_("Freeze match order"));
        p = gint_to_ptr(FREEZE_MATCHES | (t << AREA_SHIFT));
        g_signal_connect(menuitem, "activate",
                         (GCallback) freeze, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		
        menuitem = gtk_menu_item_new_with_label(_("Unfreeze exported"));
        p = gint_to_ptr(UNFREEZE_EXPORTED | (t << AREA_SHIFT));
        g_signal_connect(menuitem, "activate",
                         (GCallback) freeze, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		
        menuitem = gtk_menu_item_new_with_label(_("Unfreeze imported"));
        p = gint_to_ptr(UNFREEZE_IMPORTED | (t << AREA_SHIFT));
        g_signal_connect(menuitem, "activate",
                         (GCallback) freeze, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		
        menuitem = gtk_menu_item_new_with_label(_("Unfreeze this"));
        p = gint_to_ptr(UNFREEZE_THIS | (t << AREA_SHIFT));
        g_signal_connect(menuitem, "activate",
                         (GCallback) freeze, p);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
		
        gtk_widget_show_all(menu);

        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                       (event != NULL) ? event->button : 0,
                       gdk_event_get_time((GdkEvent*)event));

        refresh_window();
    }
    return FALSE;
}

static gboolean motion_notify(GtkWidget *sheet_page, 
                              GdkEventMotion *event, 
                              gpointer userdata)
{
    //static GTimeVal next_time, now;
    struct win_collection *w = userdata;

#if (GTKVER == 3)
    gint alloch = gtk_widget_get_allocated_height(w->scrolled_window);
#else
    gint alloch = w->scrolled_window->allocation.height;
#endif

    if (button_drag == FALSE)
        return FALSE;

    gdouble x = event->x, y = event->y;
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(w->scrolled_window));
    gdouble adjnow = gtk_adjustment_get_value(adj);
	
    if (y - adjnow > alloch - 50) {
        scroll_up_down = SCROLL_DOWN;
    } else if (y - adjnow < 20.0) {
        scroll_up_down = SCROLL_UP;
    } else
        scroll_up_down = NO_SCROLL;

    dragged_x = x;
    dragged_y = y;
	
    dragged_t = find_box(x, y);
    if (dragged_t < 0)
        return FALSE;

    struct category_data *catdata = avl_get_category(dragged_match.category);
    if (catdata)
        snprintf(dragged_text, sizeof(dragged_text), "%s:%d (%d)",
                 catdata->category, dragged_match.number, 
                 point_click_areas[dragged_t].position);
#if 0
    g_get_current_time(&now);
    if (timeval_subtract(NULL, &now, &next_time))
        return FALSE;

    g_time_val_add(&now, 500000);
    next_time = now;
#endif

    refresh_window();
    return FALSE;
}

static gboolean release_notify(GtkWidget *sheet_page, 
			       GdkEventButton *event, 
			       gpointer userdata)
{
    //gdk_window_set_cursor(GTK_WIDGET(main_window)->window, NULL);

    if (!button_drag)
        return FALSE;

    button_drag = FALSE;

    if (event->type == GDK_BUTTON_RELEASE &&  
        event->button == 1) {
        gdouble x = event->x, y = event->y; 
        gint t;

        t = find_box(x, y);
        if (t < 0 || start_box == t)
            return FALSE;

        gboolean after = FALSE;
        if (point_click_areas[t].y2 - y < 
            y - point_click_areas[t].y1)
            after = TRUE;

        db_change_freezed(dragged_match.category, 
                          dragged_match.number,
                          point_click_areas[t].tatami,
                          point_click_areas[t].position, after);
        init_display();
        return TRUE;
    }

    refresh_window();
    return FALSE;
}

void set_match_graph_titles(void)
{
    if (match_graph_label)
        gtk_label_set_text(GTK_LABEL(match_graph_label), _("Matches"));
}


