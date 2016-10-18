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
#include <librsvg/rsvg.h>
//#include <librsvg/rsvg-cairo.h>

#include "judotimer.h"

struct find_frame_internals;

static void open_media(gint num);
static void close_media(void);
static gboolean refresh_video(gpointer data);
static gchar *find_frame(gchar c, gint *len, struct find_frame_internals *vars);
static void on_button(GtkWidget *widget, gpointer data);

#define VIDEO_BUF_LEN (1<<22)
#define NUM_VIDEO_BUFFERS 1

#define USE_PROXY (video_proxy_host[0] && video_proxy_port)

struct find_frame_internals {
    gchar *data;
    gint n, state, length;
    gint bstate, estate;
    gboolean header, body;
};

static struct video_buffer {
    gchar *buffer;
    gint offset;
    gboolean full;
} video_buffers[NUM_VIDEO_BUFFERS];
static gint current = 0;

G_LOCK_DEFINE(video);

gboolean video_update = FALSE;
gchar  video_http_host[128] = {0};
guint  video_http_port = 0;
gchar  video_http_path[128] = {0};
gchar  video_http_user[32] = {0};
gchar  video_http_password[32] = {0};
gchar  video_proxy_host[128] = {0};
guint  video_proxy_port = 0;
static glong  video_http_addr = 0;
static gboolean connection_ok = FALSE;
static gboolean record = TRUE;
static gboolean header_found = FALSE;
static gint header_state = 0, end_state = 0;
static gchar boundary[64];
static gint bytecount = 0, bps = 0;

//xxxxxx

#define play_img gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_BUTTON)
#define pause_img gtk_image_new_from_stock(GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_BUTTON)

enum {
    BUTTON_CAMERA, BUTTON_REVERSE, BUTTON_PAUSE, 
    BUTTON_SLOW, BUTTON_PLAY, BUTTON_EXIT, NUM_BUTTONS
};

static gchar *button_names[NUM_BUTTONS] = {"#camera", "#reverse", "#pause", "#slow", "#play", "#exit"};
static gchar *button_names1[NUM_BUTTONS] = {"#camera1", "#reverse1", "#pause1", "#slow1", "#play1", "#exit"};

//static gchar *button_icons[NUM_BUTTONS] = {"camera.png", "reverse.png", "pause.png", "slow.png", "play.png", "exit.png"};
//static gchar *button_icons1[NUM_BUTTONS] = {"camera1.png", "reverse1.png", "pause1.png", "slow1.png", "play1.png", "exit.png"};
static GtkWidget *button_pics[NUM_BUTTONS];
static GtkWidget *button_pics1[NUM_BUTTONS];
static GtkWidget *button_w[NUM_BUTTONS];

static GtkWidget *slider;
static gint current_media = 1;
static gint direction = 1;
static gboolean play = TRUE;
static gint fps = 10, framecnt, curpos, view_fps;
static gchar *frame_now;
static gint length_now;
static gboolean view = FALSE;

#define BSIZEX 24
#define BSIZEY 24

static GtkWidget *get_image_from_svg(RsvgHandle *h, const gchar *pic)
{
    RsvgPositionData position;
    RsvgDimensionData dimensions;
    rsvg_handle_get_position_sub(h, &position, pic);
    rsvg_handle_get_dimensions_sub(h, &dimensions, pic);

    GdkPixbuf *pb = rsvg_handle_get_pixbuf_sub(h, pic);
    if (!pb) return NULL;

    gdouble scalex = 1.0*BSIZEX/dimensions.width;
    gdouble scaley = 1.0*BSIZEY/dimensions.height;
    GdkPixbuf *pb1 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, BSIZEX, BSIZEY);

    gdk_pixbuf_scale(pb, pb1, 0, 0, BSIZEX, BSIZEY, 
                     -scalex*position.x, -scaley*position.y,
                     scalex, scaley,
                     GDK_INTERP_BILINEAR);
    return gtk_image_new_from_pixbuf(pb1);
}

void video_init(void)
{
    gint i;
    gchar *file = g_build_filename(installation_dir, "etc", "buttons.svg", NULL);
    RsvgHandle *h = rsvg_handle_new_from_file(file, NULL);
    g_free(file);

    if (!h)
        return;

    for (i = 0; i < NUM_BUTTONS; i++) {
        if (button_pics[i]) g_object_unref(button_pics[i]);
        button_pics[i] = get_image_from_svg(h, button_names[i]);
        if (button_pics[i]) g_object_ref(button_pics[i]);

        if (button_pics1[i]) g_object_unref(button_pics1[i]);
        button_pics1[i] = get_image_from_svg(h, button_names1[i]);
        if (button_pics1[i]) g_object_ref(button_pics1[i]);
    }

    g_object_unref(h);
}

glong hostname_to_addr(gchar *str)
{
#if 1
    struct hostent *hp = gethostbyname(str);
    if (!hp)
        return 0;

    return *(glong *)(hp->h_addr_list[0]);

#else // cannot use, doesn't cross compile for windows
    struct addrinfo hints;
    struct addrinfo *list = NULL;
    struct sockaddr_in *addr = NULL;
    int retVal;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;    
    if ((retVal = getaddrinfo(str, NULL, &hints, &list)) != 0) {
        g_print("getaddrinfo() failed for %s.\n", str);    
    } else {
        addr = (struct sockaddr_in *)list->ai_addr;
        freeaddrinfo(list);
    }

    if (addr)
        return addr->sin_addr.s_addr;

    return 0;
#endif
}

gpointer video_thread(gpointer args)
{
    SOCKET comm_fd;
    gint n;
    struct sockaddr_in node;
    static guchar buf[2048];
    fd_set read_fd, fds;
    guint  http_port;
    struct video_buffer *vb;

    for (n = 0; n < NUM_VIDEO_BUFFERS; n++)
        video_buffers[n].buffer = g_malloc(VIDEO_BUF_LEN);

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        video_update = FALSE;

        do {
            g_usleep(1000000);
            if (video_http_host[0] && video_http_port) {
                if (USE_PROXY) {
                    video_http_addr = hostname_to_addr(video_proxy_host);
                    http_port = video_proxy_port;
                } else {
                    video_http_addr = hostname_to_addr(video_http_host);
                    http_port = video_http_port;
                }
            }
        } while (video_http_addr == 0 || http_port == 0 || video_http_port == 0);

        if ((comm_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            perror("video socket");
            g_print("CANNOT CREATE SOCKET (%s:%d)!\n", __FUNCTION__, __LINE__);
            g_thread_exit(NULL);    /* not required just good pratice */
            return NULL;
        }

        memset(&node, 0, sizeof(node));
        node.sin_family      = AF_INET;
        node.sin_port        = htons(http_port);
        node.sin_addr.s_addr = video_http_addr;

        if (connect(comm_fd, (struct sockaddr *)&node, sizeof(node))) {
            closesocket(comm_fd);
            g_usleep(1000000);
            continue;
        }

        current = 0;
        vb = &video_buffers[current];
        vb->offset = 0;
        vb->full = FALSE;

        g_print("Video connection OK.\n");

        /* send http request */
        if (USE_PROXY)
            n = snprintf((gchar *)buf, sizeof(buf), 
                         "GET http://%s:%d/%s HTTP/1.1\r\n"
                         "Host: %s:%d\r\n"
                         "Connection: keep-alive\r\nAccept: */*\r\n"
                         "User-Agent: JudoTimer\r\n", 
                         video_http_host, video_http_port, video_http_path,
                         video_http_host, video_http_port);
        else
            n = snprintf((gchar *)buf, sizeof(buf), 
                         "GET /%s HTTP/1.1\r\n"
                         "Host: %s:%d\r\n"
                         "Connection: keep-alive\r\nAccept: */*\r\n"
                         "User-Agent: JudoTimer\r\n",
                         video_http_path,
                         video_http_host, video_http_port);

        if (video_http_user[0]) {
            gchar auth[64];
            snprintf(auth, sizeof(auth), "%s:%s", video_http_user, video_http_password);
            gchar *base64 = g_base64_encode((guchar *)auth, strlen(auth));
            n += snprintf((gchar *)buf + n, sizeof(buf) - n, "Authorization: Basic %s\r\n", base64);
            g_free(base64);
        }

        n += snprintf((gchar *)buf + n, sizeof(buf) - n, "\r\n");

        send(comm_fd, (char *)buf, n, 0);

        connection_ok = TRUE;
        header_found = FALSE;
        header_state = 0;
        end_state = 0;
        memset(boundary, 0, sizeof(boundary));
        bytecount = 0;

        FD_ZERO(&read_fd);
        FD_SET(comm_fd, &read_fd);

        while (video_update == FALSE) {
            struct timeval timeout;
            gint r;
			
            fds = read_fd;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;

            r = select(comm_fd+1, &fds, NULL, NULL, &timeout);

            if (r <= 0)
                continue;

            if (FD_ISSET(comm_fd, &fds)) {
                glong now = time(NULL);
                static glong last_time;
                static glong view_start = 0;
                static gint view_frames = 0;
                static struct find_frame_internals view_vars;

                vb = &video_buffers[current];
                n = recv(comm_fd, vb->buffer + vb->offset, VIDEO_BUF_LEN - vb->offset, 0);

                G_LOCK(video);

                bytecount += n;
                if (now > last_time + 3) {
                    bps = bytecount/(now - last_time);
                    bytecount = 0;
                    last_time = now;
                }
#if 0
                FILE *f = fopen("get.log", "a");
                if (f) {
                    fwrite(vb->buffer + vb->offset, 1, n, f);
                    fclose(f);
                }
#endif
                if (n > 0 && header_found == FALSE) {
                    static gchar *boundary_text1 = "boundary=";
                    static gchar *boundary_text2 = "BOUNDARY=";
                    
                    gint i;
                    for (i = 0; i < n; i++) {
                        gchar c = *(vb->buffer + vb->offset + i);
                        
                        if (header_state < 9) {
                            if (boundary_text1[header_state] == c ||
                                boundary_text2[header_state] == c)
                                header_state++;
                            else 
                                header_state = 0;
                        } else if (c <= ' ' || c == ';') {
                            header_found = TRUE;
                            g_print("found boundary='%s'\n", boundary);
                            break;
                        } else {
                            if (header_state - 9 < sizeof(boundary) - 1) {
                                boundary[header_state - 9] = c;
                                header_state++;
                            }
                        }
                        
                        if (end_state == 0) { 
                            if (c == '\n') end_state = 1;
                        } else if (end_state == 1) {
                            if (c == '\n') end_state = 2;
                            else if (c != '\r') end_state = 0;
                        } else if (end_state == 2) {
                            if (c != '\r' && c != '\n') {
                                end_state = 3;
                                boundary[0] = c;
                            }
                        } else {
                            if (c > ' ' && c != ';') {
                                if (end_state - 2 < sizeof(boundary) - 1) {
                                    boundary[end_state - 2] = c;
                                    end_state++;
                                }
                            } else {
                                end_state = 0;
                                header_found = TRUE;
                                g_print("found boundary2='%s'\n", boundary);
                                break;
                            }
                        }
                    } // for
                } // if (header_found == FALSE)

                if (n > 0 && header_found) {
                    if (view) {
                        // find frames in real time
                        gint i;
                        for (i = 0; i < n; i++) {
                            gint len;
                            gchar *fr = find_frame(*(vb->buffer + vb->offset + i), &len, &view_vars);
                            if (fr) {
                                if (frame_now) g_free(frame_now);
                                frame_now = fr;
                                length_now = len;

                                view_frames++;
                                if (view_start == 0) view_start = now;
                                if (now > view_start + 3) {
                                    view_fps = view_frames/(now - view_start);
                                    view_frames = 0;
                                    view_start = now;
                                }
                            }
                        }
                    } else {
                        view_frames = 0;
                        view_start = 0;
                    }

                    if (record) {
                        vb->offset += n;
                        if (vb->offset >= VIDEO_BUF_LEN) {
                            vb->offset = 0;
                            vb->full = TRUE;
                            //vb->duration = now - vb->start;
                            //vb->start = now;
                            //g_print("current=%d video buf len=%d\n", current, VIDEO_BUF_LEN);
                        }
                    } // if record 
                } // if (n > 0 && header_found)

                G_UNLOCK(video);

                if (n <= 0)
                    break;
            } // if (FD_ISSET(comm_fd, &fds))
        } // while (video_update == FALSE)

        connection_ok = FALSE;
        closesocket(comm_fd);

        g_print("Video connection NOK.\n");
    }

    for (n = 0; n < NUM_VIDEO_BUFFERS; n++)
        g_free(video_buffers[n].buffer);

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

void video_record(gboolean yes)
{
    record = yes;

    /*
    if (!connection_ok)
        return;

    video_buffers[current].end = time(NULL);
    if (video_buffers[current].full == FALSE)
        video_buffers[current].duration = 
            video_buffers[current].end - video_buffers[current].start;

    if (++current >= NUM_VIDEO_BUFFERS)
        current = 0;

    video_buffers[current].offset = 0;
    video_buffers[current].full = FALSE;
    video_buffers[current].start = time(NULL);

    record = TRUE;
    */
}


extern GKeyFile *keyfile;

struct url {
    GtkWidget *address, *port, *path, *start;
    GtkWidget *user, *password, *proxy_address, *proxy_port;
};

static void video_ip_address_callback(GtkWidget *widget, 
                                     GdkEvent *event,
                                     GtkWidget *data)
{
    struct url *uri = (struct url *)data;

    if (ptr_to_gint(event) == GTK_RESPONSE_OK) {
        snprintf(video_http_host, sizeof(video_http_host), "%s", gtk_entry_get_text(GTK_ENTRY(uri->address))); 
        g_key_file_set_string(keyfile, "preferences", "videoipaddress", video_http_host);
        video_http_addr = hostname_to_addr(video_http_host);

        video_http_port = atoi(gtk_entry_get_text(GTK_ENTRY(uri->port)));
        g_key_file_set_integer(keyfile, "preferences", "videoipport", video_http_port);

        snprintf(video_http_path, sizeof(video_http_path), "%s", gtk_entry_get_text(GTK_ENTRY(uri->path))); 
        g_key_file_set_string(keyfile, "preferences", "videoippath", video_http_path);

        snprintf(video_proxy_host, sizeof(video_proxy_host), "%s", gtk_entry_get_text(GTK_ENTRY(uri->proxy_address))); 
        g_key_file_set_string(keyfile, "preferences", "videoproxyaddress", video_proxy_host);

        video_proxy_port = atoi(gtk_entry_get_text(GTK_ENTRY(uri->proxy_port)));
        g_key_file_set_integer(keyfile, "preferences", "videoproxyport", video_proxy_port);

        snprintf(video_http_user, sizeof(video_http_user), "%s", gtk_entry_get_text(GTK_ENTRY(uri->user))); 
        g_key_file_set_string(keyfile, "preferences", "videouser", video_http_user);

        snprintf(video_http_password, sizeof(video_http_password), "%s", gtk_entry_get_text(GTK_ENTRY(uri->password))); 
        g_key_file_set_string(keyfile, "preferences", "videopassword", video_http_password);

        video_update = TRUE;
    }

    g_free(uri);
    gtk_widget_destroy(widget);
}

void ask_video_ip_address( GtkWidget *w,
                           gpointer   data )
{
    gchar buf[128];
    GtkWidget *dialog, *hbox, *hbox1, *hbox2, *label;
    struct url *uri = g_malloc0(sizeof(*uri));

    dialog = gtk_dialog_new_with_buttons (_("Video server URL"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          NULL);

    uri->address = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->address), video_http_host);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->address), 20);

    sprintf(buf, "%d", video_http_port); 
    uri->port = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->port), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->port), 4);

    uri->path = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->path), video_http_path);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->path), 16);

    uri->proxy_address = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->proxy_address), video_proxy_host);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->proxy_address), 20);

    sprintf(buf, "%d", video_proxy_port);
    uri->proxy_port = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->proxy_port), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->proxy_port), 4);

    uri->user = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->user), video_http_user);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->user), 20);

    uri->password = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->password), video_http_password);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->password), 20);


#if 0
    uri->start = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->start), "5");
    gtk_entry_set_width_chars(GTK_ENTRY(uri->start), 1);
#endif

    //gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

#if (GTKVER == 3)
    hbox = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox), gtk_label_new("http://"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), uri->address,             1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), gtk_label_new(":"),       2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), uri->port,                3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), gtk_label_new("/"),       4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), uri->path,                5, 0, 1, 1);

    hbox2 = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox2), gtk_label_new(_("User:")),     0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox2), uri->user,                     1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox2), gtk_label_new(_("Password:")), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox2), uri->password,                 3, 0, 1, 1);

    hbox1 = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox1), gtk_label_new(_("Proxy:")), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox1), uri->proxy_address,         1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox1), gtk_label_new(":"),         2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox1), uri->proxy_port,            3, 0, 1, 1);
#else
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("http://"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), uri->address, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), uri->port, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("/"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), uri->path, FALSE, FALSE, 0);

    hbox2 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox2), gtk_label_new(_("User:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), uri->user, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox2), gtk_label_new(_("Password:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox2), uri->password, FALSE, FALSE, 0);

    hbox1 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox1), gtk_label_new(_("Proxy:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), uri->proxy_address, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), uri->proxy_port, FALSE, FALSE, 0);
#endif

    if (connection_ok)
        label = gtk_label_new(_("(Connection OK)"));
    else
        label = gtk_label_new(_("(Connection broken)"));

#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       label, FALSE, FALSE, 0);
#else
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox2, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox1, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, FALSE, 4);
#endif

    gtk_widget_show_all(dialog);

    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(video_ip_address_callback), uri);
}

static GtkWindow *video_window;
static const gchar *contlen1 = "content-length:";
static const gchar *contlen2 = "CONTENT-LENGTH:";

struct frame {
    struct frame *next, *prev;
    gchar *data;
    gint length;
};

static struct frame frames;
static struct frame *fp = NULL;

static void close_media(void)
{
    struct frame *f, *p = frames.next;

    while (p) {
        f = p;
        p = p->next;
        g_free(f->data);
        g_free(f);
    }

    frames.next = frames.prev = NULL;
}

static gchar *find_frame(gchar c, gint *len, struct find_frame_internals *vars)
{
    if (header_found == FALSE)
        return NULL;

    // look for boundary text
    if (boundary[vars->bstate] == c)
        vars->bstate++;
    else
        vars->bstate = 0;

    if (boundary[vars->bstate] == 0) {
        gchar *f = vars->data;
        if (vars->n > vars->bstate)
            *len = vars->n - vars->bstate;
        else
            *len = 0;
        vars->n = 0;
        vars->state = 0;
        vars->data = NULL;
        vars->header = TRUE;
        vars->body = FALSE;
        vars->bstate = 0;
        vars->length = 0;
        return f;
    }

    // look for content length
    if (vars->header) {
        if (vars->state == 15) {
            if (c >= '0' && c <= '9')
                vars->length = 10*vars->length + c - '0';
            else if (c != ' ') {
                vars->state = 0;
            }
        } else if (vars->state < 15 && 
                   (c == contlen1[vars->state] || c == contlen2[vars->state])) {
            vars->state++;
            if (vars->state == 15)
                vars->length = 0;
        } else vars->state = 0;

        if (vars->estate == 0) { 
            if (c == '\n')
                vars->estate = 1;
        } else {
            if (c == '\n') {
                vars->estate = 0;
                vars->header = FALSE;
                vars->body = TRUE;
                if (vars->length < 100 || vars->length > 100000)
                    vars->length = 60000;
                if (vars->data)
                    g_free(vars->data);
                vars->data = g_malloc(vars->length);
                vars->n = 0;
            } else if (c != '\r')
                vars->estate = 0;
        }
    } else if (vars->body) {
        if (vars->length) {
            vars->data[vars->n++] = c;
            vars->length--;
        }
        if (vars->length == 0) {
            gchar *f = vars->data;
            *len = vars->n;
            vars->n = 0;
            vars->state = 0;
            vars->body = FALSE;
            vars->header = FALSE;
            vars->data = NULL;
            return f;
        }
    }

    return NULL;
}

static void open_media(gint num) 
{
    //gint last = (current + NUM_VIDEO_BUFFERS - num) % NUM_VIDEO_BUFFERS;
    struct video_buffer *vb = &video_buffers[current];
    gint ix = 0, length, tot_length = 0;
    struct frame *f = NULL, *p = NULL;
    struct find_frame_internals media_vars;

    memset(&media_vars, 0, sizeof(media_vars));
    framecnt = 0;

    G_LOCK(video);

    if (vb->full) ix = vb->offset+1;

    while (ix != vb->offset) {
        if (ix >= VIDEO_BUF_LEN) ix = 0;

        gchar *fr = find_frame(vb->buffer[ix], &length, &media_vars);
        if (fr) {
            f = g_malloc0(sizeof(*f));
            f->data = fr;
            f->length = length;
            tot_length += length;
            p = &frames;
            while (p->next) p = p->next;
            p->next = f;
            f->prev = p;
            framecnt++;
        }

        ix++;
    }

    G_UNLOCK(video);

    if (tot_length)
        fps = (bps*framecnt)/tot_length;
    else
        fps = 5;

    //g_print("framecnt=%d tot=%d bps=%d fps=%d\n", framecnt, tot_length, bps, fps);
    fp = frames.next;
    curpos = 1;

    // find position 5 sec before end
    gint targetpos = framecnt - 5*fps;
    while (curpos <= framecnt && curpos < targetpos && fp->next && fp->next->length > 100) {
        fp = fp->next;
        curpos++;
    }

    on_button(NULL, (gpointer)BUTTON_SLOW);
}

static void on_button(GtkWidget *widget, gpointer data) 
{
    gint button = ptr_to_gint(data);
    gint i;

    view = FALSE;

    for (i = 0; i < NUM_BUTTONS; i++) {
        gtk_button_set_image(GTK_BUTTON(button_w[i]), 
                             i == button ? button_pics1[i] : button_pics[i]);
    }

    switch (button) {
#if 0
    case PREVIOUS:
        direction = 1;
        play = FALSE;
        gtk_range_set_value(GTK_RANGE(slider), 0.0);
#if 0
        if (current_media < NUM_VIDEO_BUFFERS - 1) {
            current_media++;
            close_media();
            open_media(current_media);
        }
#endif
        break;
#endif
    case BUTTON_CAMERA:
        view = TRUE;
        break;
    case BUTTON_REVERSE:
        direction = -1;
        play = TRUE;
        break;
    case BUTTON_PAUSE:
        play = FALSE;
        break;
    case BUTTON_SLOW:
        direction = 3;
        play = TRUE;
        break;
    case BUTTON_PLAY:
        direction = 1;
        play = TRUE;
        break;
#if 0
    case NEXT:
        gtk_button_set_image(GTK_BUTTON(playpause_button), play_img);
        direction = 1;
        play = FALSE;
        gtk_range_set_value(GTK_RANGE(slider), 100.0);
#if 0
        if (current_media > 0) {
            current_media--;
            close_media();
            open_media(current_media);
        }
#endif
        break;
#endif
    }
}


static gboolean delete_event_video( GtkWidget *widget, GdkEvent  *event, gpointer   data )
{
    return FALSE;
}

static void destroy_video( GtkWidget *widget, gpointer   data )
{
    close_media();
    video_window = NULL;
    if (frame_now)
        g_free(frame_now);
    frame_now = NULL;
    view = FALSE;
}

static gboolean close_video(GtkWidget *widget, gpointer userdata)
{
    gtk_widget_destroy(userdata);
    return FALSE;
}

#define SUB(a,b) (a > b ? 4*(a - b) : 4*(b - a))
//#define SUB(a,b) (a != b ? 100 : 0)

#if 0
static GdkPixbuf *sub_pixbufs(GdkPixbuf *pixbuf1, GdkPixbuf *pixbuf2)
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

    pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    pixels = gdk_pixbuf_get_pixels (pixbuf);
    rowstride = gdk_pixbuf_get_rowstride(pixbuf);

    for (x = 0; x < width; x++) {
        for (y = 0; y < height; y++) {
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
#endif

static gboolean expose_video(GtkWidget *widget, GdkEventExpose *event, gpointer userdata)
{
    gint width, height;
    GError *err = NULL;
    static GdkPixbuf *pb;//, *pb1, *pb2;

#if (GTKVER == 3)
    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);
#else
    width = widget->allocation.width;
    height = widget->allocation.height;
#endif

    if (view) {
        if (!frame_now)
            return FALSE;

        GInputStream *stream = g_memory_input_stream_new_from_data(frame_now, length_now, NULL);
        pb = gdk_pixbuf_new_from_stream(stream, NULL, &err);
        g_input_stream_close(stream, NULL, NULL);
        //pb = gdk_pixbuf_new_from_inline(length_now, frame_now, TRUE, &err);
        //g_file_set_contents("test.jpg", frame_now, length_now, NULL);
    } else {
        if (fp == NULL || fp->length < 100)
            return FALSE;

        //if (pb2) g_object_unref(pb2);
        //pb2 = pb1;
        GInputStream *stream = g_memory_input_stream_new_from_data(fp->data, fp->length, NULL);
        pb = gdk_pixbuf_new_from_stream(stream, NULL, &err);
        g_input_stream_close(stream, NULL, NULL);
        /*
        if (pb1 && pb2)
            pb = sub_pixbufs(pb1, pb2);
        else
        pb = NULL;*/
        //pb = gdk_pixbuf_new_from_inline(fp->length, fp->data, TRUE, &err);
        //g_file_set_contents("test.jpg", fp->data, fp->length, NULL);
    }

    //GdkPixbuf *pb = gdk_pixbuf_new_from_file("test.jpg", &err);
    if (!pb) {
        //g_print("\nERROR %s: %s %d\n",
        //        err->message, __FUNCTION__, __LINE__);
        //g_file_set_contents("err.jpg", fp->data, fp->length, NULL);
        return FALSE;
    }
    gint pixwidth = gdk_pixbuf_get_width(pb);
    gint pixheight = gdk_pixbuf_get_height(pb);
    gdouble scalex = 1.0*width/pixwidth, 
        scaley = 1.0*height/pixheight,
        scale = (scalex < scaley) ? scalex : scaley;

    gdouble x = (scale*pixwidth < width) ? (width/scale - pixwidth)/2.0 : 0;
    gdouble y = (scale*pixheight < height) ? (height/scale - pixheight)/2.0 : 0;

#if (GTKVER == 3)
    cairo_t *c = (cairo_t *)event;
#else
    cairo_t *c = gdk_cairo_create(widget->window);
#endif
    cairo_scale(c, scale, scale);
    gdk_cairo_set_source_pixbuf(c, pb, x, y);
    cairo_paint(c);

    if (view) {
        gchar buf[128];
        snprintf(buf, sizeof(buf), "%dx%d", pixwidth, pixheight);
        cairo_set_source_rgb(c, 1.0, 0.0, 0.0);
        cairo_set_font_size(c, 20/scale);
        cairo_move_to(c, 30.0/scale, 30.0/scale);
        cairo_show_text(c, buf);
        cairo_move_to(c, 30.0/scale, 50.0/scale);
        snprintf(buf, sizeof(buf), "%d fps", view_fps);
        cairo_show_text(c, buf);
        cairo_move_to(c, 30.0/scale, 70.0/scale);
        snprintf(buf, sizeof(buf), "%d bits/s", bps*8);
        cairo_show_text(c, buf);
    }
    
    g_object_unref(pb);
#if (GTKVER != 3)
    cairo_show_page(c);
    cairo_destroy(c);
#endif
    return FALSE;
}

static gboolean refresh_video(gpointer data)
{
    static gdouble lastval = -100.0;
    GtkWidget *widget;

    if (video_window == NULL)
        return FALSE;

    if (view)
        goto refresh;

    if (fp == NULL || framecnt == 0)
        return TRUE;

    if (!play) {
        gdouble step = 100.0/framecnt;
        gdouble val = gtk_range_get_value(GTK_RANGE(slider));
        gdouble diff = lastval - val;
        if (diff < 0) diff = -diff;
        if (diff < 0.2)
            return TRUE;

        lastval = val;
        gdouble cur = (gdouble)curpos*step;
        if (val < cur - step*0.5) {
            while (curpos > 0 && val < cur && fp->prev && fp->prev->length > 100) {
                fp = fp->prev;
                curpos--;
                cur = (gdouble)curpos*step;
            }
        } else if (val > cur + step*0.5) {
            while (curpos <= framecnt && val > cur && fp->next && fp->next->length > 100) {
                fp = fp->next;
                curpos++;
                cur = (gdouble)curpos*step;
            }
        }
        goto refresh;
    } else lastval = -100.0;

    if (direction > 0) {
        if (fp->next && fp->next->length > 100) {
            static gint cnt = 0;
            if (++cnt >= direction) {
                fp = fp->next;
                curpos++;
                cnt = 0;
            }
        }
        if (fp->next == NULL || fp->next->length < 100) {
            on_button(NULL, (gpointer)BUTTON_PAUSE);
        }
    } else if (direction < 0) {
        if (fp->prev && fp->prev->length > 100) {
            static gint cnt = 0;
            if (--cnt <= direction) {
                fp = fp->prev;
                curpos--;
                cnt = 0;
            }
        }
        if (fp->prev == NULL || fp->prev->length < 100) {
            on_button(NULL, (gpointer)BUTTON_PAUSE);
        }
    }

    gtk_range_set_value(GTK_RANGE(slider), ((gdouble)curpos/(gdouble)framecnt)*100.0);

 refresh:
    widget = GTK_WIDGET(data);
#if (GTKVER == 3)
    if (gtk_widget_get_window(widget)) {
        gdk_window_invalidate_rect(gtk_widget_get_window(widget), NULL, TRUE);
        gdk_window_process_updates(gtk_widget_get_window(widget), TRUE);
    }
#else
    if (widget->window) {
        GdkRegion *region;
        region = gdk_drawable_get_clip_region(widget->window);
        gdk_window_invalidate_region(widget->window, region, TRUE);
        gdk_window_process_updates(widget->window, TRUE);
    }
#endif
    return TRUE;
}

void create_video_window(void)
{
    static GtkWindow *window;
    GtkWidget
        *vbox,
        *player_widget,
        *hbuttonbox;
    gint width;
    gint height;
    gint i;

    gtk_window_get_size(GTK_WINDOW(main_window), &width, &height);

    window = video_window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(GTK_WINDOW(window), _("Show Video"));
    if (fullscreen)
        gtk_window_fullscreen(GTK_WINDOW(window));
    else 
        gtk_widget_set_size_request(GTK_WIDGET(window), width, height);

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);

    //setup vbox
#if (GTKVER == 3)
    vbox = gtk_grid_new();
#else
    vbox = gtk_vbox_new(FALSE, 0);
#endif
    gtk_container_add(GTK_CONTAINER(window), vbox);

    //setup player widget
    player_widget = gtk_drawing_area_new();
#if (GTKVER == 3)
    gtk_widget_set_hexpand(player_widget, TRUE);
    gtk_widget_set_vexpand(player_widget, TRUE);    
    gtk_grid_attach(GTK_GRID(vbox), player_widget, 0, 0, 1, 1);
#else
    gtk_box_pack_start(GTK_BOX(vbox), player_widget, TRUE, TRUE, 0);
#endif

    // slider
#if (GTKVER == 3)
    slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 100.0, 10.0);
    gtk_grid_attach(GTK_GRID(vbox), slider, 0, 1, 1, 1);
    hbuttonbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
#else
    slider = gtk_hscale_new_with_range(0.0, 100.0, 10.0);
    gtk_box_pack_start(GTK_BOX(vbox), slider, FALSE, FALSE, 0);
    hbuttonbox = gtk_hbutton_box_new();
#endif

    //setup controls
    //playpause_button = gtk_button_new_from_stock(GTK_STOCK_MEDIA_PLAY);

    gtk_container_set_border_width(GTK_CONTAINER(hbuttonbox), 5);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbuttonbox), GTK_BUTTONBOX_START);

    video_init();

    for (i = 0; i < NUM_BUTTONS; i++) {
        button_w[i] = gtk_button_new();
        gtk_button_set_image(GTK_BUTTON(button_w[i]), button_pics[i]);
        if (i == BUTTON_EXIT)
            g_signal_connect(button_w[i], "clicked", G_CALLBACK(close_video), window);
        else
            g_signal_connect(button_w[i], "clicked", G_CALLBACK(on_button), gint_to_ptr(i));
        gtk_box_pack_start(GTK_BOX(hbuttonbox), button_w[i], FALSE, FALSE, 0);
    }

#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(vbox), hbuttonbox, 0, 2, 1, 1);
#else
    gtk_box_pack_start(GTK_BOX(vbox), hbuttonbox, FALSE, FALSE, 0);
#endif

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

    current_media = 1;
    open_media(current_media);

    if (fps == 0)
        fps = 2;

    g_timeout_add(1000/fps, refresh_video, window);
}
