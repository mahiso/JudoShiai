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
#include <ws2tcpip.h>

#else /* UNIX */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/tcp.h>
#include <fcntl.h>

#endif /* WIN32 */

#ifdef ENABLE_NLS
#include <libintl.h>
#define _(String) gettext(String)
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#else /* NLS is disabled */
#define _(String) (String)
#define N_(String) (String)
#define textdomain(String) (String)
#define gettext(String) (String)
#define dgettext(Domain,String) (String)
#define dcgettext(Domain,String,Type) (String)
#define bindtextdomain(Domain,Directory) (Domain) 
#define bind_textdomain_codeset(Domain,Codeset) (Codeset) 
#endif /* ENABLE_NLS */

/* System-dependent definitions */
#ifndef WIN32
#define closesocket     close
#define SOCKET          gint
#define INVALID_SOCKET  -1
#define SOCKET_ERROR    -1
#define SOCKADDR_BTH    struct sockaddr_rc
#define AF_BTH          PF_BLUETOOTH
#define BTHPROTO_RFCOMM BTPROTO_RFCOMM
#endif

#include <gtk/gtk.h>

#define V_WIDTH (1280/8)
#define V_HEIGHT (720/8)
#define V_ROWSTRIDE (3*V_WIDTH)
#define V_INSIZE (V_WIDTH*V_HEIGHT)
#define V_SIZE (V_ROWSTRIDE*V_HEIGHT)

static gchar video_buffer[V_SIZE];

GtkWindow *video_window = NULL;

static gboolean expose_video(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
    static GdkPixbuf *pb;

    pb = gdk_pixbuf_new_from_data((const guchar *)video_buffer, GDK_COLORSPACE_RGB, FALSE, 8, 
                                  V_WIDTH, V_HEIGHT, V_ROWSTRIDE, NULL, NULL);

    if (!pb)
        return FALSE;

#if (GTKVER == 3)
    cairo_t *c = (cairo_t *)event;
#else
    cairo_t *c = gdk_cairo_create(gtk_widget_get_window(widget));
#endif
    gdk_cairo_set_source_pixbuf(c, pb, 0, 0);
    cairo_paint(c);

    g_object_unref(pb);
#if (GTKVER != 3)
    cairo_show_page(c);
    cairo_destroy(c);
#endif
    return FALSE;
}

static struct sockaddr_in node;

static gboolean refresh_video(gpointer data)
{
    SOCKET fd;
    static gchar inbuf[V_INSIZE];
    gint len, s, d;

    if (video_window == NULL)
        return FALSE;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        return TRUE;
    }

    if (connect(fd, (struct sockaddr *)&node, sizeof(node))) {
        closesocket(fd);
        return TRUE;
    }

    len = recv(fd, inbuf, sizeof(inbuf), 0);
    len = len;

    closesocket(fd);

    for (s = 0, d = 0; s < V_INSIZE; s++, d += 3) {
        video_buffer[d] = video_buffer[d+1] = video_buffer[d+2] = inbuf[s];
    }

    GtkWidget *widget = GTK_WIDGET(data);

    if (gtk_widget_get_window(widget)) {
        gdk_window_invalidate_rect(gtk_widget_get_window(widget), NULL, TRUE);
        gdk_window_process_updates(gtk_widget_get_window(widget), TRUE);
    }

    return TRUE;
}

static gboolean delete_event_video( GtkWidget *widget, GdkEvent  *event, gpointer   data )
{
    return FALSE;
}

static void destroy_video( GtkWidget *widget, gpointer   data )
{
    video_window = NULL;
}

void create_video_window(struct sockaddr_in src)
{
    static GtkWindow *window;
    GtkWidget *player_widget;

    memset((gchar *)&node, 0, sizeof(node));
    node.sin_family = AF_INET;
    node.sin_port = htons(2238);
    node.sin_addr.s_addr = src.sin_addr.s_addr;

    window = video_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(GTK_WINDOW(window), _("Show Video"));
    gtk_widget_set_size_request(GTK_WIDGET(window), V_WIDTH, V_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    //setup player widget
    player_widget = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(window), player_widget);

#if (GTKVER == 3)
    g_signal_connect(G_OBJECT(player_widget), 
                     "draw", G_CALLBACK(expose_video), NULL);
#else
    g_signal_connect(G_OBJECT(player_widget), 
                     "expose-event", G_CALLBACK(expose_video), NULL);
#endif
    gtk_widget_show_all(GTK_WIDGET(window));

    g_signal_connect (G_OBJECT(window), "delete_event",
                      G_CALLBACK (delete_event_video), NULL);
    g_signal_connect(G_OBJECT(window), "destroy", 
                     G_CALLBACK(destroy_video), NULL);

    g_timeout_add(200, refresh_video, window);
}

