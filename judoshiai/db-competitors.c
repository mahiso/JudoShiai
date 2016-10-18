/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <assert.h>

#include "sqlite3.h"
#include "judoshiai.h"
#include "language.h"

extern void write_competitor(FILE *f, const gchar *first, const gchar *last, const gchar *belt, 
                             const gchar *club, const gchar *category, const gint index, const gboolean by_club);

#define ADD_COMPETITORS               1
#define ADD_COMPETITORS_WITH_WEIGHTS  2
#define ADD_DELETED_COMPETITORS       4
#define PRINT_COMPETITORS             8
#define COMPETITOR_STATISTICS        16
#define PRINT_COMPETITORS_BY_CLUB    32
#define FIND_COMPETITOR_BY_ID        64
#define CLEANUP                     128
#define FIND_COMPETITOR_BY_COACHID  256
#define UPDATE_WEIGHTS              512
#define LIST_COMPETITORS           1024

static FILE *print_file = NULL;
static gint num_competitors;
static gint num_weighted_competitors;
static gint competitors_not_added, competitors_added;
static gint competitor_by_id;
static gboolean competitor_by_coach;
static gint weights_updated;

static gint competitor_list_next, competitor_list_count;
static gint competitor_list[TOTAL_NUM_COMPETITORS];

static int db_callback(void *data, int argc, char **argv, char **azColName)
{
    int i, flags = ptr_to_gint(data);
    struct judoka j;
    gchar *newcat = NULL;

    for(i = 0; i < argc; i++){
        //g_print("  %s=%s", azColName[i], argv[i] ? argv[i] : "(NULL)");
        if (IS(index))
            j.index = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(last))
            j.last = argv[i] ? argv[i] : "?";
        else if (IS(first))
            j.first = argv[i] ? argv[i] : "?";
        else if (IS(birthyear))
            j.birthyear = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(club))
            j.club = argv[i] ? argv[i] : "?";
        else if (IS(regcategory) || IS(wclass))
            j.regcategory = argv[i] ? argv[i] : "?";
        else if (IS(belt))
            j.belt = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(weight))
            j.weight = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(visible))
            j.visible = argv[i] ? atoi(argv[i]) : 1;
        else if (IS(category))
            j.category = argv[i] ? argv[i] : "?";
        else if (IS(deleted))
            j.deleted = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(country))
            j.country = argv[i] ? argv[i] : "";
        else if (IS(id))
            j.id = argv[i] ? argv[i] : "";
        else if (IS(seeding))
            j.seeding = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(clubseeding))
            j.clubseeding = argv[i] ? atoi(argv[i]) : 0;
        else if (IS(comment))
            j.comment = argv[i] ? argv[i] : "";
        else if (IS(coachid))
            j.coachid = argv[i] ? argv[i] : "";
    }
    //g_print("\n");

    if (flags & ADD_DELETED_COMPETITORS) {
        if ((j.deleted & DELETED) && j.visible) {
            j.deleted &= ~DELETED;
            if (j.index >= current_index)
                current_index = j.index + 1;
            display_one_judoka(&j);

            avl_set_competitor(j.index, &j);
            //avl_set_competitor_status(j.index, j.deleted);
        }
        return 0;
    }

    if ((j.deleted & DELETED) || j.visible == 0) {
        if (j.visible && j.index >= current_index)
            current_index = j.index + 1;
        return 0;
    }

    if (flags & (PRINT_COMPETITORS | PRINT_COMPETITORS_BY_CLUB)) {
        if (print_file == NULL)
            return 1;
        const gchar *b = "?";
        if (j.belt >= 0 && j.belt < NUM_BELTS)
            b = belts[j.belt];
        write_competitor(print_file, j.first, j.last, b, 
                         get_club_text(&j, CLUB_TEXT_ADDRESS), 
                         j.category, j.index, flags & PRINT_COMPETITORS_BY_CLUB);
        write_competitor_for_coach_display(&j);
        return 0;
    }

    if (flags & FIND_COMPETITOR_BY_ID) {
        competitor_by_id = j.index;
        return 1;
    }

    if (flags & FIND_COMPETITOR_BY_COACHID) {
        competitor_by_coach = TRUE;
        return 1;
    }

    if (flags & COMPETITOR_STATISTICS) {
        num_competitors++;
        if (j.weight)
            num_weighted_competitors++;
    }

    if (flags & UPDATE_WEIGHTS) {
        GtkTreeIter iter;
        if (find_iter_name_id(&iter, j.last, j.first, j.club, j.regcategory, j.id)) {
            struct judoka *j1 = get_data_by_iter(&iter);
            if (j1) {
                if (j1->weight == 0 && j.weight) {
                    GtkTreeSelection *selection = 
                        gtk_tree_view_get_selection(GTK_TREE_VIEW(current_view));
                    gtk_tree_selection_select_iter(selection, &iter);
                    j1->weight = j.weight;
                    put_data_by_iter(j1, &iter);
                    db_update_judoka(j1->index, j1);
                    weights_updated++;
                } else if (j1->weight && j.weight && j1->weight != j.weight) {
                    SHOW_MESSAGE("%s %s: %s!", j1->first, j1->last, _("Weights do not match"));
                }

                free_judoka(j1);
            }
        }
        return 0;
    }

    if (flags & LIST_COMPETITORS) {
        if (competitor_list_count < TOTAL_NUM_COMPETITORS)
            competitor_list[competitor_list_count++] = j.index;
        return 0;
    }

    if ((flags & ADD_COMPETITORS_WITH_WEIGHTS) ||
        (flags & ADD_COMPETITORS)) {
        GtkTreeIter iter;
        if ((flags & CLEANUP) && find_iter_name(&iter, j.last, j.first, j.club)) {
            competitors_not_added++;
            return 0;
        }
        j.index = current_index++;
        j.category = "?";

        if (flags & CLEANUP) {
            newcat = find_correct_category(j.birthyear ? current_year - j.birthyear : 0, 
                                           j.weight, 
                                           (j.deleted & GENDER_FEMALE) ? IS_FEMALE : 
                                           (j.deleted & GENDER_MALE ? IS_MALE : 0), 
                                           j.regcategory, TRUE);
            if (newcat)
                j.regcategory = newcat;
        }

        if (flags & ADD_COMPETITORS)
            j.weight = 0;
        db_add_judoka(j.index, &j);

        competitors_added++;
    } else {
        if (j.index >= current_index)
            current_index = j.index + 1;
    }

    display_one_judoka(&j);
    g_free(newcat);

    avl_set_competitor(j.index, &j);
    //avl_set_competitor_status(j.index, j.deleted);

    //find_gender(j.first);

    return 0;
}

gint db_add_judoka(int num, struct judoka *j)
{
    char buffer[1000];

    snprintf(buffer, sizeof(buffer), 
            "INSERT INTO competitors VALUES ("
            "%d, \"%s\", \"%s\", \"%d\", "
            "%d, \"%s\", \"%s\", "
            "%d, %d, \"%s\", %d, \"%s\", \"%s\","
            "%d, %d, \"%s\", \"%s\" "
            ")", 
            num, esc_quote(j->last), esc_quote(j->first), j->birthyear,
            j->belt, esc_quote(j->club), esc_quote(j->regcategory),
            j->weight, j->visible, esc_quote(j->category), j->deleted, 
            esc_quote(j->country), esc_quote(j->id),
            j->seeding, j->clubseeding, esc_quote(j->comment), esc_quote(j->coachid));

    gint rc = db_exec(db_name, buffer, NULL, db_callback);

    if (rc == SQLITE_OK) {
        avl_set_competitor(num, j);
        //avl_set_competitor_status(num, j->deleted);
    } else
        g_print("Error = %d (%s:%d)\n", rc, __FUNCTION__, __LINE__);

    return rc;
}

void db_update_judoka(int num, struct judoka *j)
{
    if (j->category)
        db_exec_str(NULL, db_callback,
                    "UPDATE competitors SET "
                    "last=\"%s\", first=\"%s\", birthyear=\"%d\", "
                    "belt=\"%d\", club=\"%s\", regcategory=\"%s\", "
                    "weight=\"%d\", visible=\"%d\", "
                    "category=\"%s\", deleted=\"%d\", country=\"%s\", id=\"%s\", "
                    "seeding=\"%d\", clubseeding=\"%d\", comment=\"%s\", coachid=\"%s\" "
                    "WHERE \"index\"=%d",
                    esc_quote(j->last), esc_quote(j->first), j->birthyear,
                    j->belt, esc_quote(j->club), esc_quote(j->regcategory),
                    j->weight, j->visible, esc_quote(j->category), j->deleted, esc_quote(j->country), 
                    esc_quote(j->id), 
                    j->seeding, j->clubseeding, esc_quote(j->comment), esc_quote(j->coachid), num);
    else
        db_exec_str(NULL, db_callback,
                    "UPDATE competitors SET "
                    "last=\"%s\", first=\"%s\", birthyear=\"%d\", "
                    "belt=\"%d\", club=\"%s\", regcategory=\"%s\", "
                    "weight=\"%d\", visible=\"%d\", "
                    "deleted=\"%d\", country=\"%s\", id=\"%s\" "
                    "seeding=\"%d\", clubseeding=\"%d\", comment=\"%s\", coachid=\"%s\" "
                    "WHERE \"index\"=%d",
                    esc_quote(j->last), esc_quote(j->first), j->birthyear,
                    j->belt, esc_quote(j->club), esc_quote(j->regcategory),
                    j->weight, j->visible, j->deleted, esc_quote(j->country), esc_quote(j->id), 
                    j->seeding, j->clubseeding, esc_quote(j->comment), esc_quote(j->coachid), num);

    avl_set_competitor(num, j);
    //avl_set_competitor_status(num, j->deleted);
}

void db_restore_removed_competitors(void)
{
    db_exec(db_name, "UPDATE competitors SET \"category\"=\"?\" WHERE \"deleted\"&1", 
            0, 
            db_callback);

    db_exec(db_name, "SELECT * FROM competitors WHERE \"deleted\"&1", 
            (void *)ADD_DELETED_COMPETITORS, 
            db_callback);

    db_exec(db_name, "UPDATE competitors SET \"deleted\"=\"deleted\"&~1 WHERE \"deleted\"&1", 
            0, 
            0);
}

void db_print_competitors(FILE *f)
{
    print_file = f;
    if (print_file == NULL)
        return;

    fprintf(print_file,
            "<td><table class=\"competitors\">"
            "<tr><td colspan=\"5\" align=\"center\"><h2>%s</h2></td></tr>\n", _T(competitor));
    fprintf(print_file, "<tr><th>%s</th><th>%s</th><th><a href=\"competitors2.html\">%s</a></th><th>%s</th><th>&nbsp;%s&nbsp;</th></tr>\n", 
            _T(name), grade_visible ? _T(grade) : "", 
            (club_text & CLUB_TEXT_COUNTRY) ? _T(country) :_T(club), 
            _T(category), create_statistics ? _T(position) : "");

    write_competitor_for_coach_display(NULL); // init file

    if (IS_LANG_IS)
        db_exec(db_name, "SELECT * FROM competitors ORDER BY \"first\" ASC, \"last\" ASC", 
                (void *)PRINT_COMPETITORS, 
                db_callback);
    else
        db_exec(db_name, "SELECT * FROM competitors ORDER BY \"last\" ASC, \"first\" ASC", 
                (void *)PRINT_COMPETITORS, 
                db_callback);

    fprintf(print_file, "</table></td>\n");
}

void db_list_competitors(gboolean by_club)
{
    competitor_list_count = competitor_list_next = 0;

    if (by_club) {
        if (club_text & CLUB_TEXT_COUNTRY) {
            if (club_text & CLUB_TEXT_CLUB)
                db_exec(db_name, "SELECT * FROM competitors ORDER BY \"country\" ASC, \"club\" ASC, \"last\" ASC, \"first\" ASC", 
                        (void *)LIST_COMPETITORS, 
                        db_callback);
            else
                db_exec(db_name, "SELECT * FROM competitors ORDER BY \"country\" ASC, \"last\" ASC, \"first\" ASC", 
                        (void *)LIST_COMPETITORS, 
                        db_callback);
        } else
            db_exec(db_name, "SELECT * FROM competitors ORDER BY \"club\" ASC, \"last\" ASC, \"first\" ASC", 
                    (void *)LIST_COMPETITORS, 
                    db_callback);
    } else {
        if (IS_LANG_IS)
            db_exec(db_name, "SELECT * FROM competitors ORDER BY \"first\" ASC, \"last\" ASC", 
                    (void *)LIST_COMPETITORS, 
                    db_callback);
        else
            db_exec(db_name, "SELECT * FROM competitors ORDER BY \"last\" ASC, \"first\" ASC", 
                    (void *)LIST_COMPETITORS, 
                    db_callback);
    }
}

gint db_get_next_listed_competitor(void)
{
    if (competitor_list_next < competitor_list_count)
        return competitor_list[competitor_list_next++];
    return 0;
}

void db_print_competitors_by_club(FILE *f)
{
    print_file = f;
    if (print_file == NULL)
        return;

    fprintf(print_file,
            "<td><table class=\"competitors\">"
            "<tr><td colspan=\"6\" align=\"center\"><h2>%s</h2></td></tr>\n", _T(competitor));
    fprintf(print_file, "<tr><th></th><th>%s</th><th><a href=\"competitors.html\">%s</a></th><th>%s</th><th>%s</th><th>&nbsp;%s&nbsp;</th></tr>\n", 
            _T(club), _T(name), grade_visible ? _T(grade) : "", _T(category), create_statistics ? _T(position) : "");

    if (club_text & CLUB_TEXT_COUNTRY) {
        if (club_text & CLUB_TEXT_CLUB)
            db_exec(db_name, "SELECT * FROM competitors ORDER BY \"country\" ASC, \"club\" ASC, \"last\" ASC, \"first\" ASC", 
                    (void *)PRINT_COMPETITORS_BY_CLUB, 
                    db_callback);
        else
            db_exec(db_name, "SELECT * FROM competitors ORDER BY \"country\" ASC, \"last\" ASC, \"first\" ASC", 
                    (void *)PRINT_COMPETITORS_BY_CLUB, 
                    db_callback);
    } else
        db_exec(db_name, "SELECT * FROM competitors ORDER BY \"club\" ASC, \"last\" ASC, \"first\" ASC", 
                (void *)PRINT_COMPETITORS_BY_CLUB, 
                db_callback);

    fprintf(print_file, "</table></td>\n");
}

void db_read_judokas(void)
{
    char buffer[100];

    sprintf(buffer, 
            "SELECT * FROM competitors");
    db_exec(db_name, buffer, NULL, db_callback);
}

void db_read_competitor_statistics(gint *numcomp, gint *numweighted)
{
    num_competitors = num_weighted_competitors = 0;
    db_exec(db_name, "SELECT * FROM competitors", (void *)COMPETITOR_STATISTICS, db_callback);
    *numcomp = num_competitors;
    *numweighted = num_weighted_competitors;
}

void db_add_competitors(const gchar *competition, gboolean with_weight, 
			gboolean weighted, gboolean cleanup, gint *added, gint *not_added)
{
    gint flags = 0;

    competitors_not_added = competitors_added = 0;

    if (with_weight)
        flags = ADD_COMPETITORS_WITH_WEIGHTS;
    else
        flags = ADD_COMPETITORS;

    if (cleanup)
        flags |= CLEANUP;

    if (weighted)
        db_exec(competition, "SELECT * FROM competitors WHERE \"weight\">0", gint_to_ptr(flags), db_callback);
    else
        db_exec(competition, "SELECT * FROM competitors", gint_to_ptr(flags), db_callback);

    *added = competitors_added;
    *not_added = competitors_not_added;
}

void db_update_weights(const gchar *competition, gint *updated)
{
    weights_updated = 0;
    db_exec(competition, "SELECT * FROM competitors WHERE \"weight\">0", (gpointer)UPDATE_WEIGHTS, db_callback);
    *updated = weights_updated;
}

#if 0 
static gboolean has_hansokumake;

static int db_hansokumake_cb(void *data, int argc, char **argv, char **azColName)
{
    has_hansokumake = TRUE;
    return 1;
}
#endif

gboolean db_has_hansokumake(gint competitor)
{
    return avl_get_competitor_status(competitor) & HANSOKUMAKE;
#if 0
    char buffer[100];

    has_hansokumake = FALSE;

    sprintf(buffer, 
            "SELECT * FROM competitors WHERE \"index\"=%d AND \"deleted\"&2",
            competitor);

    db_exec(db_name, buffer, NULL, db_hansokumake_cb);

    return has_hansokumake;
#endif
}

void db_set_match_hansokumake(gint category, gint number, gint blue, gint white)
{ 
    gchar buffer[600];

    if (blue) {
        sprintf(buffer, 
                "UPDATE competitors SET \"deleted\"=%d "
                "WHERE EXISTS (SELECT * FROM matches "
                "WHERE competitors.'index'=matches.'blue' AND "
                "matches.'category'=%d AND matches.'number'=%d)", 
                HANSOKUMAKE, category, number);
        db_exec(db_name, buffer, NULL, NULL);
        sprintf(buffer, 
                "SELECT * from competitors "
                "WHERE EXISTS (SELECT * FROM matches "
                "WHERE competitors.'index'=matches.'blue' AND "
                "matches.'category'=%d AND matches.'number'=%d)", 
                category, number);
        db_exec(db_name, buffer, NULL, db_callback);
    }
    if (white) {
        sprintf(buffer, 
                "UPDATE competitors SET \"deleted\"=%d "
                "WHERE EXISTS (SELECT * FROM matches "
                "WHERE competitors.'index'=matches.'white' AND "
                "matches.'category'=%d AND matches.'number'=%d)", 
                HANSOKUMAKE, category, number);
        db_exec(db_name, buffer, NULL, NULL);
        sprintf(buffer, 
                "SELECT * from competitors "
                "WHERE EXISTS (SELECT * FROM matches "
                "WHERE competitors.'index'=matches.'white' AND "
                "matches.'category'=%d AND matches.'number'=%d)", 
                category, number);
        db_exec(db_name, buffer, NULL, db_callback);
    }
}

gint db_get_index_by_id(const gchar *id, gboolean *coach)
{
    competitor_by_id = 0;
    competitor_by_coach = FALSE;

    db_exec_str((gpointer)FIND_COMPETITOR_BY_ID, db_callback,
                "SELECT * FROM competitors WHERE \"id\"=\"%s\"", id);
    db_exec_str((gpointer)FIND_COMPETITOR_BY_COACHID, db_callback,
                "SELECT * FROM competitors WHERE \"coachid\"=\"%s\"", id);

    *coach = competitor_by_coach;

    return competitor_by_id;
}


void write_competitor_for_coach_display(struct judoka *j)
{
    if (automatic_web_page_update == FALSE)
        return;

    gint i;
    FILE *f;
    gchar buf[32];
    gchar *file = g_build_filename(current_directory, "c-ids.txt", NULL);

    if (j == NULL) { // initialize file
        f = fopen(file, "wb");
        g_free(file);
        if (!f)
            return;
        for (i = 0; i < 10000; i++) {
            fprintf(f, "\t\t%32s\n", " ");
        }
        fclose(f);
        return;
    }

    // list of ids
    f = fopen(file, "r+b");
    g_free(file);
    if (!f)
        return;

    const gchar *id = j->id ? j->id : "";
    const gchar *cid = j->coachid ? j->coachid : "";
    gint dummy = 32 - strlen(id) - strlen(cid);

    if (fseek(f, j->index*35, SEEK_SET) == 0) {
        fprintf(f, "%s\t%s\t", id, cid);
        for (i = 0; i < dummy; i++) 
            fprintf(f, ".");
        fprintf(f, "\n");
    }
    fclose(f);

    // individual file for competitor
    snprintf(buf, sizeof(buf), "c-%d.txt", j->index);
    file = g_build_filename(current_directory, buf, NULL);
    f = fopen(file, "wb");
    g_free(file);
    if (!f)
        return;

#define X(_x) (_x ? _x : "")

    fprintf(f, "%s\n%s\n%d\n%s\n%s\n%s\n%d\n"
            "%s\n%d\n%s\n%s\n%d\n%d\n%s\n%s",
            X(j->last), X(j->first), j->birthyear,
            belts[j->belt], X(j->club), X(j->regcategory),
            j->weight, X(j->category), j->deleted,
            X(j->country), X(j->id), j->seeding, j->clubseeding,
            X(j->comment), X(j->coachid));


    fclose(f);

    //snprintf(buf, sizeof(buf), "c-%d.txt", j->index);
}

