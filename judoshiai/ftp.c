/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <curl/curl.h>

#include "sqlite3.h"
#include "judoshiai.h"

#define PROTO_FTP 0
#define PROTO_HTTP 1

static void ftp_log(gchar *format, ...);

struct url {
    GtkWidget *address, *port, *path, *start;
    GtkWidget *user, *password, *proxy_address, *proxy_port;
    GtkWidget *do_it, *proto, *proxy_user, *proxy_password;
};

static gchar *ftp_server = NULL, *ftp_path = NULL, *proxy_host = NULL,
    *ftp_user = NULL, *ftp_password = NULL, *proxy_user, *proxy_password;
static gint ftp_port = 21, proxy_port = 8080, proto = PROTO_FTP;
static gboolean ftp_update = FALSE, do_ftp = FALSE;

#define STRDUP(_d, _s) do { g_free(_d); _d = g_strdup(_s); } while (0)
#define GETSTR(_d, _s) do { g_free(_d);                                 \
        _d = g_key_file_get_string(keyfile, "preferences", _s, NULL);   \
        if (!_d) _d = g_strdup(""); } while (0)
#define GETINT(_d, _s) do { GError *x=NULL; _d = g_key_file_get_integer(keyfile, "preferences", _s, &x); } while (0)

static void ftp_to_server_callback(GtkWidget *widget,
                                   GdkEvent *event,
                                   GtkWidget *data)
{
    struct url *uri = (struct url *)data;

    if (ptr_to_gint(event) == GTK_RESPONSE_OK) {
        proto = gtk_combo_box_get_active(GTK_COMBO_BOX(uri->proto));
        if (proto < 0) proto = 0;
        g_key_file_set_integer(keyfile, "preferences", "ftpproto", proto);

        STRDUP(ftp_server, gtk_entry_get_text(GTK_ENTRY(uri->address)));
        g_key_file_set_string(keyfile, "preferences", "ftpserver", ftp_server);

        ftp_port = atoi(gtk_entry_get_text(GTK_ENTRY(uri->port)));
        g_key_file_set_integer(keyfile, "preferences", "ftpport", ftp_port);

        STRDUP(ftp_path, gtk_entry_get_text(GTK_ENTRY(uri->path)));
        g_key_file_set_string(keyfile, "preferences", "ftppath", ftp_path);

        STRDUP(proxy_host, gtk_entry_get_text(GTK_ENTRY(uri->proxy_address)));
        g_key_file_set_string(keyfile, "preferences", "ftpproxyaddress", proxy_host);

        proxy_port = atoi(gtk_entry_get_text(GTK_ENTRY(uri->proxy_port)));
        g_key_file_set_integer(keyfile, "preferences", "ftpproxyport", proxy_port);

        STRDUP(ftp_user, gtk_entry_get_text(GTK_ENTRY(uri->user)));
        g_key_file_set_string(keyfile, "preferences", "ftpuser", ftp_user);

        STRDUP(ftp_password, gtk_entry_get_text(GTK_ENTRY(uri->password)));
        g_key_file_set_string(keyfile, "preferences", "ftppassword", ftp_password);

        STRDUP(proxy_user, gtk_entry_get_text(GTK_ENTRY(uri->proxy_user)));
        g_key_file_set_string(keyfile, "preferences", "ftpproxyuser", proxy_user);

        STRDUP(proxy_password, gtk_entry_get_text(GTK_ENTRY(uri->proxy_password)));
        g_key_file_set_string(keyfile, "preferences", "ftpproxypassword", proxy_password);

        do_ftp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(uri->do_it));

        ftp_update = TRUE;
    }

    g_free(uri);
    gtk_widget_destroy(widget);
}

void ftp_to_server(GtkWidget *w, gpointer data)
{
    gchar buf[128];
    GtkWidget *dialog, *hbox, *hbox1, *hbox2, *hbox3;
    struct url *uri = g_malloc0(sizeof(*uri));
    GtkWidget *proxy_lbl = gtk_label_new(_("Proxy:"));

    dialog = gtk_dialog_new_with_buttons (_("Server URL"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          NULL);

    GETINT(proto, "ftpproto");
#if (GTKVER == 3)
    uri->proto = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(uri->proto), NULL, "ftp");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(uri->proto), NULL, "http");
#else
    uri->proto = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(uri->proto), "ftp");
    gtk_combo_box_append_text(GTK_COMBO_BOX(uri->proto), "http");
#endif
    gtk_combo_box_set_active(GTK_COMBO_BOX(uri->proto), proto);

    GETSTR(ftp_server, "ftpserver");
    uri->address = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->address), ftp_server);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->address), 20);

    GETINT(ftp_port, "ftpport");
    sprintf(buf, "%d", ftp_port);
    uri->port = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->port), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->port), 4);

    GETSTR(ftp_path, "ftppath");
    uri->path = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->path), ftp_path);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->path), 16);

    GETSTR(proxy_host, "ftpproxyaddress");
    uri->proxy_address = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->proxy_address), proxy_host);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->proxy_address), 20);

    GETINT(proxy_port, "ftpproxyport");
    sprintf(buf, "%d", proxy_port);
    uri->proxy_port = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->proxy_port), buf);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->proxy_port), 4);

    GETSTR(ftp_user, "ftpuser");
    uri->user = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->user), ftp_user);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->user), 20);

    GETSTR(ftp_password, "ftppassword");
    uri->password = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->password), ftp_password);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->password), 20);

    GETSTR(proxy_user, "ftpproxyuser");
    uri->proxy_user = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->proxy_user), proxy_user);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->proxy_user), 20);

    GETSTR(proxy_password, "ftpproxypassword");
    uri->proxy_password = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(uri->proxy_password), proxy_password);
    gtk_entry_set_width_chars(GTK_ENTRY(uri->proxy_password), 20);

    uri->do_it = gtk_check_button_new_with_label(_("Copy to Server"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(uri->do_it), do_ftp);

    //gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

#if (GTKVER == 3)
    hbox = gtk_grid_new();
    gtk_grid_attach_next_to(GTK_GRID(hbox), uri->proto, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox), gtk_label_new("://"), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox), uri->address, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox), gtk_label_new(":"), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox), uri->port, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox), gtk_label_new("/"), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox), uri->path, NULL, GTK_POS_RIGHT, 1, 1);

    hbox2 = gtk_grid_new();
    gtk_grid_attach_next_to(GTK_GRID(hbox2), gtk_label_new(_("User:")), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox2), uri->user, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox2), gtk_label_new(_("Password:")), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox2), uri->password, NULL, GTK_POS_RIGHT, 1, 1);

    hbox1 = gtk_grid_new();
    gtk_grid_attach_next_to(GTK_GRID(hbox1), proxy_lbl, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox1), uri->proxy_address, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox1), gtk_label_new(":"), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox1), uri->proxy_port, NULL, GTK_POS_RIGHT, 1, 1);

    hbox3 = gtk_grid_new();
    gtk_grid_attach_next_to(GTK_GRID(hbox3), gtk_label_new(_("Proxy User:")), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox3), uri->proxy_user, NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox3), gtk_label_new(_("Proxy Password:")), NULL, GTK_POS_RIGHT, 1, 1);
    gtk_grid_attach_next_to(GTK_GRID(hbox3), uri->proxy_password, NULL, GTK_POS_RIGHT, 1, 1);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       hbox2, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       hbox1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       hbox3, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       uri->do_it, FALSE, FALSE, 0);
#else
    hbox = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox), uri->proto, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new("://"), FALSE, FALSE, 0);
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
    gtk_box_pack_start(GTK_BOX(hbox1), proxy_lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), uri->proxy_address, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), gtk_label_new(":"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox1), uri->proxy_port, FALSE, FALSE, 0);

    hbox3 = gtk_hbox_new(FALSE, 4);
    gtk_box_pack_start(GTK_BOX(hbox3), gtk_label_new(_("Proxy User:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox3), uri->proxy_user, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox3), gtk_label_new(_("Proxy Password:")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox3), uri->proxy_password, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox2, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox1, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox3, FALSE, FALSE, 4);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), uri->do_it, FALSE, FALSE, 4);
#endif
    gtk_widget_show_all(dialog);

    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(ftp_to_server_callback), uri);
}

static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
    size_t retcode = fread(ptr, size, nmemb, stream);
    return retcode;
}

static CURL *curl = NULL;
static struct curl_slist *headerlist = NULL;

static void init_curl(void)
{
    CURLcode err;

    curl = curl_easy_init();
    if (!curl) {
        ftp_log("Cannot curl_easy_init!");
        return;
    }

    /* verbose printing */
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

    /* we want to use our own read function */
    if ((err = curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback)))
        ftp_log("Read function option error: %s!", curl_easy_strerror(err));

    /* enable uploading */
    if ((err = curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L)))
        ftp_log("Upload option error: %s!", curl_easy_strerror(err));


    /* user & password */
    if (ftp_user && ftp_user[0]) {
#if 0
        curl_easy_setopt(curl, CURLOPT_USERNAME, ftp_user);
        if (ftp_password && ftp_password[0])
            curl_easy_setopt(curl, CURLOPT_PASSWORD, ftp_password);
#else
        gchar pwd[64];
        snprintf(pwd, sizeof(pwd), "%s:%s", ftp_user,
                 ftp_password?ftp_password:"");
        if ((err = curl_easy_setopt(curl, CURLOPT_USERPWD, pwd)))
            ftp_log("Username/password option error: %s!", curl_easy_strerror(err));
#endif
    }

    /* proxy */
    if (proxy_host && proxy_host[0]) {
        if ((err = curl_easy_setopt(curl, CURLOPT_PROXY, proxy_host)))
            ftp_log("Proxy option error: %s!", curl_easy_strerror(err));
        if ((err = curl_easy_setopt(curl, CURLOPT_PROXYPORT, (long)proxy_port)))
            ftp_log("Proxy port option error: %s!", curl_easy_strerror(err));
    }

    /* proxy user & password */
    if (proxy_user && proxy_user[0]) {
        gchar pwd[64];
        snprintf(pwd, sizeof(pwd), "%s:%s", proxy_user,
                 proxy_password?proxy_password:"");
        if ((err = curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, pwd)))
            ftp_log("Proxy user/password option error: %s!", curl_easy_strerror(err));
    }

    if (ftp_port) {
        if ((err = curl_easy_setopt(curl, CURLOPT_PORT, (long)ftp_port)))
            ftp_log("FTP port option error: %s!", curl_easy_strerror(err));
    }

    if ((err = curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)300)))
        ftp_log("Timeout option error: %s!", curl_easy_strerror(err));
}

static void cleanup_curl(void)
{
    /* always cleanup */
    if (curl) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }
}

gpointer ftp_thread(gpointer args)
{
    CURLcode res;


    ftp_log("Starting FTP");

    while ((res = curl_global_init(CURL_GLOBAL_DEFAULT))) {
        ftp_log("Cannot init curl: %s!", curl_easy_strerror(res));
        g_usleep(120000000);
    }

    for ( ; *((gboolean *)args) ; )   /* exit loop when flag is cleared */
    {
        time_t last_copy = 0;
        ftp_update = FALSE;

        while (!do_ftp || !ftp_server || !ftp_server[0] ||
               !current_directory[0] || current_directory[0] == '.') {
            g_usleep(1000000);
        }

        ftp_log("Server=%s directory=%s", ftp_server, current_directory);

        while (!ftp_update) {
            time_t copy_start = time(NULL);
            GDir *dir = g_dir_open(current_directory, 0, NULL);
            if (dir) {
                init_curl();
                const gchar *fname = g_dir_read_name(dir);
                while (fname) {
                    struct stat statbuf;
                    gchar *fullname = g_build_filename(current_directory, fname, NULL);
                    if (!g_stat(fullname, &statbuf)) {
                        if (statbuf.st_mtime >= last_copy &&
                            (statbuf.st_mode & S_IFREG)) {
                            FILE *f = fopen(fullname, "rb");
                            if (f) {
                                if (curl) {
                                    if (proto == PROTO_FTP) {
                                        gchar *n = g_strdup_printf("RNTO %s", fname);
                                        headerlist = curl_slist_append(headerlist, "RNFR TMPXYZ");
                                        headerlist = curl_slist_append(headerlist, n);
                                        g_free(n);
                                        /* pass in that last of FTP commands to run after the transfer */
                                        if ((res = curl_easy_setopt(curl, CURLOPT_POSTQUOTE, headerlist)))
                                            ftp_log("Postquote option error: %s!", curl_easy_strerror(res));
                                    }

                                    curl_off_t fsize = statbuf.st_size;

                                    /* specify target */
                                    gchar *u;

                                    if (ftp_path && ftp_path[0])
                                        u = g_strdup_printf("%s://%s/%s/%s",
                                                            proto == PROTO_FTP ? "ftp" : "http",
                                                            ftp_server, ftp_path,
                                                            proto == PROTO_FTP ? "TMPXYZ" : fname);
                                    else
                                        u = g_strdup_printf("%s://%s/%s",
                                                            proto == PROTO_FTP ? "ftp" : "http",
                                                            ftp_server,
                                                            proto == PROTO_FTP ? "TMPXYZ" : fname);

                                    if ((res = curl_easy_setopt(curl, CURLOPT_URL, u)))
                                        ftp_log("URL option error: %s!", curl_easy_strerror(res));
                                    g_free(u);

                                    /* specify which file to upload */
                                    if ((res = curl_easy_setopt(curl, CURLOPT_READDATA, f)))
                                        ftp_log("Read data option error: %s!", curl_easy_strerror(res));

                                    /* Set the size of the file to upload (optional).  If you give a *_LARGE
                                       option you MUST make sure that the type of the passed-in argument is a
                                       curl_off_t. If you use CURLOPT_INFILESIZE (without _LARGE) you must
                                       make sure that to pass in a type 'long' argument. */
                                    if ((res = curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE,
                                                                (curl_off_t)fsize)))
                                        ftp_log("Large option error: %s!", curl_easy_strerror(res));

                                    //curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
                                    //curl_easy_setopt(curl, CURLOPT_FTP_USE_EPSV, 0);

                                    /* Now run off and do what you've been told! */
                                    if ((res = curl_easy_perform(curl)))
                                        ftp_log("Cannot execute: %s!\n",
                                                curl_easy_strerror(res));

                                    if (headerlist) {
                                        /* clean up the FTP commands list */
                                        curl_slist_free_all(headerlist);
                                        headerlist = NULL;
                                        if ((res = curl_easy_setopt(curl, CURLOPT_POSTQUOTE, headerlist)))
                                            ftp_log("Postquote option error: %s!", curl_easy_strerror(res));
                                    }
                                } // if (curl)
                                fclose(f);
                            } // if (f)
                        } // if mtime >
                    } // if (!g_stat)
                    g_free(fullname);
                    fname = g_dir_read_name(dir);
                    g_usleep(1000);
                } // while fname

                g_dir_close(dir);
                last_copy = copy_start;
                cleanup_curl();
            } // if (dir)
            g_usleep(2000000);
        } // while
        ftp_log("Configuration update.");
    }

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

static gchar *ftp_logfile_name = NULL;

static void ftp_log(gchar *format, ...)
{
    time_t t;
    gchar buf[256];
    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);

    t = time(NULL);

    if (ftp_logfile_name == NULL) {
        struct tm *tm = localtime(&t);
        if (tm) {
            sprintf(buf, "judoftp_%04d%02d%02d_%02d%02d%02d.log",
                    tm->tm_year+1900,
                    tm->tm_mon+1,
                    tm->tm_mday,
                    tm->tm_hour,
                    tm->tm_min,
                    tm->tm_sec);
            ftp_logfile_name = g_build_filename(g_get_user_data_dir(), buf, NULL);
        }
    }

    FILE *f = fopen(ftp_logfile_name, "a");
    if (f) {
        struct tm *tm = localtime(&t);

        fprintf(f, "%02d:%02d:%02d %s\n",
                tm->tm_hour,
                tm->tm_min,
                tm->tm_sec,
                text);
        fclose(f);
    } else {
        g_print("Cannot open ftp log file\n");
        perror("ftp_logfile_name");
    }

    g_free(text);
}
