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

#include <glib/gstdio.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>

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

void get_html(gchar *path);

enum {
    ELEM_NONE,
    ELEM_P,
    ELEM_H1,
    ELEM_H2,
    ELEM_H3,
    ELEM_A,
    ELEM_TABLE,
    ELEM_TR,
    ELEM_TD,
    ELEM_BUTTON,
    ELEM_INPUT,
    ELEM_FORM,
    NUM_ELEM
};

static gchar *elem_names[NUM_ELEM] = {
    "NONE",
    "p",
    "h1",
    "h2",
    "h3",
    "a",
    "table",
    "tr",
    "td",
    "button",
    "input",
    "form",
};

#define NUM_CSS 6
#define BGCSS(_a)							\
    "background-image: -gtk-gradient(linear,left top,left bottom,color-stop(0.0," \
    #_a "),color-stop(1.00," #_a "));"


static const gchar *styles[NUM_CSS] = {
    "color: white",
    "color: white; " BGCSS(blue),
     BGCSS(yellow),
     BGCSS(white),
    "color: yellow; background-image: "
    "-gtk-gradient(radial,"
    "center center, 0,center center, 0.5,from(lightblue), to(blue));",
    "color: white; " BGCSS(black),
};

GtkWidget *html_page = NULL;
static GtkWidget *page = NULL;
static CURL *curl;
static char docbuf[20000];

static gint elem1 = ELEM_NONE, elem2 = ELEM_NONE;
static gint row = 0;
static gchar text[4000], *txt;
static gint textix = 0;

#define NUM_LINKS 32
#define URL_LEN 32
static struct link {
    gchar url[URL_LEN];
} links[NUM_LINKS];
static gint num_links;

#define NUM_BUTTONS 32
#define STR_LEN 16
static struct button {
    gchar class[STR_LEN];
    gchar name[STR_LEN];
    gchar value[STR_LEN];
} buttons[NUM_BUTTONS];
static gint num_buttons;

#define NUM_INPUTS 32
#define STR_LEN 16
static struct input {
    gchar type[STR_LEN];
    gchar name[STR_LEN];
    gchar value[64];
    GtkWidget *entry;
} inputs[NUM_INPUTS];
static gint num_inputs;

static struct form {
    gchar action[URL_LEN];
} forms;

static struct in_addr current_addr;

static gint table_row, table_col;
static GtkWidget *table;
static gchar td_class[STR_LEN];

static GdkColor color_yellow, color_white, color_black, color_blue,
    color_selected;

#define g_print(a...) do {} while (0)

void send_mask(gint x1, gint y1, gint x2, gint y2)
{
    gchar buf[64];
    snprintf(buf, sizeof(buf), "setmask.html?x1=%d&y1=%d&x2=%d&y2=%d",
	     x1, y1, x2, y2);
    get_html(buf);
    get_html("index.html");
}

static void style_buttons(void)
{
    GtkCssProvider *provider;
    GdkDisplay *display;
    GdkScreen *screen;
    gint i;
    gchar *css, *all = NULL;

    all = g_strdup_printf("%s",
			  "* { font: Sans 10; }\n"
			  " GtkButton {\n"
			  "   -GtkWidget-focus-line-width: 0;\n"
			  "   border-radius: 3px;\n"
			  "   color: black;\n"         /* font color */
			  "   border-style: outset;\n"
			  "   border-width: 2px;\n"
			  "   padding: 2px;\n"
			  "}\n");

    for (i = 0; i < num_buttons; i++) {
	int style;

	if (buttons[i].class[0] != 's')
	    continue;

	style = atoi(buttons[i].class + 1);
	if (style < 0 || style >= NUM_CSS)
	    continue;

	css = g_strdup_printf("#button%d {\n  padding: 2px; %s\n}\n",
			      i, styles[style]);
	if (all)
	    all = g_strconcat(all, css, NULL);
	else
	    all = css;
    }

    provider = gtk_css_provider_new ();
    display = gdk_display_get_default ();
    screen = gdk_display_get_default_screen(display);

    gtk_style_context_add_provider_for_screen (screen,
					       GTK_STYLE_PROVIDER (provider),
					       GTK_STYLE_PROVIDER_PRIORITY_USER);

    gtk_css_provider_load_from_data(GTK_CSS_PROVIDER (provider), all, -1, NULL);
    g_free(all);

    g_object_unref (provider);
}

static void trim_text(void)
{
    while (textix > 0 &&
	   (text[textix-1] == '\n' ||
	    text[textix-1] == '\r')) {
	textix--;
	text[textix] = 0;
    }

    txt = text;
    while (*txt == '\n' || *txt == '\r')
	txt++;
}

GtkWidget *create_html_page(void)
{
    curl = curl_easy_init();
    return gtk_grid_new();
}

static gint write_ix = 0;

static uint write_cb(char *in, size_t size, size_t nmemb, char *out)
{
    uint r = size * nmemb;

    if (sizeof(docbuf) - write_ix < r + 1)
	return 0;

    memcpy(out + write_ix, in, r);
    write_ix += r;
    out[write_ix] = 0;

    return r;
}

static gboolean click(GtkWidget      *event_box,
		      GdkEventButton *event,
		      gpointer        data)
{
    struct link *link = data;
    g_print("Clicked %s\n", link->url);
    get_html(link->url);
    return TRUE;
}

static void click_button( GtkWidget *widget,
			  gpointer   data )
{
    gchar url[URL_LEN];
    struct button *b = data;

    snprintf(url, URL_LEN, "?%s=%s", b->name, b->value);
    get_html(url);
}

static void submit_form( GtkWidget *widget,
			  gpointer   data )
{
    gchar *url, *val;
    int i;

    url = g_strdup_printf("%s?", forms.action);

    for (i = 0; i < num_inputs; i++) {
	if (strcasecmp(inputs[i].type, "text"))
	    continue;
	val = g_uri_escape_string(gtk_entry_get_text(GTK_ENTRY(inputs[i].entry)),
						     NULL, FALSE);
	url = g_strconcat(url,
			  g_strdup_printf("%s%s=%s",
					  i ? "&":"", inputs[i].name, val),
			  NULL);
	g_free(val);
    }

    g_print("GET:%s\n", url);
    get_html(url);
    g_free(url);
}

static void start_element(void *void_context,
			  const xmlChar *name,
			  const xmlChar **attributes)
{
    gint i = 0, size;
    const char **attr = (const char **)attributes;
    char value[128];

    g_print("StartElem %s\n", name);

    if (attr) {
	while (attr[i]) {
	    g_print("  attr=%s\n", attr[i]);
	    i++;
	}
    }

    for (i = 1; i < NUM_ELEM; i++)
	if (!strcasecmp((gchar *)name, elem_names[i])) {
	    elem1 = i;
	    break;
	}

    if (i >= NUM_ELEM)
	return;

    switch (elem1) {
    case ELEM_P:
    case ELEM_H1:
    case ELEM_H2:
    case ELEM_H3:
	textix = 0;
	text[0] = 0;
	break;
    case ELEM_A:
	textix = 0;
	text[0] = 0;
	if (attr) {
	    i = 0;
	    while (attr[i]) {
		if (!strcasecmp(attr[i], "href")) {
		    snprintf(links[num_links].url, URL_LEN, "%s", attr[i+1]);
		    break;
		}
		i++;
	    }
	}
	break;
    case ELEM_TABLE:
	table = gtk_grid_new();
	table_row = table_col = 0;
	break;
    case ELEM_TD:
	if (attr) {
	    i = 0;
	    while (attr[i]) {
		if (!strcasecmp(attr[i], "class")) {
		    i++;
		    snprintf(td_class, STR_LEN, "%s", attr[i]);
		    break;
		}
		i++;
	    }
	}
	break;
    case ELEM_BUTTON:
	textix = 0;
	text[0] = 0;
	if (attr) {
	    i = 0;
	    while (attr[i]) {
		if (!strcasecmp(attr[i], "class")) {
		    i++;
		    snprintf(buttons[num_buttons].class, STR_LEN, "%s", attr[i]);
		} else if (!strcasecmp(attr[i], "name")) {
		    i++;
		    snprintf(buttons[num_buttons].name, STR_LEN, "%s", attr[i]);
		} else if (!strcasecmp(attr[i], "value")) {
		    i++;
		    snprintf(buttons[num_buttons].value, STR_LEN, "%s", attr[i]);
		}
		i++;
	    }
	}
	break;
    case ELEM_INPUT:
	textix = 0;
	text[0] = 0;
	size = 20;
	if (attr) {
	    i = 0;
	    while (attr[i]) {
		if (!strcasecmp(attr[i], "type")) {
		    i++;
		    snprintf(inputs[num_inputs].type, STR_LEN, "%s", attr[i]);
		} else if (!strcasecmp(attr[i], "name")) {
		    i++;
		    snprintf(inputs[num_inputs].name, STR_LEN, "%s", attr[i]);
		} else if (!strcasecmp(attr[i], "value")) {
		    i++;
		    strncpy(value, attr[i], 127);
		    value[127] = 0;
		} else if (!strcasecmp(attr[i], "size")) {
		    i++;
		    size = atoi(attr[i]);
		}
		i++;
	    }
	}

	if (!strcasecmp(inputs[num_inputs].type, "text")) {
	    inputs[num_inputs].entry = gtk_entry_new();
	    gtk_entry_set_max_length(GTK_ENTRY(inputs[num_inputs].entry), size);
	    gtk_entry_set_width_chars(GTK_ENTRY(inputs[num_inputs].entry), size);
	    gtk_entry_set_text(GTK_ENTRY(inputs[num_inputs].entry), value);
	} else if (!strcasecmp(inputs[num_inputs].type, "submit")) {
	    inputs[num_inputs].entry = gtk_button_new_with_label(value);
	    g_signal_connect(G_OBJECT(inputs[num_inputs].entry), "clicked",
			     G_CALLBACK(submit_form), NULL);
	}

	if (table) {
	    gtk_grid_attach(GTK_GRID(table), GTK_WIDGET(inputs[num_inputs].entry),
			    table_col, table_row, 1, 1);
	} else {
	    gtk_grid_attach(GTK_GRID(page), GTK_WIDGET(inputs[num_inputs].entry), 0, row++, 1, 1);
	}
	if (num_inputs < NUM_INPUTS - 1)
	    num_inputs++;
	break;
    case ELEM_FORM:
	textix = 0;
	text[0] = 0;
	size = 20;
	if (attr) {
	    i = 0;
	    while (attr[i]) {
		if (!strcasecmp(attr[i], "action")) {
		    i++;
		    snprintf(forms.action, URL_LEN, "%s", attr[i]);
		}
		i++;
	    }
	}
    }
}

//
//  libxml end element callback function
//

static void end_element(void *void_context,
                       const xmlChar *name)
{
    gint i;
    GtkWidget *tmp, *evbox;
    gchar buf[128], *size;

    g_print("EndElem %s\n", name);

    elem2 = ELEM_NONE;

    for (i = 1; i < NUM_ELEM; i++)
	if (!strcasecmp((gchar *)name, elem_names[i])) {
	    elem2 = i;
	    break;
	}

    if (elem2 == ELEM_NONE)
	return;

    switch (elem2) {
    case ELEM_P:
	trim_text();
	if (txt[0] == 0)
	    break;
	tmp = gtk_label_new(txt);
	gtk_misc_set_alignment(GTK_MISC(tmp), 0.0, 0.5);
	gtk_grid_attach(GTK_GRID(page), tmp, 0, row, 1, 1);
	textix = 0;
	text[0] = 0;
	row++;
	break;
    case ELEM_H1:
    case ELEM_H2:
    case ELEM_H3:
	if (elem2 == ELEM_H1) size = "xx-large";
	else if (elem2 == ELEM_H2) size = "x-large";
	else size = "large";

	trim_text();
	tmp = gtk_label_new(NULL);
	snprintf(buf, sizeof(buf),
		 "<span size=\"%s\" weight=\"bold\">%s</span>", size, txt);
	g_print("H: row=%d: %s\n", row, buf);
	gtk_label_set_markup(GTK_LABEL(tmp), buf);
	gtk_misc_set_alignment(GTK_MISC(tmp), 0.0, 0.5);
	gtk_grid_attach(GTK_GRID(page), tmp, 0, row, 1, 1);
	textix = 0;
	text[0] = 0;
	row++;
	break;
    case ELEM_A:
	trim_text();
	tmp = gtk_label_new(NULL);
	snprintf(buf, sizeof(buf), "<b>%s</b>", txt);
	//gtk_label_set_text(GTK_LABEL(tmp), buf);
	g_print("A: row=%d: %s\n", row, buf);
	gtk_label_set_markup(GTK_LABEL(tmp), buf);
	gtk_misc_set_alignment(GTK_MISC(tmp), 0.0, 0.5);
	evbox = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(evbox), tmp);

	gtk_grid_attach(GTK_GRID(page), evbox, 0, row, 1, 1);
	g_signal_connect(G_OBJECT(evbox),
			 "button_press_event",
			 G_CALLBACK(click),
			 &links[num_links]);
	if (num_links < NUM_LINKS-1)
	    num_links++;
	textix = 0;
	text[0] = 0;
	row++;
	break;
    case ELEM_TABLE:
	gtk_grid_attach(GTK_GRID(page), table, 0, row, 1, 1);
	table = NULL;
	row++;
	break;
    case ELEM_TR:
	table_row++;
	table_col = 0;
	break;
    case ELEM_TD:
	if (textix) {
	    trim_text();
	    tmp = gtk_label_new(txt);
	    gtk_widget_set_size_request(GTK_WIDGET(tmp), 80, 20);
	    if (!strcmp(td_class, "s5")) {
		GdkRGBA fg, bg;
		gdk_rgba_parse(&fg, "white");
		gdk_rgba_parse(&bg, "black");
		gtk_widget_override_color(tmp, GTK_STATE_FLAG_NORMAL, &fg);
		gtk_widget_override_background_color(tmp, GTK_STATE_FLAG_NORMAL, &bg);
	    }
	    //gtk_misc_set_alignment(GTK_MISC(tmp), 0.0, 0.5);
	    gtk_grid_attach(GTK_GRID(table), tmp, table_col, table_row, 1, 1);
	}
	textix = 0;
	text[0] = 0;
	td_class[0] = 0;
	table_col++;
	break;
    case ELEM_BUTTON:
	trim_text();
	tmp = gtk_button_new_with_label(txt);
	sprintf(buf, "button%d", num_buttons);
	gtk_widget_set_name(GTK_WIDGET(tmp), buf);
	gtk_widget_set_size_request(GTK_WIDGET(tmp), 80, 20);
	if (table) {
	    gtk_grid_attach(GTK_GRID(table), tmp, table_col, table_row, 1, 1);
	    g_signal_connect(G_OBJECT(tmp), "clicked",
			     G_CALLBACK(click_button), &buttons[num_buttons]);
	} else {
	    gtk_grid_attach(GTK_GRID(page), tmp, 0, row++, 1, 1);
	    g_signal_connect(G_OBJECT(tmp), "clicked",
			     G_CALLBACK(click_button), &buttons[num_buttons]);
	}
	if (num_buttons < NUM_BUTTONS-1)
	    num_buttons++;
	break;
    }
}

//
//  Text handling helper function
//

static void handle_characters(void *context,
			      const xmlChar *chars,
			      int length)
{
    const gchar *str = (const gchar *)chars;

    if (sizeof(text) - textix < length + 1)
	return;

    memcpy(text+textix, str, length);
    textix += length;
    text[textix] = 0;

    g_print("%s\n", chars);
}

//
//  libxml PCDATA callback function
//

static void chars(void *void_context,
		  const xmlChar *chars,
		  int length)
{
    g_print("chars:\n");
    handle_characters(void_context, chars, length);
}

//
//  libxml CDATA callback function
//

static void cdata(void *void_context,
                  const xmlChar *chars,
                  int length)
{
    int i;
    g_print("cdata:\n");
    for (i = 0; i < length; i++)
	g_print("%c", chars[i]);
    g_print("\n");
    //handle_characters(void_context, chars, length);
}

//
//  libxml SAX callback structure
//

static htmlSAXHandler sax_handler =
{
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    start_element,
    end_element,
    NULL,
    chars,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    cdata,
    NULL
};

//
//  Parse given (assumed to be) HTML text and return the title
//

static void parse_html(const char *html)
{
    htmlParserCtxtPtr ctxt;

    if (page)
	gtk_widget_destroy(page);
    page = gtk_grid_new();
    gtk_grid_set_row_homogeneous(GTK_GRID(page), FALSE);
    gtk_grid_set_row_spacing(GTK_GRID(page), 4);
    gtk_grid_attach(GTK_GRID(html_page), page, 0, 0, 1, 1);
    row = 0;

    ctxt = htmlCreatePushParserCtxt(&sax_handler, NULL, "", 0, "",
				    XML_CHAR_ENCODING_NONE);

    htmlParseChunk(ctxt, html, strlen(html), 0);
    htmlParseChunk(ctxt, "", 0, 1);

    htmlFreeParserCtxt(ctxt);

    gtk_widget_show_all(page);
}

void get_html(gchar *path)
{
    gchar *buf;
    char curl_errbuf[CURL_ERROR_SIZE];
    int err;
    static gchar current_path[128];

    textix = 0;
    write_ix = 0;
    num_links = 0;
    num_buttons = 0;

    if (path[0] == '?') {
	buf = g_strdup_printf("http://%s:8888/%s%s",
			      inet_ntoa(current_addr), current_path, path);
    } else {
	strncpy(current_path, path, sizeof(current_path)-1);
	current_path[sizeof(current_path)-1] = 0;
	gchar *p = strchr(current_path, '?');
	if (p) *p = 0;
	buf = g_strdup_printf("http://%s:8888/%s", inet_ntoa(current_addr), path);
    }
    g_print("GET '%s'\n", buf);
    curl_easy_setopt(curl, CURLOPT_URL, buf);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
    //curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &docbuf);
    err = curl_easy_perform(curl);
    if (err)
	g_print("Curl error: %s\n", curl_errbuf);
    g_free(buf);
    //curl_easy_cleanup();
    parse_html(docbuf);

    style_buttons();
}

void set_html_url(struct in_addr addr)
{
    gdk_color_parse("white", &color_white);
    gdk_color_parse("blue", &color_blue);
    gdk_color_parse("black", &color_black);
    gdk_color_parse("yellow", &color_yellow);
    gdk_color_parse("#7070ff", &color_selected);

    current_addr = addr;
    get_html("index.html");
}
