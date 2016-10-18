/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

/* Include system network headers */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef __BEOS__
#include <arpa/inet.h>
#endif

#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <time.h>
#include <string.h>

#include "judotimer.h"

void manipulate_time(GtkWidget *widget, gpointer data );
static void gen_random_key(void);

void check_ippon(void);
void give_osaekomi_score();
gint approve_osaekomi_score(gint who);
//static void ask_for_hantei(void);
gboolean ask_for_golden_score(void);

enum button_responses {
    Q_GS_NO_LIMIT = 1000,
    Q_GS_1_00,
    Q_GS_1_30,
    Q_GS_2_00,
    Q_GS_2_30,
    Q_GS_3_00,
    Q_GS_4_00,
    Q_GS_5_00,
    Q_GS_AUTO,
    Q_HANTEI_BLUE,
    Q_HANTEI_WHITE
};

struct stackval {
    gchar who;
    gchar points;
};

#define STACK_DEPTH 8

static struct _st {
    gint      osaekomi_winner;
    gboolean  action_on;
    gboolean  running;
    gboolean  oRunning;
    gdouble   startTime;
    gdouble   oStartTime;
    gdouble   elap;
    gdouble   oElap;
    gint      score;
    gint      bluepts[4], whitepts[4];
    gboolean  big_displayed;
    gdouble   match_time;
    struct stackval stackval[STACK_DEPTH];
    gint      stackdepth;
} st;
static gint stack_depth = 0;

static guint     last_m_time;
static gboolean  last_m_run;
static guint     last_m_otime;
static gboolean  last_m_orun;
static guint     last_score;
static gchar     osaekomi = ' ';
static gdouble   koka = 10.0;
static gdouble   yuko = 10.0;
static gdouble   wazaari = 15.0;
static gdouble   ippon = 20.0;
static gdouble   total;
gdouble          gs_time = 0.0;
gint             tatami;
gint             demo = 0;
gboolean         automatic;
gboolean         short_pin_times = FALSE;
gboolean         golden_score = FALSE;
gboolean         rest_time = FALSE;
gint             rest_flags = 0;
gchar           *matchlist = NULL;
static gint      gs_cat = 0, gs_num = 0;
gboolean         require_judogi_ok = FALSE;
static gboolean  jcontrol;
static gint      jcontrol_flags;

int get_elap(void)
{
    int t;

    if (st.elap == 0.0) t = total;
    else if (total == 0.0) t = st.elap;
    else {
        t = total - st.elap + 0.9;
        if (t == 0 && total > st.elap)
            t = 1;
    }
    return t;
}

int get_oelap(void)
{
    return st.oElap;
}

static void log_scores(gchar *txt, gint who)
{
    judotimer_log("%s%s: IWYKS = %d%d%d%d%d - %d%d%d%d%d", txt,
                  ((who == BLUE && white_first) || (who == WHITE && white_first == FALSE)) ? "white" : "blue",
                  st.bluepts[0]&2 ? 1 : 0,
                  st.bluepts[0]&1,
                  st.bluepts[1],
                  st.bluepts[2],
                  st.bluepts[3],
                  st.whitepts[0]&2 ? 1 : 0,
                  st.whitepts[0]&1,
                  st.whitepts[1],
                  st.whitepts[2],
                  st.whitepts[3]);
}

void beep(char *str)
{
    if (st.big_displayed)
        return;

    display_big(str, 4);
    st.big_displayed = TRUE;

    emscripten_run_script("audio.play()");
    //play_sound();
}

void update_display(void)
{
    guint t, min, sec, oSec;
    gint orun, score;
    static gint flash = 0;

    // move last second to first second i.e. 0:00 = soremade
    if (st.elap == 0.0) t = total;
    else if (total == 0.0) t = st.elap;
    else {
        t = total - st.elap + 0.9;
        if (t == 0 && total > st.elap)
            t = 1;
    }

    min = t / 60;
    sec =  t - min*60;
    oSec = st.oElap;

    if (t != last_m_time) {
        last_m_time = t;
        set_timer_value(min, sec/10, sec%10);
        //set_competitor_window_rest_time(min, sec/10, sec%10, rest_time, rest_flags);
    }

    if (oSec != last_m_otime) {
        last_m_otime = oSec;
        set_osaekomi_value(oSec/10, oSec%10);
    }

    if (st.running != last_m_run) {
        last_m_run = st.running;
        set_timer_run_color(last_m_run, rest_time);
    }

    if (st.oRunning) {
        orun = st.oRunning ? OSAEKOMI_DSP_YES : OSAEKOMI_DSP_NO;
        score = st.score;
    } else if (st.stackdepth) {
        if (st.stackval[st.stackdepth-1].who == BLUE)
            orun = OSAEKOMI_DSP_BLUE;
        else if (st.stackval[st.stackdepth-1].who == WHITE)
            orun = OSAEKOMI_DSP_WHITE;
        else
            orun = OSAEKOMI_DSP_UNKNOWN;

        if (++flash > 5)
            orun = OSAEKOMI_DSP_NO;
        if (flash > 10)
            flash = 0;

        score = st.stackval[st.stackdepth-1].points;
    } else {
        orun = OSAEKOMI_DSP_NO;
        score = 0;
    }

    if (orun != last_m_orun) {
        last_m_orun = orun;
        set_timer_osaekomi_color(last_m_orun, score);
    }

    if (last_score != score) {
        last_score = score;
        set_score(last_score);
    }
}

void update_clock(void)
{
    gboolean show_soremade = FALSE;

    if (st.running) {
	st.elap = g_timer_elapsed(timer, NULL) - st.startTime;
	if (total > 0.0 && st.elap >= total) {
	    //running = false;
	    st.elap = total;
	    if (!st.oRunning) {
		st.running = FALSE;
		if (rest_time == FALSE)
		    show_soremade = TRUE;
		if (rest_time) {
		    st.elap = 0;
		    st.oRunning = FALSE;
		    st.oElap = 0;
		    rest_time = FALSE;
		}
	    }
	}
    }

    if (st.running && st.oRunning) {
	st.oElap =  g_timer_elapsed(timer, NULL) - st.oStartTime;
	st.score = 0;
	if (st.oElap >= yuko)
	    st.score = 2;
	if (st.oElap >= wazaari) {
	    st.score = 3;
	    if ((st.osaekomi_winner == BLUE && st.bluepts[0]) ||
		(st.osaekomi_winner == WHITE && st.whitepts[0])) {
		st.running = FALSE;
		st.oRunning = FALSE;
		give_osaekomi_score();
		approve_osaekomi_score(0);
	    }
	}
	if (st.oElap >= ippon) {
	    st.score = 4;
	    st.running = FALSE;
	    st.oRunning = FALSE;
	    gint tmp = st.osaekomi_winner;
	    give_osaekomi_score();
	    if (tmp)
		approve_osaekomi_score(0);
	}
    }

    update_display();

    if (show_soremade) {
        beep("SOREMADE");
#if 0
        if (golden_score &&
            (array2int(st.bluepts) & 0xfffffff0) == (array2int(st.whitepts) & 0xfffffff0))
            ask_for_hantei();
#endif
    }

    // judogi control display for ad
    {
        static gint cnt = 0;
        if (++cnt >= 10) {
            //set_competitor_window_judogi_control(jcontrol, jcontrol_flags);
            cnt = 0;
        }
    }

    if (demo) {
        if (demo == 1)
            gen_random_key();
        else {
            static gint last_cat = 0, last_num = 0;
            static gint res[9][2][4] = {
                {{2,1,0,0},{0,2,0,2}},
                {{0,0,3,0},{1,0,0,0}},
                {{0,1,0,0},{0,2,0,0}},
                {{3,0,0,0},{0,1,0,0}},
                {{0,0,0,2},{1,1,0,0}},
                {{2,2,0,0},{0,0,0,1}},
                {{0,4,0,0},{0,1,0,0}},
                {{3,0,0,0},{1,2,0,0}},

                {{0,1,0,1},{0,1,0,1}},
            };

            if (last_cat != current_category || last_num != current_match) {
                if (matchlist) {
                    static gint next_res = 0;
                    gint len = strlen(matchlist);
                    switch (matchlist[next_res]) {
                    case '0': send_result(res[8][0], res[8][1], 0, 0, 0, 0, 0, 1); break; // hikiwake
                    case '1': send_result(res[0][0], res[0][1], 0, 0, 0, 0, 0, 0); break; // 1st wins by ippon
                    case '2': send_result(res[0][1], res[0][0], 0, 0, 0, 0, 0, 0); break; // 2nd wins by ippon
                    case '3': send_result(res[2][1], res[2][0], 0, 0, 0, 0, 0, 0); break; // 1st wins by yuko
                    default: send_result(res[2][0], res[2][1], 0, 0, 0, 0, 0, 0);         // 2nd wins by yuko
                    }
                    if (++next_res >= len) next_res = 0;
                } else {
                    static gint next_res = 0;
                    gint ix = (next_res++ /*current_category + current_match*/) & 7;
                    send_result(res[ix][0], res[ix][1], 0, 0, 0, 0, (ix&3)==2 ? 0x100 + ix : ix, 0);
                }
                last_cat = current_category;
                last_num = current_match;
            }
        }
    }
}

void hajime_inc_func(void)
{
    if (st.running)
        return;
    if (total == 0) {
        st.elap += 1.0;
    } else if (st.elap >= 1.0)
        st.elap -= 1.0;
    else
        st.elap = 0.0;
    update_display();
}

void hajime_dec_func(void)
{
    if (st.running)
        return;
    if (total == 0) {
        if (st.elap >= 1.0) st.elap -= 1.0;
    } else if (st.elap <= (total - 1.0))
        st.elap += 1.0;
    else
        st.elap = total;
    update_display();
}

void osaekomi_inc_func(void)
{
    if (st.running)
        return;
    if (st.oElap <= (ippon - 1.0))
        st.oElap += 1.0;
    else
        st.oElap = ippon;

    st.oRunning = st.oElap != 0;
    update_display();
}

void osaekomi_dec_func(void)
{
    if (st.running)
        return;
    if (st.oElap >= 1.0)
        st.oElap -= 1.0;
    else
        st.oElap = 0;

    st.oRunning = st.oElap != 0;
    update_display();
}

void set_clocks(gint clock, gint osaekomi)
{
    if (st.running)
        return;

    if ( clock >= 0 && total >= clock)
    	st.elap = total - (gdouble)clock;
    else if (clock >= 0)
        st.elap = (gdouble)clock;

    if (osaekomi) {
        st.oElap = (gdouble)osaekomi;
        st.oRunning = TRUE;
    }

    update_display();
}


static void toggle(void)
{
    if (st.running /*&& !oRunning*/) {
        judotimer_log("Shiai clock stop");
        st.running = FALSE;
        st.elap = g_timer_elapsed(timer, NULL) - st.startTime;
        if (total > 0.0 && st.elap >= total)
            st.elap = total;
        if (st.oRunning) {
#if 1 // According to Staffan's wish
            st.oRunning = FALSE;
            st.oElap = 0;
            give_osaekomi_score();
#else
            st.oElap = g_timer_elapsed(timer, NULL) - st.oStartTime;
            set_comment_text("SONOMAMA!");
#endif
        } else {
            st.oElap = 0;
            st.score = 0;
        }
        update_display();
    } else if (!st.running) {
        if (total > 9000) // don't let clock run if dashes in display '-:--'
            return;

        if (st.elap == 0)
            judotimer_log("MATCH START: %s: %s - %s",
                          get_cat(), get_name(BLUE), get_name(WHITE));
        judotimer_log("Shiai clock start");
        st.running = TRUE;
        st.startTime = g_timer_elapsed(timer, NULL) - st.elap;
        if (st.oRunning) {
            st.oStartTime = g_timer_elapsed(timer, NULL) - st.oElap;
            set_comment_text("");
        }
        update_display();
    }
}

int get_match_time(void)
{
    if (demo >= 2)
        return (10 + rand()%200);

    return st.elap + st.match_time;
}

static void oToggle() {
    if (st.running == FALSE && (st.elap < total || total == 0.0)) {
        if (st.oRunning) // yoshi
            toggle();
        return;
    }

    if (st.oRunning) {
        judotimer_log("Osaekomi clock stop after %f s", st.oElap);
        st.oRunning = FALSE;
        //st.oElap = g_timer_elapsed(timer, NULL) - st.oStartTime;
        st.oElap = 0;
        set_comment_text("");
        update_display();
        if (total > 0.0 && st.elap >= total)
            beep("SOREMADE");
    } else /*if (st.running)*/ {
        judotimer_log("Osaekomi clock start");
        st.running = TRUE;
        osaekomi = ' ';
        st.oRunning = TRUE;
        st.oStartTime = g_timer_elapsed(timer, NULL);
        update_display();
    }
}

int clock_running(void)
{
    return st.action_on;
}

gboolean osaekomi_running(void)
{
    return st.oRunning;
}

void set_hantei_winner(gint cmd)
{
    if (cmd == CLEAR_SELECTION)
        st.big_displayed = FALSE;

    if (cmd == HANSOKUMAKE_BLUE)
        st.whitepts[0] |= 2;
    else if (cmd == HANSOKUMAKE_WHITE)
        st.bluepts[0] |= 2;

    check_ippon();
}

gboolean ask_for_golden_score(void)
{
#if 0
    GtkWidget *dialog;
    gint response;

    golden_score = FALSE;
    gs_cat = gs_num = 0;

    dialog = gtk_dialog_new_with_buttons (_("Start Golden Score?"),
                                          NULL,
                                          GTK_DIALOG_MODAL,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          "Auto",           Q_GS_AUTO,
                                          _("No Limit"),    Q_GS_NO_LIMIT, 
                                          "1:00 min",       Q_GS_1_00,
                                          "1:30 min",       Q_GS_1_30,
                                          "2:00 min",       Q_GS_2_00,
                                          "2:30 min",       Q_GS_2_30,
                                          "3:00 min",       Q_GS_3_00,
                                          "4:00 min",       Q_GS_4_00,
                                          "5:00 min",       Q_GS_5_00,
                                          NULL);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    switch (response) {
    case Q_GS_NO_LIMIT: gs_time = 0.0; break;
    case Q_GS_1_00: gs_time = 60.0; break;
    case Q_GS_1_30: gs_time = 90.0; break;
    case Q_GS_2_00: gs_time = 120.0; break;
    case Q_GS_2_30: gs_time = 150.0; break;
    case Q_GS_3_00: gs_time = 180.0; break;
    case Q_GS_4_00: gs_time = 240.0; break;
    case Q_GS_5_00: gs_time = 300.0; break;
    case Q_GS_AUTO: gs_time = 1000.0; break;
    }

    if (response >= Q_GS_NO_LIMIT && response <= Q_GS_AUTO) {
        golden_score = TRUE;
        gs_cat = current_category;
        gs_num = current_match;
        if (response == Q_GS_AUTO)
            automatic = TRUE;
        else
            automatic = FALSE;
    }

    return golden_score;
#endif
    return 0;
}

void set_gs(double gs)
{
    golden_score = TRUE;
    gs_time = gs;
    gs_cat = current_category;
    gs_num = current_match;
    if (gs >= 1000.0)
	automatic = TRUE;
    else
	automatic = FALSE;
}

static gboolean asking = FALSE;
static guint key_pending = 0;
static GtkWidget *ask_area;

#define ASK_OK  0x7301
#define ASK_NOK 0x7302

static gint get_winner(void)
{
    gint winner = 0;
    if (st.bluepts[0] > st.whitepts[0]) winner = BLUE;
    else if (st.bluepts[0] < st.whitepts[0]) winner = WHITE;
    else if (st.bluepts[1] > st.whitepts[1]) winner = BLUE;
    else if (st.bluepts[1] < st.whitepts[1]) winner = WHITE;
    else if (st.bluepts[2] > st.whitepts[2]) winner = BLUE;
    else if (st.bluepts[2] < st.whitepts[2]) winner = WHITE;
    else if (blue_wins_voting) winner = BLUE;
    else if (white_wins_voting) winner = WHITE;
    else if (hansokumake_to_white) winner = BLUE;
    else if (hansokumake_to_blue) winner = WHITE;
    else if (rule_eq_score_less_shido_wins &&
             (st.bluepts[3] || st.whitepts[3])) {
        if (st.bluepts[3] > st.whitepts[3]) winner = WHITE;
        else if (st.bluepts[3] < st.whitepts[3]) winner = BLUE;
    }

    return winner;
}

static GtkWindow *ask_window = NULL;
static GtkWidget *legend_widget;
static gint legend;

static gboolean delete_event_ask( GtkWidget *widget, GdkEvent  *event, gpointer   data )
{
    return FALSE;
}

static void destroy_ask( GtkWidget *widget, gpointer   data )
{
    legend_widget = NULL;
    ask_area = NULL;
    asking = FALSE;
    ask_window = NULL;
}

void close_ask_ok(void)
{
#if 0
    if (legend_widget)
        legend = gtk_combo_box_get_active(GTK_COMBO_BOX(legend_widget));
    else
        legend = 0;

    if (legend < 0) legend = 0;
#endif

    reset(ASK_OK, NULL);
}

void close_ask_nok(void)
{
    legend = 0;
    reset(ASK_NOK, NULL);
}

#define FIRST_BLOCK_C 250
#define FIRST_BLOCK_START  0
#define SECOND_BLOCK_START  (FIRST_BLOCK_C*height/1000)
#define THIRD_BLOCK_START ((1000+FIRST_BLOCK_C)*height/2)
#define FIRST_BLOCK_HEIGHT (FIRST_BLOCK_C*height/1000)
#define OTHER_BLOCK_HEIGHT ((height-FIRST_BLOCK_HEIGHT)/2)

static gboolean expose_ask(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
#if 0
    gint width, height, winner = get_winner();
    const gchar *last_wname, *first_wname, *wtext = _("WINNER");

    if (!winner)
        return FALSE;

    if (winner == BLUE) {
        last_wname = saved_last1;
        first_wname = saved_first1;
    } else {
        last_wname = saved_last2;
        first_wname = saved_first2;
    }

#if (GTKVER == 3)
    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);
#else
    width = widget->allocation.width;
    height = widget->allocation.height;
#endif

    cairo_text_extents_t extents;
#if (GTKVER == 3)
    cairo_t *c = (cairo_t *)event;
#else
    cairo_t *c = gdk_cairo_create(widget->window);
#endif

    cairo_set_source_rgb(c, 0.0, 0.0, 0.0);
    cairo_rectangle(c, 0.0, 0.0, width, FIRST_BLOCK_HEIGHT);
    cairo_fill(c);
        
    if ((winner == BLUE && white_first) || (winner == WHITE && white_first == FALSE))
        cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
    else {
        if (blue_background())
            cairo_set_source_rgb(c, 0, 0, 1.0);
        else
            cairo_set_source_rgb(c, 1.0, 0, 0);
    }

    cairo_rectangle(c, 0.0, SECOND_BLOCK_START, width, 2*OTHER_BLOCK_HEIGHT);
    cairo_fill(c);

    cairo_set_font_size(c, 0.6*FIRST_BLOCK_HEIGHT);
    cairo_set_source_rgb(c, 1.0, 1.0, 1.0);
    cairo_text_extents(c, wtext, &extents);
    cairo_move_to(c, width - 10.0 - extents.width, (FIRST_BLOCK_HEIGHT - extents.height)/2.0 - extents.y_bearing);
    cairo_show_text(c, wtext);

    cairo_text_extents(c, saved_cat, &extents);
    cairo_move_to(c, 10.0, (FIRST_BLOCK_HEIGHT - extents.height)/2.0 - extents.y_bearing);
    cairo_show_text(c, saved_cat);

    if ((winner == BLUE && white_first) || (winner == WHITE && white_first == FALSE))
        cairo_set_source_rgb(c, 0, 0, 0);
    else
        cairo_set_source_rgb(c, 1.0, 1.0, 1.0);

    cairo_set_font_size(c, 0.6*OTHER_BLOCK_HEIGHT);
    cairo_text_extents(c, last_wname, &extents);
    cairo_move_to(c, 10.0, SECOND_BLOCK_START + (OTHER_BLOCK_HEIGHT - extents.height)/2.0 - extents.y_bearing);
    cairo_show_text(c, last_wname);
    cairo_text_extents(c, first_wname, &extents);
    cairo_move_to(c, 10.0, THIRD_BLOCK_START + (OTHER_BLOCK_HEIGHT - extents.height)/2.0 - extents.y_bearing);
    cairo_show_text(c, first_wname);

#if (GTKVER != 3)
    cairo_show_page(c);
    cairo_destroy(c);
#endif
#endif
    return FALSE;
}

static gchar *legends[] = 
    {"?", "(T)", "(H)", "(C)", "(L)", "SG", "HM", "KG", "T", "H", "S", "/P\\", "FG", "TT", "TH", "/HM\\", NULL};

#define ASK_SPACE 4
static int ask_y, ask_q, ask_yes, ask_no;

void draw_box(int x, int y, int *w, int *h,
	      unsigned char fg_r, unsigned char fg_g, unsigned char fg_b,
	      unsigned char bg_r, unsigned char bg_g, unsigned char bg_b,
	      int fontsize, int fontbold, char *txt, int margin)
{
    SDL_Rect rect;
    SDL_Color color;
    SDL_Rect dest;
    SDL_Surface *text;

    color.r = fg_r; color.g = fg_g; color.b = fg_b;
    text = TTF_RenderText_Solid(get_font(fontsize, fontbold),
				txt, color);
    rect.x = x; rect.y = y;
    rect.w = text->w + 2*margin; rect.h = text->h + 2*margin;
    *w = rect.w; *h = rect.h;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, bg_r, bg_g, bg_b));
    dest.x = x + margin; dest.y = y + margin; dest.w = dest.h = 0;
    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);
}

static void create_ask_window(void)
{
    //int answer = emscripten_run_script_int("confirm('Start New Match?')");
    //if (answer) close_ask_ok();
    //else close_ask_nok();
#if 0
    int width;
    int height;
    int full;
    int w, h;
    Uint32 color;
    SDL_Surface *text;
    SDL_Rect rect;
    SDL_Rect dest;
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color black = {0, 0, 0, 255};
    SDL_Color blue = {0, 0, 255, 255};
    SDL_Color red = {255, 0, 0, 255};
    SDL_Color rgb;

    emscripten_get_canvas_size(&width, &height, &full);

    draw_box(0, 0, &w, &h,
	     0, 0, 0,
	     180, 180, 180,
	     12, 1, "Start New Match?", 3);
    ask_y = h;
    ask_q = w + ASK_SPACE;

    draw_box(ask_q, 0, &w, &h,
	     0, 0, 0,
	     180, 180, 180,
	     12, 0, "YES", 3);
    ask_yes = ask_q + w + ASK_SPACE;

    draw_box(ask_yes, 0, &w, &h,
	     0, 0, 0,
	     180, 180, 180,
	     12, 0, "NO", 3);
    ask_no = ask_yes + w + ASK_SPACE;
#endif
    gint winner = get_winner();
    const gchar *last_wname, *first_wname, *wtext = _("WINNER");

    if (winner == BLUE) {
        last_wname = saved_last1;
        first_wname = saved_first1;
    } else {
        last_wname = saved_last2;
        first_wname = saved_first2;
    }

    ask_new_match(winner, saved_cat, last_wname, first_wname);
#if 0
    rect.x = 0; rect.y = ask_y; rect.w = width; rect.h = FIRST_BLOCK_HEIGHT - ask_y;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 0, 0, 0));

    if (winner == BLUE)
	color = SDL_MapRGB(darea->format, 255, 255, 255);
    else {
        if (blue_background())
	    color = SDL_MapRGB(darea->format, 0, 0, 255);
        else
	    color = SDL_MapRGB(darea->format, 255, 0, 0);
    }
    rect.x = 0; rect.y = SECOND_BLOCK_START; rect.w = width; rect.h = 2*OTHER_BLOCK_HEIGHT;
    SDL_FillRect(darea, &rect, color);

    text = TTF_RenderText_Solid(get_font(6*FIRST_BLOCK_HEIGHT/10, 0),
				wtext, white);
    dest.x = width - text->w - 10;
    dest.y = (FIRST_BLOCK_HEIGHT - text->h)/2;
    dest.w = dest.h = 0;
    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);

    text = TTF_RenderText_Solid(get_font(6*FIRST_BLOCK_HEIGHT/10, 0),
				saved_cat, white);
    dest.x = 10;
    dest.y = (FIRST_BLOCK_HEIGHT - text->h)/2;
    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);

    if (winner == BLUE)
	rgb = black;
    else
	rgb = white;

    text = TTF_RenderText_Solid(get_font(6*FIRST_BLOCK_HEIGHT/10, 0),
				last_wname, rgb);
    dest.x = 10;
    dest.y = SECOND_BLOCK_START + (OTHER_BLOCK_HEIGHT - text->h)/2;
    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);

    text = TTF_RenderText_Solid(get_font(6*FIRST_BLOCK_HEIGHT/10, 0),
				first_wname, rgb);
    dest.x = 10;
    dest.y = THIRD_BLOCK_START + (OTHER_BLOCK_HEIGHT - text->h)/2;
    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);
#endif
}

void close_ask_window(void)
{
    close_ask_ok();
}

void display_ask_window(gchar *name, gchar *cat, gchar winner)
{
    gchar first[32], last[32], club[32], country[32];

    parse_name(name, first, last, club, country);

    memset(st.bluepts, 0, sizeof(st.bluepts));
    memset(st.whitepts, 0, sizeof(st.whitepts));

    if (winner == BLUE) {
        st.bluepts[0] = 1;
        st.whitepts[0] = 0;
        strncpy(saved_last1, last, sizeof(saved_last1)-1);
        strncpy(saved_first1, first, sizeof(saved_first1)-1);
    } else if (winner == WHITE) {
        st.bluepts[0] = 0;
        st.whitepts[0] = 1;
        strncpy(saved_last2, last, sizeof(saved_last2)-1);
        strncpy(saved_first2, first, sizeof(saved_first2)-1);
    }

    strncpy(saved_cat, cat, sizeof(saved_cat)-1);
    create_ask_window();
}

void reset(guint key, struct msg_next_match *msg)
{
    gint i;
    gboolean asked = FALSE;

    if (key == ASK_OK) {
        key = key_pending;
        key_pending = 0;
        asking = FALSE;
        asked = TRUE;
    } else if (key == ASK_NOK) {
        key_pending = 0;
        asking = FALSE;
        return;
    }

    if ((st.running && rest_time == FALSE) || st.oRunning || asking)
        return;

    set_gs_text("");

    if (msg && (current_category != msg->category || current_match != msg->match))
        judotimer_log("Automatic next match %d:%d (%s - %s)",
                      msg->category, msg->match,
                      msg->blue_1, msg->white_1);

    if (key != GDK_0) {
        golden_score = FALSE;
    }

    gint bp;
    gint wp;
    if (rule_eq_score_less_shido_wins) {
        bp = array2int(st.bluepts);
        wp = array2int(st.whitepts);
    } else {
        bp = array2int(st.bluepts) & 0xfffffff0;
        wp = array2int(st.whitepts) & 0xfffffff0;
    }

    if (key == GDK_9 ||
        (demo == 0 && bp == wp && result_hikiwake == FALSE &&
         blue_wins_voting == white_wins_voting &&
         total > 0.0 && st.elap >= total && key != GDK_0)) {
#if 0
        asking = TRUE;
        ask_for_golden_score();
        asking = FALSE;
#endif
	golden_score = TRUE;
        if (key == GDK_9 && golden_score == FALSE)
            return;

        key = GDK_9;
        st.match_time = st.elap;
	if (gs_time >= 1000.0)
	    gs_time = (double)current_gs_time;
        judotimer_log("Golden score starts");
        set_gs_text("GOLDEN SCORE");
    } else if (demo == 0 &&
               asked == FALSE &&
               (((st.bluepts[0] & 2) == 0 && (st.whitepts[0] & 2) == 0 &&
                 st.elap > 0.01 && st.elap < total - 0.01) ||
                (st.running && rest_time) ||
                rules_confirm_match) &&
               key != GDK_0) {
        key_pending = key;
        asking = TRUE;
        create_ask_window();
        return;
    }

    if (key != GDK_0 && rest_time) {
        if (rest_flags & MATCH_FLAG_BLUE_REST)
            cancel_rest_time(TRUE, FALSE);
        else if (rest_flags & MATCH_FLAG_WHITE_REST)
            cancel_rest_time(FALSE, TRUE);
    }

    rest_time = FALSE;

    if (key != GDK_0 && golden_score == FALSE) {
        // set bit if golden score
        if (gs_cat == current_category && gs_num == current_match)
            legend |= 0x100;

	send_result(st.bluepts, st.whitepts,
		    blue_wins_voting, white_wins_voting,
		    hansokumake_to_blue, hansokumake_to_white, legend, result_hikiwake);
        st.match_time = 0;
        gs_cat = gs_num = 0;
    }

    if (golden_score == FALSE || rules_leave_score == FALSE) {
        memset(st.bluepts, 0, sizeof(st.bluepts));
        memset(st.whitepts, 0, sizeof(st.whitepts));
    }

    blue_wins_voting = 0;
    white_wins_voting = 0;
    hansokumake_to_blue = 0;
    hansokumake_to_white = 0;
    if (key != GDK_0) result_hikiwake = 0;
    st.osaekomi_winner = 0;
    st.big_displayed = FALSE;

        st.running = FALSE;
        st.elap = 0;
        st.oRunning = FALSE;
        st.oElap = 0;

        memset(&(st.stackval), 0, sizeof(st.stackval));
        st.stackdepth = 0;

    if (rule_short_pin_times) {
        koka    = 5.000;
        yuko    = 10.000;
        wazaari = 15.000;
        ippon   = 20.000;
    } else {
        koka    = 10.000;
        yuko    = 10.000;
        wazaari = 15.000;
        ippon   = 20.000;
    }

    switch (key) {
    case GDK_0:
        if (msg) {
            /*g_print("is-gs=%d match=%d gs=%d rep=%d flags=0x%x\n", 
              golden_score, msg->match_time, msg->gs_time, msg->rep_time, msg->flags);*/
            if ((msg->flags & MATCH_FLAG_REPECHAGE) && msg->rep_time) {
                total = msg->rep_time;
                golden_score = TRUE;
            } else if (golden_score)
                total = msg->gs_time;
            else 
                total = msg->match_time;

            koka    = msg->pin_time_koka;
            yuko    = msg->pin_time_yuko;
            wazaari = msg->pin_time_wazaari;
            ippon   = msg->pin_time_ippon;

            jcontrol = FALSE;
            if (require_judogi_ok &&  
                (!(msg->flags & MATCH_FLAG_JUDOGI1_OK) ||
                 !(msg->flags & MATCH_FLAG_JUDOGI2_OK))) {
                gchar buf[128];
                snprintf(buf, sizeof(buf), "%s: %s", _("CONTROL"), 
                         !(msg->flags & MATCH_FLAG_JUDOGI1_OK) ? get_name(BLUE) : get_name(WHITE));
                display_big(buf, 2);
                jcontrol = TRUE;
                jcontrol_flags = msg->flags;
            } else if (msg->rest_time) {
                gchar buf[128];
                snprintf(buf, sizeof(buf), "%s: %s", _("REST TIME"),
                         msg->minutes & MATCH_FLAG_BLUE_REST ?
                         get_name(BLUE) :
                         get_name(WHITE));
                rest_time = TRUE;
                rest_flags = msg->minutes;
                total = msg->rest_time;
                st.startTime = g_timer_elapsed(timer, NULL);
                st.running = TRUE;
                display_big(buf, msg->rest_time);
                if (current_category != msg->category || current_match != msg->match)
                    judotimer_log("Rest time (%d s) started for %d:%d (%s - %s)",
                                  msg->rest_time,
                                  msg->category, msg->match,
                                  msg->blue_1, msg->white_1);
                return;
            }
        }
        break;
    case GDK_1:
        total   = golden_score ? gs_time : 120.0;
        koka    = 0.0;
        yuko    = 5.0;
        wazaari = 10.0;
        ippon   = 15.0;
        break;
    case GDK_2:
        total   = golden_score ? gs_time : 120.000;
        break;
    case GDK_3:
        total   = golden_score ? gs_time : 180.000;
        break;
    case GDK_4:
        total   = golden_score ? gs_time : 240.000;
        break;
    case GDK_5:
        total   = golden_score ? gs_time : 300.000;
        break;
    case GDK_6:
        total   = golden_score ? gs_time : 150.000;
        break;
    case GDK_7:
        total   = 10000.000;
        break;
    case GDK_9:
        if (golden_score)
            total = gs_time;
        break;
    }

    if (short_pin_times) {
        koka    = 0.0;
        yuko    = 5.0;
        wazaari = 10.0;
        ippon   = 15.0;
    }

    st.action_on = 0;

    if (key != GDK_0) {
        set_comment_text("");
        delete_big(NULL);
    }

    reset_display(key);
    update_display();

    if (key != GDK_0) {
        //display_ad_window();
	char buf[16];
	snprintf(buf, sizeof(buf), "/nextmatch?t=%d", tatami);
	emscripten_async_wget_data(buf, NULL, nextmatchonload, nextmatchonerror);
	g_print("send %s\n", buf);
    }
}

static void incdecpts(gint *p, gboolean decrement)
{
    if (decrement) {
        st.big_displayed = FALSE;
        if (*p > 0) (*p)--;
    } else if (*p < 99)
        (*p)++;
}

#define INC FALSE
#define DEC TRUE

void check_ippon(void)
{
    if (st.bluepts[0] > 3)
        st.bluepts[0] = 3;
    if (st.whitepts[0] > 3)
        st.whitepts[0] = 3;

    set_points(st.bluepts, st.whitepts);

    if (st.whitepts[0] >= 2 || st.bluepts[0] >= 2) {
        if (rules_stop_ippon_2 == TRUE) {
              if (st.oRunning)
              oToggle();
              if (st.running)
              toggle();
        }
        beep("IPPON");

        gchar *name = get_name(st.whitepts[0] >= 2 ? WHITE : BLUE);
        if (name == NULL || name[0] == 0)
            name = st.whitepts[0] >= 2 ? "white" : "blue";

        judotimer_log("%s: %s wins by %f s Ippon)!",
                      get_cat(), name, st.elap);
    } else if (golden_score && get_winner()) {
        beep(_("Golden Score"));
    } else if ((st.whitepts[3] >= 4 || st.bluepts[3] >= 4) &&
               rule_eq_score_less_shido_wins && rules_stop_ippon_2) {
    	if (st.oRunning)
    		oToggle();
    	if (st.running)
    		toggle();
        beep("SHIDO, Hansokumake");
        gchar *name = get_name(st.whitepts[0] >= 2 ? WHITE : BLUE);
        if (name == NULL || name[0] == 0)
            name = st.whitepts[0] >= 2 ? "white" : "blue";

        judotimer_log("%s: %s wins by %f s Shido)!",
                      get_cat(), name, st.elap);
    }

}

gboolean set_osaekomi_winner(gint who)
{
    if (st.oRunning == 0) {
        return approve_osaekomi_score(who & CMASK);
    }

    if (who & GIVE_POINTS /*st.osaekomi_winner == who*/)
        return FALSE;

    st.osaekomi_winner = who;

    if ((who == BLUE && white_first == FALSE) ||
        ((who == WHITE && white_first == TRUE)))
        set_comment_text(_("Points going to blue"));
    else if ((who == WHITE && white_first == FALSE) ||
             ((who == BLUE && white_first == TRUE)))
        set_comment_text(_("Points going to white"));
    else
        set_comment_text("");

    return TRUE;
}

gint approve_osaekomi_score(gint who)
{
    if (st.stackdepth == 0)
        return FALSE;

    st.stackdepth--;

    if (clocks_only)
        return FALSE;

    if (who == 0)
        who = st.stackval[st.stackdepth].who;

    switch (st.stackval[st.stackdepth].points) {
    case 1:
        if (who == BLUE)
            incdecpts(&st.bluepts[2], 0);
        else if (who == WHITE)
            incdecpts(&st.whitepts[2], 0);
        break;
    case 2:
        if (who == BLUE)
            incdecpts(&st.bluepts[1], 0);
        else if (who == WHITE)
            incdecpts(&st.whitepts[1], 0);
        break;
    case 4:
        if (who == BLUE)
            incdecpts(&st.bluepts[0], 0);
        else if (who == WHITE)
            incdecpts(&st.whitepts[0], 0);
    case 3:
        if (who == BLUE)
            incdecpts(&st.bluepts[0], 0);
        else if (who == WHITE)
            incdecpts(&st.whitepts[0], 0);
        break;
    }

    if (who == BLUE)
        log_scores("Osaekomi score to ", BLUE);
    else
        log_scores("Osaekomi score to ", WHITE);

    check_ippon();

    return TRUE;
}

void give_osaekomi_score()
{
#if 0
    switch (st.score) {
    case 1:
        if (st.osaekomi_winner == BLUE)
            incdecpts(&st.bluepts[2], 0);
        else if (st.osaekomi_winner == WHITE)
            incdecpts(&st.whitepts[2], 0);
        break;
    case 2:
        if (st.osaekomi_winner == BLUE)
            incdecpts(&st.bluepts[1], 0);
        else if (st.osaekomi_winner == WHITE)
            incdecpts(&st.whitepts[1], 0);
        break;
    case 4:
        if (st.osaekomi_winner == BLUE)
            incdecpts(&st.bluepts[0], 0);
        else if (st.osaekomi_winner == WHITE)
            incdecpts(&st.whitepts[0], 0);
    case 3:
        if (st.osaekomi_winner == BLUE)
            incdecpts(&st.bluepts[0], 0);
        else if (st.osaekomi_winner == WHITE)
            incdecpts(&st.whitepts[0], 0);
        break;
    }
#endif
    if (st.score) {
        st.stackval[st.stackdepth].who =
            st.osaekomi_winner;
        st.stackval[st.stackdepth].points =
            st.score;
        if (st.stackdepth < (STACK_DEPTH - 1))
            st.stackdepth++;
    }

    st.score = 0;
    st.osaekomi_winner = 0;
#if 0
    check_ippon();
#endif
}

void clock_key(guint key, guint event_state)
{
    gint i;
    gboolean shift = (event_state & SDLK_LSHIFT) != 0;
    gboolean ctl = (event_state & SDLK_LCTRL) != 0;
    static guint lastkey;
    static gdouble lasttime, now;

    now = g_timer_elapsed(timer, NULL);
    if (now < lasttime + 0.1 && key == lastkey) {
	printf("FAIL: now=%f lasttime=%f key=%x lastkey=%x\n",
	       now, lasttime, key, lastkey);
        return;
    }

    lastkey = key;
    lasttime = now;

    if (key == GDK_c && ctl) {
    	//manipulate_time(main_window, (gpointer)4 );
    	return;
    }
    if (key == GDK_b && ctl) {
    	if(white_first){
    		voting_result(NULL,(gpointer)HANTEI_WHITE);
    	}else{
           	voting_result(NULL,(gpointer)HANTEI_BLUE);
    	}
    	return;
    }
    if (key == GDK_w && ctl) {
    	if(white_first){
    		voting_result(NULL,(gpointer)HANTEI_BLUE);
    	}else{
           	voting_result(NULL,(gpointer)HANTEI_WHITE);
    	}
    	return;
    }
    if (key == GDK_e && ctl) {
       	voting_result(NULL,(gpointer)CLEAR_SELECTION);
    	return;
    }

    if (key == GDK_t) {
        display_comp_window(saved_cat, saved_last1, saved_last2, 
                            saved_first1, saved_first2, saved_country1, saved_country2);
        return;
    }

    st.action_on = 1;

    switch (key) {
    case GDK_0:
        automatic = TRUE;
        reset(GDK_7, NULL);
        break;
    case GDK_1:
    case GDK_2:
    case GDK_3:
    case GDK_4:
    case GDK_5:
    case GDK_6:
    case GDK_9:
        automatic = FALSE;
        reset(key, NULL);
        break;
    case GDK_space:
        toggle();
        break;
    case GDK_s:
        if (st.running) {
            st.running = FALSE;
            st.elap = g_timer_elapsed(timer, NULL) - st.startTime;
            if (total > 0.0 && st.elap >= total)
                st.elap = total;
            if (st.oRunning) {
                st.oElap = g_timer_elapsed(timer, NULL) - st.oStartTime;
                set_comment_text("SONOMAMA!");
            } else {
                st.oElap = 0;
                st.score = 0;
            }
            update_display();
        } else if (!st.running) {
            st.running = TRUE;
            st.startTime = g_timer_elapsed(timer, NULL) - st.elap;
            if (st.oRunning) {
                st.oStartTime = g_timer_elapsed(timer, NULL) - st.oElap;
                set_comment_text("");
            }
            update_display();
        }
        break;
    case GDK_Up:
        set_osaekomi_winner(BLUE);
        break;
    case GDK_Down:
        set_osaekomi_winner(WHITE);
        break;
    case GDK_Return:
    case GDK_KP_Enter:
        oToggle();
        if (!st.oRunning) {
            give_osaekomi_score();
        }
        break;
    case GDK_F5:
	if ((st.whitepts[0] & 2) && !shift)
	    break;
        incdecpts(&st.whitepts[0], shift);
	incdecpts(&st.whitepts[0], shift);
	log_scores("Ippon to ", WHITE);
#if 0 //XXXXXXXXXX
        if (st.whitepts[0] > 2)
            st.whitepts[0] = 2;
#endif
        check_ippon();
        break;
    case GDK_F6:
	incdecpts(&st.whitepts[0], shift);
	log_scores("Waza-ari to ", WHITE);
        check_ippon();
        break;
    case GDK_F7:
	incdecpts(&st.whitepts[1], shift);
	log_scores("Yuko to ", WHITE);
        break;
    case GDK_F8:
        if (shift) {
            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st.whitepts[3]) {
                case 0: break;
                case 1: break;
                case 2: 
                    incdecpts(&st.bluepts[1], DEC);
                    break;
                case 3: incdecpts(&st.bluepts[1], INC); incdecpts(&st.bluepts[0], DEC); break;
                case 4: incdecpts(&st.bluepts[0], DEC);
                }
            } else if (st.whitepts[3] == 4) {
                st.bluepts[0] &= ~2;
            }
            incdecpts(&st.whitepts[3], DEC);
            log_scores("Cancel shido to ", WHITE);
        } else {
            incdecpts(&st.whitepts[3], INC);

            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st.whitepts[3]) {
                case 1: break;
                case 2: 
                    incdecpts(&st.bluepts[1], INC);
                    break;
                case 3: incdecpts(&st.bluepts[1], DEC); incdecpts(&st.bluepts[0], INC); break;
                case 4: incdecpts(&st.bluepts[0], INC);
                }
            } else if (st.whitepts[3] >= 4) {
                st.bluepts[0] |= 2;
            }
            log_scores("Shido to ", WHITE);
        }
        break;
    case GDK_F1:
	if ((st.bluepts[0] & 2) && !shift)
	    break;
        incdecpts(&st.bluepts[0], shift);
	incdecpts(&st.bluepts[0], shift);
	log_scores("Ippon to ", BLUE);
        check_ippon();
        break;
    case GDK_F2:
	incdecpts(&st.bluepts[0], shift);
	log_scores("Waza-ari to ", BLUE);
        check_ippon();
        break;
    case GDK_F3:
	incdecpts(&st.bluepts[1], shift);
	log_scores("Yuko to ", BLUE);
        break;
    case GDK_F4:
        if (shift) {
            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st.bluepts[3]) {
                case 0: break;
                case 1: break;
                case 2: 
                    incdecpts(&st.whitepts[1], DEC);
                    break;
                case 3: incdecpts(&st.whitepts[1], INC); incdecpts(&st.whitepts[0], DEC); break;
                case 4: incdecpts(&st.whitepts[0], DEC);
                }
            } else if (st.bluepts[3] == 4) {
                st.whitepts[0] &= ~2;
            }
            incdecpts(&st.bluepts[3], DEC);
            log_scores("Cancel shido to ", BLUE);
        } else {
            incdecpts(&st.bluepts[3], INC);
            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st.bluepts[3]) {
                case 1: break;
                case 2: 
                    incdecpts(&st.whitepts[1], INC);
                    break;
                case 3: incdecpts(&st.whitepts[1], DEC); incdecpts(&st.whitepts[0], INC); break;
                case 4: incdecpts(&st.whitepts[0], INC);
                }
            } else if (st.bluepts[3] >= 4) {
                st.whitepts[0] |= 2;
            }
            log_scores("Shido to ", BLUE);
        }
        break;
    default:
        ;
    }

    /* check for shido amount of points */
    if (st.bluepts[3] > 4)
        st.bluepts[3] = 4;
    if (rule_eq_score_less_shido_wins == FALSE) { 
        if (st.bluepts[3] == 4 && (st.whitepts[0] & 2) == 0)
            st.whitepts[0] |= 2;
        else if (st.bluepts[3] == 3 && st.whitepts[0] == 0)
            st.whitepts[0] = 1;
        else if (st.bluepts[3] == 2 && st.whitepts[1] == 0)
            st.whitepts[1] = 1;
    }    

    if (st.whitepts[3] > 4)
        st.whitepts[3] = 4;
    if (rule_eq_score_less_shido_wins == FALSE) { 
        if (st.whitepts[3] == 4 && (st.bluepts[0] & 2) == 0)
            st.bluepts[0] |= 2;
        else if (st.whitepts[3] == 3 && st.bluepts[0] == 0)
            st.bluepts[0] = 1;
        else if (st.whitepts[3] == 2 && st.bluepts[1] == 0)
            st.bluepts[1] = 1;
    }
    
    check_ippon();
}

#define NUM_KEYS 9

static void gen_random_key(void)
{
    extern int current_category;
    extern int current_match;
    /*static int last_category, last_match;*/
    /*static gboolean wait_for_new_match = FALSE;*/
    static guint /*last_time = 0, */now = 0, /*wait_set_time = 0,*/ next_time = 0;
    /*
      static guint keys[NUM_KEYS] =
      {GDK_F1, GDK_F2, GDK_F3,
      GDK_F5, GDK_F6, GDK_F7,
      GDK_2,  GDK_space, GDK_Return};
      guint key = 0;*/
    static gint /*test_state = 0,*/ last_cat = 0, last_num = 0;
    static gboolean last_win = FALSE;

    //return;

    now++;
    if (now < next_time /*last_time + 40*/)
        return;

    //last_time = now;

    if (st.running) {
        if (st.bluepts[0] == 0 && st.whitepts[0] == 0) {
            if (last_win) {
                clock_key(GDK_F5, 0);
                last_win = FALSE;
            } else {
                clock_key(GDK_F1, 0);
                last_win = TRUE;
            }
            next_time = now + rand()%20 + 10;
        } else {
            clock_key(GDK_space, 0);
            next_time = now + rand()%100 + 50;
        }
    } else {
        if (current_category == last_cat &&
            current_match == last_num) {
            clock_key(GDK_0, 0);
            next_time = now + rand()%60 + 40;
        } else {
            clock_key(GDK_space, 0);
            last_cat = current_category;
            last_num = current_match;
            next_time = now + rand()%1000 + 100;
        }
    }

    return;
#if 0
    switch (test_state) {
    case 0: clock_key(GDK_2, 0); break;
    case 1: clock_key(GDK_space, 0); break;
    case 2: clock_key(GDK_F1, 0); break;
    case 3: clock_key(GDK_space, 0); break;
    case 4: clock_key(GDK_2, 0); break;
    case 5: clock_key(GDK_space, 0); break;
    case 6: clock_key(GDK_F5, 0); break;
    case 7: clock_key(GDK_space, 0); break;
    }
    test_state++;
    if (test_state >= 8)
        test_state = 0;
    return;

    if (wait_for_new_match && st.running == 0 &&
        last_category == current_category &&
        last_match == current_match &&
        now < wait_set_time + 100)
        return;

    wait_for_new_match = FALSE;

    if (st.running == 0 && st.oRunning == 0) {
        if (st.action_on) {
            last_category = current_category;
            last_match = current_match;
            wait_for_new_match = TRUE;
            wait_set_time = now;
            key = GDK_0;
            //last_time += 20;
        } else
            key = GDK_space;
    } else if (st.oRunning) {
        if (now % 10 == 0)
            key = GDK_Return;
    } else if (st.stackdepth) {
        key = (rand()&1) ? GDK_Up : GDK_Down;
    } else if ((st.bluepts[0] & 2 || st.whitepts[0] & 2) &&
               st.running) {
        key = GDK_space;
    } else {
        key = keys[rand() % NUM_KEYS];
    }

    if (key)
        clock_key(key, 0);

#endif
}

gchar *logfile_name = NULL;

void judotimer_log(gchar *format, ...)
{
#if 0
    time_t t;
    gchar buf[256];
    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);

    t = time(NULL);

    if (logfile_name == NULL) {
        struct tm *tm = localtime(&t);
        if (tm) {
	        sprintf(buf, "judotimer_%04d%02d%02d_%02d%02d%02d.log",
	                tm->tm_year+1900,
	                tm->tm_mon+1,
	                tm->tm_mday,
	                tm->tm_hour,
	                tm->tm_min,
	                tm->tm_sec);
	        logfile_name = g_build_filename(g_get_user_data_dir(), buf, NULL);
	    }
        g_print("logfile_name=%s\n", logfile_name);
    }

    FILE *f = fopen(logfile_name, "a");
    if (f) {
        struct tm *tm = localtime(&t);

        guint t = (total > 0.0) ? (total - st.elap) : st.elap;
        guint min = t / 60;
        guint sec =  t - min*60;

#ifdef USE_ISO_8859_1
        gsize x;
        gchar *text_ISO_8859_1 =
            g_convert(text, -1, "ISO-8859-1", "UTF-8", NULL, &x, NULL);
#else
        gchar *text_ISO_8859_1 = text;
#endif
		if (tm) {
	        if (t > 610)
	            fprintf(f, "%02d:%02d:%02d [-:--] <%d-%02d> %s\n",
	                    tm->tm_hour,
	                    tm->tm_min,
	                    tm->tm_sec,
	                    current_category, current_match,
	                    text_ISO_8859_1);
	        else
	            fprintf(f, "%02d:%02d:%02d [%d:%02d] <%d-%02d> %s\n",
	                    tm->tm_hour,
	                    tm->tm_min,
	                    tm->tm_sec,
	                    min, sec,
	                    current_category, current_match,
	                    text_ISO_8859_1);
		}
#ifdef USE_ISO_8859_1
        g_free(text_ISO_8859_1);
#endif
        fclose(f);
    } else {
        g_print("Cannot open log file\n");
        perror("logfile_name");
    }

    g_free(text);
#endif
}
