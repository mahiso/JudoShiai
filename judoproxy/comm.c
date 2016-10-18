/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#if defined(__WIN32__) || defined(WIN32)

#define  __USE_W32_SOCKETS
//#define Win32_Winsock

#include <windows.h>
#include <stdio.h>
#include <initguid.h>
#include <winsock2.h>

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
#include <gdk/gdkkeysyms.h>
#ifdef WIN32
//#include <glib/gwin32.h>
#endif

#include "judoproxy.h"
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

time_t traffic_last_rec_time;

void copy_packet(struct message *msg)
{
    /* dummy function to make linker happy (defined in comm.c) */
}

gint timeout_callback(gpointer data)
{
    struct message msg;
    extern gint my_address;

    msg.type = MSG_DUMMY;
    msg.sender = my_address;
    msg.u.dummy.application_type = application_type();
    send_packet(&msg);

    return TRUE;
}

#define ASK_TABLE_LEN 1024
static gint ask_table[ASK_TABLE_LEN];
static gint put_ptr = 0, get_ptr = 0;

gint timeout_ask_for_data(gpointer data)
{
    struct message msg;
    extern gint my_address;

    if (get_ptr != put_ptr) {
        msg.type = MSG_NAME_REQ;
        msg.sender = my_address;
        msg.u.name_req.index = ask_table[get_ptr++];
        send_packet(&msg);
		
        if (get_ptr >= ASK_TABLE_LEN)
            get_ptr = 0;
    }
    return TRUE;
}
#if 0
static void ask_for_data(gint index)
{
    ask_table[put_ptr++] = index;
    if (put_ptr >= ASK_TABLE_LEN)
        put_ptr = 0;
}
#endif
gboolean msg_accepted(struct message *m)
{
    switch (m->type) {
    case MSG_MATCH_INFO:
    case MSG_NAME_INFO:
    case MSG_CANCEL_REST_TIME:
    case MSG_EDIT_COMPETITOR:
        return TRUE;
    }
    return FALSE;
}

void msg_received(struct message *input_msg)
{
    if (input_msg->sender < 10)
        return;

    traffic_last_rec_time = time(NULL);
#if 0
    g_print("msg type = %d from %d\n", 
            input_msg->type, input_msg->sender);
#endif
    switch (input_msg->type) {
    }
}


void send_packet(struct message *msg)
{
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

gboolean keep_connection(void)
{
    return TRUE;
}

gint get_port(void)
{
    return SHIAI_PORT;
}
