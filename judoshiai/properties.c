/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2013 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "sqlite3.h"
#include "judoshiai.h"

#define NUM_DEFAULT_CATS 6

struct default_cat {
    gint max_age;
    gint comp_min, comp_max;
    gint catsys;
};

struct default_cat default_cats[NUM_DEFAULT_CATS];

struct {
    GtkWidget *age, *min, *max, *sys;
} catwidgets[NUM_DEFAULT_CATS];

static GtkWidget *belt_widgets[NUM_BELTS];

enum {
    PROP_TYPE_TEXT,
    PROP_TYPE_INT,
    PROP_TYPE_CHECK
};

struct property {
    gchar *name; // name in database
    gchar *label;
    gint type;
    gint row, col;
    gchar *value; // value in database
    gint intval; // value of int and others
    GtkWidget *w;
    gint min, max;
    gint table;
} props[NUM_PROPERTIES] = {
    {
        .name = "Competition",
        .label = N_("Tournament:"),
        .type = PROP_TYPE_TEXT,
        .row = 1, .col = 1,
    },
    {
        .name = "Date",
        .label = N_("Date:"),
        .type = PROP_TYPE_TEXT,
    },
    {
        .name = "Place",
        .label = N_("Place:"),
        .type = PROP_TYPE_TEXT,
    },
    {
        .name = "Time",
        .label = N_("Start time:"),
        .type = PROP_TYPE_TEXT,
    },
    {
        .name = "NumTatamis",
        .label = N_("Number of tatamis:"),
        .type = PROP_TYPE_INT,
        .min = 1, .max = NUM_TATAMIS,
    },
    {
        .name = "WhiteFirst",
        .label = N_("White first:"),
        .type = PROP_TYPE_CHECK,
    },
    {
        .name = "three_matches_for_two",
        .label = N_("Best two out of three matches:"),
        .type = PROP_TYPE_CHECK,
        .table = 1,
        .row = -1, .col = -1,
    },
    {
        .name = "WinNeededForMedal",
        .label = N_("Medal will be awarded \nonly if a contest was won:"),
        .type = PROP_TYPE_CHECK,
    },
    {
        .name = "SeededToFixedPlaces",
        .label = N_("Seeded to fixed places:"),
        .type = PROP_TYPE_CHECK,
    },
    {
        .name = "EqualScoreLessShidoWins",
        .label = N_("If equal score less shido wins"),
        .type = PROP_TYPE_CHECK,
    },
    {
        .name = "GoldenScoreWinGives1Point",
        .label = N_("Win in Golden Score gives 1 point"),
        .type = PROP_TYPE_CHECK,
    },
    {
        .name = "DefaultCat1",
        .label = "",
        .type = PROP_TYPE_TEXT,
        .row = -1, .col = -1,
    },
    {
        .name = "DefaultCat2",
        .label = "",
        .type = PROP_TYPE_TEXT,
        .row = -1, .col = -1,
    },
    {
        .name = "DefaultCat3",
        .label = "",
        .type = PROP_TYPE_TEXT,
        .row = -1, .col = -1,
    },
    {
        .name = "DefaultCat4",
        .label = "",
        .type = PROP_TYPE_TEXT,
        .row = -1, .col = -1,
    },
    {
        .name = "DefaultCat5",
        .label = "",
        .type = PROP_TYPE_TEXT,
        .row = -1, .col = -1,
    },
    {
        .name = "DefaultCat6",
        .label = "",
        .type = PROP_TYPE_TEXT,
        .row = -1, .col = -1,
    },
    {
        .name = "DPool2CarriedForwardPoints",
        .label = N_("Double Pool 2 with carried forward points:"),
        .type = PROP_TYPE_CHECK,
        .table = 1,
    },
    {
        .name = "TwoPoolBronzes",
        .label = N_("Two pool bronzes:"),
        .type = PROP_TYPE_CHECK,
        .table = 1,
    },
    {
        .name = "Resolve3WayTiesByWeights",
        .label = N_("Resolve 3-way ties by weights:"),
        .type = PROP_TYPE_CHECK,
        .table = 1,
    },
    {
        .name = "GradeNames",
        .label = "",
        .type = PROP_TYPE_TEXT,
        .row = -1, .col = -1,
    },
};

gint prop_get_int_val(gint name)
{
    return props[name].intval;
}

const gchar *prop_get_str_val(gint name)
{
    if (props[name].value == NULL) return "";
    return props[name].value;
}

static void set_val(gint n, const gchar *dbval, gint intval)
{
    g_free(props[n].value); 
    if (dbval == NULL) {
        gchar buf[16];
        snprintf(buf, sizeof(buf), "%d", intval);
        props[n].value = g_strdup(buf);
    } else
        props[n].value = g_strdup(dbval);
    props[n].intval = intval;
}

static void value_to_widget(gint n)
{
    gchar buf[16];

    if (props[n].w == NULL)
        return;

    switch (props[n].type) {
    case PROP_TYPE_TEXT:
        gtk_entry_set_text(GTK_ENTRY(props[n].w), props[n].value ? props[n].value : "");
        break;
    case PROP_TYPE_INT:
        snprintf(buf, sizeof(buf), "%d", props[n].intval);
        gtk_entry_set_text(GTK_ENTRY(props[n].w), buf);
        break;
    case PROP_TYPE_CHECK:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(props[n].w), (gboolean)props[n].intval);
        break;
    }
}

static void widget_to_value(gint i)
{
    gint n;
    gchar buf[16];

    if (props[i].w == NULL)
        return;

    switch (props[i].type) {
    case PROP_TYPE_TEXT:
        set_val(i, gtk_entry_get_text(GTK_ENTRY(props[i].w)), 0);
        break;
    case PROP_TYPE_INT:
        n = atoi(gtk_entry_get_text(GTK_ENTRY(props[i].w)));
        if (n < props[i].min) n = props[i].min;
        else if (n > props[i].max) n = props[i].max;
        snprintf(buf, sizeof(buf), "%d", n);
        set_val(i, buf, n);
        break;
    case PROP_TYPE_CHECK:
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(props[i].w)))
            set_val(i, "1", 1);
        else
            set_val(i, "0", 0);
        break;
    default:
        g_print("error\n");
    }
}

void init_property(gchar *prop, gchar *val)
{
    gint i;

    for (i = 0; i < NUM_PROPERTIES; i++) {
        if (strcmp(prop, props[i].name) == 0) {
            g_free(props[i].value);
            props[i].value = g_strdup(val ? val : "");

            switch (props[i].type) {
            case PROP_TYPE_TEXT:
                break;
            case PROP_TYPE_INT:
            case PROP_TYPE_CHECK:
                props[i].intval = atoi(props[i].value);
                break;
            }

            if (i >= PROP_DEFAULT_CAT_1 && i < PROP_DEFAULT_CAT_1 + NUM_DEFAULT_CATS) {
                sscanf(props[i].value, "%d %d %d %d",
                       &default_cats[i-PROP_DEFAULT_CAT_1].max_age,
                       &default_cats[i-PROP_DEFAULT_CAT_1].comp_min,
                       &default_cats[i-PROP_DEFAULT_CAT_1].comp_max,
                       &default_cats[i-PROP_DEFAULT_CAT_1].catsys);
            }

            if (i == PROP_GRADE_NAMES) {
                gint k;
                gchar **b = g_strsplit(val, ";", NUM_BELTS);
                for (k = 0; k < NUM_BELTS; k++) {
                    g_free(belts[k]);
                    belts[k] = NULL;
                }
                for (k = 0; k < NUM_BELTS && b[k]; k++)
                    belts[k] = g_strdup(b[k]);
                g_strfreev(b);
                //g_print("init prop: %s\n", props[i].value);
            }

            if (i == PROP_NUM_TATAMIS)
                number_of_tatamis = props[i].intval;

            return;
        } // if strcmp

    }
    g_print("Unknown property '%s'='%s'\n", prop, val);
}

static void values_to_widgets(void)
{
    gint i;

    for (i = 0; i < NUM_PROPERTIES; i++)
        value_to_widget(i);

    for (i = 0; i < NUM_DEFAULT_CATS; i++) {
        gchar buf[16];

        snprintf(buf, sizeof(buf), "%d", default_cats[i].max_age);
        gtk_entry_set_text(GTK_ENTRY(catwidgets[i].age), buf);

        snprintf(buf, sizeof(buf), "%d", default_cats[i].comp_min);
        gtk_entry_set_text(GTK_ENTRY(catwidgets[i].min), buf);

        snprintf(buf, sizeof(buf), "%d", default_cats[i].comp_max);
        gtk_entry_set_text(GTK_ENTRY(catwidgets[i].max), buf);

        gtk_combo_box_set_active(GTK_COMBO_BOX(catwidgets[i].sys), 
                                 get_system_menu_selection(default_cats[i].catsys));
    }

    for (i = 0; i < NUM_BELTS; i++) {
        gtk_entry_set_text(GTK_ENTRY(belt_widgets[i]), belts[i] ? belts[i] : "");
    }
}

static void widgets_to_values(void)
{
    gint i;
    gchar buf[512];

    for (i = 0; i < NUM_PROPERTIES; i++)
        widget_to_value(i);

    for (i = 0; i < NUM_DEFAULT_CATS; i++) {
        gint real = get_system_number_by_menu_pos(gtk_combo_box_get_active(GTK_COMBO_BOX(catwidgets[i].sys)));
        snprintf(buf, sizeof(buf), "%s %s %s %d",
                 gtk_entry_get_text(GTK_ENTRY(catwidgets[i].age)),
                 gtk_entry_get_text(GTK_ENTRY(catwidgets[i].min)),
                 gtk_entry_get_text(GTK_ENTRY(catwidgets[i].max)),
                 real);
        //gtk_entry_set_text(GTK_ENTRY(props[i + PROP_DEFAULT_CAT_1].w), buf);
        set_val(i + PROP_DEFAULT_CAT_1, buf, 0);

        sscanf(buf, "%d %d %d %d",
               &default_cats[i].max_age,
               &default_cats[i].comp_min,
               &default_cats[i].comp_max,
               &default_cats[i].catsys);
    }

    buf[0] = 0;
    for (i = 0; i < NUM_BELTS; i++) {
        if (i) g_strlcat(buf, ";", sizeof(buf));
        g_strlcat(buf, gtk_entry_get_text(GTK_ENTRY(belt_widgets[i])), sizeof(buf));
        g_free(belts[i]);
        belts[i] = g_strdup(gtk_entry_get_text(GTK_ENTRY(belt_widgets[i])));
    }
    set_val(PROP_GRADE_NAMES, buf, 0);
    //gtk_entry_set_text(GTK_ENTRY(props[PROP_GRADE_NAMES].w), buf);
}

void props_save_to_db(void)
{
    gint i;

    for (i = 0; i < NUM_PROPERTIES; i++)
        db_set_info(props[i].name, props[i].value);

    number_of_tatamis = prop_get_int_val(PROP_NUM_TATAMIS);
}

#define SET_VAL(_n, _s, _i) do { if (if_unset==FALSE || props[_n].value[0]==0) set_val(_n, _s, _i); } while (0)

void reset_props(GtkWidget *button, void *data)
{
    reset_props_1(button, data, FALSE);
}

void reset_props_1(GtkWidget *button, void *data, gboolean if_unset)
{
    gint i;
    GtkComboBox *combo = data;
    gchar buf[512];

    if (combo)
        draw_system = gtk_combo_box_get_active(combo);

    for (i = 0; i < NUM_PROPERTIES; i++)
        if (props[i].value == NULL) set_val(i, "", 0);

    SET_VAL(PROP_WHITE_FIRST, "1", 1);
    SET_VAL(PROP_THREE_MATCHES_FOR_TWO, "0", 0);
    SET_VAL(PROP_EQ_SCORE_LESS_SHIDO_WINS, "1", 1);
    SET_VAL(PROP_GS_WIN_GIVES_1_POINT, "0", 0);

    if (number_of_tatamis == 0)
        number_of_tatamis = 3;

    SET_VAL(PROP_NUM_TATAMIS, NULL, number_of_tatamis);

    if (draw_system == DRAW_INTERNATIONAL || draw_system == DRAW_NORWEGIAN || draw_system == DRAW_DJB)
        SET_VAL(PROP_WIN_NEEDED_FOR_MEDAL, "1", 1);
    else
        SET_VAL(PROP_WIN_NEEDED_FOR_MEDAL, "0", 0);

    if (draw_system == DRAW_SPANISH) {
        SET_VAL(PROP_SEEDED_TO_FIXED_PLACES, "1", 1);
        SET_VAL(PROP_TWO_POOL_BRONZES, "1", 1);
    } else {
        SET_VAL(PROP_SEEDED_TO_FIXED_PLACES, "0", 0);
        SET_VAL(PROP_TWO_POOL_BRONZES, "0", 0);
    }

    if (draw_system == DRAW_AUSTRALIAN)
        SET_VAL(PROP_DPOOL2_WITH_CARRIED_FORWARD_POINTS, "1", 1);
    else
        SET_VAL(PROP_DPOOL2_WITH_CARRIED_FORWARD_POINTS, "0", 0);

    if (draw_system == DRAW_FINNISH || draw_system == DRAW_SWEDISH || draw_system == DRAW_ESTONIAN || draw_system == DRAW_NORWEGIAN)
        SET_VAL(PROP_RESOLVE_3_WAY_TIES_BY_WEIGHTS, "1", 1);
    else
        SET_VAL(PROP_RESOLVE_3_WAY_TIES_BY_WEIGHTS, "0", 0);

    // default catsystems
    if (if_unset==FALSE || props[PROP_DEFAULT_CAT_1].value[0]==0) {
        memset(default_cats, 0, sizeof(default_cats));
        default_cats[0] = (struct default_cat){0, 2, 2, CAT_SYSTEM_BEST_OF_3};
        default_cats[1] = (struct default_cat){0, 3, 5, CAT_SYSTEM_POOL};
        default_cats[2] = (struct default_cat){0, 6, 7, CAT_SYSTEM_DPOOL};
        default_cats[3] = (struct default_cat){0, 8, 0, CAT_SYSTEM_REPECHAGE};

        switch (draw_system) {
        case DRAW_INTERNATIONAL:
            default_cats[2] = (struct default_cat){0, 6, 0, CAT_IJF_DOUBLE_REPECHAGE};
            default_cats[3] = (struct default_cat){0, 0, 0, 0};
            break;
        case DRAW_FINNISH:
            break;
        case DRAW_SWEDISH:
            default_cats[3] = (struct default_cat){0, 8, 0, CAT_SYSTEM_DIREKT_AATERKVAL};
            break;
        case DRAW_ESTONIAN:
            default_cats[3] = (struct default_cat){10, 6, 0, CAT_SYSTEM_EST_D_KLASS};
            default_cats[4] = (struct default_cat){0, 6, 0, CAT_SYSTEM_REPECHAGE};
            break;
        case DRAW_SPANISH:
            default_cats[2] = (struct default_cat){0, 6, 10, CAT_SYSTEM_DPOOL};
            default_cats[3] = (struct default_cat){10, 8, 16, CAT_ESP_DOBLE_PERDIDA};
            default_cats[4] = (struct default_cat){0, 8, 0, CAT_ESP_REPESCA_DOBLE};
            break;
        case DRAW_NORWEGIAN:
            default_cats[2] = (struct default_cat){0, 6, 10, CAT_SYSTEM_DPOOL};
            default_cats[3] = (struct default_cat){0, 8, 0, CAT_SYSTEM_DUBBELT_AATERKVAL};
            break;
        case DRAW_BRITISH:
            default_cats[3] = (struct default_cat){0, 8, 0, CAT_SYSTEM_GBR_KNOCK_OUT};
            break;
        case DRAW_AUSTRALIAN:
            break;
        case DRAW_DANISH:
            default_cats[3] = (struct default_cat){0, 8, 0, CAT_SYSTEM_DEN_DOUBLE_ELIMINATION};
            break;
        case DRAW_POLISH:
            default_cats[2] = (struct default_cat){0, 6, 8, CAT_SYSTEM_DPOOL};
            default_cats[3] = (struct default_cat){0, 9, 0, CAT_IJF_DOUBLE_REPECHAGE};
            break;
        case DRAW_SLOVAKIAN:
            default_cats[2] = (struct default_cat){0, 6, 8, CAT_SYSTEM_DPOOL};
            default_cats[3] = (struct default_cat){0, 9, 0, CAT_SYSTEM_REPECHAGE};
            break;
        case DRAW_UKRAINIAN:
            default_cats[2] = (struct default_cat){0, 6, 0, CAT_SYSTEM_REPECHAGE};
            default_cats[3] = (struct default_cat){0, 0, 0, 0};
            break;
	case DRAW_DJB:
	    default_cats[2] = (struct default_cat){0, 6, 0, CAT_IJF_DOUBLE_REPECHAGE};
            default_cats[3] = (struct default_cat){0, 0, 0, 0};
            break;    
	}

        for (i = 0; i < NUM_DEFAULT_CATS; i++) {
            snprintf(buf, sizeof(buf), "%d %d %d %d", 
                     default_cats[i].max_age,
                     default_cats[i].comp_min,
                     default_cats[i].comp_max,
                     default_cats[i].catsys);
            set_val(PROP_DEFAULT_CAT_1 + i, buf, 0);
        }
    }

    if (if_unset==FALSE || props[PROP_GRADE_NAMES].value[0]==0) {
        for (i = 0; i < NUM_BELTS; i++) {
            g_free(belts[i]);
            belts[i] = NULL;
        }
        for (i = 0; belts_defaults[i]; i++) {
            belts[i] = g_strdup(belts_defaults[i]);
        }
        buf[0] = 0;
        for (i = 0; i < NUM_BELTS; i++) {
            if (i) g_strlcat(buf, ";", sizeof(buf));
            if (belts[i]) g_strlcat(buf, belts[i], sizeof(buf));
        }
        set_val(PROP_GRADE_NAMES, buf, 0);
    }

    g_key_file_set_integer(keyfile, "preferences", "drawsystem", draw_system);
    refresh_window();
}

static void reset_props1(GtkWidget *button, void *data)
{
    reset_props(button, data);
    values_to_widgets();    
}

static const gchar *draw_system_names[NUM_DRAWS] = 
    {N_("International System"), N_("Finnish System"), N_("Swedish System"), N_("Estonian System"), N_("Spanish System"), 
     N_("Norwegian System"), N_("British System"), N_("Australian System"), N_("Danish System"), 
     N_("Polish System"), N_("Slovakian System"), N_("Ukrainian System"), N_("German Judo Federation")};

#define NUM_TBLS 2

void properties(GtkWidget *w, gpointer data)
{
    GtkWidget *dialog, *tmp, *reset;
#if (GTKVER == 3)
    GtkWidget *hbox = gtk_grid_new();
    GtkWidget *hbox1 = gtk_grid_new();
    GtkWidget *vbox1 = gtk_grid_new();
    GtkWidget *vbox2 = gtk_grid_new();
    GtkWidget *table2 = gtk_grid_new();
    GtkWidget *table3 = gtk_grid_new();
    GtkWidget *table4 = gtk_grid_new();
    GtkWidget *table5 = gtk_grid_new();
    GtkWidget *table6 = gtk_grid_new();

    gtk_grid_set_column_spacing(GTK_GRID(vbox1), 5);
    gtk_grid_set_row_spacing(GTK_GRID(vbox1), 5);
    gtk_grid_set_column_spacing(GTK_GRID(vbox2), 5);
    gtk_grid_set_row_spacing(GTK_GRID(vbox2), 5);
    gtk_grid_set_column_spacing(GTK_GRID(hbox1), 5);
    gtk_grid_set_row_spacing(GTK_GRID(hbox1), 5);
    gtk_grid_set_column_spacing(GTK_GRID(hbox), 5);
    gtk_grid_set_row_spacing(GTK_GRID(hbox), 5);
#else
    GtkWidget *hbox = gtk_hbox_new(FALSE, 8);
    GtkWidget *hbox1 = gtk_hbox_new(TRUE, 8);
    GtkWidget *vbox1 = gtk_vbox_new(FALSE, 8);
    GtkWidget *vbox2 = gtk_vbox_new(FALSE, 8);
    GtkWidget *table2 = gtk_table_new(1, 3, FALSE);
    GtkWidget *table3 = gtk_table_new(3, 8, FALSE);
    GtkWidget *table4 = gtk_table_new(1, 3, FALSE);
    GtkWidget *table5 = gtk_table_new(8, 4, FALSE);
    GtkWidget *table6 = gtk_table_new(2, 2, FALSE);
#endif
    GtkWidget *tables[NUM_TBLS]; 
    gint       num_comp, num_weighted;
    gint       row[NUM_TBLS] = {0}, col[NUM_TBLS] = {0}, i;
    gint       tbl = 0;
    gchar      buf[16];

    for (i = 0; i < NUM_TBLS; i++)
#if (GTKVER == 3)
        tables[i] = gtk_grid_new();
#else
        tables[i] = gtk_table_new(1, 1, FALSE);
#endif
    db_read_competitor_statistics(&num_comp, &num_weighted);

    dialog = gtk_dialog_new_with_buttons (_("Tournament properties"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                                          NULL);

    for (i = 0; i < NUM_TBLS; i++) {
#if (GTKVER == 3)
        gtk_grid_set_column_spacing(GTK_GRID(tables[i]), 5);
        gtk_grid_set_row_spacing(GTK_GRID(tables[i]), 5);
#else
        gtk_table_set_col_spacings(GTK_TABLE(tables[i]), 5);
        gtk_table_set_row_spacings(GTK_TABLE(tables[i]), 5);
#endif
    }

    for (i = 0; i < NUM_PROPERTIES; i++) {
        GtkWidget *tmp;

        tbl = props[i].table;

        if (props[i].row >= 0) {
            if (props[i].row) row[tbl] = props[i].row - 1;
            if (props[i].col) col[tbl] = props[i].col - 1;
            tmp = gtk_label_new(_(props[i].label));
            gtk_misc_set_alignment(GTK_MISC(tmp), 1, 0.5);
#if (GTKVER == 3)
            gtk_grid_attach(GTK_GRID(tables[tbl]), tmp, col[tbl], row[tbl], 1, 1);
#else
            gtk_table_attach_defaults(GTK_TABLE(tables[tbl]), tmp, col[tbl], col[tbl]+1, row[tbl], row[tbl]+1);
#endif
            switch (props[i].type) {
            case PROP_TYPE_TEXT:
            case PROP_TYPE_INT:
                //gtk_entry_set_max_length(GTK_ENTRY(tmp), 20);
                //gtk_entry_set_text(GTK_ENTRY(tmp), props[i].value ? props[i].value : "");
                tmp = gtk_entry_new();
                gtk_entry_set_width_chars(GTK_ENTRY(tmp), 20);
#if (GTKVER == 3)
                gtk_grid_attach(GTK_GRID(tables[tbl]), tmp, col[tbl]+1, row[tbl], 1, 1);
#else
                gtk_table_attach_defaults(GTK_TABLE(tables[tbl]), tmp, 
                                          col[tbl]+1, col[tbl]+2, row[tbl], row[tbl]+1);
#endif
                break;

            case PROP_TYPE_CHECK:
                //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(tmp), (gboolean)props[i].intval);
                tmp = gtk_check_button_new();
#if (GTKVER == 3)
                gtk_grid_attach(GTK_GRID(tables[tbl]), tmp, col[tbl]+1, row[tbl], 1, 1);
#else
                gtk_table_attach_defaults(GTK_TABLE(tables[tbl]), tmp, 
                                          col[tbl]+1, col[tbl]+2, row[tbl], row[tbl]+1);
#endif
                break;
            }

            row[tbl]++;
            props[i].w = tmp;
        } // if (props[i].row >= 0)
    } // for (i = 0; i < NUM_PROPERTIES; i++)

    for (i = 0; i < NUM_DEFAULT_CATS; i++) {
        gint j;

        tmp = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(tmp), 3);
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table3), tmp, 0, i+3, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(table3), tmp, 0, 1, i+3, i+4);
#endif
        catwidgets[i].age = tmp;

        tmp = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(tmp), 3);
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table3), tmp, 1, i+3, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(table3), tmp, 1, 2, i+3, i+4);
#endif
        catwidgets[i].min = tmp;

        tmp = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(tmp), 3);
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table3), tmp, 2, i+3, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(table3), tmp, 2, 3, i+3, i+4);
#endif
        catwidgets[i].max = tmp;

#if (GTKVER == 3)
        tmp = gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, "?");
	for (j = 1; j < NUM_SYSTEMS; j++)
	    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, get_system_name_for_menu(j));

        gtk_grid_attach(GTK_GRID(table3), tmp, 3, i+3, 1, 1);
#else
        tmp = gtk_combo_box_new_text();
        gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), "?");
	for (j = 1; j < NUM_SYSTEMS; j++)
	    gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), get_system_name_for_menu(j));

        gtk_table_attach_defaults(GTK_TABLE(table3), tmp, 3, 4, i+3, i+4);
#endif
        catwidgets[i].sys = tmp;
    }

    for (i = 0; i < NUM_BELTS; i++) {
        tmp = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(tmp), 5);
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table5), tmp, i%8, i/8, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(table5), tmp, i%8, i%8+1, i/8, i/8+1);
#endif
        belt_widgets[i] = tmp;
    }

    values_to_widgets();

    GtkWidget *frame = gtk_frame_new(_("General"));
    gtk_container_add(GTK_CONTAINER(frame), tables[0]);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(vbox2), frame, 0, 0, 1, 1);
#else
    gtk_box_pack_start_defaults(GTK_BOX(vbox2), frame);
#endif
    frame = gtk_frame_new(_("Pool settings"));
    gtk_container_add(GTK_CONTAINER(frame), tables[1]);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(vbox2), frame, 0, 1, 1, 1);
#else
    gtk_box_pack_start_defaults(GTK_BOX(vbox2), frame);
#endif
    // table6 -- statistics
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table6), gtk_label_new(_("Competitors:")), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table6), gtk_label_new(_("Weighted:")), 0, 1, 1, 1);
#else
    gtk_table_attach_defaults(GTK_TABLE(table6), gtk_label_new(_("Competitors:")), 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table6), gtk_label_new(_("Weighted:")), 0, 1, 1, 2);
#endif
    sprintf(buf, "%d", num_comp);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table6), gtk_label_new(buf), 1, 0, 1, 1);
#else
    gtk_table_attach_defaults(GTK_TABLE(table6), gtk_label_new(buf), 1, 2, 0, 1);
#endif
    sprintf(buf, "%d", num_weighted);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table6), gtk_label_new(buf), 1, 1, 1, 1);
#else
    gtk_table_attach_defaults(GTK_TABLE(table6), gtk_label_new(buf), 1, 2, 1, 2);
#endif
    frame = gtk_frame_new(_("Statistics"));
    gtk_container_add(GTK_CONTAINER(frame), table6);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(vbox2), frame, 0, 2, 1, 1);

    gtk_grid_attach(GTK_GRID(hbox), vbox2, 0, 0, 1, 1);
#else
    gtk_box_pack_start_defaults(GTK_BOX(vbox2), frame);

    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);
#endif
    // table4: category properties button
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table4), gtk_label_new(" "), 0, 0, 1, 1);
#else
    gtk_table_attach(GTK_TABLE(table4), gtk_label_new(" "), 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
#endif
    tmp = gtk_button_new_with_label(_("Properties"));
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table4), tmp, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table4), gtk_label_new(" "), 0, 2, 1, 1);
#else
    gtk_table_attach(GTK_TABLE(table4), tmp, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
    gtk_table_attach(GTK_TABLE(table4), gtk_label_new(" "), 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
#endif
    g_signal_connect(G_OBJECT(tmp), "clicked", G_CALLBACK(set_categories_dialog), NULL);
    //gtk_box_pack_start(GTK_BOX(vbox1), table4, FALSE, FALSE, 0);
    frame = gtk_frame_new(_("Categories"));
    gtk_container_add(GTK_CONTAINER(frame), table4);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(hbox1), frame, 0, 0, 1, 1);
#else
    gtk_box_pack_start(GTK_BOX(hbox1), frame, FALSE, FALSE, 0);
#endif
    // table2
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table2), gtk_label_new(_("Reset to")), 0, 0, 1, 1);

    tmp = gtk_combo_box_text_new();
    for (i = 0; i < NUM_DRAWS; i++)
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, _(draw_system_names[i]));
    gtk_grid_attach(GTK_GRID(table2), tmp, 0, 1, 1, 1);
#else
    gtk_table_attach(GTK_TABLE(table2), gtk_label_new(_("Reset to")), 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

    tmp = gtk_combo_box_new_text();
    for (i = 0; i < NUM_DRAWS; i++)
        gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), _(draw_system_names[i]));
    gtk_table_attach(GTK_TABLE(table2), tmp, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
#endif
    gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), draw_system);
    //g_signal_connect(G_OBJECT(tmp), "changed", G_CALLBACK(reset_props), NULL); 

    reset = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    g_signal_connect(G_OBJECT(reset), "clicked", G_CALLBACK(reset_props1), tmp);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table2), reset, 0, 2, 1, 1);
#else
    gtk_table_attach(GTK_TABLE(table2), reset, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
#endif
    frame = gtk_frame_new(_("Initialize"));
    gtk_container_add(GTK_CONTAINER(frame), table2);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(hbox1), frame, 1, 0, 1, 1);

    gtk_grid_attach(GTK_GRID(vbox1), hbox1, 0, 0, 1, 1);
#else
    gtk_box_pack_start(GTK_BOX(hbox1), frame, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);
#endif
    // table3
    //gtk_table_attach_defaults(GTK_TABLE(table3), gtk_label_new(_("Default systems")), 0, 4, 0, 1);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table3), gtk_label_new(_("Players")), 1, 1, 2, 1);
    gtk_grid_attach(GTK_GRID(table3), gtk_label_new(_("Max age")), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(table3), gtk_label_new(_("Min")),     1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(table3), gtk_label_new(_("Max")),     2, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(table3), gtk_label_new(_("System")),  3, 2, 1, 1);
#else
    gtk_table_attach_defaults(GTK_TABLE(table3), gtk_label_new(_("Players")), 1, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table3), gtk_label_new(_("Max age")), 0, 1, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table3), gtk_label_new(_("Min")), 1, 2, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table3), gtk_label_new(_("Max")), 2, 3, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table3), gtk_label_new(_("System")), 3, 4, 2, 3);
#endif

    frame = gtk_frame_new(_("Default systems"));
    gtk_container_add(GTK_CONTAINER(frame), table3);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(vbox1), frame, 0, 1, 2, 1);
#else
    gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);
#endif

    frame = gtk_frame_new(_("Grade names"));
    gtk_container_add(GTK_CONTAINER(frame), table5);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(vbox1), frame, 0, 2, 2, 1);

    gtk_grid_attach(GTK_GRID(hbox), vbox1, 1, 0, 2, 1);
#else
    gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, FALSE, 0);

    gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox1);
#endif

    gtk_widget_show_all(hbox);
#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox, FALSE, FALSE, 0);
#else
    gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), hbox);
#endif
    if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        widgets_to_values();
        props_save_to_db();
    }
        
    gtk_widget_destroy (dialog);        

    update_match_pages_visibility();
    set_match_col_titles();
}

gint props_get_default_wishsys(gint age, gint competitors)
{
    gint i;

    for (i = 0; i < NUM_DEFAULT_CATS; i++) {
        if ((age == 0 || default_cats[i].max_age == 0 || age <= default_cats[i].max_age) &&
            (competitors >= default_cats[i].comp_min || default_cats[i].comp_min == 0)   && 
            (competitors <= default_cats[i].comp_max || default_cats[i].comp_max == 0))
            return default_cats[i].catsys;
    }

    return 0;
}

gint props_get_grade(gchar *b)
{
    gint i;
    gchar first_letter = 0;
    gint grade = 0;
    gchar *b1 = g_utf8_casefold(b, -1);

    // look for exact match
    for (i = 0; i < NUM_BELTS; i++) {
        gchar *s = belts[i];
        if (s == NULL || s[0] == 0)
            continue;

        gchar *t = g_utf8_casefold(s, -1);
        if (strcmp(b1, t) == 0) { // hit
            g_free(b1);
            g_free(t);
            return i;
        }
        g_free(t);
    }
    g_free(b1);

    while (*b) {
        if (*b >= '0' && *b <= '9') grade = 10*grade + *b - '0';
        else if (first_letter == 0) {
            if (*b >= 'a' && *b <= 'z') first_letter = *b;
            else if (*b >= 'A' && *b <= 'Z') first_letter = *b + 'a' - 'A';
        }
        b++;
    }

    for (i = 0; i < NUM_BELTS; i++) {
        gchar letter = 0;
        gint  number = 0;
        gchar *s = belts[i];
        if (s == NULL || s[0] == 0)
            continue;

        while (*s) {
            if (*s >= '0' && *s <= '9') number = 10*number + *s - '0';
            else if (letter == 0) {
                if (*s >= 'a' && *s <= 'z') letter = *s;
                else if (*s >= 'A' && *s <= 'Z') letter = *s + 'a' - 'A';
            }
            s++;
        }

        if (first_letter == letter && grade == number)
            return i;
    }

    return 0;
}
