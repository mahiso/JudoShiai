/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#if (GTKVER == 3)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#include "sqlite3.h"
#include "judoshiai.h"

#define RESPONSE_PRINT 1000
#define RESPONSE_COACH 1001
#define JUDOGI_CONTROL

struct treedata {
    GtkTreeStore  *treestore;
    GtkTreeIter    toplevel, child;
};

struct model_iter {
    GtkTreeModel *model;
    GtkTreeIter   iter;
};

#define NUM_JUDOKAS 100
#define NUM_WCLASSES 100
#define NEW_JUDOKA 0x7ffffff1
#define NEW_WCLASS 0x7ffffff2
#define IS_NEW_WCLASS(_x) (_x >= NEW_WCLASS && _x <= NEW_WCLASS + 2)

GtkWidget *competitor_dialog = NULL; 
GtkWindow *bcdialog = NULL;

gchar *belts[NUM_BELTS] = {0};

gchar *belts_defaults[] = {
    "?", "6.kyu", "5.kyu", "4.kyu", "3.kyu", "2.kyu", "1.kyu",
    "1.dan", "2.dan", "3.dan", "4.dan", "5.dan", "6.dan", "7.dan", "8.dan", "9.dan",
    "5.mon", "4.mon", "3.mon", "2.mon", "1.mon", "9.kyu", "8.kyu", "7.kyu",
    0
};

struct judoka_widget {
    gint       index;
    gboolean   visible;
    gchar     *category;
    GtkWidget *last;
    GtkWidget *first;
    GtkWidget *birthyear;
    GtkWidget *belt;
    GtkWidget *club;
    GtkWidget *regcategory;
    GtkWidget *weight;
    GtkWidget *ok;
    GtkWidget *seeding;
    GtkWidget *clubseeding;
    GtkWidget *hansokumake;
    GtkWidget *country;
    GtkWidget *id;
    GtkWidget *system;
    GtkWidget *realcategory;
    GtkWidget *gender;
    GtkWidget *judogi;
    GtkWidget *comment;
    GtkWidget *coachid;
};

GtkWidget *weight_entry = NULL;

static gboolean      editing_ongoing = FALSE;
static GtkWidget     *competitor_label = NULL;

static void display_competitor(gint indx);
static gboolean foreach_select_coach(GtkTreeModel *model,
                                     GtkTreePath  *path,
                                     GtkTreeIter  *iter,
                                     void         *arg);
void db_update_judoka(int num, struct judoka *j);
gint sort_iter_compare_func(GtkTreeModel *model,
                            GtkTreeIter  *a,
                            GtkTreeIter  *b,
                            gpointer      userdata);
void toolbar_sort(void);
static gboolean set_weight_on_button_pressed(GtkWidget *treeview, 
                                             GdkEventButton *event, 
                                             gpointer userdata);


static void judoka_edited_callback(GtkWidget *widget, 
				   GdkEvent *event,
				   gpointer data)
{
    gint ret;
    gint event_id = ptr_to_gint(event);
    struct judoka edited;
    struct judoka_widget *judoka_tmp = data;
    gint ix = judoka_tmp->index;
    struct category_data *catdata = avl_get_category(ix);
    struct compsys system = {0};
    gchar *realcategory = NULL;

    weight_entry = NULL;
    memset(&edited, 0, sizeof(edited));
        
    if (catdata) {
        system = catdata->system;
        edited.deleted = catdata->deleted & (TEAM | TEAM_EVENT);
    }

    edited.index    = judoka_tmp->index;
    edited.visible  = judoka_tmp->visible;
    edited.category = judoka_tmp->category;

    if (judoka_tmp->last)
        edited.last        = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->last)));

    if (judoka_tmp->first)
        edited.first       = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->first)));

    if (judoka_tmp->club)
        edited.club        = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->club)));

    if (judoka_tmp->country)
        edited.country     = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->country)));

    if (judoka_tmp->regcategory)
        edited.regcategory = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->regcategory)));

    if (judoka_tmp->realcategory)
#if (GTKVER == 3)
        realcategory = g_strdup(gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(judoka_tmp->realcategory)));
#else
        realcategory = g_strdup(gtk_combo_box_get_active_text(GTK_COMBO_BOX(judoka_tmp->realcategory)));
#endif
    if (!realcategory)
        realcategory = g_strdup("?");

    if (judoka_tmp->birthyear)
        edited.birthyear = atoi(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->birthyear)));

    if (judoka_tmp->belt)
        edited.belt = gtk_combo_box_get_active(GTK_COMBO_BOX(judoka_tmp->belt));
    if (edited.belt < 0 || edited.belt >= NUM_BELTS)
        edited.belt = 0;

    if (judoka_tmp->weight)
        edited.weight = weight_grams(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->weight)));

    if (judoka_tmp->seeding)
        edited.seeding = gtk_combo_box_get_active(GTK_COMBO_BOX(judoka_tmp->seeding));

    if (judoka_tmp->clubseeding)
        edited.clubseeding = atoi(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->clubseeding)));

    if (judoka_tmp->gender) {
        gint gender = gtk_combo_box_get_active(GTK_COMBO_BOX(judoka_tmp->gender));
        if (gender == 1)
            edited.deleted |= GENDER_MALE;
        else if (gender == 2)
            edited.deleted |= GENDER_FEMALE;
    }

#ifdef JUDOGI_CONTROL
    if (judoka_tmp->judogi) {
        gint judogi = gtk_combo_box_get_active(GTK_COMBO_BOX(judoka_tmp->judogi));
        if (judogi == 1)
            edited.deleted |= JUDOGI_OK;
        else if (judogi == 2)
            edited.deleted |= JUDOGI_NOK;
    }
#endif
    if (judoka_tmp->hansokumake && 
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(judoka_tmp->hansokumake)))
        edited.deleted |= HANSOKUMAKE;

    if (judoka_tmp->id)
        edited.id = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->id)));

    if (judoka_tmp->system)
        system.wishsys = get_system_number_by_menu_pos(gtk_combo_box_get_active
                                                       (GTK_COMBO_BOX(judoka_tmp->system)));

    if (judoka_tmp->comment)
        edited.comment = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->comment)));

    if (judoka_tmp->coachid)
        edited.coachid = g_strdup(gtk_entry_get_text(GTK_ENTRY(judoka_tmp->coachid)));

    if ((event_id != GTK_RESPONSE_OK && event_id != RESPONSE_PRINT && 
         event_id != RESPONSE_COACH) || 
        edited.last == NULL || 
        edited.last[0] == 0 || 
        (edited.last[0] == '?' && edited.last[1] == 0))
        goto out;

    if (!edited.visible) {
        /* Check if the category exists. */
        GtkTreeIter tmp_iter;
        if (find_iter_category(&tmp_iter, edited.last)) {
            struct judoka *g = get_data_by_iter(&tmp_iter);
            gint gix = g ? g->index : 0;
            free_judoka(g);

            if (ix != gix) {
                SHOW_MESSAGE("%s %s %s", _("Category"), edited.last, _("already exists!"));
                goto out;
            }
        }
    }

#if 0
    if (edited.visible && edited.birthyear > 20 && edited.birthyear <= 99)
        edited.birthyear += 1900;
    else if (edited.visible && edited.birthyear <= 20)
        edited.birthyear += 2000;
#endif

    complete_add_if_not_exist(club_completer, edited.club);

    if (ix == NEW_JUDOKA) {
        const gchar *lastname = edited.last;

        if (edited.first == NULL || edited.first[0] == 0)
            edited.first = g_strdup(" ");

        const gchar *firstname = edited.first;
        gchar *letter = g_utf8_strup(firstname, 1);
		
        edited.first = g_strdup_printf("%s%s", letter, 
                                       g_utf8_next_char(firstname));
        g_free((void *)firstname);
        g_free((void *)letter);

        edited.last = g_utf8_strup(lastname, -1);
        g_free((void *)lastname);

        edited.index = current_index++;
                
        if ((edited.regcategory == NULL || edited.regcategory[0] == 0) &&
            edited.weight > 10000) {
            gint gender = 0;
            gint age = current_year - edited.birthyear;

            if (edited.deleted & GENDER_MALE)
                gender = IS_MALE;
            else if (edited.deleted & GENDER_FEMALE)
                gender = IS_FEMALE;
            else
                gender = find_gender(edited.first);

            if (edited.regcategory)
                g_free((gchar *)edited.regcategory);

            edited.regcategory = find_correct_category(age, edited.weight, gender, NULL, TRUE);
        }

        db_add_judoka(edited.index, &edited);
    } else if (IS_NEW_WCLASS(ix)){
        edited.index = 0;
    } else {
        edited.index = ix;
        if (edited.visible) {
            db_update_judoka(edited.index, &edited);
        } else {
            /* update displayed category for competitors */
            gboolean create_teams = (edited.deleted & HANSOKUMAKE) != 0;
            GtkTreeIter iter, comp;
            if (find_iter(&iter, ix)) {
                gboolean ok = gtk_tree_model_iter_children(current_model, &comp, &iter);
                while (ok) {
                    gtk_tree_store_set((GtkTreeStore *)current_model, 
                                       &comp,
                                       COL_CATEGORY, edited.last,
                                       -1);
                    ok = gtk_tree_model_iter_next(current_model, &comp);
                }
            }

            // create default teams
            edited.deleted &= ~HANSOKUMAKE;
            if (create_teams) {
                db_create_default_teams(edited.index);
            }

            /* update database */
            db_update_category(edited.index, &edited);
            db_set_system(edited.index, system);
            //XXX???? db_read_categories();
            //XXX???? db_read_judokas();
        }
    }

    ret = display_one_judoka(&edited);
    if (IS_NEW_WCLASS(ix) && ret >= 0) {
        edited.index = ret;
        if (ix == NEW_WCLASS+1)
            edited.deleted |= TEAM;
        else if (ix == NEW_WCLASS+2)
            edited.deleted |= TEAM_EVENT;
        db_add_category(ret, &edited);
        db_set_system(ret, system);
    }

    // check for changed category
    if (edited.visible && realcategory && strcmp(edited.category, realcategory)) {
        GtkTreeIter iter;
        gint index1;
        gchar *cat1 = NULL;

        gboolean ok = gtk_tree_model_get_iter_first(current_model, &iter);
        while (ok) {
            gtk_tree_model_get(current_model, &iter,
                               COL_INDEX, &index1,
                               COL_LAST_NAME, &cat1,
                               -1);
            if (strcmp(cat1, realcategory) == 0) {
                g_free(cat1);
                break;
            }
            g_free(cat1);
            ok = gtk_tree_model_iter_next(current_model, &iter);
        }

        if (!ok) {
            SHOW_MESSAGE("Error!");
        } else if (db_category_match_status(index1) & REAL_MATCH_EXISTS) {
            SHOW_MESSAGE("%s: %s", realcategory, _("Remove drawing first"));
        } else if (db_competitor_match_status(edited.index) & MATCH_EXISTS) {
            SHOW_MESSAGE("%s %s: %s.", 
                         edited.first, edited.last, _("Remove drawing first"));
        } else {
            g_free((gpointer)edited.category);
            edited.category = g_strdup(realcategory);
            display_one_judoka(&edited);
            db_update_judoka(edited.index, &edited);
        }
    }

    //db_read_matches();
    if (edited.visible) {
        update_competitors_categories(edited.index);

        if (event_id == RESPONSE_PRINT) {
            selected_judokas[0] = edited.index;
            num_selected_judokas = 1;
            print_accreditation_cards(FALSE);
        } else if (event_id == RESPONSE_COACH) {
            num_selected_judokas = 0;
            gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                                   (GtkTreeModelForeachFunc) foreach_select_coach,
                                   (gpointer)edited.id);
            print_accreditation_cards(FALSE);
        }
    }

    //matches_refresh();

    if (!edited.visible) {
        category_refresh(edited.index);
        update_category_status_info_all();

        if (event_id == RESPONSE_PRINT)
            print_doc(NULL, gint_to_ptr(edited.index | PRINT_SHEET | PRINT_TO_PRINTER));
    }

    // Update JudoInfo
    struct message output_msg;
    memset(&output_msg, 0, sizeof(output_msg));
    output_msg.type = MSG_NAME_INFO;
    output_msg.u.name_info.index = edited.index;
    if (edited.last)
        strncpy(output_msg.u.name_info.last, edited.last, sizeof(output_msg.u.name_info.last)-1);

    if (edited.visible) {
        if (edited.first)
            strncpy(output_msg.u.name_info.first, edited.first, sizeof(output_msg.u.name_info.first)-1);
        gchar *ct = get_club_text(&edited, CLUB_TEXT_ABBREVIATION);
        if (ct)
            strncpy(output_msg.u.name_info.club, ct,
                    sizeof(output_msg.u.name_info.club)-1);
    }
    send_packet(&output_msg);
    make_backup();

out:
    g_free((gpointer)edited.category);
    g_free((gpointer)edited.last);
    g_free((gpointer)edited.first);
    g_free((gpointer)edited.club);
    g_free((gpointer)edited.country);
    g_free((gpointer)edited.regcategory);
    g_free((gpointer)edited.id);
    g_free((gpointer)edited.comment);
    g_free((gpointer)edited.coachid);
    g_free(data);
    g_free(realcategory);

    competitor_dialog = NULL;
    editing_ongoing = FALSE;
    gtk_widget_destroy(widget);

    if (bcdialog) {
        gtk_window_present(bcdialog);
    }
}

static GtkWidget *set_entry(GtkWidget *table, int row, 
			    char *text, const char *deftxt)
{
    GtkWidget *tmp;

    tmp = gtk_label_new(text);
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table), tmp, 0, row, 1, 1);
#else
    gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, row, row+1);
#endif
    tmp = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(tmp), 20);
    gtk_entry_set_text(GTK_ENTRY(tmp), deftxt ? deftxt : "");
#if (GTKVER == 3)
    gtk_grid_attach(GTK_GRID(table), tmp, 1, row, 1, 1);
#else
    gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, row, row+1);
#endif
    SET_ACCESS_NAME(tmp, text);

    return tmp;
}

/*
 * Activations 
 */

void view_on_row_activated(GtkTreeView        *treeview,
                           GtkTreePath        *path,
                           GtkTreeViewColumn  *col,
                           gpointer            userdata)
{
    GtkTreeModel *model = NULL;
    GtkTreeIter   iter;
    int i;
    GtkWidget *dialog, *table, *tmp;
    gchar   *last = NULL, 
        *first = NULL, 
        *club = NULL, 
        *regcategory = NULL,
        *category = NULL,
	*country = NULL,
	*id = NULL,
        *comment = NULL,
        *coachid = NULL;
    guint belt, index = 0, birthyear;
    gint weight, seeding = 0, clubseeding = 0;
    gboolean visible;
    guint deleted;
    gchar weight_s[10], birthyear_s[10], clubseeding_s[10];
    GtkAccelGroup *accel_group;

    if (editing_ongoing)
        return;

    struct judoka_widget *judoka_tmp = g_malloc(sizeof(*judoka_tmp));
    memset(judoka_tmp, 0, sizeof(*judoka_tmp));

    if (treeview) {
        model = gtk_tree_view_get_model(treeview);
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_model_get(model, &iter,
                               COL_INDEX, &index,
                               COL_LAST_NAME, &last,
                               COL_FIRST_NAME, &first, 
                               COL_BIRTHYEAR, &birthyear, 
                               COL_CLUB, &club, 
                               COL_WCLASS, &regcategory, 
                               COL_BELT, &belt, 
                               COL_WEIGHT, &weight, 
                               COL_VISIBLE, &visible,
                               COL_CATEGORY, &category,
                               COL_DELETED, &deleted,
			       COL_COUNTRY, &country,
			       COL_ID, &id,
			       COL_SEEDING, &seeding,
			       COL_CLUBSEEDING, &clubseeding,
			       COL_COMMENT, &comment,
			       COL_COACHID, &coachid,
                               -1);
            snprintf(weight_s, sizeof(weight_s), "%d,%02d", weight/1000, (weight%1000)/10);
            snprintf(birthyear_s, sizeof(birthyear_s), "%d", birthyear);
            snprintf(clubseeding_s, sizeof(clubseeding_s), "%d", clubseeding);
	}
    } else {
        visible = (gboolean) (ptr_to_gint(userdata) == NEW_JUDOKA ? TRUE : FALSE);
        category = strdup("?");
        deleted = 0;
        sprintf(weight_s, "0");
        sprintf(birthyear_s, "0");
        belt = 0;
        index = ptr_to_gint(userdata);
        strcpy(clubseeding_s, "0");
    }
	
    judoka_tmp->index = index;
    judoka_tmp->visible = visible;
    judoka_tmp->category = g_strdup(category);

    gchar titlebuf[64];
    if (visible)
        snprintf(titlebuf, sizeof(titlebuf), "%s [%d]", _("Competitor"), index);
    else
        snprintf(titlebuf, sizeof(titlebuf), "%s", _("Category"));

    competitor_dialog =
        dialog = gtk_dialog_new_with_buttons (titlebuf,
                                              GTK_WINDOW(main_window),
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                              GTK_STOCK_PRINT, RESPONSE_PRINT,
                                              NULL);

    if (treeview && userdata) {
        // coach dialog
        GtkWidget *coach_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                         GTK_STOCK_PRINT,
                                                         RESPONSE_COACH);
        gtk_button_set_label(GTK_BUTTON(coach_button), _("Competitors"));
        gtk_button_set_image(GTK_BUTTON(coach_button), 
                             gtk_image_new_from_stock(GTK_STOCK_PRINT, GTK_ICON_SIZE_BUTTON));
    }

    GtkWidget *ok_button = gtk_dialog_add_button (GTK_DIALOG (dialog),
                                                  GTK_STOCK_OK,
                                                  GTK_RESPONSE_OK);

    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);
    gtk_widget_add_accelerator(ok_button, "clicked", accel_group, GDK_Return, 0, GTK_ACCEL_VISIBLE );
    gtk_widget_add_accelerator(ok_button, "clicked", accel_group, GDK_KP_Enter, 0, GTK_ACCEL_VISIBLE );

    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(judoka_edited_callback), (gpointer)judoka_tmp);

#if (GTKVER == 3)
    table = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), table, FALSE, FALSE, 0);
#else
    table = gtk_table_new(2, 15, FALSE);
    gtk_container_add(GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), table);
#endif

    if (visible) {
        gint row = 0;

        judoka_tmp->last = set_entry(table, row++, _("Last Name:"), last);
        judoka_tmp->first = set_entry(table, row++, _("First Name:"), first);
        judoka_tmp->birthyear = set_entry(table, row++, _("Year of Birth:"), birthyear_s);

        tmp = gtk_label_new(_("Grade:"));
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), tmp, 0, row, 1, 1);

        judoka_tmp->belt = tmp = gtk_combo_box_text_new();
        for (i = 0; i < NUM_BELTS && ((belts[i] && belts[i][0]) || (i == 0 && belts[i])); i++)
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, belts[i]);
        gtk_grid_attach(GTK_GRID(table), tmp, 1, row, 1, 1);
        gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), belt);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, row, row+1);

        judoka_tmp->belt = tmp = gtk_combo_box_new_text();
        for (i = 0; i < NUM_BELTS && ((belts[i] && belts[i][0]) || (i == 0 && belts[i])); i++)
            gtk_combo_box_append_text((GtkComboBox *)tmp, belts[i]);
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, row, row+1);
        gtk_combo_box_set_active((GtkComboBox *)tmp, belt);
#endif
        row++;

        judoka_tmp->club = set_entry(table, row++, _("Club:"), club);
        judoka_tmp->country = set_entry(table, row++, _("Country:"), country);
        judoka_tmp->regcategory = set_entry(table, row++, _("Reg. Category:"), regcategory);

        tmp = gtk_label_new(_("Category:"));
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), tmp, 0, row, 1, 1);
        judoka_tmp->realcategory = tmp = gtk_combo_box_text_new();
#else
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, row, row+1);
        judoka_tmp->realcategory = tmp = gtk_combo_box_new_text();
#endif

        gint active = 0, loop = 0;
        gboolean ok = gtk_tree_model_get_iter_first(current_model, &iter);
        while (ok) {
            gint index1;
            gchar *cat1 = NULL;
            gtk_tree_model_get(current_model, &iter,
                               COL_INDEX, &index1,
                               COL_LAST_NAME, &cat1,
                               -1);

            if (strcmp(category, cat1) == 0)
                active = loop;
#if (GTKVER == 3)
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, cat1);
#else
            gtk_combo_box_append_text((GtkComboBox *)tmp, cat1);
#endif
            g_free(cat1);
            ok = gtk_tree_model_iter_next(current_model, &iter);
            loop++;
        }

#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), tmp, 1, row, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, row, row+1);
#endif
        gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), active);
        row++;

        if (serial_used) {
#if (GTKVER == 3)
            gtk_grid_attach(GTK_GRID(table), gtk_label_new(_("Weight:")), 0, row, 1, 1);
#else
            gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new(_("Weight:")), 0, 1, row, row+1);
#endif
            judoka_tmp->weight = gtk_entry_new();
            gtk_entry_set_max_length(GTK_ENTRY(judoka_tmp->weight), 7);
            gtk_entry_set_width_chars(GTK_ENTRY(judoka_tmp->weight), 7);
            gtk_entry_set_text(GTK_ENTRY(judoka_tmp->weight), weight_s);

            GtkWidget *wbutton = gtk_button_new_with_label("---");
#if (GTKVER == 3)
            GtkWidget *whbox = gtk_grid_new();
            gtk_grid_attach(GTK_GRID(whbox), judoka_tmp->weight, 0, 0, 1, 1);
            gtk_grid_attach(GTK_GRID(whbox), wbutton,            1, 0, 1, 1);
            gtk_grid_attach(GTK_GRID(table), whbox, 1, row, 1, 1);
#else
            GtkWidget *whbox = gtk_hbox_new(FALSE, 5);
            gtk_box_pack_start_defaults(GTK_BOX(whbox), judoka_tmp->weight);
            gtk_box_pack_start_defaults(GTK_BOX(whbox), wbutton);
            gtk_table_attach_defaults(GTK_TABLE(table), whbox, 1, 2, row, row+1);
#endif
            weight_entry = wbutton;
            if (last && last[0])
                gtk_widget_grab_focus(wbutton);
            g_signal_connect(G_OBJECT(wbutton), "button-press-event", 
                             (GCallback) set_weight_on_button_pressed, judoka_tmp->weight);
            g_signal_connect(G_OBJECT(wbutton), "key-press-event", 
                             (GCallback) set_weight_on_button_pressed, judoka_tmp->weight);
        } else {
            judoka_tmp->weight = set_entry(table, 8, _("Weight:"), weight_s);
            if (last && last[0])
                gtk_widget_grab_focus(judoka_tmp->weight);
        }
        row++;

        tmp = gtk_label_new(_("Seeding:"));
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), tmp, 0, row, 1, 1);
        judoka_tmp->seeding = tmp = gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, _("No seeding"));
        gchar buf[8];
        for (i = 0; i < NUM_SEEDED; i++) {
            snprintf(buf, sizeof(buf), "%d", i+1); 
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, buf);
        }
        gtk_grid_attach(GTK_GRID(table), tmp, 1, row, 1, 1);
        gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), seeding);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, row, row+1);
        judoka_tmp->seeding = tmp = gtk_combo_box_new_text();
        gtk_combo_box_append_text((GtkComboBox *)tmp, _("No seeding"));
        gchar buf[8];
        for (i = 0; i < NUM_SEEDED; i++) {
            snprintf(buf, sizeof(buf), "%d", i+1); 
            gtk_combo_box_append_text((GtkComboBox *)tmp, buf);
        }
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, row, row+1);
        gtk_combo_box_set_active((GtkComboBox *)tmp, seeding);
#endif
        row++;

        judoka_tmp->clubseeding = set_entry(table, row++, _("Club Seeding:"), clubseeding_s);

        judoka_tmp->id = set_entry(table, row++, _("Id:"), id);

        judoka_tmp->coachid = set_entry(table, row++, _("Coach Id:"), coachid);

        tmp = gtk_label_new(_("Gender:"));
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), tmp, 0, row, 1, 1);
        judoka_tmp->gender = tmp = gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, "?");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, _("Male"));
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, _("Female"));
        gtk_grid_attach(GTK_GRID(table), tmp, 1, row, 1, 1);
        if (deleted & GENDER_MALE)
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 1);
        else if (deleted & GENDER_FEMALE)
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 2);
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 0);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, row, row+1);
        judoka_tmp->gender = tmp = gtk_combo_box_new_text();
        gtk_combo_box_append_text((GtkComboBox *)tmp, "?");
        gtk_combo_box_append_text((GtkComboBox *)tmp, _("Male"));
        gtk_combo_box_append_text((GtkComboBox *)tmp, _("Female"));
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, row, row+1);
        if (deleted & GENDER_MALE)
            gtk_combo_box_set_active((GtkComboBox *)tmp, 1);
        else if (deleted & GENDER_FEMALE)
            gtk_combo_box_set_active((GtkComboBox *)tmp, 2);
        else
            gtk_combo_box_set_active((GtkComboBox *)tmp, 0);
#endif
        row++;

#ifdef JUDOGI_CONTROL
        tmp = gtk_label_new(_("Control:"));
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), tmp, 0, row, 1, 1);
        judoka_tmp->judogi = tmp = gtk_combo_box_text_new();
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, "?");
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, _("OK"));
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, _("NOK"));
        gtk_grid_attach(GTK_GRID(table), tmp, 1, row, 1, 1);
        if (deleted & JUDOGI_OK)
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 1);
        else if (deleted & JUDOGI_NOK)
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 2);
        else
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 0);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, row, row+1);
        judoka_tmp->judogi = tmp = gtk_combo_box_new_text();
        gtk_combo_box_append_text((GtkComboBox *)tmp, "?");
        gtk_combo_box_append_text((GtkComboBox *)tmp, _("OK"));
        gtk_combo_box_append_text((GtkComboBox *)tmp, _("NOK"));
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, row, row+1);
        if (deleted & JUDOGI_OK)
            gtk_combo_box_set_active((GtkComboBox *)tmp, 1);
        else if (deleted & JUDOGI_NOK)
            gtk_combo_box_set_active((GtkComboBox *)tmp, 2);
        else
            gtk_combo_box_set_active((GtkComboBox *)tmp, 0);
#endif
        row++;
#endif
        tmp = gtk_label_new("Hansoku-make:");
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), tmp, 0, row, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, row, row+1);
#endif
        judoka_tmp->hansokumake = gtk_check_button_new();
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(table), judoka_tmp->hansokumake, 1, row, 1, 1);
#else
        gtk_table_attach_defaults(GTK_TABLE(table), judoka_tmp->hansokumake, 1, 2, row, row+1);
#endif
        if (deleted & HANSOKUMAKE)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(judoka_tmp->hansokumake), TRUE);

#if (GTKVER == 3)
        gtk_entry_set_completion(GTK_ENTRY(judoka_tmp->club), club_completer);
#else
        g_signal_connect(G_OBJECT(judoka_tmp->club), "key-press-event", 
                         G_CALLBACK(complete_cb), club_completer);
#endif
        row++;

        judoka_tmp->comment = set_entry(table, row++, _("Comment:"), comment);
        gtk_entry_set_max_length(GTK_ENTRY(judoka_tmp->comment), 100);
    } else {
        struct category_data *catdata = NULL;
        catdata = avl_get_category(index);

        if ((catdata && (catdata->deleted & TEAM)) ||
            ptr_to_gint(userdata) == NEW_WCLASS+1) {
            judoka_tmp->last = set_entry(table, 0, _("Team:"), last ? last : "");
        } else {
            judoka_tmp->last = set_entry(table, 0, _("Category:"), last ? last : "");

            tmp = gtk_label_new(_("System:"));
#if (GTKVER == 3)
            gtk_grid_attach(GTK_GRID(table), tmp, 0, 1, 1, 1);
            judoka_tmp->system = tmp = gtk_combo_box_text_new();

            for (i = 0; i < NUM_SYSTEMS; i++)
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, get_system_name_for_menu(i));

            gtk_grid_attach(GTK_GRID(table), tmp, 1, 1, 1, 1);
#else
            gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, 1, 2);
            judoka_tmp->system = tmp = gtk_combo_box_new_text();

            for (i = 0; i < NUM_SYSTEMS; i++)
                gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), get_system_name_for_menu(i));

            gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, 1, 2);
#endif
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), 
                                     catdata ? 
                                     get_system_menu_selection(catdata->system.wishsys) :
                                     CAT_SYSTEM_DEFAULT);

            tmp = gtk_label_new(_("Tatami:"));
#if (GTKVER == 3)
            gtk_grid_attach(GTK_GRID(table), tmp, 0, 2, 1, 1);
            judoka_tmp->belt = tmp = gtk_combo_box_text_new();
            gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, "?");
            for (i = 0; i < NUM_TATAMIS; i++) {
                char buf[10];
                sprintf(buf, "T %d", i+1);
                gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(tmp), NULL, buf);
            }
            gtk_grid_attach(GTK_GRID(table), tmp, 1, 2, 1, 1);
#else
            gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, 2, 3);
            judoka_tmp->belt = tmp = gtk_combo_box_new_text();
            gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), "?");
            for (i = 0; i < NUM_TATAMIS; i++) {
                char buf[10];
                sprintf(buf, "T %d", i+1);
                gtk_combo_box_append_text(GTK_COMBO_BOX(tmp), buf);
            }
            gtk_table_attach_defaults(GTK_TABLE(table), tmp, 1, 2, 2, 3);
#endif
            gtk_combo_box_set_active(GTK_COMBO_BOX(tmp), belt);

            judoka_tmp->birthyear = set_entry(table, 3, _("Group:"), birthyear_s);

            if (treeview && model && index && catdata && (catdata->deleted & TEAM_EVENT)) {
                tmp = gtk_label_new(_("Create default members:"));
#if (GTKVER == 3)
                gtk_grid_attach(GTK_GRID(table), tmp, 0, 4, 1, 1);
#else
                gtk_table_attach_defaults(GTK_TABLE(table), tmp, 0, 1, 4, 5);
#endif
                judoka_tmp->hansokumake = gtk_check_button_new();
#if (GTKVER == 3)
                gtk_grid_attach(GTK_GRID(table), judoka_tmp->hansokumake, 1, 4, 1, 1);
#else
                gtk_table_attach_defaults(GTK_TABLE(table), judoka_tmp->hansokumake, 1, 2, 4, 5);
#endif
                gint n = gtk_tree_model_iter_n_children(model, &iter);
                if (n <= 1) {
                    gtk_widget_set_sensitive(tmp, FALSE);
                    gtk_widget_set_sensitive(judoka_tmp->hansokumake, FALSE);
                }
            }
        }
    }

    g_free(last); 
    g_free(first); 
    g_free(club); 
    g_free(regcategory);
    g_free(category);
    g_free(country);
    g_free(id);
    g_free(comment);
    g_free(coachid);

    gtk_widget_show_all(dialog);
    editing_ongoing = TRUE;
}

void new_judoka(GtkWidget *w, gpointer data)
{
    view_on_row_activated(NULL, NULL, NULL, gint_to_ptr(NEW_JUDOKA));
}

void new_regcategory(GtkWidget *w, gpointer data)
{
    view_on_row_activated(NULL, NULL, NULL, gint_to_ptr(NEW_WCLASS+ptr_to_gint(data)));
}

static void print_competitors_callback(GtkWidget *widget, 
                                       GdkEvent *event,
                                       gpointer data)
{
    gint event_id = ptr_to_gint(event);

    if (event_id == RESPONSE_COACH) {
        num_selected_judokas = 0;
        gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                               (GtkTreeModelForeachFunc) foreach_select_coach,
                               data);
        print_accreditation_cards(FALSE);
    }

    g_free(data);
    gtk_widget_destroy(widget);
}

static gboolean display_coach(GtkWidget *treeview, 
                              GdkEventButton *event, 
                              gpointer userdata)
{
    display_competitor(ptr_to_gint(userdata) | 0x10000);
    return TRUE;
}

void print_competitors_dialog(const gchar *cid, gint ix)
{
    gint i;
    GtkWidget *tmp;
    struct judoka *j;
    gchar buf[128];

    num_selected_judokas = 0;
    gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                           (GtkTreeModelForeachFunc) foreach_select_coach,
                           (gpointer)cid);

    GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Competitor"),
                                                     GTK_WINDOW(main_window),
                                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                     GTK_STOCK_PRINT, RESPONSE_COACH,
                                                     NULL);

    if (ix) {
        tmp = gtk_button_new_with_label("");
        j = get_data(ix);
        if (j) {
            snprintf(buf, sizeof(buf), "%s %s: %s %s",
                     _("Coach"), cid, j->first, j->last);
            gtk_button_set_label(GTK_BUTTON(tmp), buf);
            free_judoka(j);
        }

        g_signal_connect(G_OBJECT(tmp), "button-press-event", 
                         (GCallback) display_coach, gint_to_ptr(ix));
        g_signal_connect(G_OBJECT(tmp), "key-press-event", 
                         (GCallback) display_coach, gint_to_ptr(ix));
    } else {
        tmp = gtk_label_new("");
        gchar *markup = g_markup_printf_escaped ("<b>%s %s</b>", _("Coach"), cid);
        gtk_label_set_markup(GTK_LABEL(tmp), markup);
        g_free (markup);
        g_object_set(tmp, "xalign", 0.0, NULL);
    }

#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       tmp, FALSE, FALSE, 0);
#else
    gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), tmp);
#endif
    for (i = 0; i < num_selected_judokas; i++) {
        j = get_data(selected_judokas[i]);
        if (j) {
            snprintf(buf, sizeof(buf), "    %s %s", j->first, j->last);
            tmp = gtk_label_new(buf);
            g_object_set(tmp, "xalign", 0.0, NULL);
#if (GTKVER == 3)
            gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                               tmp, FALSE, FALSE, 0);
#else
            gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), tmp);
#endif
            free_judoka(j);
        }
    }

    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(print_competitors_callback), g_strdup(cid));
    
    gtk_widget_show_all(dialog);

    num_selected_judokas = 0;
}

/*
 * Create the view
 */

static GtkTreeModel *create_and_fill_model (void)
{
    struct treedata td;
    GtkTreeSortable *sortable;

    td.treestore = gtk_tree_store_new(NUM_COLS,
                                      G_TYPE_UINT,
                                      G_TYPE_STRING, /* last */
                                      G_TYPE_STRING, /* first */
                                      G_TYPE_UINT,   /* birthyear */
                                      G_TYPE_UINT,   /* belt */
                                      G_TYPE_STRING, /* club */
				      G_TYPE_STRING, /* country */
                                      G_TYPE_STRING, /* regcategory */
                                      G_TYPE_INT,    /* weight */
                                      G_TYPE_BOOLEAN,/* visible */
                                      G_TYPE_STRING, /* category */
                                      G_TYPE_UINT,   /* deleted */
				      G_TYPE_STRING, /* id */
                                      G_TYPE_UINT,   /* seeding */
                                      G_TYPE_UINT,   /* clubseeding */
				      G_TYPE_STRING, /* comment */
				      G_TYPE_STRING  /* coachid */
        );
    current_model = (GtkTreeModel *)td.treestore;
    sortable = GTK_TREE_SORTABLE(current_model);
# if 0
    gtk_tree_sortable_set_sort_func(sortable, SORTID_NAME, sort_iter_compare_func,
                                    GINT_TO_POINTER(SORTID_NAME), NULL);

    gtk_tree_sortable_set_sort_func(sortable, SORTID_WEIGHT, sort_iter_compare_func,
                                    GINT_TO_POINTER(SORTID_NAME), NULL);
#endif
    /* set initial sort order */
    gtk_tree_sortable_set_sort_column_id(sortable, SORTID_NAME, GTK_SORT_ASCENDING);

    db_read_categories();
    db_read_judokas();

    return GTK_TREE_MODEL(td.treestore);
}

void last_name_cell_data_func (GtkTreeViewColumn *col,
                               GtkCellRenderer   *renderer,
                               GtkTreeModel      *model,
                               GtkTreeIter       *iter,
                               gpointer           user_data)
{
    extern gint get_competitor_position(gint ix);
    gchar  buf[100];
    gboolean visible;
    gchar *last;
    guint  deleted;
    guint  index;
    guint  weight;
    gint   seeding;
    gchar *comment = NULL;
    GtkTreeIter parent;

    gtk_tree_model_get(model, iter, 
                       COL_INDEX, &index,
                       COL_LAST_NAME, &last, 
                       COL_VISIBLE, &visible, 
                       COL_WEIGHT, &weight,
                       COL_SEEDING, &seeding,
                       COL_DELETED, &deleted, 
                       COL_COMMENT, &comment, -1);

    if (visible) {
        gint jstatus = get_competitor_position(index), cstatus = 0;
        gchar pos[8];
        if (jstatus&0xf)
            sprintf(pos, " [%d]", jstatus&0xf);
        else
            pos[0] = 0;

        if (gtk_tree_model_iter_parent(model, &parent, iter)) {
            guint catindex;
            gtk_tree_model_get(model, &parent, COL_INDEX, &catindex, -1);
            struct category_data *catdata = avl_get_category(catindex);
            if (catdata)
                cstatus = catdata->match_status;
        }

        if (seeding)
            g_snprintf(buf, sizeof(buf), "%s (%d)%s%s", last, seeding,
                       comment && comment[0] ? " *" : "",
                       jstatus&0xf ? pos : "");
        else
            g_snprintf(buf, sizeof(buf), "%s%s%s", last, 
                       comment && comment[0] ? " *" : "",
                       jstatus&0xf ? pos : "");

        if (deleted & HANSOKUMAKE)
            g_object_set(renderer, "strikethrough", TRUE, "cell-background-set", FALSE, NULL);
        else
            g_object_set(renderer, "strikethrough", FALSE, "cell-background-set", FALSE, NULL);

        if (deleted & JUDOGI_OK)
            g_object_set(renderer, 
                         "foreground", "darkgreen", FALSE, 
                         NULL);
        else if (deleted & JUDOGI_NOK)
            g_object_set(renderer, 
                         "foreground", "darkred", FALSE, 
                         NULL);
        else if (jstatus == 0 && (cstatus & REAL_MATCH_EXISTS))
            g_object_set(renderer, 
                         "foreground", "blue", FALSE, 
                         NULL);
        else
            g_object_set(renderer, 
                         "foreground", "black", FALSE, 
                         NULL);
    } else {
        gint status = 0;
        gboolean defined = TRUE;
        gboolean tie = FALSE;
        gboolean extra = FALSE;
        gint numcomp = 0;
		
        if (user_data == NULL) {
            //status = weight;
            struct category_data *catdata = avl_get_category(index);
            if (catdata) {
                deleted = catdata->deleted;
                status = catdata->match_status;
                defined = catdata->defined;
                tie = catdata->tie;
                numcomp = catdata->system.numcomp;
            }

            // look for homeless competitors
            if ((status & SYSTEM_DEFINED) && numcomp < gtk_tree_model_iter_n_children(model, iter))
                extra = TRUE;
#if 0
            if (status & SYSTEM_DEFINED /*REAL_MATCH_EXISTS*/) {
                gboolean ok = gtk_tree_model_iter_children(model, &child, iter);
                while (ok) {
                    gint jindex;
                    gtk_tree_model_get(model, &child, COL_INDEX, &jindex, -1);
                    if ((get_competitor_position(jindex) & COMP_POS_DRAWN) == 0) {
                        extra = TRUE;
                        break;
                    }
                    ok = gtk_tree_model_iter_next(model, &child);
                }
            }
#endif
        }

        if (tie && prop_get_int_val(PROP_RESOLVE_3_WAY_TIES_BY_WEIGHTS) == FALSE)
            g_object_set(renderer, 
                         "cell-background", "Red", 
                         "cell-background-set", TRUE, 
                         NULL);
        else if (extra)
            g_object_set(renderer, 
                         "cell-background", "Lightblue", 
                         "cell-background-set", TRUE, 
                         NULL);
        else if (deleted & TEAM)
            g_object_set(renderer, 
                         "cell-background", "Lightgreen", 
                         "cell-background-set", TRUE, 
                         NULL);
        else if (((status & SYSTEM_DEFINED /*REAL_MATCH_EXISTS*/) && (status & MATCH_UNMATCHED) == 0))
            g_object_set(renderer, 
                         "cell-background", "Green", 
                         "cell-background-set", TRUE, 
                         NULL);
        else if (status & MATCH_MATCHED)
            g_object_set(renderer, 
                         "cell-background", "Orange", 
                         "cell-background-set", TRUE, 
                         NULL);
        else if (status & REAL_MATCH_EXISTS)
            g_object_set(renderer, 
                         "cell-background", "Yellow", 
                         "cell-background-set", TRUE, 
                         NULL);
        else
            g_object_set(renderer, 
                         "cell-background-set", FALSE, 
                         NULL);

        if (last[0] == '_' || (deleted & TEAM))
            g_object_set(renderer, 
                         "foreground", "Gray", FALSE, 
                         NULL);
        else if (deleted & TEAM_EVENT)
            g_object_set(renderer, 
                         "foreground", "Blue", FALSE, 
                         NULL);
        else if (defined)
            g_object_set(renderer, 
                         "foreground", "Black", FALSE, 
                         NULL);
        else
            g_object_set(renderer, 
                         "foreground", "Brown", FALSE, 
                         NULL);

        g_snprintf(buf, sizeof(buf), "%s%s", extra ? "! " : "", last);
        g_object_set(renderer, "strikethrough", FALSE, NULL);
    }

    g_object_set(renderer, "text", buf, NULL);
    g_free(last);
    g_free(comment);
}

void first_name_cell_data_func (GtkTreeViewColumn *col,
				GtkCellRenderer   *renderer,
				GtkTreeModel      *model,
				GtkTreeIter       *iter,
				gpointer           user_data)
{
    gchar  buf[100];
    gboolean visible;
    gchar *first = NULL;
    guint  deleted;
    guint  index;
    guint  weight;

    gtk_tree_model_get(model, iter, 
                       COL_INDEX, &index,
                       COL_FIRST_NAME, &first, 
                       COL_VISIBLE, &visible, 
                       COL_WEIGHT, &weight,
                       COL_DELETED, &deleted, -1);
  
    if (visible) {
        g_snprintf(buf, sizeof(buf), "%s", first);
    } else {
        buf[0] = 0;
        gint n = gtk_tree_model_iter_n_children(model, iter);
        struct category_data *data = avl_get_category(index);
        if (data) {
            if (data->deleted & TEAM)
                g_snprintf(buf, sizeof(buf), "%s", _("Team"));
            else
                g_snprintf(buf, sizeof(buf), "%s", get_system_description(index, n));
        }
    }

    g_object_set(renderer, "text", buf, NULL);
    g_free(first);
}

void birthyear_cell_data_func (GtkTreeViewColumn *col,
                               GtkCellRenderer   *renderer,
                               GtkTreeModel      *model,
                               GtkTreeIter       *iter,
                               gpointer           user_data)
{
    gchar  buf[100];
    gboolean visible;
    guint  index;
    guint  yob;

    gtk_tree_model_get(model, iter, 
                       COL_INDEX, &index,
                       COL_VISIBLE, &visible, 
                       COL_BIRTHYEAR, &yob,
                       -1);
  
    if (visible) {
        g_object_set(renderer,
                     "weight-set", FALSE,
                     NULL);
        g_snprintf(buf, sizeof(buf), "%d", yob);
    } else {
        gchar print_stat = ' ';
        gint n = gtk_tree_model_iter_n_children(model, iter);
        struct category_data *data = avl_get_category(index);
        if (data && (data->match_status & CAT_PRINTED))
            print_stat = 'P';

        if (n <= 2)
            g_object_set(renderer,
                         "weight", PANGO_WEIGHT_BOLD,
                         "weight-set", TRUE,
                         NULL);
        else
            g_object_set(renderer,
                         "weight-set", FALSE,
                         NULL);

        g_snprintf(buf, sizeof(buf), "[%d]%c", n, print_stat);
    }

    g_object_set(renderer, "text", buf, NULL);
}

void weight_cell_data_func (GtkTreeViewColumn *col,
                            GtkCellRenderer   *renderer,
                            GtkTreeModel      *model,
                            GtkTreeIter       *iter,
                            gpointer           user_data)
{
    gchar  buf[64];
    gint   weight, birthyear;
    gboolean visible;

    gtk_tree_model_get(model, iter, 
                       COL_WEIGHT, &weight, 
                       COL_VISIBLE, &visible, 
                       COL_BIRTHYEAR, &birthyear,
                       -1);
  
    if (visible) {
        g_snprintf(buf, sizeof(buf), "%d,%02d", weight/1000, (weight%1000)/10);
        g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
    } else {
        g_snprintf(buf, sizeof(buf), "%s %d", _("Group"), birthyear);
    }

    g_object_set(renderer, "text", buf, NULL);
}

void belt_cell_data_func (GtkTreeViewColumn *col,
                          GtkCellRenderer   *renderer,
                          GtkTreeModel      *model,
                          GtkTreeIter       *iter,
                          gpointer           user_data)
{
    gchar  buf[64];
    guint  belt;
    gboolean visible;

    gtk_tree_model_get(model, iter, COL_BELT, &belt, COL_VISIBLE, &visible, -1);

    strcpy(buf, "?");
  
    if (visible) {
        if (belt >= 0 && belt < NUM_BELTS) {
            g_snprintf(buf, sizeof(buf), "%s", belts[belt]);
            g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
        }
    } else {
        if (belt >= 0 && belt <= NUM_TATAMIS) {
            g_snprintf(buf, sizeof(buf), "T %d", belt);
            g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
        }
    }

    g_object_set(renderer, "text", buf, NULL);
}

void seeding_cell_data_func (GtkTreeViewColumn *col,
                             GtkCellRenderer   *renderer,
                             GtkTreeModel      *model,
                             GtkTreeIter       *iter,
                             gpointer           user_data)
{
    gchar  buf[64];
    gint   seeding, clubseeding;
    gboolean visible;

    gtk_tree_model_get(model, iter, 
                       COL_VISIBLE, &visible, 
                       COL_SEEDING, &seeding,
                       COL_CLUBSEEDING, &clubseeding,
                       -1);
  
    buf[0] = 0;

    if (visible) {
        gint s = user_data ? clubseeding : seeding;
        if (s)
            g_snprintf(buf, sizeof(buf), "%d", s);

        //g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
    }

    g_object_set(renderer, "text", buf, NULL);
}

static gchar *selected_regcategory = NULL;
static gboolean selected_visible;

static gboolean view_on_button_pressed(GtkWidget *treeview, 
                                       GdkEventButton *event, 
                                       gpointer userdata)
{
    gboolean handled = FALSE;

    /* single click with the left or right mouse button? */
    if (event->type == GDK_BUTTON_PRESS  &&  
        (event->button == 1 || event->button == 3)) {
        GtkTreePath *path;
        GtkTreeModel *model;
        GtkTreeIter iter;
        guint index;

        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
                                          (gint) event->x, 
                                          (gint) event->y,
                                          &path, NULL, NULL, NULL)) {
            GtkTreeIter parent;
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
            gtk_tree_model_get_iter(model, &iter, path);

            if (selected_regcategory)
                g_free(selected_regcategory);
            selected_regcategory = NULL;

            gtk_tree_model_get(model, &iter,
                               COL_VISIBLE, &selected_visible, -1);

            if (selected_visible) {
                if (gtk_tree_model_iter_parent(model, &parent, &iter)) {
                    gtk_tree_model_get(model, &parent,
                                       COL_INDEX, &index,
                                       COL_LAST_NAME, &selected_regcategory,
                                       -1);
                }
            } else {
                gtk_tree_model_get(model, &iter,
                                   COL_INDEX, &index,
                                   COL_LAST_NAME, &selected_regcategory,
                                   -1);
            }

            if (event->button == 3) {
                view_popup_menu(treeview, event, 
                                gint_to_ptr(index),
                                selected_regcategory,
                                selected_visible);
                if (selected_visible == FALSE)
                    current_category = index;
                handled = TRUE;
            } else if (event->button == 1 && !selected_visible) {
                current_category = index;
            }

            gtk_tree_path_free(path);
        }
        return handled; /* we handled this */
    }

    return handled; /* we did not handle this */
}

static gboolean set_weight_on_button_pressed(GtkWidget *treeview, 
                                             GdkEventButton *event, 
                                             gpointer userdata)
{
    GtkEntry *weight = userdata;
    GdkEventKey *key = (GdkEventKey *)event;

    if (event->type == GDK_KEY_PRESS && key->keyval != ' ')
        return FALSE;

    if (weight_entry)
        gtk_entry_set_text(GTK_ENTRY(weight), gtk_button_get_label(GTK_BUTTON(weight_entry)));

    return TRUE;
}

#if 0
gboolean view_onPopupMenu (GtkWidget *treeview, gpointer userdata)
{
    view_popup_menu(treeview, NULL, userdata, "?", FALSE);
        
    return TRUE; /* we handled this */
}
#endif

void cell_edited_callback(GtkCellRendererText *cell,
                          gchar               *path_string,
                          gchar               *new_text,
                          gpointer             user_data)
{
    //g_print("new text=%s to %s\n", new_text, path_string);
}

static gboolean view_key_press(GtkWidget *widget, GdkEventKey *event, gpointer userdata)
{
    if (event->keyval == GDK_Shift_L ||
        event->keyval == GDK_Shift_R ||
        event->keyval == GDK_Control_L ||
        event->keyval == GDK_Control_R)
        return TRUE;
    return FALSE;
}

static GtkWidget *create_view_and_model(void)
{
    GtkTreeViewColumn   *col;
    GtkCellRenderer     *renderer;
    GtkWidget           *view;
    GtkTreeModel        *model;
    //GtkTreeSelection    *selection;

    gint col_offset;

    current_view = view = gtk_tree_view_new();
    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(current_view), TRUE);
    //gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(current_view), TRUE);

    /* --- Column last name --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "weight", PANGO_WEIGHT_BOLD,
                 "weight-set", TRUE,
                 "xalign", 0.0,
                 NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Last Name"),
                                                              renderer, "text",
                                                              COL_LAST_NAME,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, last_name_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_LAST_NAME);

    /* --- Column first name --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("First Name"),
                                                              renderer, "text",
                                                              COL_FIRST_NAME,
                                                              //"visible",
                                                              //COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, first_name_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);


    /* --- Column birthyear name --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Year of Birth"),
                                                              renderer, "text",
                                                              COL_BIRTHYEAR,
                                                              //"visible",
                                                              //COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, birthyear_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);


    /* --- Column belt --- */

    renderer = gtk_cell_renderer_text_new();
    //g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Grade"),
                                                              renderer, "text",
                                                              COL_BELT,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, belt_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_BELT);

    /* --- Column club --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Club"),
                                                              renderer, "text",
                                                              COL_CLUB,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_CLUB);

    /* --- Column country --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Country"),
                                                              renderer, "text",
                                                              COL_COUNTRY,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_COUNTRY);

    /* --- Column regcategory --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Reg. Category"),
                                                              renderer, "text",
                                                              COL_WCLASS,
                                                              "visible",
                                                              COL_VISIBLE,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_WCLASS);

    /* --- Column weight --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Weight"),
                                                              renderer, "text",
                                                              COL_WEIGHT,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, weight_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_WEIGHT);

    /* --- Column id --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Id"),
                                                              renderer, "text",
                                                              COL_ID,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_ID);

  /* --- Column seeding --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Seeding"),
                                                              renderer, "text",
                                                              COL_SEEDING,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, seeding_cell_data_func, NULL, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_SEEDING);

  /* --- Column clubseeding --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set (renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (view),
                                                              -1, _("Club Seeding"),
                                                              renderer, "text",
                                                              COL_CLUBSEEDING,
                                                              NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer, seeding_cell_data_func, (gpointer)1, NULL);
    //gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), TRUE);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_CLUBSEEDING);

    /*****/

    model = create_and_fill_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
                                GTK_SELECTION_MULTIPLE);
				    
    g_object_unref(model); /* destroy model automatically with view */

    /*
     * gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
     *                           GTK_SELECTION_NONE);
     */
    g_signal_connect(view, "row-activated", (GCallback) view_on_row_activated, NULL);
    g_signal_connect(view, "button-press-event", (GCallback) view_on_button_pressed, NULL);
    g_signal_connect(view, "key-press-event", G_CALLBACK(view_key_press), NULL);

    //g_signal_connect(view, "popup-menu", (GCallback) view_onPopupMenu, NULL);

    //selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    //gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(current_model),
                                     COL_LAST_NAME,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_NAME),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(current_model),
                                     COL_WEIGHT,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_WEIGHT),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(current_model),
                                     COL_CLUB,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_CLUB),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(current_model),
                                     COL_WCLASS,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_WCLASS),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(current_model),
                                     COL_BELT,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_BELT),
                                     NULL);
    gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE(current_model),
                                     COL_COUNTRY,
                                     sort_iter_compare_func,
                                     GINT_TO_POINTER(SORTID_COUNTRY),
                                     NULL);

    gtk_tree_view_set_search_column(GTK_TREE_VIEW(view), COL_LAST_NAME);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);


    return view;
}

#if 0
static GtkTreeViewSearchEqualFunc old_search_compare_func;

static gboolean compare_search_key(GtkTreeModel *model,
                                   gint column,
                                   const gchar *key,
                                   GtkTreeIter *iter,
                                   gpointer search_data)
{
    gboolean result = old_search_compare_func(model, column, key, iter, search_data);

    if (result == FALSE) {
        GtkTreePath* path = gtk_tree_model_get_path(GTK_TREE_MODEL(current_model), iter);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(current_view), path, NULL, TRUE, 0.5, 0.0 );
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(current_view), path, NULL, FALSE);
        gtk_tree_path_free(path);
    }

    return result;
}
#endif

#if 1
gboolean query_tooltip (GtkWidget  *widget, gint x, gint y, gboolean keyboard_mode,
                        GtkTooltip *tooltip, gpointer user_data)
{
    GtkTreePath* path = NULL;
    GtkTreeViewColumn *col, *last_col;
    gint cx, cy, bx, by;
    gboolean r = FALSE;

    if (keyboard_mode)
        gtk_tree_view_convert_widget_to_tree_coords(GTK_TREE_VIEW(widget), x, y, &bx, &by);
    else
        gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(widget), x, y, &bx, &by);

    if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bx, by, &path,
                                       &col, &cx, &cy))
        goto out;

    last_col = gtk_tree_view_get_column(GTK_TREE_VIEW(widget), 0);

    if (col != last_col)
        goto out;

    if (path) {
        GtkTreeIter iter;
        gtk_tree_model_get_iter(current_model, &iter, path);
#if 0
        printf("INFO: get_path_at_pos: %s -- x,y: %d/%d %d/%d %d/%d\n",
               (path == NULL ? "NULL" : gtk_tree_path_to_string(path)),
               x, y, bx, by, cx, cy);
#endif
        struct judoka *j = get_data_by_iter(&iter);
        if (j) {
            if (j->comment && j->comment[0]) {
                r = TRUE;
                gtk_tooltip_set_text(tooltip, j->comment);
            }
            free_judoka(j);
        }
    }

 out:
    if (path) gtk_tree_path_free(path);

    return r;
}
#endif

/*
 * Page init
 */

void set_judokas_page(GtkWidget *notebook)
{
    GtkWidget *judokas_scrolled_window;
    GtkWidget *view;
    GtkTreeViewColumn *col;
    gboolean retval = FALSE;
    
#if 0
    GtkSettings *settings = gtk_settings_get_default();
    gtk_settings_set_long_property(settings ,"gtk-tooltip-timeout", 500, NULL);
#endif

    /* 
     * list of judokas
     */
    judokas_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(judokas_scrolled_window), 10);

    view = create_view_and_model();
#if 1
    /* TESTING CODE */
    gtk_widget_set_has_tooltip(view, TRUE);
    //gtk_tree_view_set_tooltip_column(view, COL_COMMENT);
    g_signal_connect (view, "query-tooltip", G_CALLBACK(query_tooltip), NULL);
    /****/
#endif
    /* pack the table into the scrolled window */
#if (GTKVER == 3) // && GTK_CHECK_VERSION(3,8,0)
    gtk_container_add(GTK_CONTAINER(judokas_scrolled_window), view);
#else
    gtk_scrolled_window_add_with_viewport (
        GTK_SCROLLED_WINDOW(judokas_scrolled_window), view);
#endif

    competitor_label = gtk_label_new (_("Competitors"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), judokas_scrolled_window, competitor_label);
    update_category_status_info_all();

    col = gtk_tree_view_get_column(GTK_TREE_VIEW(current_view), 0);
    g_signal_emit_by_name(col, "clicked", NULL, &retval);

#if 0
    old_search_compare_func = gtk_tree_view_get_search_equal_func(current_view);
    gtk_tree_view_set_search_equal_func (current_view, compare_search_key, NULL, NULL);
#endif
    //gtk_widget_show_all(notebook);
}

gint sort_by_regcategory(gchar *name1, gchar *name2)
{
    return compare_categories(name1, name2);
}

gint sort_by_name(const gchar *name1, const gchar *name2)
{
    if (name1 == NULL || name2 == NULL) {
        if (name1 == NULL && name2 == NULL)
            return 0;
        return (name1 == NULL) ? -1 : 1;
    }
    return g_utf8_collate(name1,name2);
}


gint sort_iter_compare_func(GtkTreeModel *model,
                            GtkTreeIter  *a,
                            GtkTreeIter  *b,
                            gpointer      userdata)
{
    gint sortcol = GPOINTER_TO_INT(userdata);
    gint ret = 0;

    switch (sortcol) {
    case SORTID_NAME:
    {
        gchar *name1, *name2;
        gboolean visible1, visible2;

        gtk_tree_model_get(model, a, COL_LAST_NAME, &name1, COL_VISIBLE, &visible1, -1);
        gtk_tree_model_get(model, b, COL_LAST_NAME, &name2, COL_VISIBLE, &visible2, -1);

        if (visible1 == FALSE && visible2 == FALSE)
            ret = sort_by_regcategory(name1, name2);
        else
            ret = sort_by_name(name1, name2);

        g_free(name1);
        g_free(name2);
    }
    break;

    case SORTID_WEIGHT:
    {
        gint weight1, weight2;

        gtk_tree_model_get(model, a, COL_WEIGHT, &weight1, -1);
        gtk_tree_model_get(model, b, COL_WEIGHT, &weight2, -1);

        if (weight1 != weight2)
        {
            ret = (weight1 > weight2) ? 1 : -1;
        }
        /* else both equal => ret = 0 */
    }
    break;

    case SORTID_CLUB:
    {
        gchar *name1, *name2;
        gtk_tree_model_get(model, a, COL_CLUB, &name1, -1);
        gtk_tree_model_get(model, b, COL_CLUB, &name2, -1);
        ret = sort_by_name(name1, name2);
        g_free(name1);
        g_free(name2);
    }
    break;

    case SORTID_WCLASS:
    {
        gchar *name1, *name2;
        gint weight1, weight2;
        gtk_tree_model_get(model, a, COL_WCLASS, &name1, COL_WEIGHT, &weight1, -1);
        gtk_tree_model_get(model, b, COL_WCLASS, &name2, COL_WEIGHT, &weight2, -1);
        ret = sort_by_regcategory(name1, name2);
        if (ret == 0 && weight1 != weight2)
            ret = (weight1 > weight2) ? 1 : -1;
        g_free(name1);
        g_free(name2);
    }
    break;

    case SORTID_BELT:
    {
        guint belt1, belt2;

        gtk_tree_model_get(model, a, COL_BELT, &belt1, -1);
        gtk_tree_model_get(model, b, COL_BELT, &belt2, -1);

        if (belt1 != belt2) {
            ret = (belt1 > belt2) ? 1 : -1;
        } else {
            gint weight1, weight2;

            gtk_tree_model_get(model, a, COL_WEIGHT, &weight1, -1);
            gtk_tree_model_get(model, b, COL_WEIGHT, &weight2, -1);

            if (weight1 != weight2) {
                ret = (weight1 > weight2) ? 1 : -1;
            }
        }
    }
    break;

    case SORTID_COUNTRY:
    {
        gchar *club1, *club2, *country1, *country2;
        gtk_tree_model_get(model, a, COL_CLUB, &club1, COL_COUNTRY, &country1, -1);
        gtk_tree_model_get(model, b, COL_CLUB, &club2, COL_COUNTRY, &country2, -1);
        ret = sort_by_name(country1, country2);
	if (ret == 0)
	    ret = sort_by_name(club1, club2);
        g_free(club1);
        g_free(club2);
        g_free(country1);
        g_free(country2);
    }
    break;

    default:
        g_return_val_if_reached(0);
    }

    return ret;
}

static gboolean foreach_regcategory(GtkTreeModel *model,
                                    GtkTreePath  *path,
                                    GtkTreeIter  *iter,
                                    GList       **rowref_list)
{
    GtkTreeIter parent, child;

    g_assert ( rowref_list != NULL );

    if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(current_model),
                                   &parent, iter) == FALSE) {
        /* this is top level */
        if (gtk_tree_model_iter_children(GTK_TREE_MODEL(current_model),
                                         &child, iter) == FALSE) {
            /* no children */
            GtkTreeRowReference  *rowref;
            rowref = gtk_tree_row_reference_new(GTK_TREE_MODEL(current_model), path);
            *rowref_list = g_list_append(*rowref_list, rowref);
        }
    }

    return FALSE; /* do not stop walking the store, call us with next row */
}

void remove_empty_regcategories(GtkWidget *w, gpointer data)
{
    GList *rr_list = NULL;    /* list of GtkTreeRowReferences to remove */
    GList *node;

    gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                           (GtkTreeModelForeachFunc) foreach_regcategory,
                           &rr_list);

    for ( node = rr_list;  node != NULL;  node = node->next ) {
        GtkTreePath *path;

        path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);

        if (path) {
            GtkTreeIter  iter;

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(current_model), &iter, path)) {
                struct judoka *j = get_data_by_iter(&iter);
                j->deleted |= DELETED;
                db_remove_matches(j->index);
                db_update_category(j->index, j);
                free_judoka(j);
                gtk_tree_store_remove((GtkTreeStore *)current_model, &iter);
            }

            /* FIXME/CHECK: Do we need to free the path here? */
        }
    }

    g_list_foreach(rr_list, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free(rr_list);        

    SYS_LOG_INFO(_("Empty categories removed"));
}

/************************************************/

static gboolean foreach_competitor(GtkTreeModel *model,
                                   GtkTreePath  *path,
                                   GtkTreeIter  *iter,
                                   GList       **rowref_list)
{
    gint   weight;
    gboolean visible;

    g_assert ( rowref_list != NULL );

    gtk_tree_model_get(model, iter,
                       COL_VISIBLE, &visible,
                       COL_WEIGHT, &weight,
                       -1);

    if (weight < 10 && visible) {
        GtkTreeRowReference  *rowref;
        rowref = gtk_tree_row_reference_new(GTK_TREE_MODEL(current_model), path);
        *rowref_list = g_list_append(*rowref_list, rowref);
    }

    return FALSE; /* do not stop walking the store, call us with next row */
}

void remove_unweighed_competitors(GtkWidget *w, gpointer data)
{
    GList *rr_list = NULL;    /* list of GtkTreeRowReferences to remove */
    GList *node;

    gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                           (GtkTreeModelForeachFunc) foreach_competitor,
                           &rr_list);

    for (node = rr_list; node != NULL; node = node->next) {
        GtkTreePath *path;

        path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);

        if (path) {
            GtkTreeIter  iter;

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(current_model), &iter, path)) {
                struct judoka *j = get_data_by_iter(&iter);

                if (j && (db_competitor_match_status(j->index) & MATCH_EXISTS) == 0) {
                    j->deleted |= DELETED;
                    db_update_judoka(j->index, j);
                    gtk_tree_store_remove((GtkTreeStore *)current_model, &iter);
                }
                free_judoka(j);
            }

            /* FIXME/CHECK: Do we need to free the path here? */
        }
    }

    g_list_foreach(rr_list, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free(rr_list);        

    SYS_LOG_INFO(_("Unweighted competitors removed"));
}

static gboolean foreach_selected_competitor(GtkTreeModel *model,
                                            GtkTreePath  *path,
                                            GtkTreeIter  *iter,
                                            GList       **rowref_list)
{
    gboolean visible;
    GtkTreeSelection *selection = 
        gtk_tree_view_get_selection(GTK_TREE_VIEW(current_view));

    g_assert ( rowref_list != NULL );

    gtk_tree_model_get(model, iter,
                       COL_VISIBLE, &visible,
                       -1);

    if (visible && gtk_tree_selection_iter_is_selected(selection, iter)) {
        GtkTreeRowReference  *rowref;
        rowref = gtk_tree_row_reference_new(GTK_TREE_MODEL(current_model), path);
        *rowref_list = g_list_append(*rowref_list, rowref);
    }

    return FALSE; /* do not stop walking the store, call us with next row */
}

void remove_competitors(GtkWidget *w, gpointer data)
{
    GList *rr_list = NULL;    /* list of GtkTreeRowReferences to remove */
    GList *node;

    gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                           (GtkTreeModelForeachFunc) foreach_selected_competitor,
                           &rr_list);

    for (node = rr_list; node != NULL; node = node->next) {
        GtkTreePath *path;

        path = gtk_tree_row_reference_get_path((GtkTreeRowReference*)node->data);

        if (path) {
            GtkTreeIter  iter;

            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(current_model), &iter, path)) {
                struct judoka *j = get_data_by_iter(&iter);

                if (j && (db_competitor_match_status(j->index) & MATCH_EXISTS)) {
                    SHOW_MESSAGE("%s %s: %s", 
                                 j->first, j->last, _("Matches exist. Undo the drawing first."));
                    free_judoka(j);
                } else if (j) {
                    j->deleted |= DELETED;
                    db_update_judoka(j->index, j);
                    free_judoka(j);
                    gtk_tree_store_remove((GtkTreeStore *)current_model, &iter);
                }
            }

            /* FIXME/CHECK: Do we need to free the path here? */
        }
    }

    g_list_foreach(rr_list, (GFunc) gtk_tree_row_reference_free, NULL);
    g_list_free(rr_list);        

    SYS_LOG_INFO(_("Competitors removed"));
}

/* barcode stuff */

static gboolean foreach_comp_dsp(GtkTreeModel *model,
                                 GtkTreePath  *path,
                                 GtkTreeIter  *iter,
                                 void         *indx)
{
    gint   id;
    gint   lookfor = ptr_to_gint(indx) & 0xffff;

    gtk_tree_model_get(model, iter,
                       COL_INDEX, &id,
                       -1);

    if (id == lookfor) {
        view_on_row_activated(GTK_TREE_VIEW(current_view), path, NULL, gint_to_ptr(ptr_to_gint(indx) & 0x10000));

        return TRUE;
    }

    return FALSE; /* do not stop walking the store, call us with next row */
}

static void display_competitor(gint indx)
{
    gtk_tree_model_foreach(GTK_TREE_MODEL(current_model),
                           (GtkTreeModelForeachFunc) foreach_comp_dsp,
                           gint_to_ptr(indx));
}

#if 0
static gboolean key_press(GtkWidget *widget, GdkEventKey *event, gpointer userdata)
{
    static gchar keys[6] = {0};
    gchar  label[10];
    gint i;

    if (event->type != GDK_KEY_PRESS)
        return FALSE;

    if (event->keyval == GDK_Return || event->keyval == GDK_KP_Enter) {
        gint jnum = 0;
        for (i = 0; i < 6; i++) {
            jnum = jnum*10 + keys[i];
            keys[i] = 0;
            label[i] = '0';
        }
        label[6] = 0;

        display_competitor(jnum);
                
        if (barcode)
            gtk_label_set_text(GTK_LABEL(barcode), label);

        return TRUE;
    }

    if ((event->keyval >= GDK_0 && event->keyval <= GDK_9) ||
        (event->keyval >= GDK_KP_0 && event->keyval <= GDK_KP_9)) {
        for (i = 0; i < 5; i++)
            keys[i] = keys[i+1];

        if (event->keyval >= GDK_0 && event->keyval <= GDK_9)
            keys[5] = event->keyval - GDK_0;
        else
            keys[5] = event->keyval - GDK_KP_0;
                
        for (i = 0; i < 6; i++)
            label[i] = keys[i] + '0';
        label[6] = 0;

        if (barcode)
            gtk_label_set_text(GTK_LABEL(barcode), label);

        return TRUE;
    }

    if (event->keyval == GDK_BackSpace || event->keyval == GDK_Delete) {
        for (i = 5; i > 0; i--)
            keys[i] = keys[i-1];
        keys[0] = 0;
        for (i = 0; i < 6; i++)
            label[i] = keys[i] + '0';
        label[6] = 0;
        if (barcode)
            gtk_label_set_text(GTK_LABEL(barcode), label);

        return TRUE;
    }

    return FALSE;
}
#endif

static gboolean foreach_select_coach(GtkTreeModel *model,
                                     GtkTreePath  *path,
                                     GtkTreeIter  *iter,
                                     void         *arg)
{
    gchar *coachid = arg;
    gchar *cid = NULL;
    gint   index;

    gtk_tree_model_get(model, iter,
                       COL_INDEX, &index,
                       COL_COACHID, &cid,
                       -1);

    if (coachid && coachid[0] && cid && cid[0] && strcmp(cid, coachid) == 0) {
        if (num_selected_judokas < TOTAL_NUM_COMPETITORS - 1)
            selected_judokas[num_selected_judokas++] = index;
    }

    g_free(cid);

    return FALSE; /* do not stop walking the store, call us with next row */
}

static void on_enter(GtkEntry *entry, gpointer user_data)  { 
    const gchar *the_text;
    gint indx;
    gboolean coach;

    the_text = gtk_entry_get_text(GTK_ENTRY(entry)); 
    indx = db_get_index_by_id(the_text, &coach);

    if (coach) {
        print_competitors_dialog(the_text, indx);
        // if (indx) display_competitor(indx | 0x10000);
    } else if (indx)
        display_competitor(indx);
    else
        display_competitor(atoi(the_text));
    gtk_entry_set_text(GTK_ENTRY(entry), "");
    //gtk_widget_grab_focus(GTK_WIDGET(entry));
}

#if 0
static void on_expose(GtkEntry *dialog, gpointer user_data)  { 
    //g_print("expose\n");
    //gtk_window_present(GTK_WINDOW(dialog));
    //gtk_widget_grab_focus(GTK_WIDGET(dialog));
}
#endif

static void barcode_delete_callback(GtkWidget *widget, 
                                    GdkEvent *event,
                                    gpointer data)
{
    bcdialog = NULL;
    gtk_widget_destroy(widget);
}

void barcode_search(GtkWidget *w, gpointer data)
{
    GtkWidget *dialog, *label, *hbox, *bcentry;

    /* Create a non-modal dialog with one OK button. */
    dialog = gtk_dialog_new_with_buttons (_("Bar code search"), GTK_WINDOW(main_window),
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_CANCEL,
                                          GTK_RESPONSE_REJECT,
                                          NULL);

#if (GTKVER != 3)
    gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
#endif
    label = gtk_label_new (_("Type the barcode:"));
    //barcode = gtk_label_new ("000000");
    bcentry = gtk_entry_new();
    gtk_widget_set_can_focus(GTK_WIDGET(bcentry), TRUE);

#if (GTKVER == 3)
    hbox = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(hbox), label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), bcentry, 1, 0, 1, 1);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), 
                       hbox, FALSE, FALSE, 0);
#else
    hbox = gtk_hbox_new (FALSE, 5);
    gtk_container_set_border_width (GTK_CONTAINER (hbox), 10);
    gtk_box_pack_start_defaults (GTK_BOX (hbox), label);
    //gtk_box_pack_start_defaults (GTK_BOX (hbox), barcode);
    gtk_box_pack_start_defaults (GTK_BOX (hbox), bcentry);

    gtk_box_pack_start_defaults(GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox);
#endif
    gtk_widget_show_all(dialog);

    bcdialog = (void *)dialog;

    /* Call gtk_widget_destroy() when the dialog emits the response signal. */
    g_signal_connect(G_OBJECT(bcentry), 
                     "activate", G_CALLBACK(on_enter), dialog);
    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(barcode_delete_callback), NULL);
#if 0
    g_signal_connect(G_OBJECT(dialog), 
                     "key-press-event", G_CALLBACK(key_press), NULL);
#endif
}

static void set_col_title(gint column, gchar *title)
{
    GtkTreeViewColumn *col = gtk_tree_view_get_column(GTK_TREE_VIEW(current_view), column);
    if (col)
        gtk_tree_view_column_set_title(col, title);
    else
        g_print("No column for '%s'!\n", title);
}

void set_competitors_col_titles(void)
{
    if (!current_view)
        return;

    set_col_title(COL_VISIBLE_LAST_NAME, _("Last Name"));
    set_col_title(COL_VISIBLE_FIRST_NAME, _("First Name"));
    set_col_title(COL_VISIBLE_BIRTHYEAR, _("Year of Birth"));
    set_col_title(COL_VISIBLE_BELT, _("Grade"));
    set_col_title(COL_VISIBLE_CLUB, _("Club"));
    set_col_title(COL_VISIBLE_COUNTRY, _("Country"));
    set_col_title(COL_VISIBLE_WCLASS, _("Reg. Category"));
    set_col_title(COL_VISIBLE_WEIGHT, _("Weight"));
    set_col_title(COL_VISIBLE_ID, _("Id"));
    set_col_title(COL_VISIBLE_SEEDING, _("Seeding"));
    set_col_title(COL_VISIBLE_CLUBSEEDING, _("Club Seeding"));

    if (competitor_label)
        gtk_label_set_text(GTK_LABEL(competitor_label), _("Competitors"));
}
