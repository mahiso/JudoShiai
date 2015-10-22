/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

/* Include system network headers */
#if defined(__WIN32__) || defined(WIN32)

#define  __USE_W32_SOCKETS
//#define Win32_Winsock

#include <windows.h>
#include <stdio.h>
#include <initguid.h>
#include <winsock2.h>

#else /* UNIX */

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

#endif /* WIN32 */

#include <gtk/gtk.h>
#include <glib.h>

#if (GTKVER == 3)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#ifdef WIN32
//#include <glib/gwin32.h>
#endif

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
#define NUM_STATES 4

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
} st[NUM_STATES];
static gint stack_depth = 0;

static guint     last_m_time;
static gboolean  last_m_run;
static guint     last_m_otime;
static gboolean  last_m_orun;
static guint     last_score;
static gchar     osaekomi = ' ';
static gdouble   koka = 10.0;
static gdouble   yuko = 15.0;
static gdouble   wazaari = 20.0;
static gdouble   ippon = 25.0;
static gdouble   total;
static gdouble   gs_time = 0.0;
gint             tatami;
gint             demo = 0;
gboolean         automatic;
gboolean         short_pin_times = FALSE;
gboolean         golden_score = FALSE;
gboolean         rest_time = FALSE;
static gint      rest_flags = 0;
gchar           *matchlist = NULL;
static gint      gs_cat = 0, gs_num = 0;
gboolean         require_judogi_ok = FALSE;
static gboolean  jcontrol;
static gint      jcontrol_flags;

static void log_scores(gchar *txt, gint who)
{
    judotimer_log("%s%s: IWYKS = %d%d%d%d%d - %d%d%d%d%d", txt,
                  ((who == BLUE && white_first) || (who == WHITE && white_first == FALSE)) ? "white" : "blue",
                  st[0].bluepts[0]&2 ? 1 : 0,
                  st[0].bluepts[0]&1,
                  st[0].bluepts[1],
                  st[0].bluepts[2],
                  st[0].bluepts[3],
                  st[0].whitepts[0]&2 ? 1 : 0,
                  st[0].whitepts[0]&1,
                  st[0].whitepts[1],
                  st[0].whitepts[2],
                  st[0].whitepts[3]);
}

void beep(char *str)
{
    if (st[0].big_displayed)
        return;

    display_big(str, 4);
    st[0].big_displayed = TRUE;

    play_sound();
}

void update_display(void)
{
    guint t, min, sec, oSec;
    gint orun, score;
    static gint flash = 0;

    // move last second to first second i.e. 0:00 = soremade
    if (st[0].elap == 0.0) t = total;
    else if (total == 0.0) t = st[0].elap;
    else {
        t = total - st[0].elap + 0.9;
        if (t == 0 && total > st[0].elap)
            t = 1;
    }

    min = t / 60;
    sec =  t - min*60;
    oSec = st[0].oElap;

    if (t != last_m_time) {
        last_m_time = t;
        set_timer_value(min, sec/10, sec%10);
        set_competitor_window_rest_time(min, sec/10, sec%10, rest_time, rest_flags);
    }

    if (oSec != last_m_otime) {
        last_m_otime = oSec;
        set_osaekomi_value(oSec/10, oSec%10);
    }

    if (st[0].running != last_m_run) {
        last_m_run = st[0].running;
        set_timer_run_color(last_m_run, rest_time);
    }

    if (st[0].oRunning) {
        orun = st[0].oRunning ? OSAEKOMI_DSP_YES : OSAEKOMI_DSP_NO;
        score = st[0].score;
    } else if (st[0].stackdepth) {
        if (st[0].stackval[st[0].stackdepth-1].who == BLUE)
            orun = OSAEKOMI_DSP_BLUE;
        else if (st[0].stackval[st[0].stackdepth-1].who == WHITE)
            orun = OSAEKOMI_DSP_WHITE;
        else
            orun = OSAEKOMI_DSP_UNKNOWN;

        if (++flash > 5)
            orun = OSAEKOMI_DSP_NO;
        if (flash > 10)
            flash = 0;

        score = st[0].stackval[st[0].stackdepth-1].points;
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
    gint i;
    gboolean show_soremade = FALSE;

    for (i = 0; i < NUM_STATES; i++) {
        //guint t, min, sec, oSec;

        if (st[i].running) {
            st[i].elap = g_timer_elapsed(timer, NULL) - st[i].startTime;
            if (total > 0.0 && st[i].elap >= total) {
                //running = false;
                st[i].elap = total;
                if (!st[i].oRunning) {
                    st[i].running = FALSE;
                    if (i == 0 && rest_time == FALSE)
                        show_soremade = TRUE;
                    if (rest_time) {
                        st[0].elap = 0;
                        st[0].oRunning = FALSE;
                        st[0].oElap = 0;
                        rest_time = FALSE;
                    }
                }
            }
        }

        if (st[i].running && st[i].oRunning) {
            st[i].oElap =  g_timer_elapsed(timer, NULL) - st[i].oStartTime;
            st[i].score = 0;
            if (rules_no_koka_dsp == FALSE && st[i].oElap >= koka)
                st[i].score = 1;
            if (st[i].oElap >= yuko)
                st[i].score = 2;
            if (st[i].oElap >= wazaari) {
                st[i].score = 3;
                if ((st[i].osaekomi_winner == BLUE && st[i].bluepts[0]) ||
                    (st[i].osaekomi_winner == WHITE && st[i].whitepts[0])) {
                    st[i].running = FALSE;
                    st[i].oRunning = FALSE;
                    if (i == 0) {
                        give_osaekomi_score();
                        approve_osaekomi_score(0);
                    }
                }
            }
            if (st[i].oElap >= ippon) {
                st[i].score = 4;
                st[i].running = FALSE;
                st[i].oRunning = FALSE;
                if (i == 0) {
                    gint tmp = st[0].osaekomi_winner;
                    give_osaekomi_score();
                    if (tmp)
                        approve_osaekomi_score(0);
                }
            }
        }

        /*if (i != 0)
          continue;*/
    }

    update_display();

    if (show_soremade) {
        beep("SOREMADE");
#if 0
        if (golden_score &&
            (array2int(st[0].bluepts) & 0xfffffff0) == (array2int(st[0].whitepts) & 0xfffffff0))
            ask_for_hantei();
#endif
    }

    // judogi control display for ad
    {
        static gint cnt = 0;
        if (++cnt >= 10) {
            set_competitor_window_judogi_control(jcontrol, jcontrol_flags);
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
    if (st[0].running)
        return;
    if (total == 0) {
        st[0].elap += 1.0;
    } else if (st[0].elap >= 1.0)
        st[0].elap -= 1.0;
    else
        st[0].elap = 0.0;
    update_display();
}

void hajime_dec_func(void)
{
    if (st[0].running)
        return;
    if (total == 0) {
        if (st[0].elap >= 1.0) st[0].elap -= 1.0;
    } else if (st[0].elap <= (total - 1.0))
        st[0].elap += 1.0;
    else
        st[0].elap = total;
    update_display();
}

void osaekomi_inc_func(void)
{
    if (st[0].running)
        return;
    if (st[0].oElap <= (ippon - 1.0))
        st[0].oElap += 1.0;
    else
        st[0].oElap = ippon;
    update_display();
}

void osaekomi_dec_func(void)
{
    if (st[0].running)
        return;
    if (st[0].oElap >= 1.0)
        st[0].oElap -= 1.0;
    else
        st[0].oElap = 0;
    update_display();
}

void set_clocks(gint clock, gint osaekomi)
{
    if (st[0].running)
        return;

    if ( clock >= 0 && total >= clock)
    	st[0].elap = total - (gdouble)clock;
    else if (clock >= 0)
        st[0].elap = (gdouble)clock;

    if (osaekomi) {
        st[0].oElap = (gdouble)osaekomi;
        st[0].oRunning = TRUE;
    }

    update_display();
}


static void toggle(void)
{
    if (st[0].running /*&& !oRunning*/) {
        judotimer_log("Shiai clock stop");
        st[0].running = FALSE;
        st[0].elap = g_timer_elapsed(timer, NULL) - st[0].startTime;
        if (total > 0.0 && st[0].elap >= total)
            st[0].elap = total;
        if (st[0].oRunning) {
#if 1 // According to Staffan's wish
            st[0].oRunning = FALSE;
            st[0].oElap = 0;
            give_osaekomi_score();
#else
            st[0].oElap = g_timer_elapsed(timer, NULL) - st[0].oStartTime;
            set_comment_text("SONOMAMA!");
#endif
        } else {
            st[0].oElap = 0;
            st[0].score = 0;
        }
        update_display();
        video_record(FALSE);
    } else if (!st[0].running) {
        if (total > 9000) // don't let clock run if dashes in display '-:--'
            return;

        if (st[0].elap == 0)
            judotimer_log("MATCH START: %s: %s - %s",
                          get_cat(), get_name(BLUE), get_name(WHITE));
        judotimer_log("Shiai clock start");
        st[0].running = TRUE;
        st[0].startTime = g_timer_elapsed(timer, NULL) - st[0].elap;
        if (st[0].oRunning) {
            st[0].oStartTime = g_timer_elapsed(timer, NULL) - st[0].oElap;
            set_comment_text("");
        }
        update_display();
        video_record(TRUE);
    }
}

int get_match_time(void)
{
    if (demo >= 2)
        return (10 + rand()%200);

    return st[0].elap + st[0].match_time;
}

static void oToggle() {
    if (st[0].running == FALSE && (st[0].elap < total || total == 0.0)) {
        if (st[0].oRunning) // yoshi
            toggle();
        return;
    }

    if (st[0].oRunning) {
        judotimer_log("Osaekomi clock stop after %f s", st[0].oElap);
        st[0].oRunning = FALSE;
        //st[0].oElap = g_timer_elapsed(timer, NULL) - st[0].oStartTime;
        st[0].oElap = 0;
        set_comment_text("");
        update_display();
        if (total > 0.0 && st[0].elap >= total)
            beep("SOREMADE");
    } else /*if (st[0].running)*/ {
        judotimer_log("Osaekomi clock start");
        st[0].running = TRUE;
        osaekomi = ' ';
        st[0].oRunning = TRUE;
        st[0].oStartTime = g_timer_elapsed(timer, NULL);
        update_display();
    }
}

int clock_running(void)
{
    return st[0].action_on;
}

gboolean osaekomi_running(void)
{
    return st[0].oRunning;
}

void set_hantei_winner(gint cmd)
{
    if (cmd == CLEAR_SELECTION)
        st[0].big_displayed = FALSE;

    if (cmd == HANSOKUMAKE_BLUE)
        st[0].whitepts[0] |= 2;
    else if (cmd == HANSOKUMAKE_WHITE)
        st[0].bluepts[0] |= 2;

    check_ippon();
}

gboolean ask_for_golden_score(void)
{
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
}

#if 0
static void ask_for_hantei(void)
{
    GtkWidget *dialog;
    gint response;

    golden_score = FALSE;
    dialog = gtk_dialog_new_with_buttons (_("Hantei"),
                                          NULL,
                                          GTK_DIALOG_MODAL,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          _("Blue wins"),      Q_HANTEI_BLUE,
                                          _("White wins"),     Q_HANTEI_WHITE,
                                          NULL);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    switch (response) {
    case Q_HANTEI_BLUE: voting_result(NULL, (gpointer)HANTEI_BLUE); break;
    case Q_HANTEI_WHITE: voting_result(NULL, (gpointer)HANTEI_WHITE); break;
    }
}
#endif

static gboolean asking = FALSE;
static guint key_pending = 0;
static GtkWidget *ask_area;

#define ASK_OK  0x7301
#define ASK_NOK 0x7302

static gint get_winner(void)
{
    gint winner = 0;
    if (st[0].bluepts[0] > st[0].whitepts[0]) winner = BLUE;
    else if (st[0].bluepts[0] < st[0].whitepts[0]) winner = WHITE;
    else if (st[0].bluepts[1] > st[0].whitepts[1]) winner = BLUE;
    else if (st[0].bluepts[1] < st[0].whitepts[1]) winner = WHITE;
    else if (st[0].bluepts[2] > st[0].whitepts[2]) winner = BLUE;
    else if (st[0].bluepts[2] < st[0].whitepts[2]) winner = WHITE;
    else if (blue_wins_voting) winner = BLUE;
    else if (white_wins_voting) winner = WHITE;
    else if (hansokumake_to_white) winner = BLUE;
    else if (hansokumake_to_blue) winner = WHITE;
    else if (rule_eq_score_less_shido_wins &&
             (st[0].bluepts[3] || st[0].whitepts[3])) {
        if (st[0].bluepts[3] > st[0].whitepts[3]) winner = WHITE;
        else if (st[0].bluepts[3] < st[0].whitepts[3]) winner = BLUE;
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
    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_UPDATE_LABEL;
    msg.u.update_label.label_num = STOP_WINNER;
    /*write_tv_logo(&(msg.u.update_label));*/

    if (mode != MODE_SLAVE)
        send_label_msg(&msg);

    legend_widget = NULL;
    ask_area = NULL;
    asking = FALSE;
    ask_window = NULL;
}

static gboolean close_ask_ok(GtkWidget *widget, gpointer userdata)
{
    if (legend_widget)
        legend = gtk_combo_box_get_active(GTK_COMBO_BOX(legend_widget));
    else
        legend = 0;

    if (legend < 0) legend = 0;

    reset(ASK_OK, NULL);
    gtk_widget_destroy(userdata);
    return FALSE;
}

static gboolean close_ask_nok(GtkWidget *widget, gpointer userdata)
{
    legend = 0;
    reset(ASK_NOK, NULL);
    gtk_widget_destroy(userdata);
    return FALSE;
}

#define FIRST_BLOCK_C 0.25
#define FIRST_BLOCK_START  0.0
#define SECOND_BLOCK_START  (FIRST_BLOCK_C*height)
#define THIRD_BLOCK_START ((1.0+FIRST_BLOCK_C)*height/2.0)
#define FIRST_BLOCK_HEIGHT (FIRST_BLOCK_C*height)
#define OTHER_BLOCK_HEIGHT ((height-FIRST_BLOCK_HEIGHT)/2.0)

static gboolean expose_ask(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
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

    return FALSE;
}

static gchar *legends[] = 
    {"?", "(T)", "(H)", "(C)", "(L)", "SG", "HM", "KG", "T", "H", "S", "/P\\", "FG", "TT", "TH", "/HM\\", NULL};

static void create_ask_window(void)
{
    GtkWidget *vbox, *hbox, *ok, *nok, *lbl;
    gint width;
    gint height;
    gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);

    GtkWindow *window = ask_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_keep_above(GTK_WINDOW(ask_window), TRUE);
    gtk_window_set_title(GTK_WINDOW(window), _("Start New Match?"));
    if (fullscreen ) // Clicking outside  && show_competitor_names && get_winner())
        gtk_window_fullscreen(GTK_WINDOW(window));
    else if (show_competitor_names && get_winner())
        gtk_widget_set_size_request(GTK_WIDGET(window), width, height);

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

#if (GTKVER == 3)
    vbox = gtk_grid_new();
#else
    vbox = gtk_vbox_new(FALSE, 1);
#endif
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 1);

    if (mode != MODE_SLAVE) {
#if (GTKVER == 3)
        hbox = gtk_grid_new();
#else
        hbox = gtk_hbox_new(FALSE, 1);
#endif
        lbl = gtk_label_new(_("Start New Match?"));
        ok = gtk_button_new_with_label(_("OK"));
        nok = gtk_button_new_with_label(_("Cancel"));

        gint i;
#if (GTKVER == 3)
        legend_widget = gtk_combo_box_text_new();
#else
        legend_widget = gtk_combo_box_new_text();
#endif
        for (i = 0; legends[i]; i++)
#if (GTKVER == 3)
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(legend_widget), NULL, legends[i]);
#else
            gtk_combo_box_append_text(GTK_COMBO_BOX(legend_widget), legends[i]);
#endif
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(hbox), lbl, 0, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(hbox), ok, 1, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(hbox), nok, 2, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(hbox), legend_widget, 3, 0, 1, 1);

        gtk_grid_attach(GTK_GRID(vbox), hbox, 0, 0, 1, 1);
#else
        gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(hbox), ok, FALSE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(hbox), nok, FALSE, TRUE, 5);
        gtk_box_pack_start(GTK_BOX(hbox), legend_widget, FALSE, TRUE, 5);

        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);
#endif
    } else {
        lbl = gtk_label_new(_("- - -"));
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(vbox), lbl, 0, 0, 1, 1);
#else
        gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, TRUE, 5);
#endif
    }

    if (show_competitor_names && get_winner()) {
        ask_area = gtk_drawing_area_new();
#if (GTKVER == 3)
        gtk_widget_set_hexpand(ask_area, TRUE);
        gtk_widget_set_vexpand(ask_area, TRUE);    
        gtk_grid_attach(GTK_GRID(vbox), ask_area, 0, 1, 1, 1);
        g_signal_connect(G_OBJECT(ask_area), 
                         "draw", G_CALLBACK(expose_ask), NULL);
#else
        gtk_box_pack_start(GTK_BOX(vbox), ask_area, TRUE, TRUE, 5);
        g_signal_connect(G_OBJECT(ask_area), 
                         "expose-event", G_CALLBACK(expose_ask), NULL);
#endif
    }

    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show_all(GTK_WIDGET(window));

    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event_ask), NULL);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (destroy_ask), NULL);

    if (mode != MODE_SLAVE) {
        g_signal_connect(G_OBJECT(ok), 
                         "clicked", G_CALLBACK(close_ask_ok), window);
        g_signal_connect(G_OBJECT(nok), 
                         "clicked", G_CALLBACK(close_ask_nok), window);
    }
    gtk_window_set_modal(GTK_WINDOW(window),TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(window),GTK_WINDOW(main_window));

}

void close_ask_window(void)
{
    close_ask_ok(NULL, ask_window);
}

void display_ask_window(gchar *name, gchar *cat, gchar winner)
{
    gchar first[32], last[32], club[32], country[32];

    parse_name(name, first, last, club, country);

    memset(st[0].bluepts, 0, sizeof(st[0].bluepts));
    memset(st[0].whitepts, 0, sizeof(st[0].whitepts));

    if (winner == BLUE) {
        st[0].bluepts[0] = 1;
        st[0].whitepts[0] = 0;
        strncpy(saved_last1, last, sizeof(saved_last1)-1);
        strncpy(saved_first1, first, sizeof(saved_first1)-1);
    } else if (winner == WHITE) {
        st[0].bluepts[0] = 0;
        st[0].whitepts[0] = 1;
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
        if (0 && rest_time) {
            cancel_rest_time(rest_flags & MATCH_FLAG_BLUE_REST,
                             rest_flags & MATCH_FLAG_WHITE_REST);
        }
        key = key_pending;
        key_pending = 0;
        asking = FALSE;
        asked = TRUE;
    } else if (key == ASK_NOK) {
        key_pending = 0;
        asking = FALSE;
        return;
    }

    if ((st[0].running && rest_time == FALSE) || st[0].oRunning || asking)
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
        bp = array2int(st[0].bluepts);
        wp = array2int(st[0].whitepts);
    } else {
        bp = array2int(st[0].bluepts) & 0xfffffff0;
        wp = array2int(st[0].whitepts) & 0xfffffff0;
    }

    if (key == GDK_9 ||
        (demo == 0 && bp == wp && result_hikiwake == FALSE &&
         blue_wins_voting == white_wins_voting &&
         total > 0.0 && st[0].elap >= total && key != GDK_0)) {
        asking = TRUE;
        ask_for_golden_score();
        asking = FALSE;
        if (key == GDK_9 && golden_score == FALSE)
            return;
        key = GDK_9;
        st[0].match_time = st[0].elap;
        judotimer_log("Golden score starts");
        set_gs_text("GOLDEN SCORE");
    } else if (demo == 0 &&
               asked == FALSE &&
               (((st[0].bluepts[0] & 2) == 0 && (st[0].whitepts[0] & 2) == 0 &&
                 st[0].elap > 0.01 && st[0].elap < total - 0.01) ||
                (st[0].running && rest_time) ||
                rules_confirm_match) &&
               key != GDK_0) {
        key_pending = key;
        asking = TRUE;
        create_ask_window();

        struct message msg;
        gint winner = get_winner();
        memset(&msg, 0, sizeof(msg));
        msg.type = MSG_UPDATE_LABEL;
        msg.u.update_label.label_num = START_WINNER;
        if (result_hikiwake)
            snprintf(msg.u.update_label.text, sizeof(msg.u.update_label.text), 
                     "%s\t\t\t", _("Hikiwake"));
        else
            snprintf(msg.u.update_label.text, sizeof(msg.u.update_label.text), 
                     "%s\t%s\t\t", 
                     winner == BLUE ? saved_last1 : saved_last2,
                     winner == BLUE ? saved_first1 : saved_first2);
        strncpy(msg.u.update_label.text2, saved_cat,
                sizeof(msg.u.update_label.text2)-1);
        msg.u.update_label.text3[0] = winner;
        /*write_tv_logo(&(msg.u.update_label));*/

        if (mode != MODE_SLAVE)
            send_label_msg(&msg);

        return;
#if 0
        dialog = gtk_dialog_new_with_buttons (_("Start New Match?"),
					      NULL,
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

        GtkWidget *darea = gtk_drawing_area_new();
        gtk_container_add(GTK_CONTAINER (window), darea);

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        asking = FALSE;
        if (response != GTK_RESPONSE_OK)
            return;
#endif
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

        if (sides_switched) {
            send_result(st[0].whitepts, st[0].bluepts,
                        white_wins_voting, blue_wins_voting,
                        hansokumake_to_white, hansokumake_to_blue, legend, result_hikiwake);
            clear_switch_sides();
        } else
            send_result(st[0].bluepts, st[0].whitepts,
                        blue_wins_voting, white_wins_voting,
                        hansokumake_to_blue, hansokumake_to_white, legend, result_hikiwake);
        st[0].match_time = 0;
        gs_cat = gs_num = 0;
    }

    if (golden_score == FALSE || rules_leave_score == FALSE) {
        memset(st[0].bluepts, 0, sizeof(st[0].bluepts));
        memset(st[0].whitepts, 0, sizeof(st[0].whitepts));
    }

    blue_wins_voting = 0;
    white_wins_voting = 0;
    hansokumake_to_blue = 0;
    hansokumake_to_white = 0;
    if (key != GDK_0) result_hikiwake = 0;
    st[0].osaekomi_winner = 0;
    st[0].big_displayed = FALSE;

    for (i = 0; i < NUM_STATES; i++) {
        st[i].running = FALSE;
        st[i].elap = 0;
        st[i].oRunning = FALSE;
        st[i].oElap = 0;

        memset(&(st[i].stackval), 0, sizeof(st[i].stackval));
        st[i].stackdepth = 0;
    }

    if (rule_short_pin_times) {
        koka    = 5.000;
        yuko    = 10.000;
        wazaari = 15.000;
        ippon   = 20.000;
    } else {
        koka    = 10.000;
        yuko    = 15.000;
        wazaari = 20.000;
        ippon   = 25.000;
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
                st[0].startTime = g_timer_elapsed(timer, NULL);
                st[0].running = TRUE;
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

    st[0].action_on = 0;

    if (key != GDK_0) {
        set_comment_text("");
        delete_big(NULL);
    }

    reset_display(key);
    update_display();

    if (key != GDK_0) {
        display_ad_window();
        if (mode != MODE_SLAVE)
            send_label(START_ADVERTISEMENT);
    }
}

static void incdecpts(gint *p, gboolean decrement)
{
    if (decrement) {
        st[0].big_displayed = FALSE;
        if (*p > 0) (*p)--;
    } else if (*p < 99)
        (*p)++;
}

#define INC FALSE
#define DEC TRUE

void check_ippon(void)
{
    if (st[0].bluepts[0] > 3)
        st[0].bluepts[0] = 3;
    if (st[0].whitepts[0] > 3)
        st[0].whitepts[0] = 3;

    set_points(st[0].bluepts, st[0].whitepts);

    if (st[0].whitepts[0] >= 2 || st[0].bluepts[0] >= 2) {
        if (rules_stop_ippon_2 == TRUE) {
              if (st[0].oRunning)
              oToggle();
              if (st[0].running)
              toggle();
        }
        beep("IPPON");

        gchar *name = get_name(st[0].whitepts[0] >= 2 ? WHITE : BLUE);
        if (name == NULL || name[0] == 0)
            name = st[0].whitepts[0] >= 2 ? "white" : "blue";

        judotimer_log("%s: %s wins by %f s Ippon)!",
                      get_cat(), name, st[0].elap);
    } else if (golden_score && get_winner()) {
        beep(_("Golden Score"));
    } else if ((st[0].whitepts[3] >= 4 || st[0].bluepts[3] >= 4) &&
               rule_eq_score_less_shido_wins && rules_stop_ippon_2) {
    	if (st[0].oRunning)
    		oToggle();
    	if (st[0].running)
    		toggle();
        beep("SHIDO, Hansokumake");
        gchar *name = get_name(st[0].whitepts[0] >= 2 ? WHITE : BLUE);
        if (name == NULL || name[0] == 0)
            name = st[0].whitepts[0] >= 2 ? "white" : "blue";

        judotimer_log("%s: %s wins by %f s Shido)!",
                      get_cat(), name, st[0].elap);
    }

}

gboolean set_osaekomi_winner(gint who)
{
    if (st[0].oRunning == 0) {
        return approve_osaekomi_score(who & CMASK);
    }

    if (who & GIVE_POINTS /*st[0].osaekomi_winner == who*/)
        return FALSE;

    st[0].osaekomi_winner = who;

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
    if (st[0].stackdepth == 0)
        return FALSE;

    st[0].stackdepth--;

    if (clocks_only)
        return FALSE;

    if (who == 0)
        who = st[0].stackval[st[0].stackdepth].who;

    switch (st[0].stackval[st[0].stackdepth].points) {
    case 1:
        if (who == BLUE)
            incdecpts(&st[0].bluepts[2], 0);
        else if (who == WHITE)
            incdecpts(&st[0].whitepts[2], 0);
        break;
    case 2:
        if (who == BLUE)
            incdecpts(&st[0].bluepts[1], 0);
        else if (who == WHITE)
            incdecpts(&st[0].whitepts[1], 0);
        break;
    case 4:
        if (who == BLUE)
            incdecpts(&st[0].bluepts[0], 0);
        else if (who == WHITE)
            incdecpts(&st[0].whitepts[0], 0);
    case 3:
        if (who == BLUE)
            incdecpts(&st[0].bluepts[0], 0);
        else if (who == WHITE)
            incdecpts(&st[0].whitepts[0], 0);
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
    switch (st[0].score) {
    case 1:
        if (st[0].osaekomi_winner == BLUE)
            incdecpts(&st[0].bluepts[2], 0);
        else if (st[0].osaekomi_winner == WHITE)
            incdecpts(&st[0].whitepts[2], 0);
        break;
    case 2:
        if (st[0].osaekomi_winner == BLUE)
            incdecpts(&st[0].bluepts[1], 0);
        else if (st[0].osaekomi_winner == WHITE)
            incdecpts(&st[0].whitepts[1], 0);
        break;
    case 4:
        if (st[0].osaekomi_winner == BLUE)
            incdecpts(&st[0].bluepts[0], 0);
        else if (st[0].osaekomi_winner == WHITE)
            incdecpts(&st[0].whitepts[0], 0);
    case 3:
        if (st[0].osaekomi_winner == BLUE)
            incdecpts(&st[0].bluepts[0], 0);
        else if (st[0].osaekomi_winner == WHITE)
            incdecpts(&st[0].whitepts[0], 0);
        break;
    }
#endif
    if (st[0].score) {
        st[0].stackval[st[0].stackdepth].who =
            st[0].osaekomi_winner;
        st[0].stackval[st[0].stackdepth].points =
            st[0].score;
        if (st[0].stackdepth < (STACK_DEPTH - 1))
            st[0].stackdepth++;
    }

    st[0].score = 0;
    st[0].osaekomi_winner = 0;
#if 0
    check_ippon();
#endif
}

void clock_key(guint key, guint event_state)
{
    gint i;
    gboolean shift = event_state & 1;
    gboolean ctl = event_state & 4;
    static guint lastkey;
    static gdouble lasttime, now;

    now = g_timer_elapsed(timer, NULL);
    if (now < lasttime + 0.1 && key == lastkey)
        return;

    lastkey = key;
    lasttime = now;

    if (key == GDK_c && ctl) {
    	manipulate_time(main_window, (gpointer)4 );
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

    if (key == GDK_v) {
        create_video_window();
        return;
    }

    if (key == GDK_t) {
        display_comp_window(saved_cat, saved_last1, saved_last2, 
                            saved_first1, saved_first2, saved_country1, saved_country2);
        return;

        extern GtkWidget *main_window;
#if (GTKVER == 3)
        gdk_window_invalidate_rect(gtk_widget_get_window(main_window), NULL, TRUE);
#else
        gdk_window_invalidate_rect(main_window->window, NULL, TRUE);
#endif
        return;
    }

    if (key == GDK_z && ctl) {
        if (stack_depth > 0)
            stack_depth--;

        for (i = 0; i < NUM_STATES-1; i++)
            st[i] = st[i+1];

        update_display(); //THESE LINES CAUSE PROBLEMS!
        check_ippon();
        return;
    }

    switch (key) {
    case GDK_0:
    case GDK_1:
    case GDK_2:
    case GDK_3:
    case GDK_4:
    case GDK_5:
    case GDK_6:
    case GDK_9:
    case GDK_space:
    case GDK_Up:
    case GDK_Down:
    case GDK_Return:
    case GDK_KP_Enter:
    case GDK_F1:
    case GDK_F2:
    case GDK_F3:
    case GDK_F4:
    case GDK_F5:
    case GDK_F6:
    case GDK_F7:
    case GDK_F8:
    case GDK_s:
        for (i = NUM_STATES-1; i > 0; i--)
            st[i] = st[i-1];

        stack_depth++;
        if (stack_depth > NUM_STATES)
            stack_depth = NUM_STATES;
        break;
    }

    st[0].action_on = 1;

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
        if (st[0].running) {
            st[0].running = FALSE;
            st[0].elap = g_timer_elapsed(timer, NULL) - st[0].startTime;
            if (total > 0.0 && st[0].elap >= total)
                st[0].elap = total;
            if (st[0].oRunning) {
                st[0].oElap = g_timer_elapsed(timer, NULL) - st[0].oStartTime;
                set_comment_text("SONOMAMA!");
            } else {
                st[0].oElap = 0;
                st[0].score = 0;
            }
            update_display();
        } else if (!st[0].running) {
            st[0].running = TRUE;
            st[0].startTime = g_timer_elapsed(timer, NULL) - st[0].elap;
            if (st[0].oRunning) {
                st[0].oStartTime = g_timer_elapsed(timer, NULL) - st[0].oElap;
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
        if (!st[0].oRunning) {
            give_osaekomi_score();
        }
        break;
    case GDK_F5:
	if ((st[0].whitepts[0] & 2) && !shift)
	    break;
        incdecpts(&st[0].whitepts[0], shift);
        if (rules_no_koka_dsp) {
            incdecpts(&st[0].whitepts[0], shift);
            log_scores("Ippon to ", WHITE);
        }
#if 0 //XXXXXXXXXX
        if (st[0].whitepts[0] > 2)
            st[0].whitepts[0] = 2;
#endif
        check_ippon();
        break;
    case GDK_F6:
        if (rules_no_koka_dsp) {
            incdecpts(&st[0].whitepts[0], shift);
            log_scores("Waza-ari to ", WHITE);
        } else
            incdecpts(&st[0].whitepts[1], shift);
        check_ippon();
        break;
    case GDK_F7:
        if (rules_no_koka_dsp) {
            incdecpts(&st[0].whitepts[1], shift);
            log_scores("Yuko to ", WHITE);
        } else
            incdecpts(&st[0].whitepts[2], shift);
        break;
    case GDK_F8:
        if (shift) {
            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st[0].whitepts[3]) {
                case 0: break;
                case 1: if (!rules_no_koka_dsp) incdecpts(&st[0].bluepts[2], DEC); break;
                case 2: if (!rules_no_koka_dsp) incdecpts(&st[0].bluepts[2], INC);
                    incdecpts(&st[0].bluepts[1], DEC);
                    break;
                case 3: incdecpts(&st[0].bluepts[1], INC); incdecpts(&st[0].bluepts[0], DEC); break;
                case 4: incdecpts(&st[0].bluepts[0], DEC);
                }
            } else if (st[0].whitepts[3] == 4) {
                st[0].bluepts[0] &= ~2;
            }
            incdecpts(&st[0].whitepts[3], DEC);
            log_scores("Cancel shido to ", WHITE);
        } else {
            incdecpts(&st[0].whitepts[3], INC);

            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st[0].whitepts[3]) {
                case 1: if (!rules_no_koka_dsp) incdecpts(&st[0].bluepts[2], INC); break;
                case 2: if (!rules_no_koka_dsp) incdecpts(&st[0].bluepts[2], DEC);
                    incdecpts(&st[0].bluepts[1], INC);
                    break;
                case 3: incdecpts(&st[0].bluepts[1], DEC); incdecpts(&st[0].bluepts[0], INC); break;
                case 4: incdecpts(&st[0].bluepts[0], INC);
                }
            } else if (st[0].whitepts[3] >= 4) {
                st[0].bluepts[0] |= 2;
            }
            log_scores("Shido to ", WHITE);
        }
        break;
    case GDK_F1:
	if ((st[0].bluepts[0] & 2) && !shift)
	    break;
        incdecpts(&st[0].bluepts[0], shift);
        if (rules_no_koka_dsp) {
            incdecpts(&st[0].bluepts[0], shift);
            log_scores("Ippon to ", BLUE);
        }
#if 0 //XXXXXXXX
        if (st[0].bluepts[0] > 2)
            st[0].bluepts[0] = 2;
#endif
        check_ippon();
        break;
    case GDK_F2:
        if (rules_no_koka_dsp) {
            incdecpts(&st[0].bluepts[0], shift);
            log_scores("Waza-ari to ", BLUE);
        } else
            incdecpts(&st[0].bluepts[1], shift);
        check_ippon();
        break;
    case GDK_F3:
        if (rules_no_koka_dsp) {
            incdecpts(&st[0].bluepts[1], shift);
            log_scores("Yuko to ", BLUE);
        } else
            incdecpts(&st[0].bluepts[2], shift);
        break;
    case GDK_F4:
        if (shift) {
            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st[0].bluepts[3]) {
                case 0: break;
                case 1: if (!rules_no_koka_dsp) incdecpts(&st[0].whitepts[2], DEC); break;
                case 2: if (!rules_no_koka_dsp) incdecpts(&st[0].whitepts[2], INC);
                    incdecpts(&st[0].whitepts[1], DEC);
                    break;
                case 3: incdecpts(&st[0].whitepts[1], INC); incdecpts(&st[0].whitepts[0], DEC); break;
                case 4: incdecpts(&st[0].whitepts[0], DEC);
                }
            } else if (st[0].bluepts[3] == 4) {
                st[0].whitepts[0] &= ~2;
            }
            incdecpts(&st[0].bluepts[3], DEC);
            log_scores("Cancel shido to ", BLUE);
        } else {
            incdecpts(&st[0].bluepts[3], INC);
            if (rule_eq_score_less_shido_wins == FALSE) { 
                switch (st[0].bluepts[3]) {
                case 1: if (!rules_no_koka_dsp) incdecpts(&st[0].whitepts[2], INC); break;
                case 2: if (!rules_no_koka_dsp) incdecpts(&st[0].whitepts[2], DEC);
                    incdecpts(&st[0].whitepts[1], INC);
                    break;
                case 3: incdecpts(&st[0].whitepts[1], DEC); incdecpts(&st[0].whitepts[0], INC); break;
                case 4: incdecpts(&st[0].whitepts[0], INC);
                }
            } else if (st[0].bluepts[3] >= 4) {
                st[0].whitepts[0] |= 2;
            }
            log_scores("Shido to ", BLUE);
        }
        break;
    default:
        ;
    }

    /* check for shido amount of points */
    if (st[0].bluepts[3] > 4)
        st[0].bluepts[3] = 4;
    if (rule_eq_score_less_shido_wins == FALSE) { 
        if (st[0].bluepts[3] == 4 && (st[0].whitepts[0] & 2) == 0)
            st[0].whitepts[0] |= 2;
        else if (st[0].bluepts[3] == 3 && st[0].whitepts[0] == 0)
            st[0].whitepts[0] = 1;
        else if (st[0].bluepts[3] == 2 && st[0].whitepts[1] == 0)
            st[0].whitepts[1] = 1;
        else if (st[0].bluepts[3] == 1 && st[0].whitepts[2] == 0 && !rules_no_koka_dsp)
            st[0].whitepts[2] = 1;
    }    

    if (st[0].whitepts[3] > 4)
        st[0].whitepts[3] = 4;
    if (rule_eq_score_less_shido_wins == FALSE) { 
        if (st[0].whitepts[3] == 4 && (st[0].bluepts[0] & 2) == 0)
            st[0].bluepts[0] |= 2;
        else if (st[0].whitepts[3] == 3 && st[0].bluepts[0] == 0)
            st[0].bluepts[0] = 1;
        else if (st[0].whitepts[3] == 2 && st[0].bluepts[1] == 0)
            st[0].bluepts[1] = 1;
        else if (st[0].whitepts[3] == 1 && st[0].bluepts[2] == 0 && !rules_no_koka_dsp)
            st[0].bluepts[2] = 1;
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

    if (st[0].running) {
        if (st[0].bluepts[0] == 0 && st[0].whitepts[0] == 0) {
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

    if (wait_for_new_match && st[0].running == 0 &&
        last_category == current_category &&
        last_match == current_match &&
        now < wait_set_time + 100)
        return;

    wait_for_new_match = FALSE;

    if (st[0].running == 0 && st[0].oRunning == 0) {
        if (st[0].action_on) {
            last_category = current_category;
            last_match = current_match;
            wait_for_new_match = TRUE;
            wait_set_time = now;
            key = GDK_0;
            //last_time += 20;
        } else
            key = GDK_space;
    } else if (st[0].oRunning) {
        if (now % 10 == 0)
            key = GDK_Return;
    } else if (st[0].stackdepth) {
        key = (rand()&1) ? GDK_Up : GDK_Down;
    } else if ((st[0].bluepts[0] & 2 || st[0].whitepts[0] & 2) &&
               st[0].running) {
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

        guint t = (total > 0.0) ? (total - st[0].elap) : st[0].elap;
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
}
