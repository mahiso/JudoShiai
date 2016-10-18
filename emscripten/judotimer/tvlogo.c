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
#include <sys/time.h>

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
#include <librsvg/rsvg.h>

#include "judotimer.h"

#define NUM_LOGO_FILES 4
gchar *tvlogofile[NUM_LOGO_FILES];
gint tvlogo_update = 0;
gboolean vlc_connection_ok = FALSE;
static gchar  vlc_host[128] = {0};
guint  vlc_port = 0, tvlogo_port = 0;
gint   tvlogo_x = 10;
gint   tvlogo_y = 10;

G_LOCK_DEFINE(logofile);

gpointer tvlogo_thread(gpointer args)
{
    SOCKET comm_fd;
    gint n, i;
    struct sockaddr_in node;
    static gchar buffer[512];
    fd_set read_fd, fds;
    guint  current_port = 0;
    glong vlc_addr = 0;
    gint waittime = 1000000;

    g_print("User data dir = %s\n", g_get_user_data_dir());

    strcpy(vlc_host, "localhost");

    for (i = 0; i < NUM_LOGO_FILES; i++)
        tvlogofile[i] = NULL;
    
    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        gint last_tatami = tatami;
        tvlogo_update = FALSE;

        G_LOCK(logofile);
        for (i = 0; i < NUM_LOGO_FILES; i++) {
            snprintf(buffer, sizeof(buffer), "tvlogo%d-%d.png", tatami, i);
            g_free(tvlogofile[i]);
            tvlogofile[i] = g_build_filename(g_get_user_data_dir(), buffer, NULL);
        }
        G_UNLOCK(logofile);

        do {
            g_usleep(1000000);
            if (vlc_host[0] && vlc_port) {
                vlc_addr = hostname_to_addr(vlc_host);
                current_port = vlc_port;
            }
        } while (vlc_addr == 0 || current_port == 0 || vlc_port == 0);

        if ((comm_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            perror("vlc socket");
            g_print("CANNOT CREATE VLC SOCKET (%s:%d)!\n", __FUNCTION__, __LINE__);
            g_thread_exit(NULL);    /* not required just good pratice */
            return NULL;
        }

        memset(&node, 0, sizeof(node));
        node.sin_family      = AF_INET;
        node.sin_port        = htons(current_port);
        node.sin_addr.s_addr = vlc_addr;

        if (connect(comm_fd, (struct sockaddr *)&node, sizeof(node))) {
            closesocket(comm_fd);
            g_usleep(1000000);
            continue;
        }

        g_print("VLC connection OK.\n");
        vlc_connection_ok = TRUE;

        FD_ZERO(&read_fd);
        FD_SET(comm_fd, &read_fd);

        while (current_port == vlc_port && tatami == last_tatami) {
            static gint lastx = 0, lasty = 0, last_update = 0;
            struct timeval timeout;
            gint r;
			
            fds = read_fd;
            timeout.tv_sec = 0;
            timeout.tv_usec = waittime;

            r = select(comm_fd+1, &fds, NULL, NULL, &timeout);

            G_LOCK(logofile);
            if (r == 0 && tvlogo_update != last_update) {
                last_update = tvlogo_update;
                G_UNLOCK(logofile);
                waittime = 5000000; // don't send again before reply
                gint n = snprintf(buffer, sizeof(buffer), "@name logo-file %s\n", 
                                  tvlogofile[last_update]);
                send(comm_fd, buffer, n, 0);
                //g_print("TX: %s\n", buffer);
            } else 
                G_UNLOCK(logofile);
    
            if (r == 0 && (lastx != tvlogo_x || lasty != tvlogo_y)) {
                n = snprintf(buffer, sizeof(buffer), "@name logo-x %d\n", tvlogo_x);
                send(comm_fd, buffer, n, 0);
                n = snprintf(buffer, sizeof(buffer), "@name logo-y %d\n", tvlogo_y);
                send(comm_fd, buffer, n, 0);
                lastx = tvlogo_x; lasty = tvlogo_y;
            }

            if (r <= 0)
                continue;

            if (FD_ISSET(comm_fd, &fds)) {
                n = recv(comm_fd, buffer, sizeof(buffer), 0);
                waittime = 1000000;
                if (n <= 0)
                    break;
                //g_print("\nVLC reply: '%s'\n", buffer);
            } // if (FD_ISSET(comm_fd, &fds))
        } // while (current_port == vlc_port)

        vlc_connection_ok = FALSE;
        closesocket(comm_fd);

        g_print("VLC connection NOK.\n");
    }

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

struct vlc_settings {
    GtkWidget *port, *scale, *x, *y, *srvport;
};

static void vlc_settings_callback(GtkWidget *widget, 
                                  GdkEvent *event,
                                  GtkWidget *data)
{
    struct vlc_settings *vlc = (struct vlc_settings *)data;

    if (ptr_to_gint(event) == GTK_RESPONSE_OK) {
        vlc_port = atoi(gtk_entry_get_text(GTK_ENTRY(vlc->port)));
        g_key_file_set_integer(keyfile, "preferences", "vlcport", vlc_port);

        tvlogo_scale = gtk_spin_button_get_value(GTK_SPIN_BUTTON(vlc->scale));
        g_key_file_set_double(keyfile, "preferences", "tvlogoscale", tvlogo_scale);

        tvlogo_x = atoi(gtk_entry_get_text(GTK_ENTRY(vlc->x)));
        g_key_file_set_integer(keyfile, "preferences", "tvlogox", tvlogo_x);

        tvlogo_y = atoi(gtk_entry_get_text(GTK_ENTRY(vlc->y)));
        g_key_file_set_integer(keyfile, "preferences", "tvlogoy", tvlogo_y);

        tvlogo_port = atoi(gtk_entry_get_text(GTK_ENTRY(vlc->srvport)));
        g_key_file_set_integer(keyfile, "preferences", "tvlogoport", tvlogo_port);
    }

    g_free(vlc);
    gtk_widget_destroy(widget);
}

void ask_tvlogo_settings( GtkWidget *w,
                          gpointer   data )
{
    gchar buf[16];
    GtkWidget *dialog, *hbox, *hbox1, *hbox2, /* *hbox3,*/ *label;
    struct vlc_settings *vlc = g_malloc0(sizeof(*vlc));

    dialog = gtk_dialog_new_with_buttons (_("VLC control"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          NULL);

    sprintf(buf, "%d", vlc_port); 
    vlc->port = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(vlc->port), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(vlc->port), 4);

    vlc->scale = gtk_spin_button_new_with_range(0.3, 3.0, 0.1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(vlc->scale), tvlogo_scale);

    sprintf(buf, "%d", tvlogo_x); 
    vlc->x = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(vlc->x), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(vlc->x), 4);

    sprintf(buf, "%d", tvlogo_y); 
    vlc->y = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(vlc->y), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(vlc->y), 4);

#if (GTKVER == 3)
    hbox = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox), gtk_label_new("VLC port:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), vlc->port,                  1, 0, 1, 1);

    hbox1 = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox1), gtk_label_new(_("X:")), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox1), vlc->x,                 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox1), gtk_label_new(_("Y:")), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox1), vlc->y,                 3, 0, 1, 1);

    hbox2 = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox2), gtk_label_new(_("Scale:")), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox2), vlc->scale,                 1, 0, 1, 1);
#else
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("VLC port:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vlc->port, FALSE, FALSE, 0);

    hbox1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox1), gtk_label_new(_("X:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), vlc->x, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox1), gtk_label_new(_("Y:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), vlc->y, FALSE, FALSE, 5);

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox2), gtk_label_new(_("Scale:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), vlc->scale, FALSE, FALSE, 5);
#endif

    if (vlc_connection_ok)
        label = gtk_label_new(_("(Connection OK)"));
    else
        label = gtk_label_new(_("(Connection broken)"));

    sprintf(buf, "%d", tvlogo_port); 
    vlc->srvport = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(vlc->srvport), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(vlc->srvport), 4);

    /*
    hbox3 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox3), gtk_label_new("TV logo port:"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox3), vlc->srvport, FALSE, FALSE, 0);
    */

#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox1, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox2, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       label, FALSE, FALSE, 4);
#else
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox1, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox2, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 4);
    //gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox3, FALSE, FALSE, 4);
#endif

    gtk_widget_show_all(dialog);

    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(vlc_settings_callback), vlc);
}

// Not used. Let VLC do the job.
#if 0

#define NUM_CONNECTIONS 4
static struct {
    guint fd;
    gulong addr;
    gboolean req;
} connections[NUM_CONNECTIONS];

static gchar outbuf[2000];

extern gchar *tvlogo_jpeg;
extern gsize tvlogo_jpeg_len;

gpointer mjpeg_thread(gpointer args)
{
    SOCKET node_fd, tmp_fd;
    guint alen;
    struct sockaddr_in my_addr, caller;
    gint reuse = 1;
    fd_set read_fd, fds;
    static gint last_tvlogo_port;

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        struct timeval timeout, timestamp;
        gint r, i, n;
                
        while (tvlogo_port == 0)
            g_usleep(1000000);

        last_tvlogo_port = tvlogo_port;

        if ((node_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            perror("MJPEG serv socket");
            g_thread_exit(NULL);    /* not required just good pratice */
            return NULL;
        }

        memset(&my_addr, 0, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(tvlogo_port);
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(node_fd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
            perror("Bind");
            g_print("MJPEG: Cannot bind to port %d (%d), fd=%d\n", tvlogo_port, htons(tvlogo_port), node_fd);
            closesocket(node_fd);
            g_usleep(1000000);
            continue;
        }

        listen(node_fd, 5);

        FD_ZERO(&read_fd);
        FD_SET(node_fd, &read_fd);

        while (tvlogo_port == last_tvlogo_port) {
            fds = read_fd;
            timeout.tv_sec = 0;
            timeout.tv_usec = 300000;

            r = select(32, &fds, NULL, NULL, &timeout);

            if (r == 0) {
                if (tvlogo_jpeg && tvlogo_jpeg_len) {
                    gettimeofday(&timestamp, NULL);

                    for (i = 0; i < NUM_CONNECTIONS; i++) {
                        if (connections[i].fd == 0 || connections[i].req == FALSE)
                            continue;
                        n = snprintf(outbuf, sizeof(outbuf), 
                                     "Content-Type: image/jpeg\r\n"
                                     "Content-Length: %d\r\n"
                                     "X-Timestamp: %d.%06d\r\n"
                                     "\r\n", tvlogo_jpeg_len, (int)timestamp.tv_sec, (int)timestamp.tv_usec);
                        if (write(connections[i].fd, outbuf, n) < 0)
                            g_print("MJPEG write error 1\n");

                        if ((n = write(connections[i].fd, tvlogo_jpeg, tvlogo_jpeg_len)) < 0)
                            g_print("MJPEG write error 2\n");

                        n = sprintf(outbuf, "\r\n--7b3cc56e5f51db803f790dad720ed50a\r\n");
                        if (write(connections[i].fd, outbuf, n) < 0)
                            g_print("MJPEG write error 3\n");
                    }
                }
            }

            if (r <= 0)
                continue;

            /* messages to receive */
            if (FD_ISSET(node_fd, &fds)) {
                alen = sizeof(caller);
                if ((tmp_fd = accept(node_fd, (struct sockaddr *)&caller, &alen)) < 0) {
                    perror("serv accept");
                    continue;
                }

                for (i = 0; i < NUM_CONNECTIONS; i++)
                    if (connections[i].fd == 0)
                        break;

                if (i >= NUM_CONNECTIONS) {
                    g_print("MJPEG cannot accept new connections!\n");
                    closesocket(tmp_fd);
                    continue;
                }

                connections[i].fd = tmp_fd;
                connections[i].addr = caller.sin_addr.s_addr;
                connections[i].req = FALSE;
                g_print("MJPEG: new connection[%d]: fd=%d addr=%x\n", 
                        i, tmp_fd, caller.sin_addr.s_addr);
                FD_SET(tmp_fd, &read_fd);
            }

            for (i = 0; i < NUM_CONNECTIONS; i++) {
                static gchar inbuf[2000];
			
                if (connections[i].fd == 0)
                    continue;

                if (!(FD_ISSET(connections[i].fd, &fds)))
                    continue;

                r = recv(connections[i].fd, inbuf, sizeof(inbuf), 0);
                if (r <= 0) {
                    g_print("MJPEG: connection %d fd=%d closed\n", i, connections[i].fd);
                    closesocket(connections[i].fd);
                    FD_CLR(connections[i].fd, &read_fd);
                    connections[i].fd = 0;
                } else {
                    if (strstr(inbuf, "\r\n\r\n")) {
                        //g_print("MJPEG Req: %s\n", inbuf);
                        connections[i].req = TRUE;
                        n = snprintf(outbuf, sizeof(outbuf), 
                                     "HTTP/1.0 200 OK\r\n"
                                     "Connection: close\r\n"
                                     "Server: JudoTimer\r\n"
                                     "Cache-Control: no-store, no-cache, must-revalidate, pre-check=0, post-check=0, max-age=0\r\n"
                                     "Pragma: no-cache\r\n"
                                     "Expires: Mon, 3 Jan 2000 12:34:56 GMT\r\n"
                                     "Content-Type: multipart/x-mixed-replace;boundary=7b3cc56e5f51db803f790dad720ed50a\r\n"
                                     "\r\n"
                                     "--7b3cc56e5f51db803f790dad720ed50a\r\n");
                        if (write(connections[i].fd, outbuf, n) < 0)
                            g_print("MJPEG write error 4\n");
                    }
                }
            }
        } // while port == last_port

        for (i = 0; i < NUM_CONNECTIONS; i++) {
            if (connections[i].fd > 0) {
                closesocket(connections[i].fd);
                FD_CLR(connections[i].fd, &read_fd);
                connections[i].fd = 0;
            }
        }
        closesocket(node_fd);
    } // eternal for

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}


static void copy_pixbuf(GdkPixbuf *pixbuf1, GdkPixbuf *pixbuf2)
{
    GdkPixbuf *pixbuf;
    gint width, height, rowstride;
    gint width1, height1, rowstride1, n_channels1;
    gint width2, height2, rowstride2, n_channels2;
    guchar *pixels, *pixels1, *pixels2, *p, *p1, *p2;
    gint x, y;

    n_channels1 = gdk_pixbuf_get_n_channels(pixbuf1);
    n_channels2 = gdk_pixbuf_get_n_channels(pixbuf2);
    width1 = gdk_pixbuf_get_width (pixbuf1);
    width2 = gdk_pixbuf_get_width (pixbuf2);
    height1 = gdk_pixbuf_get_height (pixbuf1);
    height2 = gdk_pixbuf_get_height (pixbuf2);
    width = width1 < width2 ? width1 : width2;
    height = height1 < height2 ? height1 : height2;
    rowstride1 = gdk_pixbuf_get_rowstride(pixbuf1);
    rowstride2 = gdk_pixbuf_get_rowstride(pixbuf2);
    pixels1 = gdk_pixbuf_get_pixels (pixbuf1);
    pixels2 = gdk_pixbuf_get_pixels (pixbuf2);

    for (x = 0; x < width2; x++) {
        for (y = 0; y < height2; y++) {
            p = pixels + y * rowstride + x * 3;
            p1 = pixels1 + y * rowstride1 + x * n_channels1;
            p2 = pixels2 + y * rowstride2 + x * n_channels2;
            p[0] = SUB(p1[0], p2[0]);
            p[1] = SUB(p1[1], p2[1]);
            p[2] = SUB(p1[2], p2[2]);
        }
    }

    return pixbuf;
}

void tvlogo_send_new_frame(gchar *frame, gint length)
{
    static GdkPixbuf *pb, *pb1;
    GInputStream *stream = g_memory_input_stream_new_from_data(frame, length, NULL);
    pb = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
    g_input_stream_close(stream, NULL, NULL);
    if (!pb)
        return;
    
    pb1 = gdk_pixbuf_new_from_file(tvlogofile, NULL);
    if (pb1) {
        g_print("alpha=%d/%d bits=%d/%d chan=%d/%d\n", gdk_pixbuf_get_has_alpha(pb), gdk_pixbuf_get_has_alpha(pb1),
                gdk_pixbuf_get_bits_per_sample(pb),  gdk_pixbuf_get_bits_per_sample(pb1),
                gdk_pixbuf_get_n_channels(pb), gdk_pixbuf_get_n_channels(pb1));
        gdk_pixbuf_copy_area(pb1, 0, 0, gdk_pixbuf_get_width(pb1), gdk_pixbuf_get_height(pb1), pb, tvlogo_x, tvlogo_y);
        g_object_unref(pb1);
    }

    gdk_pixbuf_save(pb, "frame.jpeg", "jpeg", NULL, NULL);
    g_object_unref(pb);
}

#endif
