/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "judoshiai.h"


static gint num_print_lang = 0;

gchar *print_texts[numprinttexts][NUM_PRINT_LANGS];
gchar *print_lang_menu_texts[NUM_PRINT_LANGS];
gchar  print_lang_names[NUM_PRINT_LANGS][2];

// dummy variable for xgettext
const gchar *for_xgettext[] = {
    N_("Results in Finnish"),
    N_("Results in Swedish"),
    N_("Results in English"),
    N_("Results in Spanish"),
    N_("Results in Estonian"),
    N_("Results in Ukrainian"),
    N_("Results in Norwegian"),
    N_("Results in Icelandic"),
    N_("Results in Polish"),
    N_("Results in Slovak"),
    N_("Results in Dutch"),
    N_("Results in German"),
    N_("Results in Russian")
};

static void get_print_texts(gchar a, gchar b, gchar *filename)
{
    gint linenum = 0;
    FILE *f = fopen(filename, "r");
    if (!f)
        return;

    print_lang_names[num_print_lang][0] = a;
    print_lang_names[num_print_lang][1] = b;

    gchar line[256];

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "###", 3) == 0)
            continue;

        gchar *p1 = strchr(line, '\r');
        if (p1)	*p1 = 0;
        p1 = strchr(line, '\n');
        if (p1)	*p1 = 0;

        linenum++;

        if (linenum == 1) {
            print_lang_menu_texts[num_print_lang] = g_strdup(line);
            continue;
        }
        
        if (linenum-2 < numprinttexts)
            print_texts[linenum-2][num_print_lang] = g_strdup(line);
        else
            g_print("Extra lines in %s\n", filename);
    }

    fclose(f);
    num_print_lang++;
}

void init_print_texts(void)
{
    memset(&print_texts, 0, sizeof(print_texts));
    memset(&print_lang_menu_texts, 0, sizeof(print_lang_menu_texts));
    memset(&print_lang_names, 0, sizeof(print_lang_names));

    gchar *dirname = g_build_filename(installation_dir, "etc", NULL);
    GDir *dir = g_dir_open(dirname, 0, NULL);

    if (!dir)
        return;

    const gchar *fname = g_dir_read_name(dir);
    while (fname) {
        if (strncmp(fname, "result-texts-", 13) == 0 && num_print_lang < NUM_PRINT_LANGS) {
            gchar a = fname[13], b = fname[14];
            gchar *fullname = g_build_filename(dirname, fname, NULL);
            get_print_texts(a, b, fullname);
            g_free(fullname);
        }
    
        fname = g_dir_read_name(dir);
    }

    g_dir_close(dir);
    g_free(dirname);
}
