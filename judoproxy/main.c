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
#define in_addr_t uint32_t
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

#include <time.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <glib.h>
#ifdef WEBKIT
#include <webkit/webkit.h>
#endif

#if (GTKVER == 3)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#ifdef WIN32
#include <process.h>
//#include <glib/gwin32.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

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

#include "judoproxy.h"
#include "language.h"
#include "binreloc.h"

#define APPLICATION_TYPE_CAMERA 100

void get_parameters(gint i);
void send_parameter(gint i, gint j);
void send_parameter_2(struct sockaddr_in addr, gint port, const gchar *param, const gchar *value);
void send_cmd_parameters(gint i);
void scan_interfaces(void);
static gint check_table(gpointer data);
static gpointer connection_thread(gpointer args);
#ifdef WEBKIT
static WebKitWebView *web_view;
#endif
static gchar *video_dir = NULL;
static GtkWidget *progress_bar = NULL;
#ifdef WEBKIT
static WebKitDownload *cur_download = NULL;
#endif
gchar         *program_path;
GtkWidget     *main_vbox = NULL;
GtkWidget     *main_window = NULL;
gchar          current_directory[1024] = {0};
gint           my_address;
gchar         *installation_dir = NULL;
GTimer        *timer;
static PangoFontDescription *font;
GKeyFile      *keyfile;
gchar         *conffile;
GdkCursor     *cursor = NULL;
guint          current_year;
gint           language = LANG_EN;
gint           num_lines = 3;//NUM_LINES;
gint           display_type = HORIZONTAL_DISPLAY;//NORMAL_DISPLAY;
gboolean       mirror_display = FALSE;
gboolean       white_first = FALSE;
gboolean       red_background = FALSE;
gchar         *filename = NULL;
GtkWidget     *id_box = NULL, *name_box = NULL;
GtkWidget     *connection_table;
static GdkColor color_yellow, color_white, color_grey, color_green, color_darkgreen,
    color_blue, color_red, color_darkred, color_black;
gboolean       connections_updated = FALSE;
GtkWidget     *notebook;
gboolean       advertise_addr = FALSE;

#define MY_FONT "Arial"

#define THIN_LINE     (paper_width < 700.0 ? 1.0 : paper_width/700.0)
#define THICK_LINE    (2*THIN_LINE)

#define W(_w) ((_w)*paper_width)
#define H(_h) ((_h)*paper_height)

#define BOX_HEIGHT (horiz ? paper_height/(4.0*(num_lines+0.25)*2.0) : paper_height/(4.0*(num_lines+0.25)))
//#define BOX_HEIGHT (1.4*extents.height)

#define NUM_INTERFACES 8
#define NUM_DEVS 16

struct app_type {
    gint type;
#define APP_MODE_MASTER 1
#define APP_MODE_SLAVE  2
    gint mode;
    gint tatami;
};

static struct {
    gchar *name;
    uint32_t addr;
    GtkWidget *dev_table;
    gboolean changed;
    int fdin, fdout;
    gchar *ssdp_msg;
    gint   ssdp_msg_len;
    struct application {
        gchar     *text;
        GtkWidget *textw;
        uint32_t   addr;
        GtkWidget *addrw;
        GtkWidget *check;
        struct app_type apptype;
    } device[NUM_DEVS];
} iface[NUM_INTERFACES];

static gint addrcnt = 0;

#define NUM_CONNECTIONS 10
#define START_PORT 2321

static struct {
    SOCKET fd_listen, fd_conn, fd_out;
    struct sockaddr_in caller, destination;
    GtkWidget *in_addr, *out_addr;
    GtkWidget *scrolled_window;
    gboolean changed;
} connections[NUM_CONNECTIONS];

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    return FALSE;
}

extern void print_stat(void);

void destroy( GtkWidget *widget,
	      gpointer   data )
{
    gsize length;
    gchar *inidata = g_key_file_to_data (keyfile,
                                         &length,
                                         NULL);

    print_stat();

    g_file_set_contents(conffile, inidata, length, NULL);
    g_free(inidata);
    g_key_file_free(keyfile);

    gtk_main_quit ();
}

static void enter_callback( GtkWidget *widget,
                            GtkWidget *entry )
{
    gulong a,b,c,d;
    gint i = ptr_to_gint(entry);
    const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(connections[i].out_addr));
    g_print("TIMER IP ENTRY=%s con=%d\n", entry_text, i);
    sscanf(entry_text, "%ld.%ld.%ld.%ld", &a, &b, &c, &d);
    connections[i].destination.sin_addr.s_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
    connections[i].changed = TRUE;
}

#ifdef WEBKIT
static void reload_web( GtkWidget *widget,
			GtkWidget *entry )
{
    gchar url[256];
    gint i = ptr_to_gint(entry);
    snprintf(url, sizeof(url), "http://%s:8888/index.html",
	     inet_ntoa(connections[i].caller.sin_addr));
    webkit_web_view_load_uri(web_view, url);
    gtk_widget_grab_focus(GTK_WIDGET(web_view));
}
#else
static void select_camera_video( GtkWidget *widget,
				 GtkWidget *entry )
{
    gint i = ptr_to_gint(entry);
    set_camera_addr(connections[i].caller.sin_addr.s_addr);
    set_html_url(connections[i].caller.sin_addr);
}
#endif

static void camera_ip_enter_callback( GtkWidget *widget,
                                      GtkWidget *entry )
{
    gulong a,b,c,d;
    gint i = ptr_to_gint(entry);
    const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(connections[i].in_addr));
    g_print("CAM IP ENTRY=%s con=%d\n", entry_text, i);
    sscanf(entry_text, "%ld.%ld.%ld.%ld", &a, &b, &c, &d);
    connections[i].caller.sin_addr.s_addr = htonl((a << 24) | (b << 16) | (c << 8) | d);
    connections[i].changed = TRUE;
}

#ifdef WEBKIT
gboolean download_req(WebKitWebView* webView, WebKitDownload *download, gboolean *handled)
{
    const gchar *uri = webkit_download_get_uri(download);
    char *p = strrchr(uri, '/');
    if (!p)
	return FALSE;

    gchar *dest = g_strdup_printf("file:///%s/%s", video_dir, p+1);
    webkit_download_set_destination_uri(download, dest);
    g_free(dest);

    cur_download = download;
    return TRUE;
}

void update_progress(void)
{
    if (0 && cur_download) {
	gdouble prog = webkit_download_get_progress(cur_download);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), prog);
    }
}

static void load_changed (WebKitWebView  *web_view,
                                   /*WebKitLoadEvent*/ int load_event,
                                   gpointer        user_data)
{
    g_print("LOAD %d\n", load_event);
#if 0
    gchar *provisional_uri;
    gchar *redirected_uri;
    gchar *uri;

    switch (load_event) {
    case WEBKIT_LOAD_STARTED:
        /* New load, we have now a provisional URI */
        provisional_uri = webkit_web_view_get_uri (web_view);
	g_print("provisional_uri=%s\n", provisional_uri);
        /* Here we could start a spinner or update the
         * location bar with the provisional URI */
        break;
    case WEBKIT_LOAD_REDIRECTED:
        redirected_uri = webkit_web_view_get_uri (web_view);
	g_print("redirected_uri=%s\n", redirected_uri);
        break;
    case WEBKIT_LOAD_COMMITTED:
        /* The load is being performed. Current URI is
         * the final one and it won't change unless a new
         * load is requested or a navigation within the
         * same page is performed */
        uri = webkit_web_view_get_uri (web_view);
	g_print("uri=%s\n", uri);
        break;
    case WEBKIT_LOAD_FINISHED:
        /* Load finished, we can now stop the spinner */
	g_print("finished\n");
        break;
    }
#endif
}
#endif

static void set_active_area(GtkWidget *menuitem, gpointer userdata)
{
    gint reset = ptr_to_gint(userdata);
    if (reset)
	send_mask(0, 0, 0, 0);
    else
	send_mask(pic_button_x1, pic_button_y1, pic_button_x2, pic_button_y2);
}

static gboolean button_press_callback (GtkWidget      *event_box,
				       GdkEventButton *event,
				       gpointer        data)
{
    gint ev_width, ev_height, im_width, im_height;

    GdkWindow *ev_w =
	gtk_widget_get_parent_window(event_box);

    ev_width = gdk_window_get_width(ev_w);
    ev_height = gdk_window_get_height(ev_w);

    GdkPixbuf *pix = gtk_image_get_pixbuf(data);
    im_width = gdk_pixbuf_get_width(pix);
    im_height = gdk_pixbuf_get_height(pix);

    pic_button_x1 = event->x - (ev_width - im_width)/2;
    pic_button_y1 = event->y - (ev_height - im_height)/2;
    pic_button_x2 = pic_button_x1;
    pic_button_y2 = pic_button_y1;
    pic_button_drag = TRUE;

    return TRUE;
}

static gboolean button_release_callback (GtkWidget      *event_box,
					 GdkEventButton *event,
					 gpointer        data)
{
    gint ev_width, ev_height, im_width, im_height;

    GdkWindow *ev_w =
	gtk_widget_get_parent_window(event_box);

    ev_width = gdk_window_get_width(ev_w);
    ev_height = gdk_window_get_height(ev_w);

    GdkPixbuf *pix = gtk_image_get_pixbuf(data);
    im_width = gdk_pixbuf_get_width(pix);
    im_height = gdk_pixbuf_get_height(pix);

    pic_button_x2 = event->x - (ev_width - im_width)/2;
    pic_button_y2 = event->y - (ev_height - im_height)/2;

    if (pic_button_x2 < pic_button_x1 + 50 ||
	pic_button_y2 < pic_button_y1 + 50) {
	pic_button_drag = FALSE;
    } else {
        GtkWidget *menu, *menuitem;

        menu = gtk_menu_new();

        menuitem = gtk_menu_item_new_with_label("Set mask");
        g_signal_connect(menuitem, "activate",
                         (GCallback)set_active_area, NULL);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        menuitem = gtk_menu_item_new_with_label("Reset");
        g_signal_connect(menuitem, "activate",
                         (GCallback)set_active_area, gint_to_ptr(1));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

        gtk_widget_show_all(menu);
        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                       (event != NULL) ? event->button : 0,
                       gdk_event_get_time((GdkEvent*)event));

	pic_button_released(pic_button_x2, pic_button_y2);
    }

    return TRUE;
}

static gboolean button_notify_callback (GtkWidget      *event_box,
					GdkEventButton *event,
					gpointer        data)
{
    gint ev_width, ev_height, im_width, im_height;

    GdkWindow *ev_w =
	gtk_widget_get_parent_window(event_box);

    ev_width = gdk_window_get_width(ev_w);
    ev_height = gdk_window_get_height(ev_w);

    GdkPixbuf *pix = gtk_image_get_pixbuf(data);
    im_width = gdk_pixbuf_get_width(pix);
    im_height = gdk_pixbuf_get_height(pix);

    pic_button_x2 = event->x - (ev_width - im_width)/2;
    pic_button_y2 = event->y - (ev_height - im_height)/2;

    return TRUE;
}


int main( int   argc,
          char *argv[] )
{
    /* GtkWidget is the storage type for widgets */
    GtkWidget *window;
    GtkWidget *menubar;
    time_t     now;
    struct tm *tm;
    GThread   *gth = NULL;         /* thread id */
    gboolean   run_flag = TRUE;   /* used as exit flag for threads */
    int i;

    putenv("UBUNTU_MENUPROXY=");

    //init_trees();

    gdk_color_parse("#FFFF00", &color_yellow);
    gdk_color_parse("#FFFFFF", &color_white);
    gdk_color_parse("#404040", &color_grey);
    gdk_color_parse("#00FF00", &color_green);
    gdk_color_parse("#007700", &color_darkgreen);
    gdk_color_parse("#0000FF", &color_blue);
    gdk_color_parse("#FF0000", &color_red);
    gdk_color_parse("#770000", &color_darkred);
    gdk_color_parse("#000000", &color_black);

    font = pango_font_description_from_string("Sans bold 12");

#ifdef WIN32
    installation_dir = g_win32_get_package_installation_directory_of_module(NULL);
#else
    gbr_init(NULL);
    installation_dir = gbr_find_prefix(NULL);
#endif

    program_path = argv[0];

    current_directory[0] = 0;

    if (current_directory[0] == 0)
        strcpy(current_directory, ".");

    conffile = g_build_filename(g_get_user_data_dir(), "judoproxy.ini", NULL);

    /* create video download dir */
    video_dir = g_build_filename(g_get_home_dir(), "rpivideos", NULL);
    g_print("Video dir = %s\n", video_dir);
    g_mkdir_with_parents(video_dir, 0755);

    keyfile = g_key_file_new();
    g_key_file_load_from_file(keyfile, conffile, 0, NULL);

    now = time(NULL);
    tm = localtime(&now);
    current_year = tm->tm_year+1900;
    srand(now); //srandom(now);
    my_address = now + getpid()*10000;

    gtk_init (&argc, &argv);

    main_window = window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "JudoProxy");
    gtk_widget_set_size_request(window, FRAME_WIDTH, FRAME_HEIGHT);

    gchar *iconfile = g_build_filename(installation_dir, "etc", "judoproxy.png", NULL);
    gtk_window_set_default_icon_from_file(iconfile, NULL);
    g_free(iconfile);

    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event), NULL);

    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (destroy), NULL);

    gtk_container_set_border_width (GTK_CONTAINER (window), 10);

    main_vbox = gtk_grid_new();
    gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 1);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    gtk_widget_show(main_vbox);

    /* menubar */
    menubar = get_menubar_menu(window);
    gtk_widget_show(menubar);

    gtk_grid_attach(GTK_GRID(main_vbox), menubar, 0, 0, 1, 1);
    gtk_widget_set_hexpand(menubar, TRUE);

    /* judoka info */
    //name_box = gtk_label_new("?");
    //gtk_box_pack_start(GTK_BOX(main_vbox), name_box, FALSE, TRUE, 0);

    //GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
    //gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, TRUE, 0);

    notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
    gtk_widget_set_hexpand(notebook, TRUE);
    //gtk_widget_set_vexpand(notebook, TRUE);
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    /*
    GtkWidget *camera_table = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(camera_table), 10);
    gtk_grid_set_row_spacing(GTK_GRID(camera_table), 10);
    */

    for (i = 0; i < NUM_TATAMIS; i++) {
        gchar buf[32];
        GtkWidget *data_table = gtk_grid_new();
        connections[i].scrolled_window = data_table;
        //connections[i].scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(connections[i].scrolled_window), 10);

        snprintf(buf, sizeof(buf), "T%d [--]", i+1);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), connections[i].scrolled_window,
                                 gtk_label_new(buf));

        gtk_grid_attach(GTK_GRID(data_table), gtk_label_new(_("Camera")),    0, 0, 2, 1);
        gtk_grid_attach(GTK_GRID(data_table), gtk_label_new(_("JudoTimer")), 4, 0, 2, 1);

	GtkWidget *reload = gtk_button_new_with_label(_("Reload"));
        gtk_grid_attach(GTK_GRID(data_table), reload, 0, 1, 1, 1);

        connections[i].in_addr = gtk_entry_new();
        connections[i].out_addr = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(connections[i].in_addr), 15);
        gtk_entry_set_width_chars(GTK_ENTRY(connections[i].out_addr), 15);
        gtk_grid_attach(GTK_GRID(data_table), connections[i].in_addr, 2, 0, 2, 1);
        gtk_grid_attach(GTK_GRID(data_table), connections[i].out_addr, 6, 0, 2, 1);
        gtk_grid_attach(GTK_GRID(data_table), gtk_label_new(" "), 0, 1, 1, 1);


        //gtk_container_add(GTK_CONTAINER(connections[i].scrolled_window), data_table);
        g_signal_connect (connections[i].out_addr, "activate",
                          G_CALLBACK(enter_callback),
                          gint_to_ptr(i));
        g_signal_connect (connections[i].in_addr, "activate",
                          G_CALLBACK(camera_ip_enter_callback),
                          gint_to_ptr(i));
#ifdef WEBKIT
        g_signal_connect (reload, "clicked",
                          G_CALLBACK(reload_web),
                          gint_to_ptr(i));
#else
        g_signal_connect (reload, "clicked",
                          G_CALLBACK(select_camera_video),
                          gint_to_ptr(i));
#endif
    }

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
    gtk_grid_attach_next_to(GTK_GRID(main_vbox), notebook, NULL, GTK_POS_BOTTOM, 1, 1);

    progress_bar = gtk_progress_bar_new();
    gtk_grid_attach_next_to(GTK_GRID(main_vbox), progress_bar, NULL, GTK_POS_BOTTOM, 1, 1);

    GtkWidget *w = GTK_WIDGET(gtk_scrolled_window_new(NULL, NULL));
#ifdef WEBKIT
    web_view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    webkit_web_view_set_transparent(web_view, TRUE);

    gtk_container_add(GTK_CONTAINER(w), GTK_WIDGET(web_view));
    gtk_widget_set_vexpand(GTK_WIDGET(web_view), TRUE);

    WebKitWebSettings *web_settings = webkit_web_settings_new();
    //g_object_set(G_OBJECT(web_settings), "enable-java-applet", FALSE, NULL);
    //g_object_set(G_OBJECT(web_settings), "enable-plugins", FALSE, NULL);
    webkit_web_view_set_settings(web_view, web_settings);

    gtk_grid_attach_next_to(GTK_GRID(main_vbox), GTK_WIDGET(w), NULL, GTK_POS_BOTTOM, 1, 1);
    gtk_widget_grab_focus(GTK_WIDGET(web_view));

    //webkit_web_view_load_uri(web_view, "http://www.hs.fi");
    webkit_web_view_load_uri(web_view, "http://www.quirksmode.org/html5/tests/video.html");

    g_signal_connect(web_view, "download-requested", G_CALLBACK(download_req), NULL);
    //g_signal_connect(web_view, "load-changed", G_CALLBACK(load_changed), NULL);
#else
    /* HTML page */
    html_page = create_html_page();
    gtk_grid_attach_next_to(GTK_GRID(main_vbox), GTK_WIDGET(html_page), NULL, GTK_POS_BOTTOM, 1, 1);

    /* Video display */
    camera_image = gtk_image_new();
    gtk_widget_set_size_request(GTK_WIDGET(camera_image), 640, 360);

    GtkWidget *event_box = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(event_box), camera_image);

    gtk_widget_set_events(event_box, gtk_widget_get_events(event_box) |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          /*GDK_POINTER_MOTION_MASK |*/
                          GDK_POINTER_MOTION_HINT_MASK |
                          GDK_BUTTON_MOTION_MASK);

    g_signal_connect(G_OBJECT(event_box),
		     "button_press_event",
		     G_CALLBACK(button_press_callback),
		     camera_image);
    g_signal_connect(G_OBJECT(event_box),
		     "button_release_event",
		     G_CALLBACK(button_release_callback),
		     camera_image);
    g_signal_connect(G_OBJECT(event_box),
		     "motion-notify-event",
		     G_CALLBACK(button_notify_callback),
		     camera_image);

    gtk_widget_show(camera_image);
    gtk_container_add(GTK_CONTAINER(w), GTK_WIDGET(event_box));
    gtk_widget_set_vexpand(GTK_WIDGET(event_box), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(event_box), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(camera_image), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(camera_image), TRUE);
    gtk_grid_attach_next_to(GTK_GRID(main_vbox), GTK_WIDGET(w), NULL, GTK_POS_BOTTOM, 1, 1);

#endif

    /* timers */

    timer = g_timer_new();

    gtk_widget_show_all(window);

    set_preferences();
    change_language(NULL, NULL, gint_to_ptr(language));

    open_comm_socket();
    scan_interfaces();

    /* Create a bg thread using glib */
    gpointer proxy_ssdp_thread(gpointer args);
    g_snprintf(ssdp_id, sizeof(ssdp_id), "JudoProxy");

    gth = g_thread_new("Connection",
                       (GThreadFunc)connection_thread,
                       (gpointer)&run_flag);

    gth = g_thread_new("SSDP",
                       (GThreadFunc)proxy_ssdp_thread,
                       (gpointer)&run_flag);

    gth = g_thread_new("CameraVideo",
		       camera_video,
		       (gpointer)&run_flag);
    gth = gth; // make compiler happy

    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);

    cursor = gdk_cursor_new(GDK_HAND2);
    //gdk_window_set_cursor(GTK_WIDGET(main_window)->window, cursor);

    //g_timeout_add(100, timeout_ask_for_data, NULL);
    g_timeout_add(1000, check_table, NULL);

    g_idle_add(show_camera_video, NULL);

    /* All GTK applications must have a gtk_main(). Control ends here
     * and waits for an event to occur (like a key press or
     * mouse event). */
    gtk_main();

    run_flag = FALSE;     /* flag threads to stop and exit */
    //g_thread_join(gth);   /* wait for thread to exit */

    return 0;
}

gboolean this_is_shiai(void)
{
    return FALSE;
}

gint application_type(void)
{
    return APPLICATION_TYPE_PROXY;
}

void set_write_file(GtkWidget *menu_item, gpointer data)
{
    GtkWidget *dialog;
    static gchar *last_dir = NULL;

    dialog = gtk_file_chooser_dialog_new (_("Save file"),
                                          GTK_WINDOW(main_window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);

    //gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);

    if (last_dir)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), last_dir);

    //gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), dflt);

    g_free(filename);
    filename = NULL;

    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
        if (last_dir)
            g_free(last_dir);
        last_dir = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (dialog));
    }

    gtk_widget_destroy (dialog);
}

void update_connections_table(void)
{
    gint i;

    for (i = 0; i < NUM_CONNECTIONS; i++) {
        gint stars = 0;
        gchar buf[32];

        if (connections[i].destination.sin_addr.s_addr) {
            gtk_entry_set_text(GTK_ENTRY(connections[i].out_addr), inet_ntoa(connections[i].destination.sin_addr));
            if (connections[i].fd_out > 0) stars |= 2;
        } else {
            gtk_entry_set_text(GTK_ENTRY(connections[i].out_addr), "");
        }

        if (connections[i].caller.sin_addr.s_addr) {
            gtk_entry_set_text(GTK_ENTRY(connections[i].in_addr), inet_ntoa(connections[i].caller.sin_addr));
            if (connections[i].fd_conn > 0) stars |= 1;
        } else {
            gtk_entry_set_text(GTK_ENTRY(connections[i].in_addr), "");
        }

        snprintf(buf, sizeof(buf), "T%d [%c%c]", i+1,
                 stars & 1 ? '*' : '-', stars & 2 ? '*' : '-');
        gtk_notebook_set_tab_label_text(GTK_NOTEBOOK(notebook), connections[i].scrolled_window, buf);
    }
}

static void update_column(gint i)
{
    gint j;

    if (!iface[i].changed)
        return;

    iface[i].changed = FALSE;

    for (j = 0; j < NUM_DEVS; j++) {
        if (!iface[i].device[j].text)
            continue;

        // update connection table
        if (iface[i].device[j].apptype.type == APPLICATION_TYPE_TIMER &&
            iface[i].device[j].apptype.mode == APP_MODE_MASTER) {
            gint tatami = iface[i].device[j].apptype.tatami;
            if (tatami > 0 && tatami <= NUM_TATAMIS && connections[tatami-1].fd_out < 0) {
                connections[tatami-1].destination.sin_addr.s_addr = iface[i].device[j].addr;
                connections_updated = TRUE;
            }
        } else if (iface[i].device[j].apptype.type == APPLICATION_TYPE_CAMERA) {
            gint tatami = iface[i].device[j].apptype.tatami;
            if (tatami > 0 && tatami <= NUM_TATAMIS && connections[tatami-1].fd_conn < 0) {
                connections[tatami-1].caller.sin_addr.s_addr = iface[i].device[j].addr;
                connections_updated = TRUE;
            }
        }
    }
}

static gint check_table(gpointer data)
{
    gint i;
    for (i = 0; i < addrcnt; i++)
        update_column(i);

    if (connections_updated) {
        update_connections_table();
        connections_updated = FALSE;
    }
    return TRUE;
}

static struct app_type get_app_type(gchar *s)
{
    struct app_type at;
    at.type = 0;
    at.mode = 0;
    at.tatami = 0;

    if (strstr(s, "Camera")) {
        at.type = APPLICATION_TYPE_CAMERA;
        gchar *p = strstr(s, "Camera ");
        if (p) at.tatami = atoi(p+7);
    } else if (strstr(s, "JudoShiai"))
        at.type = APPLICATION_TYPE_SHIAI;
    else if (strstr(s, "JudoInfo"))
        at.type = APPLICATION_TYPE_INFO;
    else if (strstr(s, "JudoTimer")) {
        at.type = APPLICATION_TYPE_TIMER;
        gchar *p = strstr(s, "tatami=");
        if (p) at.tatami = atoi(p+7);

        if (strstr(s, "master"))
            at.mode = APP_MODE_MASTER;
        else if (strstr(s, "slave"))
            at.mode = APP_MODE_SLAVE;
    }

    return at;
}

struct application *report_to_proxy(gchar *rec, struct sockaddr_in *client, gint ifnum)
{
    //g_print("===============\n%s\n--------------\n", rec);

    uint32_t addr = client->sin_addr.s_addr;
    gchar *p1, *p = strstr(rec, "Judo");
    gint i;

    if (!p)
        return NULL;

    p1 = strchr(p, '\r');
    if (!p1) p1 = strchr(p, '\n');
    if (!p1)
        return NULL;

    *p1 = 0;

    p1 = strchr(p, '/');
    if (p1) *p1 = 0;

    //g_print("RX: '%s'\n", p);

    in_addr_t mask = 0xffffffff;

    while (mask) {
        for (i = 0; i < addrcnt; i++) {
            if ((ntohl(addr) & mask) == (ntohl(iface[i].addr) & mask)) {
                gint j;
                for (j = 0; j < NUM_DEVS; j++) {
                    if (!iface[i].device[j].text) {
                        iface[i].device[j].text = g_strdup(p);
                        iface[i].device[j].addr = addr;
                        iface[i].device[j].apptype = get_app_type(p);
                        iface[i].changed = TRUE;
                        g_print("new device if=%d dev=%d ifnum=%d addr=%s type=%d/%d %s\n",
                                i, j, ifnum,
                                inet_ntoa(client->sin_addr),
                                iface[i].device[j].apptype.type, iface[i].device[j].apptype.tatami,
                                iface[i].device[j].text);
                        return &(iface[i].device[j]);
                    } else if (strncmp(iface[i].device[j].text, p, 7) == 0 &&
                               iface[i].device[j].addr == addr) {
                        if (strcmp(iface[i].device[j].text, p)) {
                            g_free(iface[i].device[j].text);
                            iface[i].device[j].text = g_strdup(p);
                            iface[i].changed = TRUE;
                            g_print("updated device if=%d dev=%d ifnum=%d addr=%s %s\n", i, j, ifnum,
                                    inet_ntoa(client->sin_addr), iface[i].device[j].text);
                        }
                        struct app_type at = get_app_type(p);
                        if (memcmp(&iface[i].device[j].apptype, &at, sizeof(at))) {
                            iface[i].device[j].apptype = at;
                            iface[i].changed = TRUE;
                        }
                        return &(iface[i].device[j]);
                    }
                }
                g_print("No more space\n");
                return NULL;
            }
        }
        mask <<= 1;
    }
    g_print("Cannot find interface\n");

    return NULL;
}

#define SSDP_MULTICAST      "239.255.255.250"
#define SSDP_PORT           1900
#define URN ":urn:judoshiai:service:all:" SHIAI_VERSION
#define ST "ST" URN
#define NT "NT" URN

static gchar *ssdp_req_data = NULL;
static gint   ssdp_req_data_len = 0;
gchar ssdp_id[64];
//gboolean ssdp_notify = TRUE;

#define perror(x) do {g_print("ERROR LINE=%d\n", __LINE__); perror(x); } while (0)

gpointer proxy_ssdp_thread(gpointer args)
{
    gint ret;
    struct sockaddr_in name_out, name_in;
    struct sockaddr_in clientsock;
    static gchar inbuf[1024];
    fd_set read_fd, fds;
    guint socklen;
    struct ip_mreq mreq;
    gint i;
#ifdef WIN32
    const gchar *os = "Windows";
#else
    const gchar *os = "Linux";
#endif

    ssdp_req_data = g_strdup_printf("M-SEARCH * HTTP/1.1\r\n"
                                    "HOST: 239.255.255.250:1900\r\n"
                                    "MAN: \"ssdp:discover\"\r\n"
                                    ST "\r\n"
                                    "MX:3\r\n"
                                    "\r\n");
    ssdp_req_data_len = strlen(ssdp_req_data);

    FD_ZERO(&fds);

    // receiving socket
    for (i = 0; i < addrcnt; i++) {
        g_print("Initialize %x\n", iface[i].addr);

        if ((iface[i].fdin = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
            perror("SSDP socket");
            continue;
        }

        FD_SET(iface[i].fdin , &fds);

        gint reuse = 1;
        if (setsockopt(iface[i].fdin, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse)) < 0) {
            perror("setsockopt (SO_REUSEADDR)");
        }

#if 1 // SO_BINDTODEVICE requires root privileges
        struct ifreq ifr;
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", iface[i].name);
        if (setsockopt(iface[i].fdin, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {
            perror("bindtodevice in");
        }
#endif

        memset((gchar *)&name_in, 0, sizeof(name_in));
        name_in.sin_family = AF_INET;
        name_in.sin_port = htons(SSDP_PORT);
        name_in.sin_addr.s_addr = 0;//htonl(iface[i].addr);
        if (bind(iface[i].fdin, (struct sockaddr *)&name_in, sizeof(name_in)) == -1) {
            perror("SSDP bind");
        }

        memset(&mreq, 0, sizeof(mreq));
        mreq.imr_multiaddr.s_addr = inet_addr(SSDP_MULTICAST);
        mreq.imr_interface.s_addr = iface[i].addr;
        if (setsockopt(iface[i].fdin, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == -1) {
            perror("SSDP setsockopt");
        }

        // transmitting socket
        if ((iface[i].fdout = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
            perror("SSDP socket");
            continue;
        }

#if 1 // SO_BINDTODEVICE requires root privileges
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", iface[i].name);
        if (setsockopt(iface[i].fdout, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0) {
            perror("bindtodevice out");
        }
#endif
        FD_SET(iface[i].fdout , &fds);

        gint ttl = 3;
        setsockopt(iface[i].fdout, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&ttl, sizeof(ttl));
    } // for

    memset((gchar*)&name_out, 0, sizeof(name_out));
    name_out.sin_family = AF_INET;
    name_out.sin_port = htons(SSDP_PORT);
    name_out.sin_addr.s_addr = inet_addr(SSDP_MULTICAST);

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        struct application *appl;
	struct timeval timeout;
        gint len;

        timeout.tv_sec=1;
        timeout.tv_usec=0;
        read_fd = fds;

        if (select(32, &read_fd, NULL, NULL, &timeout) < 0) {
            perror("SSDP select");
            continue;
        }
#ifdef WEBKIT
	update_progress();
#endif
        for (i = 0; i < addrcnt; i++) {
            if (iface[i].fdin > 0 && FD_ISSET(iface[i].fdin, &read_fd)) {
                socklen = sizeof(clientsock);
                if ((len = recvfrom(iface[i].fdin, inbuf, sizeof(inbuf)-1, 0,
                                    (struct sockaddr *)&clientsock, &socklen)) < 0) {
                    perror("SSDP recvfrom");
                    continue;
                }
                inbuf[len] = 0;
                //g_print("RXin[%d]: '%s'\n", i, inbuf);

                if (strncmp(inbuf, "NOTIFY", 6) == 0) {
                    appl = report_to_proxy(inbuf, &clientsock, i);
                    appl = appl; // make compiler happy
                }
            }

            if (iface[i].fdout > 0 && FD_ISSET(iface[i].fdout, &read_fd)) {
                socklen = sizeof(clientsock);
                if ((len = recvfrom(iface[i].fdout, inbuf, sizeof(inbuf)-1, 0,
                                    (struct sockaddr *)&clientsock, &socklen)) < 0) {
                    perror("SSDP recvfrom");
                    continue;
                }
                inbuf[len] = 0;
                //g_print("RXout[%d]: '%s'\n", i, inbuf);

                if (strncmp(inbuf, "HTTP/1.1 200 OK", 12) == 0) {
                    appl = report_to_proxy(inbuf, &clientsock, i);
#if 0
                    if (0 && appl && appl->apptype.type == APPLICATION_TYPE_TIMER &&
                        appl->apptype.mode == APP_MODE_MASTER) {
                        gint j;

                        for (j = 0; j < addrcnt; j++) {
                            if (i == j || iface[j].fdout <= 0)
                                continue;

                            ret = sendto(iface[j].fdout, inbuf, len, 0,
                                         (struct sockaddr*) &name_out,
                                         sizeof(struct sockaddr_in));
                            g_print("SSDP message forwarded to port %d\n", j);

                        } // for
                    } // if
#endif
                } // if (strncmp(inbuf, "HTTP/1.1 200 OK", 12) == 0)
            }
        }

        static time_t last_time = 0;
        time_t now = time(NULL);

        if (now > last_time + 5) {
            last_time = now;

            for (i = 0; i < addrcnt; i++) {
                if (iface[i].fdout <= 0)
                    continue;

		if (iface[i].ssdp_msg == NULL) {
#define URN ":urn:judoshiai:service:all:" SHIAI_VERSION
#define ST "ST" URN
#define NT "NT" URN
		    struct sockaddr_in a;
		    a.sin_addr.s_addr = iface[i].addr;

		    iface[i].ssdp_msg =
			g_strdup_printf("NOTIFY * HTTP/1.1\r\n"
					"HOST: 239.255.255.250:1900\r\n"
					"CACHE-CONTROL: max-age=1800\r\n"
					"LOCATION: http://%s/UPnP/desc.xml\r\n"
					NT "\r\n"
					"NTS: ssdp-alive\r\n"
					"SERVER:%s/1 UPnP/1.0 %s/%s\r\n"
					"USN: uuid:%08x-cafe-babe-737%d-%012x\r\n"
					"\r\n",
					inet_ntoa(a.sin_addr), os,
					ssdp_id,
					SHIAI_VERSION, my_address,
					application_type(), iface[i].addr);
		    iface[i].ssdp_msg_len = strlen(iface[i].ssdp_msg);
		}

		if (advertise_addr) {
		    ret = sendto(iface[i].fdout, iface[i].ssdp_msg,
				 iface[i].ssdp_msg_len, 0,
				 (struct sockaddr*) &name_out,
				 sizeof(struct sockaddr_in));

		    //g_print("SSDP %s REQ SEND by timeout\n\n", APPLICATION);
		    if (ret != iface[i].ssdp_msg_len) {
			perror("SSDP send req");
		    }
		}
            } // for
        }
    } // eternal for

    g_print("SSDP OUT!\n");
    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}


void scan_interfaces(void)
{
    addrcnt = 0;

#ifdef WIN32
    gint i;

    SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
    if (sd == SOCKET_ERROR) {
        return 0;
    }

    INTERFACE_INFO InterfaceList[20];
    unsigned long nBytesReturned;
    if (WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
                 sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) {
        closesocket(sd);
        return 0;
    }

    gint nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

    for (i = 0; i < nNumInterfaces; ++i) {
        struct sockaddr_in *pAddress;
        pAddress = (struct sockaddr_in *) & (InterfaceList[i].iiAddress);
        g_print("Interface %s\n", inet_ntoa(pAddress->sin_addr));
        if (addrcnt < NUM_INTERFACES) {
            iface[addrcnt].name = "";
            iface[addrcnt++].addr = pAddress->sin_addr.s_addr;
        }
    }

    closesocket(sd);

#else // !WIN32

    struct if_nameindex *ifnames, *ifnm;
    struct ifreq ifr;
    struct sockaddr_in sin;
    gint   fd;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return;

    ifnames = if_nameindex();
    for (ifnm = ifnames; ifnm && ifnm->if_name && ifnm->if_name[0]; ifnm++) {
        strncpy (ifr.ifr_name, ifnm->if_name, IFNAMSIZ);
        if (ioctl (fd, SIOCGIFADDR, &ifr) == 0) {
            memcpy (&sin, &(ifr.ifr_addr), sizeof (sin));
            if (addrcnt < NUM_INTERFACES) {
                iface[addrcnt].name = g_strdup(ifnm->if_name);
                iface[addrcnt++].addr = sin.sin_addr.s_addr;
                g_print ("Proxy Interface %s = IP %s\n", ifnm->if_name, inet_ntoa(sin.sin_addr));
            }
        }
    }
    if_freenameindex(ifnames);
    close(fd);

#endif // WIN32
}

static gpointer connection_thread(gpointer args)
{
    gint i;
    struct sockaddr_in my_addr, node;
    static guchar inbuf[2048];
    fd_set read_fd, fds;
    gint reuse = 1;

    FD_ZERO(&read_fd);

    for (i = 0; i < NUM_CONNECTIONS; i++) {
        if ((connections[i].fd_listen = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
            perror("listen socket");
            g_thread_exit(NULL);    /* not required just good pratice */
            return NULL;
        }

        if (setsockopt(connections[i].fd_listen, SOL_SOCKET, SO_REUSEADDR, (void *)&reuse, sizeof(reuse)) < 0) {
            perror("setsockopt (SO_REUSEADDR)");
        }

        memset(&my_addr, 0, sizeof(my_addr));
        my_addr.sin_family = AF_INET;
        my_addr.sin_port = htons(START_PORT + i);
        my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(connections[i].fd_listen, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) < 0) {
            perror("listen bind");
            g_print("CANNOT BIND!\n");
            g_thread_exit(NULL);    /* not required just good pratice */
            return NULL;
        }

        listen(connections[i].fd_listen, 5);

        FD_SET(connections[i].fd_listen, &read_fd);
        connections[i].fd_conn = -1;
        connections[i].fd_out = -1;
    }

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        struct timeval timeout;
        gint r, i;

        fds = read_fd;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        r = select(64, &fds, NULL, NULL, &timeout);

        for (i = 0; i < NUM_CONNECTIONS; i++) {
            if (connections[i].fd_out > 0 && connections[i].changed) {
                g_print("Closing out connection %d\n", i);
                closesocket(connections[i].fd_out);
                connections[i].fd_out = -1;
                connections_updated = TRUE;
            }

            if (connections[i].fd_out < 0 && connections[i].destination.sin_addr.s_addr) {
                if ((connections[i].fd_out = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
                    perror("out socket");
                    continue;
                }
                connections[i].changed = FALSE;

                memset(&node, 0, sizeof(node));
                node.sin_family      = AF_INET;
                node.sin_port        = htons(SHIAI_PORT+2);
                node.sin_addr.s_addr = connections[i].destination.sin_addr.s_addr;

                if (connect(connections[i].fd_out, (struct sockaddr *)&node, sizeof(node))) {
                    closesocket(connections[i].fd_out);
                    connections[i].fd_out = -1;
                    g_print("Connection out %d to %s failed!\n", i, inet_ntoa(node.sin_addr));
                } else {
                    FD_SET(connections[i].fd_out, &read_fd);
                    g_print("Out connection %d (fd=%d) to %s\n", i,
                            connections[i].fd_out, inet_ntoa(node.sin_addr));
                    connections_updated = TRUE;
                }
            }

            if (r <= 0)
                continue;

            /* messages to receive */
            if (FD_ISSET(connections[i].fd_listen, &fds)) {
                guint alen = sizeof(connections[i].caller);
                if (connections[i].fd_conn < 0) {
                    if ((connections[i].fd_conn = accept(connections[i].fd_listen,
                                                         (struct sockaddr *)&connections[i].caller,
                                                         &alen)) < 0) {
                        perror("conn accept");
                        continue;
                    } else {
                        FD_SET(connections[i].fd_conn, &read_fd);
                        connections_updated = TRUE;
                        g_print("New connection in %d (fd=%d) from %s\n", i, connections[i].fd_conn,
                                inet_ntoa(connections[i].caller.sin_addr));
			//reload_web(NULL, gint_to_ptr(i));
                    }
                } else {
                    g_print("Connection %d already in use.", i);
                }
            }

            if (connections[i].fd_conn > 0 && FD_ISSET(connections[i].fd_conn, &fds)) {
                r = recv(connections[i].fd_conn, inbuf, sizeof(inbuf), 0);
                if (r <= 0) {
                    g_print("Connection in %d fd=%d closed\n", i, connections[i].fd_conn);
                    closesocket(connections[i].fd_conn);
                    FD_CLR(connections[i].fd_conn, &read_fd);
                    connections[i].fd_conn = -1;
                    connections_updated = TRUE;
                } else {
                    if (connections[i].fd_out > 0) {
                        send(connections[i].fd_out, inbuf, r, 0);
                    }
                }
            }

            if (connections[i].fd_out > 0 && FD_ISSET(connections[i].fd_out, &fds)) {
                r = recv(connections[i].fd_out, inbuf, sizeof(inbuf), 0);
                if (r <= 0) {
                    g_print("Connection out %d fd=%d closed\n", i, connections[i].fd_out);
                    closesocket(connections[i].fd_out);
                    FD_CLR(connections[i].fd_out, &read_fd);
                    connections[i].fd_out = -1;
                    connections_updated = TRUE;
                } else {
                    if (connections[i].fd_conn > 0) {
                        send(connections[i].fd_conn, inbuf, r, 0);
                    }
                }
            }

        } // for connections


    } // outer for

    g_print("CONNECT OUT!\n");
    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

void toggle_advertise(GtkWidget *menu_item, gpointer data)
{
    advertise_addr = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item));
    g_key_file_set_boolean(keyfile, "preferences", "advertise", advertise_addr);
}
