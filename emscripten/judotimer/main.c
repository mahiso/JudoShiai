/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <locale.h>

#ifdef WIN32
#include <process.h>
//#include <glib/gwin32.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include "judotimer.h"
#include "language.h"

void delete_comp_window(void);
void delete_new_match(void);
int setclocks_c(int yes);
void menuicononload(const char *str);
void menupiconload(const char *str);
void menupiconerror(const char *str);
void customonload(const char *str);
void custombgonload(const char *str);
void flagonload(const char *str);
void flagonerror(const char *str);

static void show_menu(void);
void textbox(int x1, int y1, int w1, int h1, const char *txt);
void checkbox(int x1, int y1, int w1, int h1, int yes);
static void set_colors(void);
gboolean show_competitor_names = TRUE;
gboolean showletter = FALSE;

struct stack_item stack[8];
int sp = 0;
static SDL_Surface *menubg, *menuicon;
static int menu_on = FALSE;
static int icontimer = 0;
static int req_width = 0, req_height = 0;
static int flag_req = 0, flag_pending = 0;

static inline void swap32(uint32_t *d) {
    uint32_t x = *d;
    uint8_t *p = (uint8_t *)d;
    p[0] = (x >> 24) & 0xff;
    p[1] = (x >> 16) & 0xff;
    p[2] = (x >> 8) & 0xff;
    p[3] = x & 0xff;
}

static inline uint32_t hton32(uint32_t x) {
    uint32_t a = x;
    swap32(&a);
    return a;
}
#define ntoh32 hton32

int tatami = 1;
static void show_big(void);
static void expose_label(cairo_t *c, gint w);
static void expose(void);
static void init_display(void);
gboolean delete_big(gpointer data);
gint language = LANG_EN;
static int expose_now = 0;

//#define TABLE_PARAM (GTK_EXPAND)
#define TABLE_PARAM (GTK_EXPAND|GTK_FILL)

static gint dsp_layout = 6;
static gint selected_name_layout;
static const gchar *num2str[11] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "+"};
static const gchar *pts2str[6]  = {"-", "K", "Y", "W", "I", "S"};
guint current_year;
gchar *installation_dir = NULL;
GTimer *timer;
gboolean blue_wins_voting = FALSE, white_wins_voting = FALSE;
gboolean hansokumake_to_blue = FALSE, hansokumake_to_white = FALSE;
gboolean result_hikiwake = FALSE;
static PangoFontDescription *font;
gint mode = 0;
GKeyFile *keyfile;
gchar *conffile;

static gboolean big_dialog = FALSE;
static gchar big_text[40];
static time_t big_end;

gboolean rules_leave_score = TRUE;
gboolean rules_stop_ippon_2 = FALSE;
gboolean rules_confirm_match = TRUE;
GdkCursor *cursor = NULL;
gboolean sides_switched = FALSE;
gboolean white_first = TRUE;
gboolean no_big_text = FALSE;
gboolean fullscreen = FALSE;
gboolean menu_hidden = FALSE;
gboolean supports_alpha = FALSE;
//gboolean rule_no_free_shido = FALSE;
gboolean rule_eq_score_less_shido_wins = TRUE;
gboolean rule_short_pin_times = FALSE;

#define MY_FONT "Arial"
static gchar font_face[32];
static gint  font_slant = CAIRO_FONT_SLANT_NORMAL, font_weight = CAIRO_FONT_WEIGHT_NORMAL;
static int font_size = 100;

gboolean update_tvlogo = FALSE;
SDL_Surface *darea = NULL;
static cairo_surface_t *surface = NULL;

gchar *custom_layout_file = NULL;
static cairo_surface_t *background_image = NULL;


static const gchar *num_to_str(guint num)
{

    static gchar result[10];
    if (num < 10)
	return num2str[num];

    sprintf(result, "%d", num);
    return result;
}

static const gchar *pts_to_str(guint num)
{
    if (num < 6)
	return pts2str[num];

    return ".";
}

static void refresh_darea(void)
{
    expose_now = 1;
}

#define CBFUNC(_f) static gboolean cb_##_f(GtkWidget *eventbox, SDL_Event *event, void *param)
#define CBFUNCX(_f)							\
    static gboolean cb_##_f(GtkWidget *eventbox, SDL_Event *event, void *param) { \
	return FALSE;							\
    }

CBFUNCX(match1)

CBFUNC(match2)
{
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_0, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNCX(blue_name_1)
CBFUNCX(white_name_1)
CBFUNCX(blue_name_2)
CBFUNCX(white_name_2)
CBFUNCX(blue_club)
CBFUNCX(white_club)

CBFUNC(wazaari)
{
	if (big_dialog)
		delete_big(NULL);
        return FALSE;
}

CBFUNC(yuko)
{
	if (big_dialog)
		delete_big(NULL);
        return FALSE;
}

CBFUNC(koka)
{
	if (big_dialog)
		delete_big(NULL);
        return FALSE;
}

CBFUNC(shido)
{
	if (big_dialog)
		delete_big(NULL);
        return FALSE;
}

CBFUNC(padding)
{
	if (big_dialog)
		delete_big(NULL);
        return FALSE;
}

CBFUNC(sonomama)
{
	if (big_dialog)
		delete_big(NULL);

        clock_key(GDK_s, 0);
        return FALSE;
}

CBFUNC(bw)
{
    if (set_osaekomi_winner(BLUE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F1, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(by)
{
    if (set_osaekomi_winner(BLUE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F2, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(bk)
{
    if (set_osaekomi_winner(BLUE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F3, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(bs)
{
    if (set_osaekomi_winner(BLUE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F4, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(ww)
{
    if (set_osaekomi_winner(WHITE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F5, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(wy)
{
    if (set_osaekomi_winner(WHITE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F6, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(wk)
{
    if (set_osaekomi_winner(WHITE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F7, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(ws)
{
    if (set_osaekomi_winner(WHITE | GIVE_POINTS))
	return FALSE;

    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent *)event;
    clock_key(GDK_F8, (m->button == 3) ? SDLK_LSHIFT : 0);
    return FALSE;
}

CBFUNC(t_min)
{
	clock_key(GDK_space, event->key.keysym.mod);
        return FALSE;
}

CBFUNC(colon)
{
	clock_key(GDK_space, event->key.keysym.mod);
        return FALSE;
}

CBFUNC(t_tsec)
{
	clock_key(GDK_space, event->key.keysym.mod);
        return FALSE;
}

CBFUNC(t_sec)
{
	clock_key(GDK_space, event->key.keysym.mod);
        return FALSE;
}

CBFUNC(o_tsec)
{
        clock_key(GDK_Return, event->key.keysym.mod);
        return FALSE;
}

CBFUNC(o_sec)
{
        clock_key(GDK_Return, event->key.keysym.mod);
        return FALSE;
}

CBFUNC(points)
{
	set_osaekomi_winner(0);
        return FALSE;
}

CBFUNCX(comment)

CBFUNC(pts_to_blue)
{
        set_osaekomi_winner(BLUE);
        return FALSE;
}

CBFUNC(pts_to_white)
{
        set_osaekomi_winner(WHITE);
        return FALSE;
}

CBFUNCX(cat1)
CBFUNCX(cat2)
CBFUNCX(gs)
CBFUNCX(flag_blue)
CBFUNCX(flag_white)
CBFUNCX(ask_box)

CBFUNC(yes_box)
{
    delete_comp_window();
    close_ask_ok();
    return FALSE;
}

CBFUNC(no_box)
{
    delete_comp_window();
    close_ask_nok();
    return FALSE;
}

CBFUNC(ok_box)
{
    delete_comp_window();
    return FALSE;
}

CBFUNC(upper_box)
{
    return FALSE;
}

CBFUNC(middle_box)
{
    return FALSE;
}

CBFUNC(lower_box)
{
    //emscripten_run_script_int("setclocks()");

    return FALSE;
}

CBFUNC(rest_time_box)
{
    return FALSE;
}
CBFUNC(rest_ind_box)
{
    return FALSE;
}


/* globals */
gchar *program_path;

GtkWidget     *main_vbox = NULL;
GtkWidget     *main_window = NULL;
GtkWidget     *menubar = NULL;
gchar          current_directory[1024] = {0};
gint           my_address;
gboolean       clocks_only = FALSE;

static gint match1, match2, colon;
static gint blue_name_1, white_name_1;
static gint blue_name_2, white_name_2;
static gint blue_club, white_club;
static gint wazaari, yuko, koka, shido;
static gint bw, by, bk, bs;
static gint ww, wy, wk, ws;
static gint t_min, t_tsec, t_sec;
static gint o_tsec, o_sec, padding, sonomama;
static gint points, comment, cat1, cat2, gs;
static gint pts_to_blue, pts_to_white, flag_blue, flag_white;
static gint ask_box, yes_box, no_box, ok_box, upper_box, middle_box, lower_box;
static gint rest_time_box, rest_ind_box;

static GdkColor color_yellow, color_white, color_grey, color_green, color_blue, color_red, color_black;
static GdkColor *bgcolor = &color_blue, bgcolor_pts, bgcolor_points;
static int background_r, background_g, background_b;
static GdkColor clock_run, clock_stop, clock_bg;
static GdkColor oclock_run, oclock_stop, oclock_bg;
static gint hide_clock_if_osaekomi, hide_zero_osaekomi_points;
static gint current_osaekomi_state;

#define MY_LABEL(_x) _x

static int paper_width, paper_height;
#define W(_w) ((_w)*paper_width/1000)
#define H(_h) ((_h)*paper_height/1000)

#define LABEL_STATUS_EXPOSE 1
#define LABEL_STATUS_CHANGED 2
#define NUM_LABELS 64
struct label {
    int x, y;
    int w, h;
    gchar *text;
    gchar *text2;
    int size;
    gint xalign;
    int fg_r, fg_g, fg_b;
    int bg_r, bg_g, bg_b, bg_a;
    gboolean (*cb)(GtkWidget *, SDL_Event *, void *);
    gchar status;
    gboolean wrap;
    gboolean hide;
} labels[NUM_LABELS], defaults_for_labels[NUM_LABELS];
static gint num_labels = 0;

static struct {
    SDL_Surface *image;
    char         name[16];
} current_flags[2];

#define SET_COLOR(_w) do {			\
		set_fg_color(_w, 0, &fg);	\
		set_bg_color(_w, 0, &bg);	\
	} while (0)

#define GET_LABEL(_w, _l, _x, _y, _width, _height) do {		\
		labels[num_labels].x = _x;		\
		labels[num_labels].y = _y;		\
		labels[num_labels].w = _width;		\
		labels[num_labels].h = _height;		\
		labels[num_labels].cb = cb_##_w;	\
		labels[num_labels].text = g_strdup(_l); \
		labels[num_labels].text2 = 0;		\
		labels[num_labels].size = 0;		\
		labels[num_labels].fg_r = 255;		\
		labels[num_labels].fg_g = 255;		\
		labels[num_labels].fg_b = 255;		\
		labels[num_labels].bg_r = 0;		\
		labels[num_labels].bg_g = 0;		\
		labels[num_labels].bg_b = 0;		\
		labels[num_labels].bg_a = 255;		\
                labels[num_labels].status = 0;          \
                labels[num_labels].wrap = 0;            \
                labels[num_labels].hide = 0;            \
		_w = num_labels;			\
		num_labels++;				\
                /*printf("%d = %s\n", _w, #_w);*/ } while (0)

static void set_fg_color(gint w, gint s, GdkColor *c)
{
    labels[w].fg_r = c->r;
    labels[w].fg_g = c->g;
    labels[w].fg_b = c->b;

    labels[w].status |= LABEL_STATUS_CHANGED;
}

static void set_bg_color(gint w, gint s, GdkColor *c)
{
    labels[w].bg_r = c->r;
    labels[w].bg_g = c->g;
    labels[w].bg_b = c->b;

    labels[w].status |= LABEL_STATUS_CHANGED;
}

static void set_text(gint w, const gchar *text)
{
	g_free(labels[w].text);
	labels[w].text = g_strdup(text);

        labels[w].status |= LABEL_STATUS_CHANGED;
}

static void set_text2(gint w, const gchar *text)
{
	g_free(labels[w].text2);
	labels[w].text2 = g_strdup(text);

        labels[w].status |= LABEL_STATUS_CHANGED;
}

static gchar *get_text(gint w)
{
	return labels[w].text;
}

void set_timer_run_color(gboolean running, gboolean resttime)
{
    GdkColor *color;

    if (running) {
	if (resttime)
	    color = &color_red;
	else
	    color = &clock_run;
	//color = &color_white;
    } else
	color = &clock_stop;
    //color = &color_yellow;

    set_fg_color(t_min, GTK_STATE_NORMAL, color);
    set_fg_color(colon, GTK_STATE_NORMAL, color);
    set_fg_color(t_tsec, GTK_STATE_NORMAL, color);
    set_fg_color(t_sec, GTK_STATE_NORMAL, color);

    expose_label(NULL, t_min);
    expose_label(NULL, colon);
    expose_label(NULL, t_tsec);
    expose_label(NULL, t_sec);
}

void set_timer_osaekomi_color(gint osaekomi_state, gint pts)
{
    GdkColor *fg = &color_white, *bg = &color_black, *color = &color_green;

    if (pts) {
        if (osaekomi_state == OSAEKOMI_DSP_BLUE ||
            osaekomi_state == OSAEKOMI_DSP_WHITE ||
            osaekomi_state == OSAEKOMI_DSP_UNKNOWN ||
            osaekomi_state == OSAEKOMI_DSP_NO) {
            gint b = bk, w = wk;
            int sb, sw;

	    switch (pts) {
	    case 2: b = bk; w = wk; break;
	    case 3: b = by; w = wy; break;
	    case 4: b = bw; w = ww; break;
	    }

            sb = labels[b].size;
            sw = labels[w].size;

            if (osaekomi_state == OSAEKOMI_DSP_BLUE ||
                osaekomi_state == OSAEKOMI_DSP_UNKNOWN)
                labels[b].size = 500;
            if (osaekomi_state == OSAEKOMI_DSP_WHITE ||
                osaekomi_state == OSAEKOMI_DSP_UNKNOWN)
                labels[w].size = 500;

            expose_label(NULL, b);
            expose_label(NULL, w);
            labels[b].size = sb;
            labels[w].size = sw;
        }
    } else {
        expose_label(NULL, bw);
        expose_label(NULL, by);
        expose_label(NULL, bk);
        expose_label(NULL, ww);
        expose_label(NULL, wy);
        expose_label(NULL, wk);
    }

    switch (osaekomi_state) {
    case OSAEKOMI_DSP_NO:
        fg = &color_grey;
        bg = &bgcolor_points;
        break;
    case OSAEKOMI_DSP_YES:
        fg = &color_green;
        bg = &bgcolor_points;
        break;
    case OSAEKOMI_DSP_BLUE:
        if (white_first) {
            fg = &color_black;
            bg = &color_white;
        } else {
            fg = &color_white;
            bg = bgcolor;
        }
        break;
    case OSAEKOMI_DSP_WHITE:
        if (white_first) {
            fg = &color_white;
            bg = bgcolor;
        } else {
            fg = &color_black;
            bg = &color_white;
        }
        break;
    case OSAEKOMI_DSP_UNKNOWN:
        fg = &color_white;
        bg = &bgcolor_points;
        break;
    }

    set_fg_color(points, GTK_STATE_NORMAL, fg);
    set_bg_color(points, GTK_STATE_NORMAL, bg);

    gint pts1, pts2;
    if (white_first) {
        pts1 = pts_to_white;
        pts2 = pts_to_blue;
    } else {
        pts1 = pts_to_blue;
        pts2 = pts_to_white;
    }

    current_osaekomi_state = osaekomi_state;

    if (osaekomi_state == OSAEKOMI_DSP_YES) {
        color = &oclock_run;
        //color = &color_green;
        set_fg_color(pts1, GTK_STATE_NORMAL, &color_white);
        set_bg_color(pts1, GTK_STATE_NORMAL, bgcolor);
        set_fg_color(pts2, GTK_STATE_NORMAL, &color_black);
        set_bg_color(pts2, GTK_STATE_NORMAL, &color_white);
    } else {
        color = &oclock_stop;
        //color = &color_grey;
        set_fg_color(pts1, GTK_STATE_NORMAL, &color_grey);
        set_bg_color(pts1, GTK_STATE_NORMAL, &bgcolor_pts);
        set_fg_color(pts2, GTK_STATE_NORMAL, &color_grey);
        set_bg_color(pts2, GTK_STATE_NORMAL, &bgcolor_pts);
	if (hide_clock_if_osaekomi) {
	    expose_label(NULL, t_min);
	    expose_label(NULL, colon);
	    expose_label(NULL, t_tsec);
	    expose_label(NULL, t_sec);
	}
    }

    set_fg_color(o_tsec, GTK_STATE_NORMAL, color);
    set_fg_color(o_sec, GTK_STATE_NORMAL, color);

    expose_label(NULL, points);
    expose_label(NULL, o_tsec);
    expose_label(NULL, o_sec);
    expose_label(NULL, pts_to_blue);
    expose_label(NULL, pts_to_white);
}

void set_timer_value(guint min, guint tsec, guint sec)
{
    if (min < 10000/60) {
        set_text(MY_LABEL(t_min), num_to_str(min%10));
        set_text(MY_LABEL(t_tsec), num_to_str(tsec));
        set_text(MY_LABEL(t_sec), num_to_str(sec));
    } else {
        set_text(MY_LABEL(t_min), "-");
        set_text(MY_LABEL(t_tsec), "-");
        set_text(MY_LABEL(t_sec), "-");
    }

    expose_label(NULL, t_min);
    expose_label(NULL, t_tsec);
    expose_label(NULL, t_sec);

    update_tvlogo = TRUE;

    if (labels[rest_time_box].hide == 0) {
	char buf[8];
	snprintf(buf, sizeof(buf), "%d:%d%d",
		 min, tsec, sec);
	set_text(rest_time_box, buf);
	expose_label(NULL, rest_time_box);

	if (sec & 1) {
	    if (rest_flags & MATCH_FLAG_BLUE_REST)
		set_bg_color(rest_ind_box, 0, &color_white);
	    else
		set_bg_color(rest_ind_box, 0, &color_blue);
	} else
	    set_bg_color(rest_ind_box, 0, &color_black);

	expose_label(NULL, rest_ind_box);
    }
}

void set_osaekomi_value(guint tsec, guint sec)
{
    set_text(MY_LABEL(o_tsec), num_to_str(tsec));
    set_text(MY_LABEL(o_sec), num_to_str(sec));
    expose_label(NULL, o_tsec);
    expose_label(NULL, o_sec);
    update_tvlogo = TRUE;
}

static void set_number(gint w, gint num)
{
    const gchar *s, *n;
    s = get_text(w);
    n = num_to_str(num);
    if ((s && n && strcmp(s, n)) || s == NULL) {
        set_text(MY_LABEL(w), n);
        expose_label(NULL, w);
    }
}

void set_points(gint blue[], gint white[])
{
    if (blue[0] >= 2)
	set_number(bw, 1);
    else
	set_number(bw, 0);

    if (blue[0] & 1)
	set_number(by, 1);
    else
	set_number(by, 0);

    set_number(bk, blue[1]);
    set_number(bs, blue[3]);

    if (white[0] >= 2)
	set_number(ww, 1);
    else
	set_number(ww, 0);

    if (white[0] & 1)
	set_number(wy, 1);
    else
	set_number(wy, 0);

    set_number(wk, white[1]);
    set_number(ws, white[3]);

    update_tvlogo = TRUE;
}

void set_score(guint score)
{
    set_text(MY_LABEL(points), pts_to_str(score));
    expose_label(NULL, points);
}

void parse_name(const gchar *s, gchar *first, gchar *last, gchar *club, gchar *country)
{
    const guchar *p = (const guchar *)s;
    gint i;

    *first = *last = *club = *country = 0;

    i = 0;
    while (*p >= ' ' && i < 31) {
        *last++ = *p++;
        i++;
    }
    *last = 0;
    p++;
    i = 0;
    while (*p >= ' ' && i < 31) {
        *first++ = *p++;
        i++;
    }
    *first = 0;
    p++;
    i = 0;
    while (*p >= ' ' && i < 31) {
        *country++ = *p++;
        i++;
    }
    *country = 0;
    p++;
    i = 0;
    while (*p >= ' ' && i < 31) {
        *club++ = *p++;
        i++;
    }
    *club = 0;
}

static gchar *get_name_by_layout(gchar *first, gchar *last, gchar *club, gchar *country)
{
    switch (selected_name_layout) {
    case 0:
        if (country == NULL || country[0] == 0)
            return g_strconcat(first, " ", last, ", ", club, NULL);
        else if (club == NULL || club[0] == 0)
            return g_strconcat(first, " ", last, ", ", country, NULL);
        return g_strconcat(first, " ", last, ", ", country, "/", club, NULL);
    case 1:
        if (country == NULL || country[0] == 0)
            return g_strconcat(last, ", ", first, ", ", club, NULL);
        else if (club == NULL || club[0] == 0)
            return g_strconcat(last, ", ", first, ", ", country, NULL);
        return g_strconcat(last, ", ", first, ", ", country, "/", club, NULL);
    case 2:
        if (country == NULL || country[0] == 0)
            return g_strconcat(club, "  ", last, ", ", first, NULL);
        else if (club == NULL || club[0] == 0)
            return g_strconcat(country, "  ", last, ", ", first, NULL);
        return g_strconcat(country, "/", club, "  ", last, ", ", first, NULL);
    case 3:
        return g_strconcat(country, "  ", last, ", ", first, NULL);
    case 4:
        return g_strconcat(club, "  ", last, ", ", first, NULL);
    case 5:
        return g_strconcat(country, "  ", last, NULL);
    case 6:
        return g_strconcat(club, "  ", last, NULL);
    case 7:
        return g_strconcat(last, ", ", first, NULL);
    case 8:
        return g_strdup(last);
    case 9:
        return g_strdup(country);
    case 10:
        return g_strdup(club);
    }

    return NULL;
}

gchar saved_first1[32], saved_first2[32], saved_last1[32], saved_last2[32], saved_cat[16];
gchar saved_country1[8], saved_country2[8];

void show_message(gchar *cat_1,
                  gchar *blue_1,
                  gchar *white_1,
                  gchar *cat_2,
                  gchar *blue_2,
                  gchar *white_2,
                  gint flags)
{
    gchar buf[32], *name;
    gchar *b_tmp = blue_1, *w_tmp = white_1;
    gchar b_first[32], b_last[32], b_club[32], b_country[32];
    gchar w_first[32], w_last[32], w_club[32], w_country[32];

    b_first[0] = b_last[0] = b_club[0] = b_country[0] = 0;
    w_first[0] = w_last[0] = w_club[0] = w_country[0] = 0;
    saved_first1[0] = saved_first2[0] = saved_last1[0] = saved_last2[0] = saved_cat[0] = 0;
    saved_country1[0] = saved_country2[0] = 0;

    if (sides_switched) {
        blue_1 = w_tmp;
        white_1 = b_tmp;
    }

    parse_name(blue_1, b_first, b_last, b_club, b_country);
    parse_name(white_1, w_first, w_last, w_club, w_country);

    strncpy(saved_last1, b_last, sizeof(saved_last1)-1);
    strncpy(saved_last2, w_last, sizeof(saved_last2)-1);
    strncpy(saved_first1, b_first, sizeof(saved_first1)-1);
    strncpy(saved_first2, w_first, sizeof(saved_first2)-1);
    strncpy(saved_cat, cat_1, sizeof(saved_cat)-1);
    strncpy(saved_country1, b_country, sizeof(saved_country1)-1);
    strncpy(saved_country2, w_country, sizeof(saved_country2)-1);

    if (dsp_layout == 6) {
        g_utf8_strncpy(buf, b_first, 1);
        snprintf(b_first, sizeof(b_first), "%s.", buf);

        g_utf8_strncpy(buf, w_first, 1);
        snprintf(w_first, sizeof(w_first), "%s.", buf);
    }

    set_text(MY_LABEL(cat1), cat_1);
    set_text2(MY_LABEL(cat1), "");

    if (dsp_layout == 7) {
#if 0
        // divide category on two lines
        snprintf(buf, sizeof(buf), "%s", cat_1);
        gchar *p = strrchr(buf, '-');
        if (!p)
            p = strrchr(buf, '+');
        if (!p)
            p = strrchr(buf, ' ');
        if (p) {
            set_text2(cat1, p);
            *p = 0;
            set_text(cat1, buf);
        }
#endif
        // Show flags. Country must be in IOC format.
        set_text(flag_blue, b_country);
        set_text(flag_white, w_country);
    }

    if (labels[blue_club].w > 0)
        set_text(MY_LABEL(blue_club), b_club);

    if (labels[white_club].w > 0)
        set_text(MY_LABEL(white_club), w_club);

    name = get_name_by_layout(b_first, b_last, b_club, b_country);
    set_text(MY_LABEL(blue_name_1), name);
    g_free(name);

    name = get_name_by_layout(w_first, w_last, w_club, w_country);
    set_text(MY_LABEL(white_name_1), name);
    g_free(name);

    set_text(MY_LABEL(cat2),         cat_2);

    parse_name(blue_2, b_first, b_last, b_club, b_country);
    parse_name(white_2, w_first, w_last, w_club, w_country);

    name = get_name_by_layout(b_first, b_last, b_club, b_country);
    set_text(MY_LABEL(blue_name_2), name);
    g_free(name);

    name = get_name_by_layout(w_first, w_last, w_club, w_country);
    set_text(MY_LABEL(white_name_2), name);
    g_free(name);

    if (flags & MATCH_FLAG_JUDOGI1_NOK)
        set_text(MY_LABEL(comment),
                 white_first ?
                 _("White has a judogi problem.") :
                 _("Blue has a judogi problem."));
    else if (flags & MATCH_FLAG_JUDOGI2_NOK)
        set_text(MY_LABEL(comment),
                 white_first ?
                 _("Blue has a judogi problem.") :
                 _("White has a judogi problem."));
    else
        set_text(MY_LABEL(comment), "");

    expose_label(NULL, cat1);
    expose_label(NULL, blue_name_1);
    expose_label(NULL, white_name_1);
    expose_label(NULL, cat2);
    expose_label(NULL, blue_name_2);
    expose_label(NULL, white_name_2);
    expose_label(NULL, blue_club);
    expose_label(NULL, white_club);
    expose_label(NULL, flag_blue);
    expose_label(NULL, flag_white);
    expose_label(NULL, comment);

    if (big_dialog)
        show_big();

    update_tvlogo = TRUE;
}

gchar *get_name(gint who)
{
        if (who == BLUE)
                return get_text(blue_name_1);
        return get_text(white_name_1);
}

gchar *get_cat(void)
{
        return get_text(cat1);
}

void set_comment_text(gchar *txt)
{
    if (golden_score && (txt == NULL || txt[0] == 0))
        txt = _("Golden Score");

        set_text(MY_LABEL(comment), txt);
	expose_label(NULL, comment);

	if (big_dialog)
		show_big();
}

void voting_result(GtkWidget *w,
                   gpointer   data)
{
    blue_wins_voting = FALSE;
    white_wins_voting = FALSE;
    hansokumake_to_blue = FALSE;
    hansokumake_to_white = FALSE;
    result_hikiwake = FALSE;

    switch (ptr_to_gint(data)) {
    case HANTEI_BLUE:
        if (white_first)
            set_text(MY_LABEL(comment), _("White won the voting"));
        else
            set_text(MY_LABEL(comment), _("Blue won the voting"));
	blue_wins_voting = TRUE;
        break;
    case HANTEI_WHITE:
        if (white_first)
            set_text(MY_LABEL(comment), _("Blue won the voting"));
        else
            set_text(MY_LABEL(comment), _("White won the voting"));
	white_wins_voting = TRUE;
        break;
    case HANSOKUMAKE_BLUE:
        if (white_first)
            set_text(MY_LABEL(comment), _("Hansokumake to white"));
        else
            set_text(MY_LABEL(comment), _("Hansokumake to blue"));
	hansokumake_to_blue = TRUE;
        break;
    case HANSOKUMAKE_WHITE:
        if (white_first)
            set_text(MY_LABEL(comment), _("Hansokumake to blue"));
        else
            set_text(MY_LABEL(comment), _("Hansokumake to white"));
	hansokumake_to_white = TRUE;
        break;
    case HIKIWAKE:
        set_text(MY_LABEL(comment), _("Hikiwake"));
        result_hikiwake = TRUE;
        break;
    case CLEAR_SELECTION:
        set_text(MY_LABEL(comment), "");
        break;
    }
    expose_label(NULL, comment);

    set_hantei_winner(ptr_to_gint(data));
}

gboolean delete_big(gpointer data)
{
    big_dialog = FALSE;
    gtk_window_set_title(GTK_WINDOW(main_window), "JudoTimer");
    init_display();
    return FALSE;
}

static void show_big(void)
{
    SDL_Rect rect;

    /* Show dialog */
    if (labels[ask_box].hide == 0 || labels[upper_box].hide == 0)
	return;

    rect.x = rect.y = 0;
    rect.w = W(1000); rect.h = H(200);
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 255, 255, 255));

    SDL_Color color;
    SDL_Surface *text;
    SDL_Rect dest;
    int size = H(100);
    if (strlen(big_text) >= 12) size = H(50);
    color.r = 0; color.g = 0; color.b = 0;
    text = TTF_RenderText_Solid(get_font(size, 0), big_text, color);
    dest.x = (W(1000) - text->w)/2;
    dest.y = (H(200) - text->h)/2;
    dest.w = dest.h = 0;

    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);

    //refresh_darea();
}

void display_big(gchar *txt, gint tmo_sec)
{
    gtk_window_set_title(GTK_WINDOW(main_window), txt);
    strncpy(big_text, txt, sizeof(big_text)-1);
    big_end = time(NULL) + tmo_sec;
    big_dialog = TRUE;
    if (no_big_text == FALSE)
	show_big();
}

void reset_display(gint key)
{
    gint pts[4] = {0,0,0,0};

    set_timer_run_color(FALSE, FALSE);
    set_timer_osaekomi_color(OSAEKOMI_DSP_NO, 0);
    set_osaekomi_value(0, 0);

    if (golden_score == FALSE || rules_leave_score == FALSE)
        set_points(pts, pts);

    //       set_text(MY_LABEL(comment), "");
    //expose(darea, 0, 0);
}

#define T printf("here %d\n", __LINE__)
static void expose_label(cairo_t *c, gint w)
{
    cairo_text_extents_t extents;
    int x, y, x1, y1, wi, he;
    gboolean delc = FALSE;
    gboolean two_lines = labels[w].text2 && labels[w].text2[0];
    gchar buf1[32], buf2[32];
    gchar *txt1 = labels[w].text, *txt2 = labels[w].text2;
    int fsize;
    gint zok = 0, orun = current_osaekomi_state == OSAEKOMI_DSP_YES;
    SDL_Surface *text, *text2 = NULL;

    if (labels[w].w == 0 || labels[w].hide)
        return;

    x1 = W(labels[w].x);
    y1 = H(labels[w].y);
    wi = W(labels[w].w);
    he = H(labels[w].h);

    if (hide_clock_if_osaekomi) {
	if ((w >= t_min && w <= t_sec && orun) ||
	    ((w == o_tsec || w == o_sec) && orun == 0)) {
	    return;
	}
    }

    if (hide_zero_osaekomi_points) {
	if ((w == bs || w == ws) && txt1 &&
	    ((txt1[0] == '0' && txt1[1] == 0) || txt1[0] == 0))
	    zok = hide_zero_osaekomi_points;
    }

    labels[w].status |= LABEL_STATUS_EXPOSE;

    if (!c) {
        delc = TRUE;
	c = darea;
    }


    if (labels[w].bg_r >= 0 && (zok & 2) == 0) {
	SDL_Rect rect;
	rect.x = x1; rect.y = y1; rect.w = wi; rect.h = he;
	SDL_FillRect(c, &rect, SDL_MapRGB(c->format, labels[w].bg_r,
					  labels[w].bg_g, labels[w].bg_b));
    } else {
	SDL_Rect rect;
	rect.x = x1; rect.y = y1; rect.w = wi; rect.h = he;
	SDL_FillRect(c, &rect, SDL_MapRGBA(c->format, 0, 0, 0, 255));

	if (dsp_layout == 7 && background_image) {
	    SDL_Rect dest, src;
	    int w2 = background_image->w;
	    int h2 = background_image->h;
	    src.x = labels[w].x*w2/1000;
	    src.y = labels[w].y*h2/1000;
	    src.w = labels[w].w*w2/1000;
	    src.h = labels[w].h*h2/1000;
	    dest.x = x1; dest.y = y1; dest.w = wi; dest.h = he;
	    SDL_BlitScaled(background_image, &src, darea, &dest);
	}
    }

    if (zok & 1)
	txt1 = "";

    if (labels[w].size)
        fsize = H(labels[w].size * labels[w].h / 1000);
    else if (two_lines)
        fsize =  H(labels[w].h*4/10);
    else
        fsize = H(labels[w].h*8/10);

#if 0
    if (font_face[0])
        cairo_select_font_face(c, font_face, font_slant, font_weight);
#endif
    //    cairo_set_font_size(c, fsize);
    //cairo_text_extents(c, txt1, &extents);

    if (w != flag_blue && w != flag_white) {
	SDL_Color color;
	color.r = labels[w].fg_r;
	color.g = labels[w].fg_g;
	color.b = labels[w].fg_b;

	SDL_Surface *text = TTF_RenderText_Solid(get_font(fsize, 0), txt1, color);

	if (two_lines)
	    text2 = TTF_RenderText_Solid(get_font(fsize, 0), txt2, color);
	else if (labels[w].wrap &&
		 text->w > W(labels[w].w)) {
	    // needs two lines
	    snprintf(buf1, sizeof(buf1), "%s", labels[w].text);
	    gchar *p = strrchr(buf1, '-');
	    if (!p)
		p = strrchr(buf1, '+');
	    if (!p)
		p = strrchr(buf1, ' ');
	    if (p) {
		snprintf(buf2, sizeof(buf2), "%s", p);
		*p = 0;
		txt1 = buf1;
		txt2 = buf2;
		fsize /= 2;
		SDL_FreeSurface(text);
		text = TTF_RenderText_Solid(get_font(fsize, 0), txt1, color);
		text2 = TTF_RenderText_Solid(get_font(fsize, 0), txt2, color);
		two_lines = TRUE;
	    }
        }

	if (labels[w].xalign < 0)
	    x = x1;
	else if (labels[w].xalign > 0)
	    x = x1 + wi - text->w;
	else
	    x = x1 + (wi - text->w)/2;
	SDL_Rect dest, src;
	dest.w = dest.h = 0;

	if (two_lines) {
	    dest.x = x; dest.y = y1 + he/2 - text->h;
	    src.x = 0; src.y = 0; src.w = text->w; src.h = text->h;
	    if (src.w > wi) src.w = wi;
	    //if (src.h > he/2) src.h = he/2;
	    SDL_BlitSurface(text, &src, c, &dest);
	    SDL_FreeSurface(text);

	    dest.x = x; dest.y = y1 + he/2;
	    src.x = 0; src.y = 0; src.w = text2->w; src.h = text2->h;
	    if (src.w > wi) src.w = wi;
	    //if (src.h > he/2) src.h = he/2;
	    SDL_BlitSurface(text2, &src, c, &dest);
	    SDL_FreeSurface(text2);
	} else {
	    dest.x = x; dest.y = y1 + (he - text->h)/2;
	    src.x = 0; src.y = 0; src.w = text->w; src.h = text->h;
	    if (src.w > wi) src.w = wi;
	    //if (src.h > he) src.h = he;
	    SDL_BlitSurface(text, &src, c, &dest);
	    SDL_FreeSurface(text);
	}
    }

    if (w == flag_blue || w == flag_white) {
	if (txt1 && txt1[0]) {
	    SDL_Surface *flag = get_flag(txt1);
	    if (flag) {
		SDL_Rect dest;
		dest.x = x1; dest.y = y1; dest.w = wi; dest.h = he;
		SDL_BlitScaled(flag, NULL, darea, &dest);
	    } else {
		if (w == flag_blue) flag_req |= 1;
		else flag_req |= 2;
#if 0
		snprintf(buf1, sizeof(buf1), "/flags-ioc/%s.png", txt1);
		snprintf(buf2, sizeof(buf2), "%s.png", txt1);
		emscripten_async_wget(buf1, buf2, flagonload, menupiconerror);
#endif
	    }
	}
    }
}

static void clear_bg(cairo_t *c)
{
    SDL_Rect dest;
    dest.x = 0; dest.y = 0; dest.w = paper_width; dest.h = paper_height;

    if (background_r < 0)
	SDL_FillRect(darea, &dest, SDL_MapRGB(darea->format, 0, 0, 0));
    else
	SDL_FillRect(darea, &dest,
		     SDL_MapRGB(darea->format, background_r, background_g, background_b));
}

static void init_display(void)
{
    gint i;

    if (!darea)
	return;
#if 0
    clear_bg(darea);

    if (dsp_layout == 7 && background_image) {
	SDL_Rect dest;
	dest.x = 0; dest.y = 0; dest.w = darea->w; dest.h = darea->h;
	SDL_BlitScaled(background_image, NULL, darea, &dest);
    }

    for (i = 0; i < num_labels; i++)
        expose_label(darea, i);

    if (big_dialog)
        show_big();
#endif
    refresh_darea();
}

#if 0
static gboolean configure_event_cb(GtkWidget         *widget,
                                   GdkEventConfigure *event,
                                   gpointer           data)
{
    int isFullscreen;

    emscripten_get_canvas_size(&paper_width, &paper_height, &isFullscreen);
    paper_width = gtk_widget_get_allocated_width(widget);
    paper_height = gtk_widget_get_allocated_height(widget);

    init_display();

    return TRUE;
}
#endif

/* This is called when we need to draw the windows contents */
static void expose(void)
{
    gint i;
    int isFullscreen;
    //static int xxx;

    emscripten_get_canvas_size(&paper_width, &paper_height, &isFullscreen);
    //printf("%d: paper =%d/%d\n", xxx++, paper_width, paper_height);

    clear_bg(surface);

    if (dsp_layout == 7 && background_image) {
	SDL_Rect dest;
	dest.x = 0; dest.y = 0; dest.w = paper_width; dest.h = paper_height;
	SDL_BlitScaled(background_image, NULL, darea, &dest);
	//SDL_BlitSurface(background_image, NULL, darea, NULL);
    }

    for (i = 0; i < num_labels; i++) {
        expose_label(surface, i);
    }

    SDL_Rect dest;
    dest.x = dest.y = dest.w = dest.h = 0;
    dest.y = icontimer - 50;
    if (dest.y > 0) dest.y = 0;
    if (menuicon && icontimer > 2)
	SDL_BlitSurface(menuicon, NULL, darea, &dest);

    if (big_dialog)
        show_big();

    if (menu_on && menubg)
	show_menu();
}

#define CLOSE(_x) (x > _x - 50 && x < _x + 50)
#define MENU_ROWS 10
#define X_L(_x) (x > _x - 50 && x < _x)
#define X_R(_x) (x > _x && x < _x + 50)
#define Y_REG(_y) (y <= _y && y > _y - 70)
// left side buttons
#define X0 14
#define X0_1 84
#define X0_2 211
#define X0_3 117
#define X0_4 234
#define X0_5 149
#define X0_6 189
#define BETWEEN(_x1, _x2) (x >= _x1 && x <= _x2)

#define L1 874
#define R1 950
#define R2 900
#define L3 287
#define R3 396
#define CLMIN 725
#define CRMIN 750
#define CLSEC 810
#define CRSEC 844
#define CLOSEC 915
#define CROSEC 952
#define LINE1 180
#define LINE2 256
#define LINE3 331
#define LINE4 409
#define LINE5 484
#define LINE6 562
#define LINE7 638
#define LINE8 715
#define LINE9 813
#define LINE10 865
#define TEXTBOX_X 534
#define TEXTBOX_W (R1 - L1)

static int matchtime = 0, hansokumake = 0;
static int goldenscore = 0, hantei = 0;

static const char *matchtimetext[] = {
    "Automatic", "2 min (kids)", "2 min", "3 min", "4 min", "5 min"
};
static const char *gstext[] = {
    "Auto", "No limit", "1 min", "2 min", "3 min", "4 min", "5 min"
};
static double gstime[] = {
    1000.0, 0.0, 10.0, 120.0, 180.0, 240.0, 300.0
};
static const char *hanteitext[] = {
    " ", "White wins", "Blue wins"
};
static const char *hansokumaketext[] = {
    " ", "To white", "To blue"
};
static const char *audiotext[] = {
    "None", "AirHorn", "BikeHorn", "CarAlarm", "Doorbell",
    "IndutrialAlarm", "IntruderAlarm", "RedAlert", "TrainHorn", "TwoToneDoorbell",
    NULL
};
static int audio = 0;

static int handle_menu(int x1, int y1)
{
    int x, y, a;

    /* scaled coordinates 0 - 999 */
    x = x1*1000/menubg->w;
    y = y1*1000/menubg->h;
    if (Y_REG(LINE1)) {
	if (X_R(R1) && tatami < 10) {
	    tatami++;
	    return TRUE;
	} else if (X_L(L1) && tatami > 1) {
	    tatami--;
	    return TRUE;
	}

	if (X_R(R3) && matchtime < 5) {
	    matchtime++;
	    return TRUE;
	} else if (X_L(L3) && matchtime > 0) {
	    matchtime--;
	    return TRUE;
	}

	if (BETWEEN(X0, X0_1)) {
	    menu_on = FALSE;
	    clock_key(SDLK_0 + matchtime, 0);
	    return TRUE;
	}
    } else if (Y_REG(LINE2)) {
	a = dsp_layout;
	if (X_R(R1) && a < 7) {
	    a++;
	    select_display_layout(NULL, gint_to_ptr(a));
	    return TRUE;
	} else if (X_L(L1) && a > 1) {
	    a--;
	    select_display_layout(NULL, gint_to_ptr(a));
	    return TRUE;
	}

	if (X_R(R3) && goldenscore < 6) {
	    goldenscore++;
	    return TRUE;
	} else if (X_L(L3) && goldenscore > 0) {
	    goldenscore--;
	    return TRUE;
	}

	if (BETWEEN(X0, X0_2)) {
	    menu_on = FALSE;
	    set_gs(gstime[goldenscore]);
	    clock_key(SDLK_9, 0);
	    return TRUE;
	}
    } else if (Y_REG(LINE3)) {
	if (X_L(L1) || X_R(R1)) {
	    if (bgcolor == &color_red)
		bgcolor = &color_blue;
	    else
		bgcolor = &color_red;
	    set_colors();
	    init_display();
	    return TRUE;
	}

	if (X_R(R3) && hantei < 2) {
	    hantei++;
	    return TRUE;
	} else if (X_L(L3) && hantei > 0) {
	    hantei--;
	    return TRUE;
	}

	if (BETWEEN(X0, X0_3)) {
	    menu_on = FALSE;
	    voting_result(NULL, (gpointer)(hantei == 1 ?
					   HANTEI_BLUE : HANTEI_WHITE));
	    return TRUE;
	}
    } else if (Y_REG(LINE4)) {
	char buf[64];

	if (X_R(R1) && audio < 9) {
	    audio++;
	    snprintf(buf, sizeof(buf), "audio = new Audio('%s.mp3')",
		     audiotext[audio]);
	    emscripten_run_script(buf);
	    emscripten_run_script("audio.play()");
	    return TRUE;
	} else if (X_L(L1) && audio > 0) {
	    audio--;
	    snprintf(buf, sizeof(buf), "audio = new Audio('%s.mp3')",
		     audiotext[audio]);
	    emscripten_run_script(buf);
	    emscripten_run_script("audio.play()");
	    return TRUE;
	}

	if (X_R(R3) && hansokumake < 2) {
	    hansokumake++;
	    return TRUE;
	} else if (X_L(L3) && hansokumake > 0) {
	    hansokumake--;
	    return TRUE;
	}

	if (BETWEEN(X0, X0_4)) {
	    menu_on = FALSE;
	    voting_result(NULL, (gpointer)(hansokumake == 1 ?
					   HANSOKUMAKE_BLUE : HANSOKUMAKE_WHITE));
	    return TRUE;
	}
    } else if (Y_REG(LINE5)) {
	if (X_R(R2)) {
	    rules_stop_ippon_2 = !rules_stop_ippon_2;
	    return TRUE;
	}

	if (BETWEEN(X0, X0_5)) {
	    menu_on = FALSE;
	    voting_result(NULL, (gpointer)HIKIWAKE);
	    return TRUE;
	}
    } else if (Y_REG(LINE6)) {
	if (X_R(R2)) {
	    rules_confirm_match = !rules_confirm_match;
	    return TRUE;
	}

	if (BETWEEN(X0, X0_6)) {
	    menu_on = FALSE;
	    display_comp_window(saved_cat, saved_last1, saved_last2,
				saved_first1, saved_first2, saved_country1, saved_country2);
	    return TRUE;
	}
    } else if (Y_REG(LINE7)) {
	if (X_R(R2)) {
	    show_competitor_names = !show_competitor_names;
	    return TRUE;
	}
    } else if (Y_REG(LINE8)) {
	if (X_R(R2)) {
	    no_big_text = !no_big_text;
	    return TRUE;
	}
    } else if (Y_REG(LINE10)) {
	if (X_L(CLMIN)) {
	    set_clocks(get_elap()-60, get_oelap());
	    return TRUE;
	} else if (X_R(CRMIN)) {
	    set_clocks(get_elap()+60, get_oelap());
	    return TRUE;
	} else if (X_L(CLSEC)) {
	    hajime_dec_func();
	    return TRUE;
	} else if (X_R(CRSEC)) {
	    hajime_inc_func();
	    return TRUE;
	} else if (X_L(CLOSEC)) {
	    osaekomi_dec_func();
	    return TRUE;
	} else if (X_R(CROSEC)) {
	    osaekomi_inc_func();
	    return TRUE;
	}
    } else if (Y_REG(866) && X_R(42) && X_L(92)) {
	menu_on = FALSE;
	return TRUE;
    }

    return FALSE;
}

static void show_menu(void)
{
    char buf[16];
    SDL_Rect dest;
    int f = 9*(LINE2-LINE1)/10;

    dest.x = dest.y = dest.w = dest.h = 0;
    SDL_BlitSurface(menubg, NULL, darea, &dest);

    snprintf(buf, sizeof(buf), "%d", tatami);
    textbox(L1, LINE1, TEXTBOX_W, f, buf);

    snprintf(buf, sizeof(buf), "%d", dsp_layout);
    textbox(L1, LINE2, TEXTBOX_W, f, buf);

    textbox(L1, LINE3, TEXTBOX_W, f, (bgcolor == &color_red) ? "Red" : "Blue");
    textbox(L1, LINE4, TEXTBOX_W, f, audiotext[audio]);

    checkbox(R2, LINE5, 0, f, rules_stop_ippon_2);
    checkbox(R2, LINE6, 0, f, rules_confirm_match);
    checkbox(R2, LINE7, 0, f, show_competitor_names);
    checkbox(R2, LINE8, 0, f, no_big_text);

    int t, min, sec, oSec;
    t = get_elap();
    min = t / 60;
    sec =  t - min*60;
    oSec = get_oelap();

    snprintf(buf, sizeof(buf), "%d", min);
    textbox(CLMIN, LINE10, CRMIN - CLMIN, f, buf);
    snprintf(buf, sizeof(buf), "%d", sec);
    textbox(CLSEC, LINE10, CRSEC - CLSEC, f, buf);
    snprintf(buf, sizeof(buf), "%d", oSec);
    textbox(CLOSEC, LINE10, CROSEC - CLOSEC, f, buf);

    textbox(L3, LINE1, R3 - L3, f, matchtimetext[matchtime]);
    textbox(L3, LINE2, R3 - L3, f, gstext[goldenscore]);
    textbox(L3, LINE3, R3 - L3, f, hanteitext[hantei]);
    textbox(L3, LINE4, R3 - L3, f, hansokumaketext[hansokumake]);


}

static void mouse_move(void)
{
    int x, y;
    SDL_GetMouseState(&x, &y);
    if (y < menuicon->h) {
	icontimer = 50;
    }
    if (menu_on &&
	(x > menubg->w || y > menubg->h))
	menu_on = FALSE;
}

static void button_pressed(SDL_Event *event)
{
    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent*)event;
    /* single click with the right mouse button? */
    if (m->button == 1 || m->button == 3) {
	gint x = m->x, y = m->y, i;

	if (menu_on && menubg &&
	    x < menubg->w &&
	    y < menubg->h) {
	    if (handle_menu(x, y))
		expose();
	    return;
	}

	if (menuicon &&
	    x < menuicon->w &&
	    y < menuicon->h) {
	    menu_on = TRUE;
	    expose();
	    return;
	}

	for (i = num_labels - 1; i >= 0; i--) {
	    if (labels[i].w &&
		!labels[i].hide &&
		x >= W(labels[i].x) &&
		x <= W(labels[i].x+labels[i].w) &&
		y >= H(labels[i].y) &&
		y <= H(labels[i].y+labels[i].h)) {
		labels[i].cb(NULL, event, NULL);
		return;
	    }
	}
    }
}

static void key_press(SDL_Event *event)
{
    gboolean ctl = event->key.keysym.mod & SDLK_LCTRL;
    int state = event->key.keysym.mod;

    if (event->type != SDL_KEYDOWN)
        return;

    if (event->key.keysym.sym == SDLK_SPACE) {
	if (!labels[yes_box].hide) {
	    cb_yes_box(NULL, event, NULL);
	    return;
	} else if (!labels[ok_box].hide) {
	    cb_ok_box(NULL, event, NULL);
	    return;
	}
    }

    if (event->key.keysym.sym == GDK_m && ctl) {
        if (menu_hidden) {
            gtk_widget_show(menubar);
	    gtk_window_set_decorated((GtkWindow*)main_window, TRUE);
	    gtk_window_set_keep_above(GTK_WINDOW(main_window), FALSE);
            menu_hidden = FALSE;
        } else {
            gtk_widget_hide(menubar);
	    gtk_window_set_decorated((GtkWindow*)main_window, FALSE);
	    gtk_window_set_keep_above(GTK_WINDOW(main_window), TRUE);
            menu_hidden = TRUE;
	}
	return;
    }
    if (event->key.keysym.sym == GDK_n && ctl && ACTIVE) {
    	clock_key(GDK_0, FALSE);
        return;
    }
    if (event->key.keysym.sym == GDK_g && ctl && ACTIVE) {
    	clock_key(GDK_9, FALSE);
        return;
    }
    if (event->key.keysym.sym == GDK_f) {
	fullscreen = TRUE;
	emscripten_run_script("setfullscreen()");
	//requestFullScreen(0, 1);
        return;
    }
    if (event->key.keysym.sym == GDK_v) // V is a menu accelerator
        return;

    clock_key(event->key.keysym.sym, state);
#if 0
    if (event->key.keysym.sym >= '0' && event->key.keysym.sym < '9') {
	char buf[16];
	snprintf(buf, sizeof(buf), "/nextmatch?t=%d", tatami);
	emscripten_async_wget_data(buf, NULL, onload, onerror);
    }
#endif
    return;
}

static void timeout(void)
{
    update_clock();

    if (big_dialog && time(NULL) > big_end)
        delete_big(NULL);
}


#if 0
static GtkWidget *get_picture(const gchar *name)
{
        gchar *file = g_build_filename(installation_dir, "etc", name, NULL);
        GtkWidget *ebox = gtk_event_box_new();
        GtkWidget *pic = gtk_image_new_from_file(file);
        gtk_widget_show(ebox);
        gtk_widget_show(pic);
        gtk_container_add(GTK_CONTAINER(ebox), pic);
        g_free(file);
        return ebox;
}
#endif

void dump_screen(void)
{
#if 0
    gint i;
    printf("# num x y width height size xalign fg-red fg-green fg-blue bg-red bg-green bg-blue wrap");
    for (i = 0; i < num_labels; i++) {
        printf("%d %1.3f %1.3f %1.3f %1.3f ", i, labels[i].x, labels[i].y, labels[i].w, labels[i].h);
        printf("%1.3f %d %1.3f %1.3f %1.3f %1.3f %1.3f %1.3f %d\n", labels[i].size, labels[i].xalign,
               labels[i].fg_r, labels[i].fg_g, labels[i].fg_b,
               labels[i].bg_r, labels[i].bg_g, labels[i].bg_b, labels[i].wrap);
    }
#endif
}

void EMSCRIPTEN_KEEPALIVE main_loop(void)
{
    time_t now = time(NULL);
    char url[64];
    SDL_Event event;
    static int t;

    while (SDL_PollEvent(&event)) {
	switch(event.type) {
	case SDL_MOUSEBUTTONDOWN: {
	    button_pressed(&event);
	    break;
	}
	case SDL_KEYDOWN: {
	    key_press(&event);
	    break;
	}
	case SDL_VIDEORESIZE: {
	    SDL_ResizeEvent *r = (SDL_ResizeEvent *)&event;
	    break;
	}
	} // switch
    }

    timeout();
    mouse_move();

    if (expose_now || icontimer) {
	expose_now = 0;
	expose();
    }

    if (icontimer > 0)
	icontimer--;

    if (flag_req && !flag_pending) {
	char *txt1, buf1[32], buf2[16];

	if (flag_req & 1) {
	    txt1 = labels[flag_blue].text;
	    flag_req &= 2;
	} else {
	    txt1 = labels[flag_white].text;
	    flag_req &= 1;
	}

	if (txt1 && txt1[0]) {
	    snprintf(buf1, sizeof(buf1), "/flags-ioc/%s.png", txt1);
	    snprintf(buf2, sizeof(buf2), "%s.png", txt1);
	    flag_pending = 1;
	    emscripten_async_wget(buf1, buf2, flagonload, flagonerror);
	}
    }
}

void main_2(void *arg)
{
    select_display_layout(NULL, gint_to_ptr(6));

    //settatami(0);
    setscreensize(0);
    //setclocks_c(0);
    icontimer = 100;

    emscripten_async_wget("/menuicon.png", "menuicon.png", menuicononload, menupiconerror);
    emscripten_async_wget("/menupic.png", "menupic.png", menupiconload, menupiconerror);
    emscripten_async_wget("/timer-custom-2.txt", "timer-custom.txt", customonload, menupiconerror);

    emscripten_set_main_loop(main_loop, 10, 0);
}

int EMSCRIPTEN_KEEPALIVE main()
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window;
    GdkColor   fg, bg;
    time_t     now;
    struct tm *tm;
    gint i;

    printf("SDL_Init=%d\n", SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO));
    darea = SDL_SetVideoMode(640, 480, 32, SDL_HWSURFACE | SDL_SRCALPHA);
    surface = darea;
    printf("TTF_Init=%d\n", TTF_Init());
    init_fonts();
    init_flags();

#define gdk_color_parse(_r, _g, _b, _c)		\
    do {					\
	(_c)->r = _r; (_c)->g = _g; (_c)->b = _b;	\
    } while (0)

    gdk_color_parse(0xff, 0xff, 0x00, &color_yellow);
    gdk_color_parse(0xff, 0xff, 0xff, &color_white);
    gdk_color_parse(0x40, 0x40,0x40, &color_grey);
    gdk_color_parse(0x00, 0xff, 0x00, &color_green);
    gdk_color_parse(0x00, 0x00, 0xff, &color_blue);
    gdk_color_parse(0xff, 0x00, 0x00, &color_red);
    gdk_color_parse(0, 0, 0, &color_black);
    bgcolor_pts = color_black;
    bgcolor_points = color_black;

    now = time(NULL);
    tm = localtime(&now);
    current_year = tm->tm_year+1900;
    srand(now); //srandom(now);
    my_address = now + getpid()*10000;

    /* labels */

    /*0.0          0.2            0.5          0.7
     * | Match:     | Nimi sin     | Next:      | Nimi2 sin
     * | Cat: xx    | Nimi valk    | Cat: yy    | Nimi2 valk
     * | Comment
     * |
     */

#define SMALL_H 32
#define BIG_H   ((1000 - 4*SMALL_H)/3)
#define BIG_H2  ((1000 - 4*SMALL_H)/2)
#define BIG_START (4*SMALL_H)
#define BIG_W  125
#define BIG_W2 250
#define TXTW   120

    GET_LABEL(blue_name_1, "",  TXTW,     0,     500-TXTW, SMALL_H);
    GET_LABEL(white_name_1, "", TXTW,     SMALL_H, 500-TXTW, SMALL_H);
    GET_LABEL(blue_name_2, "",  500+TXTW, 0,     500-TXTW, SMALL_H);
    GET_LABEL(white_name_2, "", 500+TXTW, SMALL_H, 500-TXTW, SMALL_H);
    GET_LABEL(blue_club, "", 0,0,0,0);
    GET_LABEL(white_club, "", 0,0,0,0);

    GET_LABEL(match1, _("Match:"), 0, 0, TXTW, SMALL_H);
    GET_LABEL(match2, _("Next:"),  500, 0, TXTW, SMALL_H);

    GET_LABEL(wazaari, "I", 0,       BIG_START, BIG_W, BIG_H);
    GET_LABEL(yuko, "W",    1.0*BIG_W, BIG_START, BIG_W, BIG_H);
    GET_LABEL(koka, "Y",    2.0*BIG_W, BIG_START, BIG_W, BIG_H);
    GET_LABEL(shido, "S",   3.0*BIG_W, BIG_START, BIG_W, BIG_H);
    GET_LABEL(padding, "",   4.0*BIG_W, BIG_START, 3.0*BIG_W, BIG_H);
    GET_LABEL(sonomama, "SONO", 7.0*BIG_W, BIG_START, BIG_W, BIG_H);

    GET_LABEL(bw, "0", 0,       BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(by, "0", 1.0*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(bk, "0", 2.0*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(bs, "0", 3.0*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(ww, "0", 0,       BIG_START+2*BIG_H, BIG_W, BIG_H);
    GET_LABEL(wy, "0", 1.0*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
    GET_LABEL(wk, "0", 2.0*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
    GET_LABEL(ws, "0", 3.0*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

    GET_LABEL(t_min, "0",  4.0*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(colon, ":",  5.0*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(t_tsec, "0", 6.0*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(t_sec, "0",  7.0*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
    GET_LABEL(o_tsec, "0", 4.0*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
    GET_LABEL(o_sec, "0",  5.0*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

    GET_LABEL(points, "-", 6.5*BIG_W, BIG_START+2*BIG_H, 1.5*BIG_W, BIG_H);
    GET_LABEL(pts_to_blue, "?", 6.0*BIG_W, BIG_START+2*BIG_H, BIG_W/2, BIG_H/2);
    GET_LABEL(pts_to_white, "?", 6.0*BIG_W, BIG_START+2.5*BIG_H, BIG_W/2, BIG_H/2);
    GET_LABEL(comment, "-", 0, 2.0*SMALL_H, 1000, SMALL_H);

    GET_LABEL(cat1, "-", 0, SMALL_H, TXTW, SMALL_H);
    GET_LABEL(cat2, "-", 500, SMALL_H, TXTW, SMALL_H);
    GET_LABEL(gs, "", 0, 0, 0, 0);
    GET_LABEL(flag_blue, "", 0, 0, 0, 0);
    GET_LABEL(flag_white, "", 0, 0, 0, 0);

    GET_LABEL(upper_box, "",  0,   0, 1000, 333);
    GET_LABEL(middle_box, "", 0, 333, 1000, 333);
    GET_LABEL(lower_box, "",  0, 667, 1000, 333);
    GET_LABEL(ask_box, "Start new match?", 0, 0, 300, 50);
    GET_LABEL(yes_box, "YES", 305, 0, 100, 50);
    GET_LABEL(no_box, "NO", 410, 0, 100, 50);
    GET_LABEL(ok_box, "OK", 0, 0, 1000, 50);
    GET_LABEL(rest_time_box, "", 700, 33, 300, 267);
    GET_LABEL(rest_ind_box, "", 600, 33, 90, 267);
    labels[ask_box].hide = 1;
    labels[yes_box].hide = 1;
    labels[no_box].hide = 1;
    labels[ok_box].hide = 1;
    labels[upper_box].hide = 1;
    labels[middle_box].hide = 1;
    labels[lower_box].hide = 1;
    labels[rest_time_box].hide = 1;
    labels[rest_ind_box].hide = 1;

    fg = color_red;
    bg = color_black;
    SET_COLOR(rest_time_box);
    SET_COLOR(rest_ind_box);

    fg = color_white;
    gdk_color_parse(127, 127, 127, &bg);
    SET_COLOR(ask_box);
    SET_COLOR(yes_box);
    SET_COLOR(no_box);

    labels[match1].xalign = -1;
    labels[match2].xalign = -1;
    labels[blue_name_1].xalign = -1;
    labels[white_name_1].xalign = -1;
    labels[blue_name_2].xalign = -1;
    labels[white_name_2].xalign = -1;
    labels[cat1].xalign = -1;
    labels[cat2].xalign = -1;
    labels[gs].xalign = -1;
    labels[blue_club].xalign = -1;
    labels[white_club].xalign = -1;

    labels[wazaari].size = 600;
    labels[yuko].size = 600;
    labels[koka].size = 600;
    labels[shido].size = 600;

    labels[bs].size = 600;
    labels[ws].size = 600;

    labels[pts_to_blue].size = 500;
    labels[pts_to_white].size = 500;

    labels[sonomama].size = 100;
    labels[sonomama].text2 = g_strdup("MAMA");

    /* colors */

    fg = color_white;
    gdk_color_parse(0, 0, 0, &bg);
    SET_COLOR(match1);
    SET_COLOR(match2);
    SET_COLOR(blue_name_1);
    SET_COLOR(white_name_1);
    SET_COLOR(blue_name_2);
    SET_COLOR(white_name_2);
    SET_COLOR(blue_club);
    SET_COLOR(white_club);
    SET_COLOR(comment);
    SET_COLOR(wazaari);
    SET_COLOR(yuko);
    SET_COLOR(koka);
    SET_COLOR(shido);
    SET_COLOR(cat1);
    SET_COLOR(cat2);
    SET_COLOR(gs);

    fg = color_yellow;
    SET_COLOR(t_min);
    SET_COLOR(colon);
    SET_COLOR(t_tsec);
    SET_COLOR(t_sec);

    fg = color_grey;
    SET_COLOR(o_tsec);
    SET_COLOR(o_sec);
    SET_COLOR(points);
    SET_COLOR(pts_to_blue);
    SET_COLOR(pts_to_white);

    fg = color_white;
    gdk_color_parse(0, 0, 255, &bg);
    SET_COLOR(bw);
    SET_COLOR(by);
    SET_COLOR(bk);

    gdk_color_parse(0xdd, 0xd8, 0x9a, &fg);
    SET_COLOR(bs);

    gdk_color_parse(0, 0, 0, &fg);
    gdk_color_parse(0xff, 0xff, 0xff, &bg);
    SET_COLOR(ww);
    SET_COLOR(wy);
    SET_COLOR(wk);

    gdk_color_parse(0xdd, 0x6c, 0x00, &fg);
    SET_COLOR(ws);

    gdk_color_parse(0xaf, 0, 0, &fg);
    gdk_color_parse(0, 0, 0, &bg);
    SET_COLOR(sonomama);


    gdk_color_parse(0, 0, 0, &fg);
    gdk_color_parse(0, 0, 0, &bg);

    for (i = 0; i < num_labels; i++)
        defaults_for_labels[i] = labels[i];

    automatic = TRUE;
#if 0
    emscripten_async_call(main_2, NULL, 1000); // avoid startup delays and intermittent errors
#endif
#if 1
    select_display_layout(NULL, gint_to_ptr(6));

    //settatami(0);
    setscreensize(0);
    //setclocks_c(0);
    icontimer = 100;

    emscripten_async_wget("/menuicon.png", "menuicon.png", menuicononload, menupiconerror);
    emscripten_async_wget("/menupic.png", "menupic.png", menupiconload, menupiconerror);
    emscripten_async_wget("/timer-custom-2.txt", "timer-custom.txt", customonload, menupiconerror);

    emscripten_set_main_loop(main_loop, 10, 0);
#endif
    return 0;
}

void menuicononload(const char *str)
{
    menuicon = IMG_Load(str);
}

void custombgonload(const char *str)
{
    background_image = IMG_Load(str);
}

void customonload(const char *str)
{
}

void menupiconload(const char *str)
{
    menubg = IMG_Load(str);
}

void menupiconerror(const char *str)
{
    printf("%s failed\n", str);
}

void flagonload(const char *str)
{
    SDL_Surface *img = IMG_Load(str);
    insert_flag(str, img);
    refresh_darea();
    flag_pending = 0;
}
void flagonerror(const char *str)
{
    printf("Flag %s failed\n", str);
    flag_pending = 0;
}

void flag1onload(const char *str)
{
    SDL_Surface *img = IMG_Load(str);
    if (current_flags[0].image)
	SDL_FreeSurface(current_flags[0].image);
    current_flags[0].image = img;
    strcpy(current_flags[0].name, str);

    expose_label(NULL, flag_blue);
    //flagonload(flag_blue, str);
}

void flag2onload(const char *str)
{
    SDL_Surface *img = IMG_Load(str);
    if (current_flags[1].image)
	SDL_FreeSurface(current_flags[1].image);
    current_flags[1].image = img;
    strcpy(current_flags[1].name, str);

    expose_label(NULL, flag_white);
    //flagonload(flag_white, str);
}

static void set_colors(void)
{
    GdkColor s_blue, s_white;

    gdk_color_parse(0xdd, 0xd8, 0x9a, &s_blue);
    gdk_color_parse(0xdd, 0x6c, 0x00, &s_white);

    if (dsp_layout != 7) {
	if (white_first) {
	    set_fg_color(bw, GTK_STATE_NORMAL, &color_black);
	    set_fg_color(by, GTK_STATE_NORMAL, &color_black);
	    set_fg_color(bk, GTK_STATE_NORMAL, &color_black);
	    set_fg_color(bs, GTK_STATE_NORMAL, &s_white);
	    set_fg_color(ww, GTK_STATE_NORMAL, &color_white);
	    set_fg_color(wy, GTK_STATE_NORMAL, &color_white);
	    set_fg_color(wk, GTK_STATE_NORMAL, &color_white);
	    set_fg_color(ws, GTK_STATE_NORMAL, &s_blue);

	    set_bg_color(bw, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(by, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(bk, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(bs, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(ww, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(wy, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(wk, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(ws, GTK_STATE_NORMAL, bgcolor);
	} else {
	    set_fg_color(bw, GTK_STATE_NORMAL, &color_white);
	    set_fg_color(by, GTK_STATE_NORMAL, &color_white);
	    set_fg_color(bk, GTK_STATE_NORMAL, &color_white);
	    set_fg_color(bs, GTK_STATE_NORMAL, &s_blue);
	    set_fg_color(ww, GTK_STATE_NORMAL, &color_black);
	    set_fg_color(wy, GTK_STATE_NORMAL, &color_black);
	    set_fg_color(wk, GTK_STATE_NORMAL, &color_black);
	    set_fg_color(ws, GTK_STATE_NORMAL, &s_white);

	    set_bg_color(bw, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(by, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(bk, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(bs, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(ww, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(wy, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(wk, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(ws, GTK_STATE_NORMAL, &color_white);
	}
    }

    if (dsp_layout == 7) {
#if 0
        if (white_first) {
            set_fg_color(blue_name_1, GTK_STATE_NORMAL, &color_black);
            set_fg_color(blue_club, GTK_STATE_NORMAL, &color_black);
            set_fg_color(white_name_1, GTK_STATE_NORMAL, &color_white);
            set_fg_color(white_club, GTK_STATE_NORMAL, &color_white);

            set_bg_color(blue_name_1, GTK_STATE_NORMAL, &color_white);
            set_bg_color(blue_club, GTK_STATE_NORMAL, &color_white);
            set_bg_color(white_name_1, GTK_STATE_NORMAL, bgcolor);
            set_bg_color(white_club, GTK_STATE_NORMAL, bgcolor);
        } else {
            set_fg_color(blue_name_1, GTK_STATE_NORMAL, &color_white);
            set_fg_color(blue_club, GTK_STATE_NORMAL, &color_white);
            set_fg_color(white_name_1, GTK_STATE_NORMAL, &color_black);
            set_fg_color(white_club, GTK_STATE_NORMAL, &color_black);

            set_bg_color(blue_name_1, GTK_STATE_NORMAL, bgcolor);
            set_bg_color(blue_club, GTK_STATE_NORMAL, bgcolor);
            set_bg_color(white_name_1, GTK_STATE_NORMAL, &color_white);
            set_bg_color(white_club, GTK_STATE_NORMAL, &color_white);
        }
#endif
    }
}

#if 0
#define SWITCH_TEXTS(_a, _b) \
    do { gchar _s[32]; strncpy(_s, _a, sizeof(_s)-1); \
        strncpy(_a, _b, sizeof(_a)-1); strncpy(_b, _s, sizeof(_b)-1); } while (0)

#endif

static void set_position(gint lbl, int x, int y, int w, int h)
{
	labels[lbl].x = x;
	labels[lbl].y = y;
	labels[lbl].w = w;
	labels[lbl].h = h;
}

void set_gs_text(gchar *txt)
{
    set_text(gs, txt);
}

static gchar *get_float(gchar *p, int *result)
{
    gdouble r = 0;
    gdouble decimal = 0;
    gboolean neg = FALSE;
    while (*p && *p <= ' ') p++;
    while (*p && ((*p >= '0' && *p <= '9') || *p == '.' || *p == ',' || *p == '-')) {
        if (*p == '-')
            neg = TRUE;
        else if (*p == '.' || *p == ',')
            decimal = 0.1;
        else if (decimal) {
            r = r + (*p - '0')*decimal;
            decimal *= 0.1;
        } else {
            r = 10*r + (*p - '0');
        }
        p++;
    }
    if (neg) r = -r;
    *result = (int)(1000*r);
    return p;
}

static gchar *get_integer(gchar *p, gint *result)
{
    gint r = 0;
    gboolean neg = FALSE;
    while (*p && *p <= ' ') p++;
    while (*p && ((*p >= '0' && *p <= '9') || *p == '-')) {
        if (*p == '-')
            neg = TRUE;
        else
            r = 10*r + (*p - '0');
        p++;
    }
    if (neg) r = -r;
    *result = r;
    return p;
}

#define GF(x) p = get_float(p, &x)
#define GF2(x) do {p = get_float(p, &x); x = (x*255)/1000; } while (0)
#define GI(x) p = get_integer(p, &x)
#define GC(_x) do { int a;				\
	GF(a); _x.r = 255*a/1000;			\
	GF(a); _x.g = 255*a/1000;			\
	GF(a); _x.b = 255*a/1000;			\
    } while (0)

void select_display_layout(GtkWidget *menu_item, gpointer data)
{
#define NO_SHOW(x) set_position(x, 0, 0, 0, 0)
    FILE *f;
    gint i;

    // restore default values
    for (i = 0; i < num_labels; i++) {
        labels[i].x = defaults_for_labels[i].x;
        labels[i].y = defaults_for_labels[i].y;
        labels[i].w = defaults_for_labels[i].w;
        labels[i].h = defaults_for_labels[i].h;
        labels[i].size = defaults_for_labels[i].size;
        labels[i].xalign = defaults_for_labels[i].xalign;
        labels[i].fg_r = defaults_for_labels[i].fg_r;
        labels[i].fg_g = defaults_for_labels[i].fg_g;
        labels[i].fg_b = defaults_for_labels[i].fg_b;
        labels[i].bg_r = defaults_for_labels[i].bg_r;
        labels[i].bg_g = defaults_for_labels[i].bg_g;
        labels[i].bg_b = defaults_for_labels[i].bg_b;
        labels[i].bg_a = defaults_for_labels[i].bg_a;
        labels[i].wrap = defaults_for_labels[i].wrap;
    }
    bgcolor_pts = bgcolor_points = color_black;
    background_r = 0; background_g = 0; background_b = 0;

    clock_run = color_white;
    clock_stop = color_yellow;
    clock_bg = color_black;
    oclock_run = color_green;
    oclock_stop = color_grey;
    oclock_bg = color_black;
    hide_clock_if_osaekomi = FALSE;
    req_width = 0; req_height = 0;

    clocks_only = FALSE;
    dsp_layout = ptr_to_gint(data);

    if (background_image)
	cairo_surface_destroy(background_image);
    background_image = NULL;

    switch(dsp_layout) {
        /*if (GTK_CHECK_MENU_ITEM(menu_item)->active) {*/
    case 1:
        set_position(match1, 0, 0, TXTW, SMALL_H);
        set_position(match2, 500, 0, TXTW, SMALL_H);

        set_position(blue_name_1,  TXTW,     0,     500-TXTW, SMALL_H);
        set_position(white_name_1, TXTW,     SMALL_H, 500-TXTW, SMALL_H);
        set_position(blue_name_2,  500+TXTW, 0,     500-TXTW, SMALL_H);
        set_position(white_name_2, 500+TXTW, SMALL_H, 500-TXTW, SMALL_H);
        set_position(cat1,         0, SMALL_H, TXTW, SMALL_H);
        set_position(cat2,         500, SMALL_H, TXTW, SMALL_H);

        set_position(comment,  0, 2*SMALL_H, 1, SMALL_H);

        set_position(wazaari,  0,       BIG_START, BIG_W, BIG_H);
        set_position(yuko,     BIG_W, BIG_START, BIG_W, BIG_H);
        set_position(koka,     2*BIG_W, BIG_START, BIG_W, BIG_H);
        set_position(shido,    3*BIG_W, BIG_START, BIG_W, BIG_H);
        set_position(padding,  4*BIG_W, BIG_START, 3*BIG_W, BIG_H);
        set_position(sonomama, 7*BIG_W, BIG_START, BIG_W, BIG_H);

        set_position(bw, 0,       BIG_START+BIG_H, BIG_W, BIG_H);
        set_position(by, BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
        set_position(bk, 2*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
        set_position(bs, 3*BIG_W, BIG_START+BIG_H, BIG_W, BIG_H);
        set_position(ww, 0,       BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(wy, BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(wk, 2*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(ws, 3*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

        set_position(t_min,  4*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(colon,  5*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(t_tsec, 6*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(t_sec,  7*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);

        set_position(o_tsec, 4*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(o_sec,  5*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

        set_position(points, 65*BIG_W/10, BIG_START+2*BIG_H, 15*BIG_W/10, BIG_H);
        set_position(pts_to_blue,  6*BIG_W, BIG_START+2*BIG_H, BIG_W/2, BIG_H/2);
        set_position(pts_to_white, 6*BIG_W, BIG_START+25*BIG_H/10, BIG_W/2, BIG_H/2);
        break;

    case 2:
        set_position(match1, 0, 0, 0, SMALL_H);
        set_position(match2, 500, 0, TXTW, SMALL_H);

        set_position(blue_name_1,  0,     BIG_START+3*BIG_H/10, 500, 2*BIG_H/10);
        set_position(white_name_1, 0,     BIG_START+25*BIG_H/100, 500, 2*BIG_H/10);
        set_position(blue_name_2,  500+TXTW, 0,     500-TXTW, SMALL_H);
        set_position(white_name_2, 500+TXTW, SMALL_H, 500-TXTW, SMALL_H);
        set_position(cat1,         0, 0, 500, 4*SMALL_H);
        set_position(cat2,         500, SMALL_H, TXTW, SMALL_H);

        set_position(comment,  0, 4.0*SMALL_H, 1.0, SMALL_H);

        set_position(wazaari,  0,       BIG_START+14*BIG_H/10, BIG_W, 2*BIG_H/10);
        set_position(yuko,     BIG_W, BIG_START+14*BIG_H/10, BIG_W, 2*BIG_H/10);
        set_position(koka,     2*BIG_W, BIG_START+14*BIG_H/10, BIG_W, 2*BIG_H/10);
        set_position(shido,    3*BIG_W, BIG_START+14*BIG_H/10, BIG_W, 2*BIG_H/10);
        set_position(padding,  4*BIG_W, BIG_START, 0, BIG_H);
        set_position(sonomama, 7*BIG_W, BIG_START, BIG_W, BIG_H);

        set_position(bw, 0,       BIG_START+BIG_H/2, BIG_W, 9*BIG_H/10);
        set_position(by, BIG_W, BIG_START+BIG_H/2, BIG_W, 9*BIG_H/10);
        set_position(bk, 2*BIG_W, BIG_START+BIG_H/2, BIG_W, 9*BIG_H/10);
        set_position(bs, 3*BIG_W, BIG_START+BIG_H/2, BIG_W, 9*BIG_H/10);
        set_position(ww, 0,       BIG_START+1.6*BIG_H, BIG_W, 9*BIG_H/10);
        set_position(wy, BIG_W, BIG_START+1.6*BIG_H, BIG_W, 9*BIG_H/10);
        set_position(wk, 2*BIG_W, BIG_START+1.6*BIG_H, BIG_W, 9*BIG_H/10);
        set_position(ws, 3*BIG_W, BIG_START+1.6*BIG_H, BIG_W, 9*BIG_H/10);

        set_position(t_min,  4*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(colon,  5*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(t_tsec, 6*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(t_sec,  7*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);

        set_position(o_tsec, 4*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(o_sec,  5*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

        set_position(points, 65*BIG_W/10, BIG_START+2*BIG_H, 15*BIG_W/10, BIG_H);
        set_position(pts_to_blue,  6*BIG_W, BIG_START+2*BIG_H, BIG_W/2, BIG_H/2);
        set_position(pts_to_white, 6*BIG_W, BIG_START+25*BIG_H/10, BIG_W/2, BIG_H/2);
        break;

    case 3:
        set_position(match1, 0, 0, TXTW, 0);
        set_position(match2, 500, 0, TXTW, SMALL_H);

        set_position(blue_name_1,  500,  4*SMALL_H+3*BIG_H/10, 500, 2*BIG_H/10);
        set_position(white_name_1, 0,  4*SMALL_H+3*BIG_H/10, 500, 2*BIG_H/10);
        set_position(blue_name_2,  500+TXTW, 0,     500-TXTW, SMALL_H);
        set_position(white_name_2, 500+TXTW, SMALL_H, 500-TXTW, SMALL_H);
        set_position(cat1,         0, 0, 500, 4*SMALL_H);
        set_position(cat2,         500, SMALL_H, TXTW, SMALL_H);

        set_position(comment,  0, 4.0*SMALL_H+1.5*BIG_H, 1.0, SMALL_H);

        set_position(wazaari,  0,       BIG_START, 0, BIG_H);
        set_position(yuko,     1.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(koka,     2.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(shido,    3.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(padding,  4.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(sonomama, 7.0*BIG_W, BIG_START+1.5*BIG_H, BIG_W, BIG_H/2);

        set_position(bw, 4.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(by, 5.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(bk, 6.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(bs, 7.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(ww, 0,       BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(wy, 1.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(wk, 2.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(ws, 3.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);

        set_position(t_min,  0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);
        set_position(colon,  1.0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);
        set_position(t_tsec, 2.0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);
        set_position(t_sec,  3.0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);

        set_position(o_tsec, 4.5*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(o_sec,  5.5*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

        set_position(points, 6.5*BIG_W, BIG_START+2*BIG_H, 1.5*BIG_W, BIG_H);
        set_position(pts_to_blue,  4.0*BIG_W, BIG_START+2*BIG_H, BIG_W/2, BIG_H/2.0);
        set_position(pts_to_white, 4.0*BIG_W, BIG_START+2.5*BIG_H, BIG_W/2, BIG_H/2.0);
        break;

    case 4:
        set_position(match1, 0, 0, TXTW, 0);
        set_position(match2, 500, 0, TXTW, SMALL_H);

        set_position(blue_name_1,  0,  4*SMALL_H+3*BIG_H/10, 500, 2*BIG_H/10);
        set_position(white_name_1, 500,  4*SMALL_H+3*BIG_H/10, 500, 2*BIG_H/10);
        set_position(blue_name_2,  500+TXTW, 0,     500-TXTW, SMALL_H);
        set_position(white_name_2, 500+TXTW, SMALL_H, 500-TXTW, SMALL_H);
        set_position(cat1,         0, 0, 500, 4.0*SMALL_H);
        set_position(cat2,         500, SMALL_H, TXTW, SMALL_H);

        set_position(comment,  0, 4.0*SMALL_H+1.5*BIG_H, 1.0, SMALL_H);

        set_position(wazaari,  0,       BIG_START, 0, BIG_H);
        set_position(yuko,     1.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(koka,     2.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(shido,    3.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(padding,  4.0*BIG_W, BIG_START, 0, BIG_H);
        set_position(sonomama, 7.0*BIG_W, BIG_START+1.5*BIG_H, BIG_W, BIG_H/2);

        set_position(bw, 0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(by, 1.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(bk, 2.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(bs, 3.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(ww, 4.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(wy, 5.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(wk, 6.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);
        set_position(ws, 7.0*BIG_W, BIG_START+BIG_H/2, BIG_W, BIG_H);

        set_position(t_min,  0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);
        set_position(colon,  1.0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);
        set_position(t_tsec, 2.0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);
        set_position(t_sec,  3.0*BIG_W, BIG_START+2.0*BIG_H,   BIG_W, BIG_H);

        set_position(o_tsec, 4.5*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(o_sec,  5.5*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

        set_position(points, 6.5*BIG_W, BIG_START+2*BIG_H, 1.5*BIG_W, BIG_H);
        set_position(pts_to_blue,  4.0*BIG_W, BIG_START+2*BIG_H, BIG_W/2, BIG_H/2.0);
        set_position(pts_to_white, 4.0*BIG_W, BIG_START+2.5*BIG_H, BIG_W/2, BIG_H/2.0);
        break;

    case 5:
        clocks_only = TRUE;
        labels[wazaari].w = 0;
        labels[yuko].w = 0;
        labels[koka].w = 0;
        labels[shido].w = 0;
        labels[padding].w = 0;
        labels[bw].w = 0;
        labels[by].w = 0;
        labels[bk].w = 0;
        labels[bs].w = 0;
        labels[ww].w = 0;
        labels[wy].w = 0;
        labels[wk].w = 0;
        labels[ws].w = 0;
        labels[pts_to_blue].w = 0;
        labels[pts_to_white].w = 0;

        set_position(match1, 0, 0, 0, 0);
        set_position(match2, 0, 0, 0, 0);
        set_position(blue_name_1,  0, 0, 0, 0);
        set_position(white_name_1, 0, 0, 0, 0);
        set_position(blue_name_2,  0, 0, 0, 0);
        set_position(white_name_2, 0, 0, 0, 0);
        set_position(cat1,         0, 0, 0, 0);
        set_position(cat2,         0, 0, 0, 0);
        set_position(t_min,         0, BIG_START,        BIG_W2, BIG_H2);
        set_position(colon,      BIG_W2, BIG_START,        BIG_W2, BIG_H2);
        set_position(t_tsec, 2*BIG_W2, BIG_START,        BIG_W2, BIG_H2);
        set_position(t_sec,  3*BIG_W2, BIG_START,        BIG_W2, BIG_H2);
        set_position(o_tsec,        0, BIG_START+BIG_H2, BIG_W2, BIG_H2);
        set_position(o_sec,      BIG_W2, BIG_START+BIG_H2, BIG_W2, BIG_H2);
        set_position(points, 3*BIG_W2, BIG_START+BIG_H2, BIG_W2, BIG_H2);
        break;

    case 6:
        set_position(match1, 0, 0, 0, SMALL_H);
        set_position(match2, 500, 0, TXTW, SMALL_H);

        set_position(blue_name_1,  10,     BIG_START+BIG_H/10, 490, 3*BIG_H/10);
        set_position(white_name_1, 10,     BIG_START+25*BIG_H/10, 490, 3*BIG_H/10);
        set_position(blue_name_2,  500+TXTW, 0,     500-TXTW, SMALL_H);
        set_position(white_name_2, 500+TXTW, SMALL_H, 500-TXTW, SMALL_H);
        set_position(cat1,         550, 4*SMALL_H, 325, 6*SMALL_H);
        set_position(cat2,         500, SMALL_H, TXTW, SMALL_H);

        set_position(comment,  0, 3*SMALL_H, 1000, SMALL_H);

        set_position(wazaari,  0,       BIG_START+1.3*BIG_H, BIG_W, 3*BIG_H/10);
        set_position(yuko,     BIG_W, BIG_START+1.3*BIG_H, BIG_W, 3*BIG_H/10);
        set_position(koka,     2*BIG_W, BIG_START+13*BIG_H/10, BIG_W, 3*BIG_H/10);
        set_position(shido,    3*BIG_W, BIG_START+13*BIG_H/10, BIG_W, 3*BIG_H/10);
        set_position(padding,  4*BIG_W, BIG_START, 0, BIG_H);
        set_position(sonomama, 7*BIG_W, BIG_START, BIG_W, BIG_H);

        set_position(bw, 0,       BIG_START+4*BIG_H/10, BIG_W, 9*BIG_H/10);
        set_position(by, BIG_W, BIG_START+4*BIG_H/10, BIG_W, 9*BIG_H/10);
        set_position(bk, 2*BIG_W, BIG_START+4*BIG_H/10, BIG_W, 9*BIG_H/10);
        set_position(bs, 3*BIG_W, BIG_START+4*BIG_H/10, BIG_W, 9*BIG_H/10);
        set_position(ww, 0,       BIG_START+16*BIG_H/10, BIG_W, 9*BIG_H/10);
        set_position(wy, BIG_W, BIG_START+16*BIG_H/10, BIG_W, 9*BIG_H/10);
        set_position(wk, 2*BIG_W, BIG_START+16*BIG_H/10, BIG_W, 9*BIG_H/10);
        set_position(ws, 3*BIG_W, BIG_START+16*BIG_H/10, BIG_W, 9*BIG_H/10);

        set_position(t_min,  4*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(colon,  5*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(t_tsec, 6*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);
        set_position(t_sec,  7*BIG_W, BIG_START+BIG_H,   BIG_W, BIG_H);

        set_position(o_tsec, 4*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);
        set_position(o_sec,  5*BIG_W, BIG_START+2*BIG_H, BIG_W, BIG_H);

        set_position(points, 65*BIG_W/10, BIG_START+2*BIG_H, 15*BIG_W/10, BIG_H);
        set_position(pts_to_blue,  6*BIG_W, BIG_START+2*BIG_H, BIG_W/2, BIG_H/2);
        set_position(pts_to_white, 6*BIG_W, BIG_START+25*BIG_H/10, BIG_W/2, BIG_H/2);
        break;

    case 7:
        f = fopen("timer-custom.txt", "r");
        if (f) {
            struct label lbl;
            gint num;
            gchar line[128];
            while (fgets(line, sizeof(line), f)) {
                if (line[0] == '#' || strlen(line) < 4)
                    continue;
                gchar *p = line;
                GI(num);
                if (num >= 0 && num < num_labels) {
		    GF(lbl.x); GF(lbl.y); GF(lbl.w); GF(lbl.h);
		    GF(lbl.size); GI(lbl.xalign);
		    GF2(lbl.fg_r); GF2(lbl.fg_g); GF2(lbl.fg_b);
		    GF2(lbl.bg_r); GF2(lbl.bg_g); GF2(lbl.bg_b); GI(lbl.wrap);
		    labels[num].x = lbl.x;
		    labels[num].y = lbl.y;
		    labels[num].w = lbl.w;
		    labels[num].h = lbl.h;
		    labels[num].size = lbl.size;
		    labels[num].xalign = lbl.xalign;
		    labels[num].fg_r = lbl.fg_r;
		    labels[num].fg_g = lbl.fg_g;
		    labels[num].fg_b = lbl.fg_b;
		    labels[num].bg_r = lbl.bg_r;
		    labels[num].bg_g = lbl.bg_g;
		    labels[num].bg_b = lbl.bg_b;
		    labels[num].wrap = lbl.wrap;
		    if (num == pts_to_blue) {
			bgcolor_pts.r = lbl.bg_r;
			bgcolor_pts.g = lbl.bg_g;
			bgcolor_pts.b = lbl.bg_b;
		    } else if (num == points) {
			bgcolor_points.r = lbl.bg_r;
			bgcolor_points.g = lbl.bg_g;
			bgcolor_points.b = lbl.bg_b;
		    }
		} else if (num == 100) {
		    /* backgroud color */
		    GF2(background_r); GF2(background_g); GF2(background_b);
		} else if (num == 101) {
		    /* clock colors */
		    GC(clock_run); GC(clock_stop); GC(clock_bg);
		    set_fg_color(t_min, GTK_STATE_NORMAL, &clock_stop);
		    set_fg_color(colon, GTK_STATE_NORMAL, &clock_stop);
		    set_fg_color(t_tsec, GTK_STATE_NORMAL, &clock_stop);
		    set_fg_color(t_sec, GTK_STATE_NORMAL, &clock_stop);
		    set_bg_color(t_min, GTK_STATE_NORMAL, &clock_bg);
		    set_bg_color(colon, GTK_STATE_NORMAL, &clock_bg);
		    set_bg_color(t_tsec, GTK_STATE_NORMAL, &clock_bg);
		    set_bg_color(t_sec, GTK_STATE_NORMAL, &clock_bg);
		} else if (num == 102) {
		    /* osaekomi clock colors */
		    GC(oclock_run); GC(oclock_stop); GC(oclock_bg);
		    set_fg_color(o_tsec, GTK_STATE_NORMAL, &oclock_stop);
		    set_fg_color(o_sec, GTK_STATE_NORMAL, &oclock_stop);
		    set_bg_color(o_tsec, GTK_STATE_NORMAL, &oclock_bg);
		    set_bg_color(o_sec, GTK_STATE_NORMAL, &oclock_bg);
		} else if (num == 103) {
		    /* List of miscellaneous settings */
		    gint no_frames, slave;
		    /* hide clock if osakomi clock runs */
		    GI(hide_clock_if_osaekomi);
		    /* Hide frames and menu */
		    GI(no_frames);
		    /* Do not show osaekomi points if it is zero */
		    GI(hide_zero_osaekomi_points);
		    if (no_frames) {
			gtk_widget_hide(menubar);
			gtk_window_set_decorated((GtkWindow*)main_window, FALSE);
			gtk_window_set_keep_above(GTK_WINDOW(main_window), TRUE);
			menu_hidden = TRUE;
		    }
		    /* slave mode */
		    GI(slave);
		} else if (num == 104) {
		    /* Window size and position */
		    gint x, y, w, h;
		    GI(x); GI(y); GI(w); GI(h);
		    req_width = w; req_height = h;
		    gtk_window_resize(GTK_WINDOW(main_window), w, h);
		    //gtk_window_move(GTK_WINDOW(main_window), x, y);
		} else if (num == 105) {
		    /* Background image */
		    gchar *fname = NULL;
		    while (*p && *p <= ' ') p++;
		    gchar *p1 = strchr(p, '\r');
		    if (p1) *p1 = 0;
		    p1 = strchr(p, '\n');
		    if (p1) *p1 = 0;

		    if (strncmp(p, "../etc/", 7) == 0)
			p += 7;

		    emscripten_async_wget(p, "custombg.png", custombgonload, menupiconerror);
		} else
		    g_print("Read error in file %s, num = %d\n", custom_layout_file, num);
	    }
	    fclose(f);
	} else {

#define H1 250
#define H2 250
#define H3 (1000 - H1 - H2)
#define H_NAME (6*H1/10)
#define H_CLUB (H1 - H_NAME)
#define H_TIME 250
#define H_IWYS 100
#define H_SCORE (H1 - H_IWYS/2)
#define H_COMMENT (2*(H3 - H_TIME)/10)
#define W1 500
#define W_TIME 90
#define W_COLON (6*W_TIME/10)
#define W_SEL (7*W_TIME/10)
#define W_CAT 280
#define W_SCORE 70
#define W_NAME (1000 - 4*W_SCORE/10)
#define Y_IWYS (H1 - H_IWYS/2)
#define Y_SCORE (H1 + H_IWYS/2)
#define X_TSEC (W_CAT + W_TIME + W_SCORE)

	    //labels[sonomama].size = 0;
	    labels[gs].size = 400;
	    labels[gs].xalign = 0;
	    labels[cat1].wrap = TRUE;
	    labels[cat1].size = 350;
	    set_bg_color(blue_name_1, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(blue_club, GTK_STATE_NORMAL, bgcolor);
	    set_bg_color(white_name_1, GTK_STATE_NORMAL, &color_white);
	    set_bg_color(white_club, GTK_STATE_NORMAL, &color_white);
	    set_fg_color(white_name_1, GTK_STATE_NORMAL, &color_black);
	    set_fg_color(white_club, GTK_STATE_NORMAL, &color_black);

	    set_fg_color(wazaari, GTK_STATE_NORMAL, &color_black);
	    set_bg_color(wazaari, GTK_STATE_NORMAL, &color_yellow);
	    set_fg_color(yuko, GTK_STATE_NORMAL, &color_black);
	    set_bg_color(yuko, GTK_STATE_NORMAL, &color_yellow);
	    set_fg_color(koka, GTK_STATE_NORMAL, &color_black);
	    set_bg_color(koka, GTK_STATE_NORMAL, &color_yellow);
	    set_fg_color(shido, GTK_STATE_NORMAL, &color_black);
	    set_bg_color(shido, GTK_STATE_NORMAL, &color_yellow);

	    set_fg_color(cat1, GTK_STATE_NORMAL, &color_black);
	    set_bg_color(cat1, GTK_STATE_NORMAL, &color_yellow);

	    NO_SHOW(match1);
	    NO_SHOW(match2);

	    set_position(blue_name_1,  0, 0,  W_NAME, H_NAME);
	    set_position(white_name_1, 0, H1,   W_NAME, H_NAME);
	    set_position(blue_club,  0, H_NAME,      W_NAME, H_CLUB);
	    set_position(white_club, 0, H1 + H_NAME, W_NAME, H_CLUB);

	    NO_SHOW(blue_name_2);
	    NO_SHOW(white_name_2);
	    set_position(cat1, 0, H1+H2, W_CAT, H3);
	    NO_SHOW(cat2);

	    set_position(comment, W_CAT, H1 + H2, 1 - W_CAT - W_TIME, H_COMMENT);
	    set_position(gs, W_CAT, H1 + H2 + H_COMMENT, 1 - W_CAT - W_TIME, H3 - H_TIME - H_COMMENT);
	    set_position(sonomama, 1 - W_TIME, H1 + H2, W_TIME, H3 - H_TIME);

	    set_position(wazaari, 1 - 4*W_SCORE, Y_IWYS, W_SCORE, H_IWYS);
	    set_position(yuko,    1 - 3*W_SCORE, Y_IWYS, W_SCORE, H_IWYS);
	    set_position(koka,    1 - 2*W_SCORE, Y_IWYS, W_SCORE, H_IWYS);
	    set_position(shido,   1 - W_SCORE,   Y_IWYS, W_SCORE, H_IWYS);

	    NO_SHOW(padding);

	    set_position(bw, 1 - 4*W_SCORE, 0, W_SCORE, H_SCORE);
	    set_position(by, 1 - 3*W_SCORE, 0, W_SCORE, H_SCORE);
	    set_position(bk, 1 - 2*W_SCORE, 0, W_SCORE, H_SCORE);
	    set_position(bs, 1 - W_SCORE,   0, W_SCORE, H_SCORE);

	    set_position(ww, 1 - 4*W_SCORE, Y_SCORE, W_SCORE, H_SCORE);
	    set_position(wy, 1 - 3*W_SCORE, Y_SCORE, W_SCORE, H_SCORE);
	    set_position(wk, 1 - 2*W_SCORE, Y_SCORE, W_SCORE, H_SCORE);
	    set_position(ws, 1 - W_SCORE,   Y_SCORE, W_SCORE, H_SCORE);

	    set_position(t_min,  W_CAT,   1.0 - H_TIME, W_TIME, H_TIME);
	    set_position(colon,  W_CAT + W_TIME, 1.0 - H_TIME, W_COLON, H_TIME);
	    set_position(t_tsec, X_TSEC, 1.0 - H_TIME, W_TIME, H_TIME);
	    set_position(t_sec,  X_TSEC + W_TIME, 1.0 - H_TIME, W_TIME, H_TIME);

	    set_position(o_tsec, 1 - W_SEL - 3*W_TIME, 1.0 - H_TIME, W_TIME, H_TIME);
	    set_position(o_sec,  1 - W_SEL - 2*W_TIME, 1.0 - H_TIME, W_TIME, H_TIME);

	    set_position(pts_to_blue,  1 - W_SEL - W_TIME, 1.0 - H_TIME,     W_SEL, 0.5*H_TIME);
	    set_position(pts_to_white, 1 - W_SEL - W_TIME, 1.0 - 0.5*H_TIME, W_SEL, 0.5*H_TIME);

	    set_position(points,  1 - W_TIME, 1.0 - H_TIME, W_TIME, H_TIME);
	}

	break;
    }

    set_colors();
    init_display();

    g_key_file_set_integer(keyfile, "preferences", "displaylayout", ptr_to_gint(data));
}

void change_language_1(void)
{
    set_text(MY_LABEL(match1), _("Fight:"));
    set_text(MY_LABEL(match2), _("Next:"));
    init_display();
}

gboolean this_is_shiai(void)
{
	return FALSE;
}

gint application_type(void)
{
        return APPLICATION_TYPE_TIMER;
}

gboolean blue_background(void)
{
    return bgcolor == &color_blue;
}

int EMSCRIPTEN_KEEPALIVE setscreensize(int yes)
{
    int w, h, f;

    emscripten_get_canvas_size(&w, &h, &f);
    if (1 || f == 0) {
	if (dsp_layout == 7 && req_width && req_height) {
	    w = req_width;
	    h = req_height;
	} else {
	    w = emscripten_run_script_int("window.innerWidth");
	    h = emscripten_run_script_int("window.innerHeight") - (f ? 0 : 30);
	}
	emscripten_set_canvas_size(w, h);
    }
    expose();
    return yes;
}

int setclocks_c(int yes)
{
    //printf("SUBMIT=%s\n", emscripten_run_script_string("winx.document.getElementById('mins').value"));

    return yes;
}

void delete_new_match(void)
{
    labels[ask_box].hide = 1;
    labels[yes_box].hide = 1;
    labels[no_box].hide = 1;
    expose();
}

void ask_new_match(int winner, const char *cat,
		   const char *last, const char *first)
{
    char buf[64];

    labels[ask_box].hide = 0;
    labels[yes_box].hide = 0;
    labels[no_box].hide = 0;

    if (winner) {
	snprintf(buf, sizeof(buf), "%s   WINNER:", cat);
	set_text(upper_box, buf);
	set_fg_color(upper_box, 0, &color_white);
	set_bg_color(upper_box, 0, &color_black);

	if (winner == BLUE) {
	    set_fg_color(middle_box, 0, &color_black);
	    set_bg_color(middle_box, 0, &color_white);
	    set_fg_color(lower_box, 0, &color_black);
	    set_bg_color(lower_box, 0, &color_white);
	} else {
	    set_fg_color(middle_box, 0, &color_white);
	    set_bg_color(middle_box, 0, &color_blue);
	    set_fg_color(lower_box, 0, &color_white);
	    set_bg_color(lower_box, 0, &color_blue);
	}
	set_text(middle_box, last);
	set_text(lower_box, first);

	labels[upper_box].hide = 0;
	labels[middle_box].hide = 0;
	labels[lower_box].hide = 0;
    }

    expose();
}

static gchar category[32];
static gchar b_last[32];
static gchar w_last[32];
static gchar b_country[8], w_country[8];

void g_utf8_strncpy(char *dst, char *src, int len)
{
    char c;

    while (len > 0 && *src) {
	c = *dst++ = *src++;
	if (c & 0x80) // utf-8
	    *dst++ = *src++;
	len--;
    }
    *dst = 0;
}

void display_comp_window(gchar *cat, gchar *comp1, gchar *comp2,
                         gchar *first1, gchar *first2,
                         gchar *country1, gchar *country2)
{
    if (!show_competitor_names)
        return;

    gchar *p;

    strncpy(category, cat, sizeof(category)-1);

    if (showletter) {
        gchar buf[8];
        if (first1[0]) {
            g_utf8_strncpy(buf, first1, 1);
            snprintf(b_last, sizeof(b_last), "%s.%s", buf, comp1);
        } else
            strncpy(b_last, comp1, sizeof(b_last)-1);

        if (first2[0]) {
            g_utf8_strncpy(buf, first2, 1);
            snprintf(w_last, sizeof(w_last), "%s.%s", buf, comp2);
        } else
            strncpy(w_last, comp1, sizeof(w_last)-1);
    } else {
        strncpy(b_last, comp1, sizeof(b_last)-1);
        strncpy(w_last, comp2, sizeof(w_last)-1);
    }

    strncpy(b_country, country1, sizeof(b_country)-1);
    strncpy(w_country, country2, sizeof(w_country)-1);

    p = strchr(b_last, '\t');
    if (p) *p = 0;

    p = strchr(w_last, '\t');
    if (p) *p = 0;

    set_text(upper_box, category);
    set_fg_color(upper_box, 0, &color_white);
    set_bg_color(upper_box, 0, &color_black);
    labels[upper_box].xalign = -1;

    set_text(middle_box, b_last);
    set_fg_color(middle_box, 0, &color_black);
    set_bg_color(middle_box, 0, &color_white);
    labels[middle_box].xalign = -1;

    set_text(lower_box, w_last);
    set_fg_color(lower_box, 0, &color_white);
    set_bg_color(lower_box, 0, bgcolor);
    labels[lower_box].xalign = -1;

    labels[t_min].hide = 1;
    labels[t_tsec].hide = 1;
    labels[t_sec].hide = 1;

    labels[upper_box].hide = 0;
    labels[middle_box].hide = 0;
    labels[lower_box].hide = 0;
    labels[ok_box].hide = 0;

    if (rest_time) {
	labels[rest_time_box].hide = 0;
	labels[rest_ind_box].hide = 0;
    }

    expose();
#if 0
    comp_names_start = 0;
    comp_names_pending = TRUE;
    if (ad_window == NULL)
        no_ads = TRUE;
    display_ad_window();
#endif
}

void delete_comp_window(void)
{
    labels[ask_box].hide = 1;
    labels[yes_box].hide = 1;
    labels[no_box].hide = 1;
    labels[upper_box].hide = 1;
    labels[middle_box].hide = 1;
    labels[lower_box].hide = 1;
    labels[ok_box].hide = 1;
    labels[rest_time_box].hide = 1;
    labels[rest_ind_box].hide = 1;

    labels[t_min].hide = 0;
    labels[t_tsec].hide = 0;
    labels[t_sec].hide = 0;

    expose();
}

void textbox(int x1, int y1, int w1, int h1, const char *txt)
{
    SDL_Rect rect;
    SDL_Color color;
    SDL_Rect dest;
    SDL_Surface *text;
    int x, y, w, h;
    int fontsize;

    x = x1*menubg->w/1000;
    y = (y1 - h1)*menubg->h/1000;
    w = w1*menubg->w/1000;
    h = h1*menubg->h/1000;
    fontsize = 8*h/10;

    color.r = 0; color.g = 0; color.b = 0;
    text = TTF_RenderText_Solid(get_font(fontsize, 0), txt, color);
    rect.x = x; rect.y = y; rect.w = w; rect.h = h;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 255, 255, 255));
    dest.x = x + (w - text->w)/2;
    dest.y = y + (h - text->h)/2;
    dest.w = dest.h = 0;
    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);
}

void checkbox(int x1, int y1, int w1, int h1, int yes)
{
    SDL_Rect rect;
    SDL_Color color;
    SDL_Rect dest;
    int x, y, w, h;
    //int size;

    x = x1*menubg->w/1000;
    y = (y1 - h1)*menubg->h/1000;
    w = w1*menubg->w/1000;
    h = h1*menubg->h/1000;
#if 0
    size = 8*h/10;
    if (w == 0) {
	y += (h - size)/2;
	w = size;
	h = size;
    }
#endif
    rect.x = x; rect.y = y; rect.w = h; rect.h = h;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 0, 0, 0));
    rect.x++; rect.y++; rect.w -= 2; rect.h -= 2;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 255, 255, 255));

    if (yes) {
	rect.x += 3; rect.y += 3; rect.w -= 6; rect.h -= 6;
	SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 0, 0, 0));
    }
}
