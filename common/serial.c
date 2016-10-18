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
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>

#endif /* WIN32 */

#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "comm.h"

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

#define DEV_TYPE_NORMAL   0
#define DEV_TYPE_STATHMOS 1
#define DEV_TYPE_AP1      2
#define DEV_TYPE_MYWEIGHT 3

extern struct message *put_to_rec_queue(volatile struct message *m);
extern GtkWidget     *weight_entry;
extern GtkWidget     *main_window;
extern GKeyFile      *keyfile;
extern gint           my_address;

#define NUM_BAUDRATES 5
struct baudrates {
    gint value;
    gchar *text;
};

static gchar serial_device[32] = {0};
static gint  serial_baudrate = 0; // index to a table
static gint device_type = 0;
static gboolean serial_changed = FALSE;       
gboolean serial_used = FALSE;

static gchar handle_character(gchar c)
{
    static gint weight = 0;
    static gint decimal = 0;

    if (c == '.' || c == ',') {
        decimal = 1;
    } else if (device_type == DEV_TYPE_AP1 && c == 0x06) { // ACK
        return 0x11; // DC1
    } else if (c < '0' || c > '9') {
        if ((device_type == DEV_TYPE_NORMAL   && c == '\r') ||
            (device_type == DEV_TYPE_STATHMOS && c == 0x1e) ||
            (device_type == DEV_TYPE_AP1      && (c == 'k' || c == 0x04 || c == 0x03)) ||
            (device_type == DEV_TYPE_MYWEIGHT && (c == 'k' || c == '\r'))) {

            if (weight_entry && weight > 5000 && weight < 300000) {
                struct message msg;
                memset(&msg, 0, sizeof(msg));
                msg.type = MSG_SCALE;
                msg.u.scale.weight = weight;
                put_to_rec_queue(&msg);

            }
            weight = 0;
            decimal = 0;
        }
    } else if (device_type == DEV_TYPE_STATHMOS) {
        weight = 10*weight + (c - '0');
    } else if (decimal == 0) {
        weight = 10*weight + 1000*(c - '0');
    } else {
        switch (decimal) {
        case 1: weight += 100*(c - '0'); break;
        case 2: weight += 10*(c - '0'); break;
        case 3: weight += c - '0'; break;
        }
        decimal++;
    }
    //g_print("weight=%d\n", weight);
    return 0;
}

static gchar send_chars(void)
{
    static time_t last_send = 0;

    if (device_type == DEV_TYPE_NORMAL)
        return 0;

    if (time(NULL) == last_send)
        return 0;

    last_send = time(NULL);

    if (device_type == DEV_TYPE_STATHMOS)
        return 0x05;

    if (device_type == DEV_TYPE_AP1)
        return 0x05;

    if (device_type == DEV_TYPE_MYWEIGHT)
        return 0x0d;

    return 0;
}
 
#if defined(__WIN32__) || defined(WIN32)

struct baudrates serial_baudrates[NUM_BAUDRATES] = {
    {CBR_1200, "1200,N81"}, {CBR_9600, "9600,N81"}, {CBR_19200, "19200,N81"}, {CBR_38400, "38400,N81"},
    {CBR_115200, "115200,N81"}
};

#define ERR_SER g_print("Error %s:%d\n", __FUNCTION__, __LINE__)

gpointer serial_thread(gpointer args)
{
    gint i;
    char szBuff[32];
    DWORD dwBytesRead;

    while (*((gboolean *)args)) {
        serial_changed = FALSE;

        g_usleep(500000);

        if (!serial_used)
            continue;

        if (serial_device[0] == 0)
            continue;

        HANDLE hSerial;
        hSerial = CreateFile(serial_device,
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             0,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL,
                             0);
        if(hSerial==INVALID_HANDLE_VALUE){
            if(GetLastError()==ERROR_FILE_NOT_FOUND){
                //serial port does not exist. Inform user.
                ERR_SER;
            }
            //some other error occurred. Inform user.
            ERR_SER;
            continue;
        }

        DCB dcbSerialParams = {0};
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        if (!GetCommState(hSerial, &dcbSerialParams)) {
            //error getting state
            ERR_SER;
            continue;
        }
        dcbSerialParams.BaudRate = serial_baudrates[serial_baudrate].value;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;
        if(!SetCommState(hSerial, &dcbSerialParams)) {
            //error setting serial port state
            ERR_SER;
            continue;
        }

        COMMTIMEOUTS timeouts = {0};
        timeouts.ReadIntervalTimeout=50;
        timeouts.ReadTotalTimeoutConstant=50;
        timeouts.ReadTotalTimeoutMultiplier=10;
        timeouts.WriteTotalTimeoutConstant=50;
        timeouts.WriteTotalTimeoutMultiplier=10;
        if(!SetCommTimeouts(hSerial, &timeouts)) {
            //error occureed. Inform user
            ERR_SER;
            continue;
        }

        while (*((gboolean *)args) && serial_device[0] && serial_changed == FALSE) {       /* loop for input */
            gchar ch = 0, ch2 = 0;

            if (ReadFile(hSerial, szBuff, 31, &dwBytesRead, NULL)) {
                szBuff[dwBytesRead] = 0;
                for (i = 0; i < dwBytesRead; i++) {
                    ch2 = handle_character(szBuff[i]);
                    if (ch2 && ch == 0) ch = ch2;
                }
            }

            if (!ch) ch = send_chars();

            if (ch) {
                szBuff[0] = ch;
                if(!WriteFile(hSerial, szBuff, 1, &dwBytesRead, NULL))
                    g_print("Cannot write to COM!\n");
            }
        }

        CloseHandle(hSerial);
    } // while

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}

#else // Linux

struct baudrates serial_baudrates[NUM_BAUDRATES] = {
    {B1200, "1200,N81"}, {B9600, "9600,N81"}, {B19200, "19200,N81"}, {B38400, "38400,N81"},
    {B115200, "115200,N81"}
};

gpointer serial_thread(gpointer args)
{
    gint fd, res, i;
    struct termios oldtio, newtio;
    gchar buf[32];

    while (*((gboolean *)args)) {
        serial_changed = FALSE;

        g_usleep(500000);

        if (!serial_used)
            continue;

        if (serial_device[0] == 0)
            continue;

        fd = open(serial_device, O_RDWR | O_NOCTTY ); 
        if (fd < 0)
            continue;
        
        tcgetattr(fd,&oldtio); /* save current port settings */
        
        bzero(&newtio, sizeof(newtio));
        newtio.c_cflag = serial_baudrates[serial_baudrate].value | CRTSCTS | CS8 | CLOCAL | CREAD;
        newtio.c_iflag = IGNPAR;
        newtio.c_oflag = 0;
        
        /* set input mode (non-canonical, no echo,...) */
        newtio.c_lflag = 0;
         
        newtio.c_cc[VTIME]    = 2;   /* inter-character timer in 0.1 sec */
        newtio.c_cc[VMIN]     = 0;   /* blocking read until 1 char received */
        
        tcflush(fd, TCIFLUSH);
        tcsetattr(fd,TCSANOW,&newtio);
        
        while (*((gboolean *)args) && serial_device[0] && serial_changed == FALSE) {       /* loop for input */
            gchar ch = 0, ch2 = 0;

            res = read(fd, buf, sizeof(buf)-1);   /* returns after 1 char has been input */

            for (i = 0; i < res; i++) {
                ch2 = handle_character(buf[i]);
                if (ch2 && ch == 0) ch = ch2;
            }

            if (!ch) ch = send_chars();

            if (ch) {
                buf[0] = ch;
                write(fd, buf, 1);
            }
        }

        tcsetattr(fd,TCSANOW,&oldtio);
        close(fd);
    }

    g_thread_exit(NULL);    /* not required just good pratice */
    return NULL;
}
#endif

void set_serial_dialog(GtkWidget *w, gpointer data)
{
    gint i;
    GtkWidget *dialog, *used, *device, *baudrate, *table, *devtype;

    dialog = gtk_dialog_new_with_buttons (_("Serial Communication"),
                                          GTK_WINDOW(main_window),
                                          GTK_DIALOG_MODAL,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          NULL);

#if (GTKVER == 3)
    table = gtk_grid_new();
#else
    table = gtk_table_new(2, 4, FALSE);
#endif
    used = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(used), serial_used);

    device = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(device), 20);
    gtk_entry_set_text(GTK_ENTRY(device), serial_device[0] ? serial_device : "COM1");

#if (GTKVER == 3)
    baudrate = gtk_combo_box_text_new();
    for (i = 0; i < NUM_BAUDRATES; i++)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(baudrate), NULL, serial_baudrates[i].text);
    gtk_combo_box_set_active(GTK_COMBO_BOX(baudrate), serial_baudrate);
#else
    baudrate = gtk_combo_box_new_text();
    for (i = 0; i < NUM_BAUDRATES; i++)
        gtk_combo_box_append_text((GtkComboBox *)baudrate, serial_baudrates[i].text);
    gtk_combo_box_set_active((GtkComboBox *)baudrate, serial_baudrate);
#endif

#if (GTKVER == 3)
    devtype = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(devtype), NULL, "Normal");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(devtype), NULL, "Stathmos/Allvåg");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(devtype), NULL, "AP-1");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(devtype), NULL, "My Weight");
    gtk_combo_box_set_active(GTK_COMBO_BOX(devtype), device_type);
#else
    devtype = gtk_combo_box_new_text();
    gtk_combo_box_append_text((GtkComboBox *)devtype, "Normal");
    gtk_combo_box_append_text((GtkComboBox *)devtype, "Stathmos/Allvåg");
    gtk_combo_box_append_text((GtkComboBox *)devtype, "AP-1");
    gtk_combo_box_append_text((GtkComboBox *)devtype, "My Weight");
    gtk_combo_box_set_active((GtkComboBox *)devtype, device_type);
#endif

#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("In Use")), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), used, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Device")), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), device, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Baudrate")), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(table), baudrate, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Type")), 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(table), devtype, 1, 3, 1, 1);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       table, FALSE, FALSE, 0);
#else
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("In Use")), 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), used, 1, 2, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Device")), 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), device, 1, 2, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Baudrate")), 0, 1, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), baudrate, 1, 2, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Type")), 0, 1, 3, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), devtype, 1, 2, 3, 4);

    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);
#endif

    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        strncpy(serial_device, gtk_entry_get_text(GTK_ENTRY(device)), sizeof(serial_device)-1);
        serial_baudrate = gtk_combo_box_get_active(GTK_COMBO_BOX(baudrate));
        device_type = gtk_combo_box_get_active(GTK_COMBO_BOX(devtype));
        g_key_file_set_string(keyfile, "preferences", "serialdevice", serial_device);
        g_key_file_set_integer(keyfile, "preferences", "serialbaudrate", serial_baudrate);
        g_key_file_set_integer(keyfile, "preferences", "serialtype", device_type);
        serial_used = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(used));
        serial_changed = TRUE;
    }

    gtk_widget_destroy(dialog);
}

void serial_set_device(gchar *dev)
{
    strncpy(serial_device, dev, sizeof(serial_device)-1);
    serial_changed = TRUE;
}

void serial_set_baudrate(gint baud)
{
    serial_baudrate = baud;
    serial_changed = TRUE;
}

void serial_set_type(gint type)
{
    device_type = type;
    serial_changed = TRUE;
}
