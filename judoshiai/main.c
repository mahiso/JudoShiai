/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

#if (GTKVER == 3)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#include <curl/curl.h>

#ifdef WIN32

#if 0
#include <windef.h>
#include <ansidecl.h>
#include <winbase.h>
#include <winnt.h>
#endif

#include <process.h>
//#include <glib/gwin32.h>
//#include "dbghelp.h"
#else
#include <sys/types.h>
#include <unistd.h>
#include <sys/resource.h>
#endif

//#include "shiai-i18n.h"
#include "judoshiai.h"
#include "language.h"
#include "binreloc.h"

guint current_year;
gchar *installation_dir = NULL;
GKeyFile *keyfile;
gchar *conffile, *lockfile;
#if (GTKVER == 3)
GtkEntryCompletion *club_completer;
#else
GCompletion *club_completer;
#endif
gint print_lang = 0, club_text = 0, club_abbr = 0, draw_system = 0;
gboolean first_instance = FALSE;
gint number_of_tatamis = 3;
gint language = LANG_EN;
gchar *use_logo = NULL;
gboolean print_headers = FALSE;

static gboolean delete_event( GtkWidget *widget,
                              GdkEvent  *event,
                              gpointer   data )
{
    /* If you return FALSE in the "delete_event" signal handler,
     * GTK will emit the "destroy" signal. Returning TRUE means
     * you don't want the window to be destroyed.
     * This is useful for popping up 'are you sure you want to quit?'
     * type dialogs. */

    g_print ("delete event occurred\n");

    /* Change TRUE to FALSE and the main window will be destroyed with
     * a "delete_event". */

    //return TRUE;
    return FALSE;
}

extern void print_stat(void);
extern void db_close(void);

/* Another callback */
void destroy( GtkWidget *widget,
	      gpointer   data )
{
    gint x, y, w, h;

    print_stat();

    if (first_instance && lockfile) {
        g_unlink(lockfile);
    }

    gtk_window_get_position(GTK_WINDOW(main_window), &x, &y);
    gtk_window_get_size(GTK_WINDOW(main_window), &w, &h);
    if (x < 0) { w += x - 10; x = 0; } 
    if (y < 0) { h += y - 10; y = 0; } 
    g_key_file_set_integer(keyfile, "preferences", "win_x_pos", x);
    g_key_file_set_integer(keyfile, "preferences", "win_y_pos", y);
    g_key_file_set_integer(keyfile, "preferences", "win_width", w);
    g_key_file_set_integer(keyfile, "preferences", "win_height", h);

    gsize length;
    gchar *inidata = g_key_file_to_data (keyfile,
                                         &length,
                                         NULL);
    g_file_set_contents(conffile, inidata, length, NULL);
    g_free(inidata);
    g_key_file_free(keyfile);

    gtk_main_quit ();
}

/* globals */
gchar         *program_path;
GtkWidget     *notebook = NULL;
GtkWidget     *main_vbox = NULL;
GtkWidget     *progress_bar = NULL;
guint          current_index = 10;
guint          current_category_index = 10000;
GtkWidget     *current_view = NULL;
GtkTreeModel  *current_model = NULL;
gint           current_category;
GtkWidget     *main_window = NULL;
gchar          current_directory[1024] = {0};
gint           my_address;
GtkWidget     *notes = NULL;
gboolean       automatic_sheet_update = FALSE;
gboolean       automatic_web_page_update = FALSE;
gboolean       print_svg = FALSE;
gboolean       weights_in_sheets = FALSE;
gboolean       grade_visible = FALSE;
gint           name_layout = 0;
gboolean       pool_style = FALSE;
gboolean       belt_colors = FALSE;
gboolean       cleanup_import = FALSE;

GdkCursor     *wait_cursor = NULL;

time_t msg_out_start_time = 0, msg_out_err_time = 0;
gulong msg_out_addr = 0; 
gint msg_out_text_set_tmo = 0;

void progress_show(gdouble progress, const gchar *text)
{
    msg_out_text_set_tmo = 0;

    if (!progress_bar)
        return;

    if (progress >= 0.0 && progress <= 1.0)
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), progress);

    if (text)
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), text);

    //gdk_window_process_all_updates();
    refresh_window();
}

static gboolean check_for_connection_status(gpointer data)
{
    gchar buf[64];
    time_t now = time(NULL);

    if (msg_out_start_time && now > msg_out_start_time + 1) {
        msg_out_err_time = now - msg_out_start_time;
        snprintf(buf, sizeof(buf), "%s: %ld.%ld.%ld.%ld (%ld s)",
                 _("Communication error"),
                 (msg_out_addr)&0xff, (msg_out_addr>>8)&0xff, 
                 (msg_out_addr>>16)&0xff, (msg_out_addr>>24)&0xff, 
                 msg_out_err_time);
        if (!msg_out_text_set_tmo)
            shiai_log(2, 0, buf);
        progress_show(now & 1 ? 1.0 : 0.0, buf);
        msg_out_text_set_tmo = 5;
    } else {
        if (msg_out_err_time) {
            snprintf(buf, sizeof(buf), "%s OK (%ld s)",
                     _("Communication error"), msg_out_err_time);
            shiai_log(2, 0, buf);
            msg_out_err_time = 0;
        }

        if (msg_out_text_set_tmo) {
            msg_out_text_set_tmo--;
            if (msg_out_text_set_tmo == 0)
                progress_show(0.0, "");
        }
    }

    return TRUE;
}

#if 0
static gboolean do_invalidation(gpointer data)
{
    g_print("invalidate\n");
    gdk_window_invalidate_rect(gtk_widget_get_window(main_window), NULL, TRUE);
    return FALSE;
}

static gboolean change_page(GtkNotebook *notebook, gint arg1, gpointer user_data)
{
    g_print("notebook page change\n");
    g_timeout_add(1000, do_invalidation, NULL);
    return FALSE;
}
#endif

void open_shiai_display(void)
{
    gint r;

    if (notebook)
        gtk_widget_destroy(notebook);
    notebook = NULL;
	
    current_index = 10;
    current_category_index = 10000;
    current_category = 0;

    r = db_init(database_name);
    if (r != 0 && r != 55555) {
        SHOW_MESSAGE("%s: %s", _("Cannot open"), database_name);
        return;
    }

    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos((GtkNotebook *)notebook, GTK_POS_TOP);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(main_vbox), notebook, 0, 2, 1, 1);
    gtk_widget_set_hexpand(notebook, TRUE);
    gtk_widget_set_vexpand(notebook, TRUE);
#else
    gtk_box_pack_end(GTK_BOX(main_vbox), notebook, TRUE, TRUE, 0);
#endif
    set_judokas_page(notebook);
    set_sheet_page(notebook);
    set_category_graph_page(notebook);
    set_match_graph_page(notebook);
    set_match_pages(notebook);
    set_log_page(notebook);

    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);

    gtk_widget_show_all(notebook);

    gchar *title = g_strdup_printf("JudoShiai - %s", database_name);
    gtk_window_set_title(GTK_WINDOW(main_window), title);
    g_free(title);

    gategories_refresh();
    update_match_pages_visibility();
#if 0
    g_signal_connect(G_OBJECT(notebook), 
                     "switch-page", G_CALLBACK(change_page), NULL);
#endif
    SYS_LOG_INFO("%s %s", _("Tournament"), database_name);

    if (r == 55555)
        properties(NULL, NULL);
}

#if 0
static gint find_shi_file(gchar *path, gint level)
{
    gint ret = 0;

    if (level >= 2)
        return 0;

    if (!g_file_test(path, G_FILE_TEST_IS_DIR))
        return 0;
        
    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir)
        return 0;

    const gchar *name = g_dir_read_name(dir);
    while (name && ret == 0) {
        gchar *newpath = g_build_filename(path, name, NULL);

        if (g_file_test(newpath, G_FILE_TEST_IS_DIR)) {
            ret = find_shi_file(newpath, level+1);
        } else if (g_str_has_suffix(name, ".shi")) {
            strcpy(current_directory, path);
            ret = 1;
        }

        g_free(newpath);
        name = g_dir_read_name(dir);
    }

    g_dir_close(dir);
    return ret;
}
#endif

#ifdef WIN32XXX
LONG WINAPI win_exp_filter(struct _EXCEPTION_POINTERS *exception_info)
{
    HANDLE f = CreateFile("JudoShiai.core", GENERIC_WRITE,
                          0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                          NULL);
    MINIDUMP_EXCEPTION_INFORMATION info;
    info.ThreadId = GetCurrentThreadId();
    info.ExceptionPointers = exception_info;
    info.ClientPointers = TRUE;

    MiniDumpWriteDump(GetCurrentProcess(),
                      GetCurrentProcessId(),
                      f,
                      (MINIDUMP_TYPE)(MiniDumpWithFullMemory|MiniDumpWithHandleData),
                      &info,
                      NULL, NULL);

    CloseHandle(f);

    return EXCEPTION_EXECUTE_HANDLER;
}

#endif

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

    putenv("UBUNTU_MENUPROXY=");

#ifndef WIN32
    struct rlimit rlp;
    getrlimit(RLIMIT_CORE, &rlp);
    g_print("RLIMIT_CORE: %ld/%ld\n", rlp.rlim_cur, rlp.rlim_max);
    rlp.rlim_cur = 100000000;
    g_print("Setting to max: %d\n", setrlimit(RLIMIT_CORE, &rlp));
#else
    //SetUnhandledExceptionFilter(&win_exp_filter);
#endif

    memset(&next_matches_info, 0, sizeof(next_matches_info));

#ifdef WIN32
#if (GTKVER == 3)
    installation_dir = g_win32_get_package_installation_directory_of_module(NULL);
#else
    installation_dir = g_win32_get_package_installation_directory(NULL, NULL);
#endif
#else
    gbr_init(NULL);
    installation_dir = gbr_find_prefix(NULL);
#endif
    g_print("installation_dir=%s\n", installation_dir);
    program_path = argv[0];

    current_directory[0] = 0;

#if 0
#ifdef WIN32
    if (find_shi_file("E:\\", 0)) goto ok;
    if (find_shi_file("F:\\", 0)) goto ok;
#else
    if (find_shi_file("/media", 0)) goto ok;
    if (find_shi_file("/mnt", 0)) goto ok;
    if (g_file_test("/mnt/sda1_removable", G_FILE_TEST_IS_DIR)) {
        strcpy(current_directory, "/mnt/sda1_removable");
        goto ok;
    }
#endif
ok:
#endif

    if (current_directory[0] == 0)
        strcpy(current_directory, ".");
    g_print("current_directory=%s\n", current_directory);

    conffile = g_build_filename(g_get_user_data_dir(), "judoshiai.ini", NULL);
    g_print("conf file = %s\n", conffile);
    keyfile = g_key_file_new();
    g_key_file_load_from_file(keyfile, conffile, 0, NULL);

    now = time(NULL);
    tm = localtime(&now);
    current_year = tm->tm_year+1900;
    srand(now); //srandom(now);
    my_address = now + getpid()*10000;
	
    lockfile = g_build_filename(g_get_user_data_dir(), "judoshiai.lck", NULL);
    g_print("lock file = %s\n", lockfile);
    if (g_file_test(lockfile, G_FILE_TEST_EXISTS)) {
        first_instance = FALSE;
        g_print("Second instance\n");
    } else {
        /* create a lock file */
        first_instance = TRUE;
        FILE *f = fopen(lockfile, "w");
        if (f) {
            fprintf(f, "%ld\n", now);
            fclose(f);
        }
        g_print("First instance\n");
    }

    init_print_texts();

#if 0
    g_print("LOCALE = %s homedir=%s configdir=%s\n", 
            setlocale(LC_ALL, 0), g_get_home_dir(), g_get_user_config_dir());
#endif

#if (GTKVER != 3)
    g_thread_init(NULL);    /* Initialize GLIB thread support */
    gdk_threads_init();     /* Initialize GDK locks */
    gdk_threads_enter();    /* Acquire GDK locks */ 
#endif

    gtk_init (&argc, &argv);

    /*-------- CSS -------------------------------------------*/
    gchar *file = g_build_filename(installation_dir, "etc", "gtk.css", NULL);

    if (g_file_test(file, G_FILE_TEST_EXISTS)) {
        GtkCssProvider *provider = gtk_css_provider_new();
        gtk_css_provider_load_from_path(provider, file, NULL);

        GdkDisplay *display = gdk_display_get_default();
        GdkScreen *screen = gdk_display_get_default_screen(display);

        gtk_style_context_add_provider_for_screen (screen,
                                                   GTK_STYLE_PROVIDER (provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref (provider);
    }

    g_free(file);
    /*--------------------------------------------------------*/


    wait_cursor = gdk_cursor_new(GDK_WATCH);

    main_window = window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(main_window), "JudoShiai");
    gtk_widget_set_size_request(window, FRAME_WIDTH, FRAME_HEIGHT);

    gchar *iconfile = g_build_filename(installation_dir, "etc", "judoshiai.png", NULL);
    gtk_window_set_default_icon_from_file(iconfile, NULL);
    g_free(iconfile);

    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event), NULL);
    
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (destroy), NULL);

    gtk_container_set_border_width (GTK_CONTAINER (window), 10);
    
#if (GTKVER == 3)
    main_vbox = gtk_grid_new();
#else
    main_vbox = gtk_vbox_new(FALSE, 1);
#endif
    gtk_container_set_border_width (GTK_CONTAINER (main_vbox), 1);
    gtk_container_add (GTK_CONTAINER (window), main_vbox);
    gtk_widget_show(main_vbox);

    menubar = get_menubar_menu (window);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(main_vbox), menubar, 0, 0, 1, 1);
    gtk_widget_set_hexpand(menubar, TRUE);
    //gtk_widget_set_halign(menubar, GTK_ALIGN_FILL);
#else
    gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, TRUE, 0);
#endif
    gtk_widget_show(menubar);

    //notes = gtk_label_new("");
    //gtk_box_pack_start(GTK_BOX(main_vbox), notes, FALSE, TRUE, 0);
    //gtk_widget_show(notes);

    progress_bar = gtk_progress_bar_new();
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(main_vbox), progress_bar, 0, 1, 1, 1);
#else
    gtk_box_pack_start (GTK_BOX (main_vbox), progress_bar, FALSE, FALSE, 0);
#endif
    gtk_widget_show_all(window);

#if 0
    main_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_app_paintable(main_window, TRUE);
    //gtk_window_set_decorated(GTK_WINDOW(main_window), FALSE);
    gtk_widget_show_all(main_window);
#endif

    init_club_name_tree();
    club_completer = club_completer_new();

    set_font("Arial 12");
    set_preferences();
    change_language(NULL, NULL, gint_to_ptr(language));

    if (!first_instance) {
        GtkWidget *dialog = 
            gtk_dialog_new_with_buttons(_("Is there another JudoShiai program running?"),
                                        NULL,
                                        GTK_DIALOG_MODAL,
                                        _("Yes, another JudoShiai program is running"), 1000,
                                        _("No, this the only JudoShiai program. \nPurge erroneous lock file."), 1001,
                                        NULL);
        gtk_widget_show_all(dialog);
        gint response = gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        if (response == 1001)
            first_instance = TRUE;
        else
            show_message(_("Communication is blocked! \nYou can still use the program to create categories etc."));
    }

    if (first_instance) {
        g_snprintf(ssdp_id, sizeof(ssdp_id), "JudoShiai");

        /* Only the first program can communicate. */
        open_comm_socket();

        /* libcurl init */
        curl_global_init(CURL_GLOBAL_NOTHING);

        /* Create a bg thread using glib */
        gth = gth; // make compiler happy
#if (GTKVER == 3)
    gth = g_thread_new("Node",
                       (GThreadFunc)node_thread,
                       (gpointer)&run_flag); 

    gth = g_thread_new("HTTPD",
                       (GThreadFunc)httpd_thread,
                       (gpointer)&run_flag); 

    gth = g_thread_new("Serial",
                       (GThreadFunc)serial_thread,
                       (gpointer)&run_flag); 

    gth = g_thread_new("FTP",
                       (GThreadFunc)ftp_thread,
                       (gpointer)&run_flag); 

    gth = g_thread_new("SSDP",
                       (GThreadFunc)ssdp_thread,
                       (gpointer)&run_flag);
#else // GTKVER != 3

#if 0 // NO SERVER THREAD
        gth = g_thread_create((GThreadFunc)server_thread,
                              (gpointer)&run_flag, FALSE, NULL); 
#endif // NO SERVER THREAD

        gth = g_thread_create((GThreadFunc)node_thread,
                              (gpointer)&run_flag, FALSE, NULL); 

#if 0 // NO CLIENT THREAD
#if 1
        gth = g_thread_create((GThreadFunc)client_thread,
                              (gpointer)&run_flag, FALSE, NULL); 
#else
        idle_id = g_idle_add(client_thread, NULL);
#endif
#endif // NO CLIENT THREAD

#if 1
        gth = g_thread_create((GThreadFunc)httpd_thread,
                              (gpointer)&run_flag, FALSE, NULL); 
#endif
        gth = g_thread_create((GThreadFunc)serial_thread,
                              (gpointer)&run_flag, FALSE, NULL); 

        gth = g_thread_create((GThreadFunc)ftp_thread,
                              (gpointer)&run_flag, FALSE, NULL); 

        gth = g_thread_create((GThreadFunc)ssdp_thread,
                              (gpointer)&run_flag, FALSE, NULL); 
#endif // GTKVER == 3

        g_timeout_add(1000, check_for_connection_status, NULL);

        g_print("Comm threads started\n");
    }

    if (argc > 1 && argv[1][0] != '-') {
        strncpy(database_name, argv[1], sizeof(database_name)-1);
        strncpy(logfile_name,  argv[1], sizeof(logfile_name)-1);
        gchar *p = strstr(logfile_name, ".shi");
        if (p)
            strcpy(p, ".log");
        else
            logfile_name[0] = 0;
        open_shiai_display();
        //valid_ascii_string(database_name);
    }

    update_match_pages_visibility();

    /* All GTK applications must have a gtk_main(). Control ends here
     * and waits for an event to occur (like a key press or
     * mouse event). */
    gtk_main();
    
#if (GTKVER != 3)    
    gdk_threads_leave();  /* release GDK locks */
#endif

    run_flag = FALSE;     /* flag threads to stop and exit */
    //g_thread_join(gth);   /* wait for thread to exit */ 

    curl_global_cleanup();

    return 0;
}

gboolean this_is_shiai(void)
{
    return TRUE;
}

gint application_type(void)
{
    return APPLICATION_TYPE_SHIAI;
}

void refresh_window(void)
{
    GtkWidget *widget;
    GdkRegion *region;
    widget = GTK_WIDGET(main_window);
#if (GTKVER == 3)
    if (gtk_widget_get_window(widget)) {
        cairo_rectangle_int_t r;
        r.x = 0;
        r.y = 0;
        r.width = gtk_widget_get_allocated_width(widget);
        r.height = gtk_widget_get_allocated_height(widget);
        region = cairo_region_create_rectangle(&r);
        gdk_window_invalidate_region(gtk_widget_get_window(widget), region, TRUE);
        gdk_window_process_updates(gtk_widget_get_window(widget), TRUE);
        cairo_region_destroy(region);
    }
#else
    if (widget->window) {
        region = gdk_drawable_get_clip_region(widget->window);
        gdk_window_invalidate_region(widget->window, region, TRUE);
        gdk_window_process_updates(widget->window, TRUE);
    }
#endif
}

/*** profiling stuff ***/
struct profiling_data prof_data[NUM_PROF];
gint num_prof_data;
guint64 prof_start;
gboolean prof_started = FALSE;
