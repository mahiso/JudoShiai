/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "judotimer.h"
#include "comm.h"

void send_packet(struct message *msg);

int current_category;
int current_match;
int current_gs_time;
time_t traffic_last_rec_time;
static struct message msgout;
static time_t result_send_time;

void copy_packet(struct message *msg)
{
    /* dummy function to make linker happy (defined in comm.c) */
}

int array2int(int pts[4])
{
    int x = 0;

    if (pts[0] >= 2)
        x = 0x10000;
    if (pts[0] & 1)
        x |= 0x01000;
    x |= (pts[1] << 8) | (pts[2] << 4) | pts[3];

    return x;
}

void cancel_rest_time(gboolean blue, gboolean white)
{
    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_CANCEL_REST_TIME;
    msg.u.cancel_rest_time.category = current_category;
    msg.u.cancel_rest_time.number = current_match;
    msg.u.cancel_rest_time.blue = blue;
    msg.u.cancel_rest_time.white = white;
    send_packet(&msg);
    g_print("timer: cancel blue=%d white=%d\n", blue, white);

    judotimer_log("No rest time for %d:%d %s %s",
                  current_category, current_match,
                  blue ? "Blue" : "", white ? "White" : "");
}

void send_result(int bluepts[4], int whitepts[4], char blue_vote, char white_vote,
		 char blue_hansokumake, char white_hansokumake, gint legend, gint hikiwake)
{
    memset(&msgout, 0, sizeof(msgout));

    if (current_category < 10000 || current_match >= 1000) {
        return;
    }
    msgout.type = MSG_RESULT;
    msgout.u.result.tatami = tatami;
    msgout.u.result.category = current_category;
    msgout.u.result.match = current_match;
    msgout.u.result.minutes = get_match_time();
    msgout.u.result.legend = legend;

    msgout.u.result.blue_score = array2int(bluepts);
    msgout.u.result.white_score = array2int(whitepts);
    msgout.u.result.blue_vote = hikiwake ? 1 : blue_vote;
    msgout.u.result.white_vote = hikiwake ? 1 : white_vote;
    msgout.u.result.blue_hansokumake = blue_hansokumake;
    msgout.u.result.white_hansokumake = white_hansokumake;

    if (msgout.u.result.blue_score != msgout.u.result.white_score ||
        msgout.u.result.blue_vote != msgout.u.result.white_vote || hikiwake ||
        msgout.u.result.blue_hansokumake || msgout.u.result.white_hansokumake) {
        send_packet(&msgout);
        result_send_time = time(NULL);
    }

#if 0
    show_message("", "", "", "", "", "");
    current_category = 0;
    current_match = 0;
#endif
}

void msg_received(struct message *input_msg)
{
    static time_t last_get = 0;
#if 0
    g_print("msg type = %d from %d\n",
            input_msg->type, input_msg->sender);
#endif
    switch (input_msg->type) {
    case MSG_NEXT_MATCH:
        if (input_msg->sender < 10 ||
            input_msg->u.next_match.tatami != tatami ||
            mode == MODE_SLAVE)
            return;

        if (clock_running() && demo < 2)
            return;

        traffic_last_rec_time = time(NULL);

        show_message(input_msg->u.next_match.cat_1,
                     input_msg->u.next_match.blue_1,
                     input_msg->u.next_match.white_1,
                     input_msg->u.next_match.cat_2,
                     input_msg->u.next_match.blue_2,
                     input_msg->u.next_match.white_2,
                     input_msg->u.next_match.flags);

        //g_print("minutes=%d auto=%d\n", input_msg->u.next_match.minutes, automatic);
        if (input_msg->u.next_match.minutes && automatic)
            reset(GDK_0, &input_msg->u.next_match);

        if (golden_score)
            set_comment_text(_("Golden Score"));

        if (current_category != input_msg->u.next_match.category ||
            current_match != input_msg->u.next_match.match) {
            /***
            g_print("current=%d/%d new=%d/%d\n",
                    current_category, current_match,
                    input_msg->u.next_match.category, input_msg->u.next_match.match);
            ***/
            display_comp_window(saved_cat, saved_last1, saved_last2,
                                saved_first1, saved_first2,
                                saved_country1, saved_country2);
	    current_category = input_msg->u.next_match.category;
	    current_match = input_msg->u.next_match.match;
	    current_gs_time = input_msg->u.next_match.gs_time;
        } else if (time(NULL) > last_get + 3) {
	    char buf[16];
	    snprintf(buf, sizeof(buf), "/nextmatch?t=%d", tatami);
	    emscripten_async_wget_data(buf, NULL, nextmatchonload, nextmatchonerror);
	    last_get = time(NULL);
	    g_print("send2 %s\n", buf);
	}

#if 0
	printf("old=%d:%d new=%d:%d\n",
	       current_category, current_match,
	       input_msg->u.next_match.category,
	       input_msg->u.next_match.match);

        current_category = input_msg->u.next_match.category;
        current_match = input_msg->u.next_match.match;

        if (result_send_time) {
            if (current_category != msgout.u.next_match.category ||
                current_match != msgout.u.next_match.match) {
                result_send_time = 0;
            } else if (time(NULL) - result_send_time > 14) {
		char buf[32];
		snprintf(buf, sizeof(buf), "/nextmatch?t=%d", tatami);
		emscripten_async_wget_data(buf, NULL, nextmatchonload, nextmatchonerror);
#if 0
                send_packet(&msgout);
                result_send_time = time(NULL);
#endif
                /*judotimer_log("Resend result %d:%d",
                  current_category, current_match);*/
                printf("resend result %d:%d\n", current_category, current_match);
            }
        }
#endif

        break;
    }
}

void resonload(void *arg, void *buf, int len)
{
    result_send_time = 0;
}

void resonerror(void *a)
{
    printf("resonerror %p\n", a);
}

void send_packet(struct message *msg)
{
    char buf[64];

    msg->sender = tatami;

#if 0
    if (msg->type == MSG_RESULT) {
        printf("tatami=%d cat=%d match=%d min=%d blue=%x white=%x bv=%d wv=%d\n",
               msg->u.result.tatami,
               msg->u.result.category,
               msg->u.result.match,
               msg->u.result.minutes,
               msg->u.result.blue_score,
               msg->u.result.white_score,
               msg->u.result.blue_vote,
               msg->u.result.white_vote);
    }
#endif

    if (msg->type == MSG_RESULT) {
	snprintf(buf, sizeof(buf),
		 "matchresult?t=%d&c=%d&m=%d&min=%d&s1=%d&s2=%d&v1=%d&v2=%d&h1=%d&h2=%d&l=%d\r\n",
		 msg->u.result.tatami,
		 msg->u.result.category,
		 msg->u.result.match,
		 msg->u.result.minutes,
		 msg->u.result.blue_score,
		 msg->u.result.white_score,
		 msg->u.result.blue_vote,
		 msg->u.result.white_vote,
		 msg->u.result.blue_hansokumake,
		 msg->u.result.white_hansokumake,
		 msg->u.result.legend);

	emscripten_async_wget_data(buf, NULL, resonload, resonerror);
    } else if (msg->type == MSG_CANCEL_REST_TIME) {
	snprintf(buf, sizeof(buf),
		 "restcancel?c=%d&m=%d&b=%d&w=%d\r\n",
		 msg->u.cancel_rest_time.category,
		 msg->u.cancel_rest_time.number,
		 msg->u.cancel_rest_time.blue,
		 msg->u.cancel_rest_time.white);

	emscripten_async_wget_data(buf, NULL, resonload, resonerror);
    }
}

void nextmatchonload(void *arg, void *buf, int len)
{
    char *b = buf;
    struct message msg;
    struct msg_next_match *nm = &msg.u.next_match;
    int n;
    int ippon, wazaari, yuko, koka;

    g_print("recv: %s\n", b);
    memset(nm, 0, sizeof(*nm));
    msg.type = MSG_NEXT_MATCH;
    msg.sender = 1000;

    n = sscanf(b, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
	  &nm->tatami, &nm->category, &nm->match,  &nm->minutes, &nm->match_time,
	  &nm->gs_time, &nm->rep_time, &nm->rest_time, &ippon,
	  &wazaari, &yuko, &koka,
	  &nm->flags);

    nm->pin_time_ippon = ippon;
    nm->pin_time_wazaari = wazaari;
    nm->pin_time_yuko = yuko;
    nm->pin_time_koka = koka;

    b = strchr(b, '\n');
    if (b) b++;
    else return;

    n = 0;
    while (*b == '\r' || *b == '\n') b++;
    while (*b && *b != '\r' && *b != '\n' && n < sizeof(nm->cat_1)-1)
	nm->cat_1[n++] = *b++;
    nm->cat_1[n] = 0;

    n = 0;
    while (*b == '\r' || *b == '\n') b++;
    while (*b && *b != '\r' && *b != '\n' && n < sizeof(nm->blue_1)-1)
	nm->blue_1[n++] = *b++;
    nm->blue_1[n] = 0;

    n = 0;
    while (*b == '\r' || *b == '\n') b++;
    while (*b && *b != '\r' && *b != '\n' && n < sizeof(nm->white_1)-1)
	nm->white_1[n++] = *b++;
    nm->white_1[n] = 0;

    n = 0;
    while (*b == '\r' || *b == '\n') b++;
    while (*b && *b != '\r' && *b != '\n' && n < sizeof(nm->cat_2)-1)
	nm->cat_2[n++] = *b++;
    nm->cat_2[n] = 0;

    n = 0;
    while (*b == '\r' || *b == '\n') b++;
    while (*b && *b != '\r' && *b != '\n' && n < sizeof(nm->blue_2)-1)
	nm->blue_2[n++] = *b++;
    nm->blue_2[n] = 0;

    n = 0;
    while (*b == '\r' || *b == '\n') b++;
    while (*b && *b != '\r' && *b != '\n' && n < sizeof(nm->white_2)-1)
	nm->white_2[n++] = *b++;
    nm->white_2[n] = 0;

    msg_received(&msg);
    //refresh_window();
}

void nextmatchonerror(void *a)
{
    printf("nextmatch onerror line=%d %p\n", __LINE__, a);
}
