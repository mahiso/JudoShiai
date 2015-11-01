/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2015 by Maik Hinrichs
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "judoshiai.h"

/**
 * Choose and open a file
 **/
static FILE *open_file()
{
	FILE *f = NULL;
	
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	
	dialog = gtk_file_chooser_dialog_new (_("CSV Results File"),
                                          NULL,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);
    
    chooser = GTK_FILE_CHOOSER(dialog);

	// Set current dir to database dir	
	gchar *dirname = g_path_get_dirname(database_name);
        gtk_file_chooser_set_current_folder(chooser, dirname);
        g_free(dirname);
        
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER(dialog), TRUE);
    
    gtk_file_chooser_set_current_name (chooser, _("results.csv"));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        	gchar *name;
            gchar csv_file_name[200];
            
        	name = gtk_file_chooser_get_filename(chooser);
        	if (strstr(name, ".csv") == NULL) {
            		sprintf(csv_file_name, "%s.csv", name);
        	} else {
            		sprintf(csv_file_name, "%s", name);
        	}
        	
			g_free(name);
			
			f = fopen(csv_file_name,"wb");
	}

	gtk_widget_destroy(dialog);

	return f;
}

void write_header_csv_file(FILE *f)
{
	fprintf(f,"\"LASTNAME\",\"FIRSTNAME\",\"CLUB\",\"CATEGORY\",\"POSITION\"\n");
}

static void write_result_row(FILE *f, gint category, gint num, const gchar *first, const gchar *last, const gchar *club)
{
    struct judoka *ctg = NULL;
    ctg = get_data(category);
    
    fprintf(f,"\"%s\",\"%s\",\"%s\",\"%s\",%d\n",last,first,club,ctg->last,num);
}

static void pool_results(FILE *f, gint category, struct judoka *ctg, gint num_judokas, gboolean dpool2)
{
    struct pool_matches pm;
    gint i;

    fill_pool_struct(category, num_judokas, &pm, dpool2);

    if (dpool2)
        num_judokas = 4;

    if (pm.finished)
        get_pool_winner(num_judokas, pm.c, pm.yes, pm.wins, pm.pts, pm.mw, pm.j, pm.all_matched, pm.tie);

    for (i = 1; i <= num_judokas; i++) {
        if (pm.finished == FALSE || pm.j[pm.c[i]] == NULL)
            continue;

        // Two bronzes in pool system
        if (i <= 4 && prop_get_int_val(PROP_TWO_POOL_BRONZES) &&
            (pm.j[pm.c[i]]->deleted & HANSOKUMAKE) == 0) {
            write_result_row(f, category, i == 4 ? 3 : i, pm.j[pm.c[i]]->first, 
                         pm.j[pm.c[i]]->last, pm.j[pm.c[i]]->club);
            avl_set_competitor_position(pm.j[pm.c[i]]->index, i == 4 ? 3 : i);
            db_set_category_positions(category, pm.j[pm.c[i]]->index, i);
		} else if (
            (pm.j[pm.c[i]]->deleted & HANSOKUMAKE) == 0) {
            write_result_row(f, category, i, pm.j[pm.c[i]]->first, 
                         pm.j[pm.c[i]]->last, pm.j[pm.c[i]]->club);
            avl_set_competitor_position(pm.j[pm.c[i]]->index, i);
            db_set_category_positions(category, pm.j[pm.c[i]]->index, i);
        }
    }

    /* clean up */
    empty_pool_struct(&pm);
}

static void dqpool_results(FILE *f, gint category, struct judoka *ctg, gint num_judokas, struct compsys sys)
{
    struct pool_matches pm;
    struct judoka *j1 = NULL;
    gint i;
    int gold = 0, silver = 0, bronze1 = 0, bronze2 = 0;

    fill_pool_struct(category, num_judokas, &pm, FALSE);

    i = num_matches(sys.system, num_judokas) + 
        ((sys.system == SYSTEM_DPOOL || sys.system == SYSTEM_DPOOL3) ? 1 : 5);

    if (sys.system == SYSTEM_DPOOL3) {
        if (COMP_1_PTS_WIN(pm.m[i]))
            bronze1 = pm.m[i].blue;
        else
            bronze1 = pm.m[i].white;

        i++;
    } else {
        /* first semifinal */
        if (COMP_1_PTS_WIN(pm.m[i]))
            bronze1 = pm.m[i].white;
        else
            bronze1 = pm.m[i].blue;

        i++;

        /* second semifinal */
        if (COMP_1_PTS_WIN(pm.m[i]))
            bronze2 = pm.m[i].white;
        else
            bronze2 = pm.m[i].blue;
        
        i++;
    }

    /* final */
    if (COMP_1_PTS_WIN(pm.m[i]) || pm.m[i].white == GHOST) {
        gold = pm.m[i].blue;
        silver = pm.m[i].white;
    } else if (COMP_2_PTS_WIN(pm.m[i]) || pm.m[i].blue == GHOST) {
        gold = pm.m[i].white;
        silver = pm.m[i].blue;
    }

    if (gold && (j1 = get_data(gold))) {
        write_result_row(f, category, 1, j1->first, j1->last, j1->club);
        avl_set_competitor_position(gold, 1);
        db_set_category_positions(category, gold, 1);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(silver))) {
        write_result_row(f, category, 2, j1->first, j1->last, j1->club);
        avl_set_competitor_position(silver, 2);
        db_set_category_positions(category, silver, 2);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(bronze1))) {
        write_result_row(f, category, 3, j1->first, j1->last, j1->club);
        avl_set_competitor_position(bronze1, 3);
        db_set_category_positions(category, bronze1, 3);
        free_judoka(j1);
    }
    if (sys.system != SYSTEM_DPOOL3 && gold && (j1 = get_data(bronze2))) {
        write_result_row(f, category, 3, j1->first, j1->last, j1->club);
        avl_set_competitor_position(bronze2, 3);
        db_set_category_positions(category, bronze2, 4);
        free_judoka(j1);
    }

    /* clean up */
    empty_pool_struct(&pm);
}


#define GET_WINNER_AND_LOSER(_w)                                        \
    winner = (COMP_1_PTS_WIN(m[_w]) || m[_w].white==GHOST) ? m[_w].blue : \
        ((COMP_2_PTS_WIN(m[_w]) || m[_w].blue==GHOST) ? m[_w].white : 0); \
    loser = (COMP_1_PTS_WIN(m[_w]) || m[_w].white==GHOST) ? m[_w].white : \
        ((COMP_2_PTS_WIN(m[_w]) || m[_w].blue==GHOST) ? m[_w].blue : 0)

#define GET_GOLD(_w)                                                    \
    gold = (COMP_1_PTS_WIN(m[_w]) || m[_w].white==GHOST) ? m[_w].blue : \
        ((COMP_2_PTS_WIN(m[_w]) || m[_w].blue==GHOST) ? m[_w].white : 0); \
    silver = (COMP_1_PTS_WIN(m[_w]) || m[_w].white==GHOST) ? m[_w].white : \
        ((COMP_2_PTS_WIN(m[_w]) || m[_w].blue==GHOST) ? m[_w].blue : 0)

#define GET_BRONZE1(_w)                                                 \
    bronze1 = (COMP_1_PTS_WIN(m[_w]) || m[_w].white==GHOST) ? m[_w].blue : \
        ((COMP_2_PTS_WIN(m[_w]) || m[_w].blue==GHOST) ? m[_w].white : 0); \
    fifth1 = (COMP_1_PTS_WIN(m[_w]) && m[_w].white > GHOST) ? m[_w].white : \
        ((COMP_2_PTS_WIN(m[_w]) && m[_w].blue > GHOST) ? m[_w].blue : 0)

#define GET_BRONZE2(_w)                                                 \
    bronze2 = (COMP_1_PTS_WIN(m[_w]) || m[_w].white==GHOST) ? m[_w].blue : \
        ((COMP_2_PTS_WIN(m[_w]) || m[_w].blue==GHOST) ? m[_w].white : 0); \
    fifth2 = (COMP_1_PTS_WIN(m[_w]) && m[_w].white > GHOST) ? m[_w].white : \
        ((COMP_2_PTS_WIN(m[_w]) && m[_w].blue > GHOST) ? m[_w].blue : 0)


static void french_results(FILE *f, gint category, struct judoka *ctg,
                           gint num_judokas, struct compsys systm, gint pagenum)
{
    struct match m[NUM_MATCHES];
    struct judoka *j1;
    gint gold = 0, silver = 0, bronze1 = 0, bronze2 = 0, fourth = 0, 
        fifth1 = 0, fifth2 = 0, seventh1 = 0, seventh2 = 0;
    gint winner= 0, loser = 0;
    gint sys = systm.system - SYSTEM_FRENCH_8;
    gint table = systm.table;

    memset(m, 0, sizeof(m));
    db_read_category_matches(category, m);

    GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 1, 1));
    gold = winner;
    silver = loser;
    if (table == TABLE_MODIFIED_DOUBLE_ELIMINATION) {
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 2, 1));
        silver = winner;
        bronze1 = loser;
    } else if (one_bronze(table, sys)) {
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 3, 1));
        bronze1 = winner;
        fourth = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 5, 1));
        fifth1 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 5, 2));
        fifth2 = loser;
    } else if (table == TABLE_DOUBLE_LOST) {
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 3, 1));
        bronze1 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 3, 2));
        bronze2 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 5, 1));
        fifth1 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 5, 2));
        fifth2 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 7, 1));
        seventh1 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 7, 2));
        seventh2 = loser;
    } else {
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 3, 1));
        bronze1 = winner;
        fifth1 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 3, 2));
        bronze2 = winner;
        fifth2 = loser;

        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 7, 1));
        seventh1 = loser;
        GET_WINNER_AND_LOSER(get_abs_matchnum_by_pos(systm, 7, 2));
        seventh2 = loser;
    }

    if (table == TABLE_NO_REPECHAGE ||
        table == TABLE_ESP_DOBLE_PERDIDA) {
	bronze1 = fifth1;
	bronze2 = fifth2;
	fifth1 = fifth2 = 0;
        seventh1 = seventh2 = 0;
    }

    if (gold && (j1 = get_data(gold))) {
        write_result_row(f, category, 1, j1->first, j1->last, j1->club);
        avl_set_competitor_position(gold, 1);
        db_set_category_positions(category, gold, 1);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(silver))) {
        write_result_row(f, category, 2, j1->first, j1->last, j1->club);
        avl_set_competitor_position(silver, 2);
        db_set_category_positions(category, silver, 2);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(bronze1))) {
        write_result_row(f, category, 3, j1->first, j1->last, j1->club);
        avl_set_competitor_position(bronze1, 3);
        db_set_category_positions(category, bronze1, 3);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(bronze2))) {
        write_result_row(f, category, 3, j1->first, j1->last, j1->club);
        avl_set_competitor_position(bronze2, 3);
        db_set_category_positions(category, bronze2, 4);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(fourth))) {
        write_result_row(f, category, 4, j1->first, j1->last, j1->club);
        avl_set_competitor_position(fourth, 4);
        db_set_category_positions(category, fourth, 4);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(fifth1))) {
        write_result_row(f, category, 5, j1->first, j1->last, j1->club);
        avl_set_competitor_position(fifth1, 5);
        db_set_category_positions(category, fifth1, 5);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(fifth2))) {
        write_result_row(f, category, 5, j1->first, j1->last, j1->club);
        avl_set_competitor_position(fifth2, 5);
        db_set_category_positions(category, fifth2, 6);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(seventh1))) {
        write_result_row(f, category, 7, j1->first, j1->last, j1->club);
        avl_set_competitor_position(seventh1, 7);
        db_set_category_positions(category, seventh1, 7);
        free_judoka(j1);
    }
    if (gold && (j1 = get_data(seventh2))) {
        write_result_row(f, category, 7, j1->first, j1->last, j1->club);
        avl_set_competitor_position(seventh2, 7);
        db_set_category_positions(category, seventh2, 8);
        free_judoka(j1);
    }
}

static void write_cat_result(FILE *f, gint category)
{
    gint num_judokas;
    struct judoka *ctg = NULL;
    struct compsys sys;

    ctg = get_data(category);

    if (ctg == NULL || ctg->last == NULL || ctg->last[0] == '?' || ctg->last[0] == '_')
        return;

    /* find system */
    sys = db_get_system(category);
    num_judokas = sys.numcomp;

    switch (sys.system) {
    case SYSTEM_POOL:
    case SYSTEM_DPOOL2:
    case SYSTEM_BEST_OF_3:
        pool_results(f, category, ctg, num_judokas, sys.system == SYSTEM_DPOOL2);
        break;

    case SYSTEM_DPOOL:
    case SYSTEM_DPOOL3:
    case SYSTEM_QPOOL:
        dqpool_results(f, category, ctg, num_judokas, sys);
        break;

    case SYSTEM_FRENCH_8:
    case SYSTEM_FRENCH_16:
    case SYSTEM_FRENCH_32:
    case SYSTEM_FRENCH_64:
    case SYSTEM_FRENCH_128:
        french_results(f, category, ctg, num_judokas, sys, 0);
        break;
    }

    free_judoka(ctg);
}

void write_results_csv_file(FILE *f)
{
    GtkTreeIter iter;
    gboolean ok;

    ok = gtk_tree_model_get_iter_first(current_model, &iter);

    while (ok) {
        gint n = gtk_tree_model_iter_n_children(current_model, &iter);
        gint index;

        if (n >= 1 && n <= NUM_COMPETITORS) {
            gtk_tree_model_get(current_model, &iter, COL_INDEX, &index, -1);
            write_cat_result(f, index);
        }

        ok = gtk_tree_model_iter_next(current_model, &iter);
    }
}

void write_results_csv(GtkWidget *w, gpointer data)
{
	FILE *f;
	
	f = open_file();
	if (!f)
		return;
	
	write_header_csv_file(f);
	write_results_csv_file(f);	
		
	fclose(f);

}

