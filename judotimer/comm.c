/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#if defined(__WIN32__) || defined(WIN32)

#define  __USE_W32_SOCKETS
//#define Win32_Winsock

#include <windows.h>
#include <stdio.h>
#include <initguid.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#else /* UNIX */

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

#include "judotimer.h"
#include "comm.h"

/* System-dependent definitions */
#ifndef WIN32
#define closesocket     close
#define SOCKET  int
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1
#define SOCKADDR_BTH struct sockaddr_rc
#define AF_BTH PF_BLUETOOTH
#define BTHPROTO_RFCOMM BTPROTO_RFCOMM
#endif

void send_packet(struct message *msg);

int current_category;
int current_match;
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
#if 0
        if (demo)
            while(time(NULL) % 20 != 0)
                ;
#endif
        send_packet(&msgout);
        result_send_time = time(NULL);
    }

#if 0
    show_message("", "", "", "", "", "");
    current_category = 0;
    current_match = 0;
#endif
}

gint timeout_callback(gpointer data)
{
    struct message msg;
    extern gint my_address;

    msg.type = MSG_DUMMY;
    msg.sender = my_address;
    msg.u.dummy.application_type = application_type();
    msg.u.dummy.tatami = tatami;
    send_packet(&msg);

    return TRUE;
}

gboolean msg_accepted(struct message *m)
{
    if (m->type == MSG_UPDATE_LABEL && mode != MODE_SLAVE)
        return FALSE;

    switch (m->type) {
    case MSG_NEXT_MATCH:
    case MSG_UPDATE_LABEL:
    case MSG_ACK:
        return TRUE;
    }
    return FALSE;
}

void msg_received(struct message *input_msg)
{
/**
   if (input_msg->sender < 10)
   return;
**/
#if 0
    g_print("msg type = %d from %d\n", 
            input_msg->type, input_msg->sender);
#endif
    switch (input_msg->type) {
    case MSG_NEXT_MATCH:
#if 0
        /* show ad as a slave */
        if (mode == MODE_SLAVE &&
            input_msg->u.next_match.tatami == tatami &&
            input_msg->sender >= 10 &&
            (input_msg->u.next_match.category != last_category ||
             input_msg->u.next_match.match != last_match)) {
            last_category = input_msg->u.next_match.category;
            last_match = input_msg->u.next_match.match;
            display_ad_window();
            display_comp_window(input_msg->u.next_match.cat_1,
                                input_msg->u.next_match.blue_1,
                                input_msg->u.next_match.white_1);
        }
#endif
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

            struct message msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = MSG_UPDATE_LABEL;
            msg.u.update_label.label_num = START_COMPETITORS;
            strncpy(msg.u.update_label.text, input_msg->u.next_match.blue_1,
                    sizeof(msg.u.update_label.text)-1);
            strncpy(msg.u.update_label.text2, input_msg->u.next_match.white_1,
                    sizeof(msg.u.update_label.text2)-1);
            strncpy(msg.u.update_label.text3, input_msg->u.next_match.cat_1,
                    sizeof(msg.u.update_label.text3)-1);

            /*write_tv_logo(&(msg.u.update_label));*/

            if (mode != MODE_SLAVE)
                send_label_msg(&msg);
        }

        current_category = input_msg->u.next_match.category;
        current_match = input_msg->u.next_match.match;

        if (result_send_time) {
            if (current_category != msgout.u.next_match.category ||
                current_match != msgout.u.next_match.match) {
                result_send_time = 0;
            } else if (time(NULL) - result_send_time > 14) {
                send_packet(&msgout);
                result_send_time = time(NULL);

                /*judotimer_log("Resend result %d:%d", 
                  current_category, current_match);*/
                g_print("resend result %d:%d\n", current_category, current_match);
            }
        }

        break;

    case MSG_UPDATE_LABEL:
        if (mode != MODE_SLAVE 
            /*|| input_msg->sender != tatami*/)
            return;
        update_label(&input_msg->u.update_label);
        break;

    case MSG_ACK:
        if (input_msg->sender < 10 ||
            input_msg->u.ack.tatami != tatami)
            return;
        break;
    }
}


void send_packet(struct message *msg)
{
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

    msg_to_queue(msg);
}

#define NUM_CONNECTIONS 4
static struct {
    guint fd;
    gulong addr;
    struct message m;
    gint ri;
    gboolean escape;
} connections[NUM_CONNECTIONS];

static volatile struct message message_queue[MSG_QUEUE_LEN];
static volatile gint msg_put = 0, msg_get = 0;
G_LOCK_DEFINE_STATIC(msg_mutex);

void send_label_msg(struct message *msg)
{
    msg->sender = tatami;
    G_LOCK(msg_mutex);
    message_queue[msg_put++] = *msg;
    if (msg_put >= MSG_QUEUE_LEN)
        msg_put = 0;

    if (msg_put == msg_get)
        g_print("MASTER MSG QUEUE FULL!\n");
    G_UNLOCK(msg_mutex);
}

gpointer master_thread(gpointer args)
{
    SOCKET node_fd, tmp_fd;
    socklen_t alen;
    struct sockaddr_in my_addr, caller;
    gint reuse = 1;
    fd_set read_fd, fds;

    while (mode == MODE_SLAVE)
        g_usleep(1000000);

    if ((node_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        perror("serv socket");
        g_thread_exit(NULL);    /* not required just good pratice */
        return NULL;
    }

    if (setsockopt(node_fd, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse)) < 0) {
        perror("setsockopt (SO_REUSEADDR)");
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(SHIAI_PORT+2);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(node_fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
        perror("master bind");
        g_print("CANNOT BIND!\n");
        g_thread_exit(NULL);    /* not required just good pratice */
        return NULL;
    }

    listen(node_fd, 5);

    FD_ZERO(&read_fd);
    FD_SET(node_fd, &read_fd);

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        struct timeval timeout;
        gint r, i;
                
        fds = read_fd;
        timeout.tv_sec = 0;
        timeout.tv_usec = 10000;

        r = select(32, &fds, NULL, NULL, &timeout);

        /* messages to send */

        G_LOCK(msg_mutex);
        if (msg_get != msg_put) {
            for (i = 0; i < NUM_CONNECTIONS; i++) {
                if (connections[i].fd == 0)
                    continue;

                if (send_msg(connections[i].fd, (struct message *)&message_queue[msg_get]) < 0) {
                    perror("sendto");
                    g_print("Node cannot send: conn=%d fd=%d\n", i, connections[i].fd);
                }
            }

            msg_get++;
            if (msg_get >= MSG_QUEUE_LEN)
                msg_get = 0;
        }
        G_UNLOCK(msg_mutex);

        if (r <= 0)
            continue;

        /* messages to receive */
        if (FD_ISSET(node_fd, &fds)) {
            alen = sizeof(caller);
            if ((tmp_fd = accept(node_fd, (struct sockaddr *)&caller, &alen)) < 0) {
                perror("serv accept");
                continue;
            }
#if 0
            const int nodelayflag = 1;
            if (setsockopt(tmp_fd, IPPROTO_TCP, TCP_NODELAY, 
                           (const void *)&nodelayflag, sizeof(nodelayflag))) {
                g_print("CANNOT SET TCP_NODELAY (2)\n");
            }
#endif
            for (i = 0; i < NUM_CONNECTIONS; i++)
                if (connections[i].fd == 0)
                    break;

            if (i >= NUM_CONNECTIONS) {
                g_print("Master cannot accept new connections!\n");
                closesocket(tmp_fd);
                continue;
            }

            connections[i].fd = tmp_fd;
            connections[i].addr = caller.sin_addr.s_addr;
            g_print("Master: new connection[%d]: fd=%d addr=%s\n", 
                    i, tmp_fd, inet_ntoa(caller.sin_addr));
            FD_SET(tmp_fd, &read_fd);
        }

        for (i = 0; i < NUM_CONNECTIONS; i++) {
            static guchar inbuf[2000];
			
            if (connections[i].fd == 0)
                continue;

            if (!(FD_ISSET(connections[i].fd, &fds)))
                continue;

            r = recv(connections[i].fd, (char *)inbuf, sizeof(inbuf), 0);
            if (r <= 0) {
                g_print("Master: connection %d fd=%d closed\n", i, connections[i].fd);
                closesocket(connections[i].fd);
                FD_CLR(connections[i].fd, &read_fd);
                connections[i].fd = 0;
            }
        }
    }

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

gboolean keep_connection(void)
{
    static gint old_mode = 0;

    if (mode != old_mode) {
        old_mode = mode;
        return FALSE;
    }
		
    return TRUE;
}

gint get_port(void)
{
    if (mode == MODE_SLAVE)
        return SHIAI_PORT+2;
    else
        return SHIAI_PORT;
}
