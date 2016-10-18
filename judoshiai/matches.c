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

static void set_comment(GtkWidget *menuitem, gpointer userdata);

struct matchid {
    gint group;
    gint category;
    gint number;
};

GtkWidget *match_view[NUM_TATAMIS];
GtkWidget *next_match[NUM_TATAMIS][2];

struct next_match_info next_matches_info[NUM_TATAMIS][2];

#define COPY_MATCH_INFO(_what, _tatami, _which, _str) g_strlcpy(next_matches_info[_tatami-1][_which]._what, \
                                                                _str, sizeof(next_matches_info[0][0]._what))

#define NULL_MATCH_INFO(_what, _tatami, _which) next_matches_info[_tatami-1][_which]._what[0] = 0

static gchar competitor_positions[10000];

void clear_competitor_positions(void)
{
    gint i;
    for (i = 0; i < 10000; i++)
        competitor_positions[i] &= ~COMP_POS_DRAWN;
}

void set_competitor_position(gint ix, gint status)
{
    if (ix < 0 || ix >= 10000)
        return;

    competitor_positions[ix] = status;
}

void set_competitor_position_drawn(gint ix)
{
    if (ix < 0 || ix >= 10000)
        return;

    competitor_positions[ix] |= COMP_POS_DRAWN;
}

gint get_competitor_position(gint ix)
{
    if (ix < 0 || ix >= 10000)
        return 0;

    return competitor_positions[ix];
}

void write_competitor_positions(void)
{
    gint i;

    if (automatic_web_page_update == FALSE)
        return;

    FILE *f;
    gchar *file = g_build_filename(current_directory, "c-winners.txt", NULL);
    f = fopen(file, "wb");
    g_free(file);
    if (!f)
        return;

    for (i = 0; i < 10000; i++)
        fprintf(f, "%c", competitor_positions[i] + ' ');

    fclose(f);
}

static gint sort_iter_compare_func_1(GtkTreeModel *model,
                                     GtkTreeIter  *a,
                                     GtkTreeIter  *b,
                                     gpointer      userdata)
{
    gint sortcol = GPOINTER_TO_INT(userdata);
    guint data1, data2;

    gtk_tree_model_get(model, a, sortcol, &data1, -1);
    gtk_tree_model_get(model, b, sortcol, &data2, -1);

    if (data1 > data2)
        return 1;

    if (data1 < data2)
        return -1;

    return 0;
}

static void name_cell_data_func (GtkTreeViewColumn *col,
                                 GtkCellRenderer   *renderer,
                                 GtkTreeModel      *model,
                                 GtkTreeIter       *iter,
                                 gpointer           user_data)
{
    gchar  buf[128];
    guint  index;
    gboolean visible;
    gint category;
    struct judoka *j;

    gtk_tree_model_get(model, iter,
                       ptr_to_gint(user_data), &index,
                       COL_MATCH_CAT, &category,
                       COL_MATCH_VIS, &visible,
                       -1);

    if (visible == FALSE) {
        if (ptr_to_gint(user_data) != COL_MATCH_BLUE)
            return;

        gchar *sys;
        struct category_data *data = avl_get_category(category);
        if (!data) {
            g_object_set(renderer, "text", "", NULL);
            return;
        }

        sys = get_system_description(category, data->system.numcomp);
        g_object_set(renderer,
                     "cell-background-set", FALSE,
                     NULL);
        g_snprintf(buf, sizeof(buf), "%s [%d]", sys, data->system.numcomp);
        g_object_set(renderer, "text", buf, NULL);

        return;
    }


    if (index == GHOST) {
        g_snprintf(buf, sizeof(buf), "-");
    } else {
        j = get_data(index);
        if (j && j->index)
            g_snprintf(buf, sizeof(buf), "%s %s", j->first, j->last);
        else
            g_snprintf(buf, sizeof(buf), "?");

        free_judoka(j);

        if (db_has_hansokumake(index))
            g_object_set(renderer, "strikethrough", TRUE, NULL);
        else
            g_object_set(renderer, "strikethrough", FALSE, NULL);
    }
    g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */

    g_object_set(renderer, "text", buf, NULL);
}

static void category_cell_data_func (GtkTreeViewColumn *col,
                                     GtkCellRenderer   *renderer,
                                     GtkTreeModel      *model,
                                     GtkTreeIter       *iter,
                                     gpointer           user_data)
{
    gchar  buf[128];
    guint  index;
    gboolean visible;
    gint category;
    struct judoka *j;

    gtk_tree_model_get(model, iter,
                       ptr_to_gint(user_data), &index,
                       COL_MATCH_CAT, &category,
                       COL_MATCH_VIS, &visible,
                       -1);

    if (visible || category == 0) {
        g_object_set(renderer, "text", "", NULL);
        return;
    }

    j = get_data(index);
    if (j) {
        g_snprintf(buf, sizeof(buf), "%s", j->last);
        g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
        free_judoka(j);
    } else {
        g_snprintf(buf, sizeof(buf), "?");
        g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
    }

    g_object_set(renderer, "text", buf, NULL);
}

static void number_cell_data_func (GtkTreeViewColumn *col,
                                  GtkCellRenderer   *renderer,
                                  GtkTreeModel      *model,
                                  GtkTreeIter       *iter,
                                  gpointer           user_data)
{
    gchar  buf[128];
    gboolean visible;
    guint number, cat;

    gtk_tree_model_get(model, iter,
                       COL_MATCH_CAT,    &cat,
                       COL_MATCH_NUMBER, &number,
                       COL_MATCH_VIS,    &visible,
                       -1);

    if (visible) {
        if (number & MATCH_CATEGORY_SUB_MASK) {
            gchar *wcname = get_weight_class_name_by_index(cat, (number & MATCH_CATEGORY_MASK) - 1);
            if (wcname)
                g_snprintf(buf, sizeof(buf), "%d/%s",
                           number >> MATCH_CATEGORY_SUB_SHIFT,
                           wcname);
            else
                g_snprintf(buf, sizeof(buf), "%d/%d",
                           number >> MATCH_CATEGORY_SUB_SHIFT,
                           number & MATCH_CATEGORY_MASK);
        } else
            g_snprintf(buf, sizeof(buf), "%d", number);

        g_object_set(renderer, "text", buf, NULL);
    } else {
        g_object_set(renderer, "text", "", NULL);
    }

}


static void group_cell_data_func (GtkTreeViewColumn *col,
                                  GtkCellRenderer   *renderer,
                                  GtkTreeModel      *model,
                                  GtkTreeIter       *iter,
                                  gpointer           user_data)
{
    gchar  buf[128];
    gboolean visible;
    gint category;
    gint group;

    gtk_tree_model_get(model, iter,
                       COL_MATCH_GROUP, &group,
                       COL_MATCH_CAT, &category,
                       COL_MATCH_VIS, &visible,
                       -1);

    if (category == 0) {
        g_snprintf(buf, sizeof(buf), "%d", group);
        g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
        g_object_set(renderer, "text", buf, NULL);
    } else {
        g_object_set(renderer, "text", "", NULL);
    }

}

static void score_cell_data_func (GtkTreeViewColumn *col,
                                  GtkCellRenderer   *renderer,
                                  GtkTreeModel      *model,
                                  GtkTreeIter       *iter,
                                  gpointer           user_data)
{
    gchar  buf[128];
    gboolean visible;
    guint score;

    gtk_tree_model_get(model, iter,
                       ptr_to_gint(user_data), &score,
                       COL_MATCH_VIS, &visible,
                       -1);

    if (visible) {
        g_snprintf(buf, sizeof(buf), "%d%d%d/%d%s",
                   (score>>16)&15, (score>>12)&15, (score>>8)&15,
                   score&7, score&8?"H":"");
        g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
        g_object_set(renderer, "text", buf, NULL);
    } else {
        g_object_set(renderer, "text", "", NULL);
    }

}

static void time_cell_data_func (GtkTreeViewColumn *col,
                                 GtkCellRenderer   *renderer,
                                 GtkTreeModel      *model,
                                 GtkTreeIter       *iter,
                                 gpointer           user_data)
{
    gchar  buf[128];
    gboolean visible;
    guint tm;

    gtk_tree_model_get(model, iter,
                       ptr_to_gint(user_data), &tm,
                       COL_MATCH_VIS, &visible,
                       -1);

    if (visible) {
        g_snprintf(buf, sizeof(buf), "%d:%02d",
                   tm / 60, tm % 60);
        g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */
        g_object_set(renderer, "text", buf, NULL);
    } else {
        g_object_set(renderer, "text", "", NULL);
    }
}

static void comment_cell_data_func (GtkTreeViewColumn *col,
                                    GtkCellRenderer   *renderer,
                                    GtkTreeModel      *model,
                                    GtkTreeIter       *iter,
                                    gpointer           user_data)
{
    gboolean visible;
    guint comment;
    gint bpts, wpts, b, w, status, n;
    gchar buf[128];

    gtk_tree_model_get(model, iter,
                       COL_MATCH_VIS, &visible,
                       COL_MATCH_COMMENT, &comment,
                       COL_MATCH_BLUE_POINTS, &bpts,
                       COL_MATCH_WHITE_POINTS, &wpts,
                       COL_MATCH_BLUE, &b,
                       COL_MATCH_WHITE, &w,
                       COL_MATCH_STATUS, &status,
                       -1);

    if (visible) {
        g_object_set(renderer, "foreground-set", FALSE, NULL); /* print this normal */

        if (bpts || wpts || b == GHOST || w == GHOST ||
            db_has_hansokumake(b) || db_has_hansokumake(w))
            g_object_set(renderer,
                         "cell-background", "Green",
                         "cell-background-set", TRUE,
                         NULL);
        else if (status & MATCH_STATUS_TATAMI_MASK)
            g_object_set(renderer,
                         "cell-background", "Yellow",
                         "cell-background-set", TRUE,
                         NULL);
        else
            g_object_set(renderer,
                         "cell-background-set", FALSE,
                         NULL);

        switch (comment) {
        case COMMENT_EMPTY:
            buf[0] = 0; n = 0;
            break;
        case COMMENT_MATCH_1:
            n = sprintf(buf, "%s", _("Next match"));
            break;
        case COMMENT_MATCH_2:
            n = sprintf(buf, "%s", _("Preparing"));
            break;
        case COMMENT_WAIT:
            n = sprintf(buf, "%s", _("Delay the match"));
            break;
        default:
            n = sprintf(buf, "?");
        }

        if (status & MATCH_STATUS_TATAMI_MASK)
            sprintf(buf+n, " (TATAMI %d)", status & MATCH_STATUS_TATAMI_MASK);

        g_object_set(renderer, "text", buf, NULL);
    } else {
        g_object_set(renderer, "text", "", NULL);
    }
}

#define IGNORE_MATCHES(_i) ((ju[c[_i]]->deleted & HANSOKUMAKE) && all[c[_i]] == FALSE)
#define APPROVE_MATCHES(_i) (((ju[c[_i]]->deleted & HANSOKUMAKE) == 0) || all[c[_i]])

#define TIE3(_a,_b,_c) (wins[c[_a]] == wins[c[_b]] && wins[c[_b]] == wins[c[_c]] && \
                        pts[c[_a]] == pts[c[_b]] && pts[c[_b]] == pts[c[_c]] && \
                        mw[c[_a]][c[_b]] == mw[c[_b]][c[_c]] && mw[c[_b]][c[_c]] == mw[c[_c]][c[_a]] && \
                        APPROVE_MATCHES(_a) && APPROVE_MATCHES(_b) && APPROVE_MATCHES(_c))

#define SWAP(_a,_b) do { gint tmp = c[_a]; c[_a] = c[_b]; c[_b] = tmp; } while (0)

#define SET_TIE3(_a) do {ju[c[_a]]->deleted |= POOL_TIE3;            \
        ju[c[_a+1]]->deleted |= POOL_TIE3;                           \
        ju[c[_a+2]]->deleted |= POOL_TIE3;                           \
        tie[c[_a]] = tie[c[_a+1]] = tie[c[_a+2]] = TRUE;             \
        tie[0] = TRUE;                                               \
    } while (0)

#define WEIGHT(_a) (ju[c[_a]]->weight)
#define TIME(_a) (match_time[c[_a]])

#define MATCHED(_a) (m[_a].blue_points || m[_a].white_points)

#define MATCHED_FRENCH_1(_a) (m[_a].blue_points || m[_a].white_points || \
			      m[_a].blue == GHOST || m[_a].white == GHOST || \
			      db_has_hansokumake(m[_a].blue) ||         \
			      db_has_hansokumake(m[_a].white))

#define MATCHED_FRENCH(x) (x < 0 ? MATCHED_FRENCH_1(-x) : MATCHED_FRENCH_1(x))

#define WINNER_1(_a) (COMP_1_PTS_WIN(m[_a]) ? m[_a].blue :             \
		      (COMP_2_PTS_WIN(m[_a]) ? m[_a].white :		\
		       (m[_a].blue == GHOST ? m[_a].white :		\
			(m[_a].white == GHOST ? m[_a].blue :            \
			 (db_has_hansokumake(m[_a].blue) ? m[_a].white : \
			  (db_has_hansokumake(m[_a].white) ? m[_a].blue : NO_COMPETITOR))))))

#define WINNER(x) (x < 0 ? WINNER_1(-x) : WINNER_1(x))

#define LOSER_1(_a) (COMP_1_PTS_WIN(m[_a]) ? m[_a].white :             \
		     (COMP_2_PTS_WIN(m[_a]) ? m[_a].blue :             \
		      (m[_a].blue == GHOST ? m[_a].blue :		\
		       (m[_a].white == GHOST ? m[_a].white :		\
			(db_has_hansokumake(m[_a].blue) ? m[_a].blue :  \
			 (db_has_hansokumake(m[_a].white) ? m[_a].white : NO_COMPETITOR))))))

#define LOSER(x) (x < 0 ? LOSER_1(-x) : LOSER_1(x))

#define WINNER_OR_LOSER(x) (x < 0 ? LOSER_1(-x) : WINNER_1(x))

#define PREV_BLUE(_x) french_matches[table][sys][_x][0]
#define PREV_WHITE(_x) french_matches[table][sys][_x][1]

#define GET_PREV1(_x) ((COMP_1_PTS_WIN(m[_x]) || m[_x].white == GHOST) ? PREV_BLUE(_x) : \
                       ((COMP_2_PTS_WIN(m[_x]) || m[_x].blue == GHOST) ? PREV_WHITE(_x) : \
                        (db_has_hansokumake(m[_x].white) ? PREV_BLUE(_x) : PREV_WHITE(_x))))

#define GET_PREV(_x) (_x < 0 ? (COMP_1_PTS_WIN(m[-_x]) ? PREV_WHITE(-_x) : PREV_BLUE(-_x)) : GET_PREV1(_x))

#define GET_PREV_OTHER(_x) ((COMP_1_PTS_WIN(m[_x]) || m[_x].white == GHOST) ? PREV_WHITE(_x) : \
                            ((COMP_2_PTS_WIN(m[_x]) || m[_x].blue == GHOST) ? PREV_BLUE(_x) : \
                             (db_has_hansokumake(m[_x].white) ? PREV_WHITE(_x) : PREV_BLUE(_x))))

#define GET_PREV_MATCHES(_x, _w, _l)                                    \
    do { gboolean b = FALSE;                                            \
        if (COMP_1_PTS_WIN(m[_x]) || m[_x].white == GHOST) b = TRUE;   \
        else if (COMP_2_PTS_WIN(m[_x]) || m[_x].blue == GHOST) b = FALSE; \
        else if (db_has_hansokumake(m[_x].white)) b = TRUE;             \
        if (b) { _w = french_matches[table][sys][_x][0]; _l = french_matches[table][sys][_x][1];} \
        else { _w = french_matches[table][sys][_x][1]; _l = french_matches[table][sys][_x][0];} \
    } while (0)


#define CAN_WIN_H (yes[c[j]] && (ju[c[j]]->deleted & HANSOKUMAKE) == 0)
#define CAN_WIN (yes[c[j]])

/*
#define COPY_PLAYER(_to, _from)  do {                     \
        _to = _from;                                            \
        if (db_has_hansokumake(_to)) _to = GHOST; } while (0)
*/
#define COPY_PLAYER(_to, _which, _from)  do {				\
	if (!MATCHED(_to))						\
	    m[_to]._which = _from;					\
	else if (m[_to]._which != _from) {				\
            SHOW_MESSAGE("%s #%d (%d-%d) %s!", _("Match"),              \
                         _to, m[_to].blue_points, m[_to].white_points,  \
                         _("canceled"));                                \
	    m[_to]._which = _from;					\
            m[_to].blue_points = m[_to].white_points = 0;               \
        }                                                               \
    } while (0)

#define COPY_AND_SET(_to, _which, _who) do {                    \
        COPY_PLAYER(_to, _which, _who);                  \
        set_match(&m[_to]); db_set_match(&m[_to]); } while (0)

/*
  Swedish rules. Other countries use the same or a subset:
  16.2 Poolsystem Poolsegraren räknas fram enligt följande:
  1. Antal vunna matcher.
  2. Vid lika antal vunna matcher avgör poängen.
  3. Vid lika antal vunna matcher och poäng avgör inbördes möte.
  4. Vid lika antal vinster och inbördes inte går att räkna ut vinner den med
     snabbast vinst. (Räknat på totala summan av alla vinster)
  5. Vid samma tid för vinst avgör invägningsvikten och den med lägst vikt vinner.
  6. Vid lika vikt avgör utslagsmatcher mellan de tävlande.
 */

void get_pool_winner(gint num, gint c[21], gboolean yes[21],
                     gint wins[21], gint pts[21], gint match_time[21],
                     gboolean mw[21][21], struct judoka *ju[21], gboolean all[21], gboolean tie[21])
{
    gint i, j;

    for (i = 0; i <= 20; i++) {
        c[i] = i;
        tie[i] = FALSE;
    }

    // check for not defined competitors
    for (i = 1; i <= num; i++)
        if (ju[i] == NULL)
            return;

    for (i = 1; i < 20; i++) {
        for (j = i+1; j <= 20; j++) {
            if ((yes[c[i]] && yes[c[j]] &&
                 (ju[c[i]]->deleted & HANSOKUMAKE) &&
                 all[c[i]] == FALSE) ||                 /* hansoku-make */

                (yes[c[i]] == FALSE && CAN_WIN) ||      /* competitor not active */

                (wins[c[i]] < wins[c[j]] && CAN_WIN) || /* more wins  */

                (wins[c[i]] == wins[c[j]] &&            /* same wins, more points  */
                 pts[c[i]] < pts[c[j]]  && CAN_WIN) ||

                (wins[c[i]] == wins[c[j]] &&            /* wins same, points same */
                 pts[c[i]] == pts[c[j]] &&
                 mw[c[j]][c[i]] && CAN_WIN)) {
                gint tmp = c[i];
                c[i] = c[j];
                c[j] = tmp;
            }
        }
    }

    for (i = 1; i < num - 1; i++) {
	gboolean t = TRUE;
	if (TIE3(i, i+1, i+2)) {
	    if (prop_get_int_val(PROP_RESOLVE_3_WAY_TIES_BY_TIME)) {
		/* fastest wins  */
		if (TIME(i) > TIME(i+1)) SWAP(i, i+1);
		if (TIME(i) > TIME(i+2)) SWAP(i, i+2);
		if (TIME(i+1) > TIME(i+2)) SWAP(i+1, i+2);
		if (TIME(i) == TIME(i+1) && mw[c[i+1]][c[i]]) SWAP(i,i+1);
		if (TIME(i+1) == TIME(i+2) && mw[c[i+2]][c[i+1]]) SWAP(i+1,i+2);
		if (TIME(i) != TIME(i+1) || TIME(i+1) != TIME(i+2))
		    t = FALSE; /* solved */
	    }
	    if (t && prop_get_int_val(PROP_RESOLVE_3_WAY_TIES_BY_WEIGHTS)) {
		/* weight matters */
		if (WEIGHT(i) > WEIGHT(i+1)) SWAP(i,i+1);
		if (WEIGHT(i) > WEIGHT(i+2)) SWAP(i,i+2);
		if (WEIGHT(i+1) > WEIGHT(i+2)) SWAP(i+1,i+2);
		if (WEIGHT(i) == WEIGHT(i+1) && mw[c[i+1]][c[i]]) SWAP(i,i+1);
		if (WEIGHT(i+1) == WEIGHT(i+2) && mw[c[i+2]][c[i+1]]) SWAP(i+1,i+2);
		if (WEIGHT(i) != WEIGHT(i+1) || WEIGHT(i+1) != WEIGHT(i+2))
		    t = FALSE;
	    }

	    if (t) {
		SET_TIE3(i);
		break;
	    }
	}
    }
}

static gint num_matches_table[] =   {0,0,1,3,6,10,15,21};
static gint num_matches_table_d[] = {0,0,0,0,0,0,6,9,12,16,20,25,30};
static gint num_matches_table_q[] = {0,0,0,0,0,0,0,0,4,6,8, // 0 - 10
                                     10,12,15,18,21,24,28,32,36,40}; // 11 - 20

gint num_matches(gint sys, gint num_judokas)
{
    if (num_judokas == 2 && prop_get_int_val(PROP_THREE_MATCHES_FOR_TWO))
        return 3;
    else if (sys == SYSTEM_BEST_OF_3)
        return 3;
    else if (sys == SYSTEM_DPOOL)
        return num_matches_table_d[num_judokas];
    else if (sys == SYSTEM_DPOOL2)
        return num_matches_table_d[num_judokas];
    else if (sys == SYSTEM_DPOOL3)
        return num_matches_table_d[num_judokas];
    else if (sys == SYSTEM_QPOOL)
        return num_matches_table_q[num_judokas];
    else
        return num_matches_table[num_judokas];
}

void fill_pool_struct(gint category, gint num, struct pool_matches *pm, gboolean final_pool)
{
    gint i, k;
    gboolean all_done;
    struct compsys sys;
    gboolean sysq, sysd;

    sys = get_cat_system(category);
    sysq = sys.system == SYSTEM_QPOOL;
    sysd = sys.system == SYSTEM_DPOOL || sys.system == SYSTEM_DPOOL2 || sys.system == SYSTEM_DPOOL3;

    memset(pm, 0, sizeof(*pm));
    pm->finished = TRUE;
    pm->num_matches = num_matches(sys.system, num);

    db_read_category_matches(category, pm->m);

    // two best of two pools fight in the final pool
    if (final_pool) {
        for (i = 1; i <= 6; i++)
            pm->m[i] = pm->m[pm->num_matches+i];
        pm->num_matches = 6;
        num = 4;
        sys.system = SYSTEM_POOL;
        sysq = sysd = FALSE;
    }

    if (num == 1) {
        GtkTreeIter iter, iter2;

        if (find_iter(&iter, category)) {
            if (gtk_tree_model_iter_children(current_model, &iter2, &iter)) {
                pm->j[1] = get_data_by_iter(&iter2);
            }
        }
        return;
    }

    /* initial information */
    for (i = 1; i <= num_matches(sys.system, num); i++) {
        gint blue, white;
        if (sysq) {
            blue = poolsq[num][i-1][0];
            white = poolsq[num][i-1][1];
        } else if (sysd) {
            blue = poolsd[num][i-1][0];
            white = poolsd[num][i-1][1];
        } else {
            blue = pools[num][i-1][0];
            white = pools[num][i-1][1];
        }
        pm->yes[blue] = pm->yes[white] = TRUE;
        if (pm->j[blue] == NULL)
            pm->j[blue] = get_data(pm->m[i].blue);
        if (pm->j[white] == NULL)
            pm->j[white] = get_data(pm->m[i].white);

        if (pm->j[blue] == NULL)
            pm->yes[blue] = FALSE;
        if (pm->j[white] == NULL)
            pm->yes[white] = FALSE;
    }

    /* check for hansoku-makes */
    for (k = 1; k <= num; k++) {
        if (pm->j[k] == NULL || (pm->j[k]->deleted & HANSOKUMAKE) == 0)
            continue; /* competitor ok */
        //pm->yes[k] = FALSE;
        all_done = TRUE;
        for (i = 1; i <= num_matches(sys.system, num); i++) {
            gint blue, white;
            if (sysq) {
                blue = poolsq[num][i-1][0];
                white = poolsq[num][i-1][1];
            } else if (sysd) {
                blue = poolsd[num][i-1][0];
                white = poolsd[num][i-1][1];
            } else {
                blue = pools[num][i-1][0];
                white = pools[num][i-1][1];
            }
            if ((blue == k || white == k) &&
                pm->m[i].blue_points == 0 && pm->m[i].white_points == 0) {
                all_done = FALSE; /* undone match */
                break;
            }
        }
        pm->all_matched[k] = all_done;
        if (all_done)
            continue; /* all matched, points are valid */

        for (i = 1; i <= num_matches(sys.system, num); i++) {
            gint blue, white;
            if (sysq) {
                blue = poolsq[num][i-1][0];
                white = poolsq[num][i-1][1];
            } else if (sysd) {
                blue = poolsd[num][i-1][0];
                white = poolsd[num][i-1][1];
            } else {
                blue = pools[num][i-1][0];
                white = pools[num][i-1][1];
            }
            if (blue == k || white == k)
                pm->m[i].ignore = TRUE; /* hansoku-make matches are void */
        }
    }

    for (i = 1; i <= num_matches(sys.system, num); i++) {
        gint blue, white;
        if (sysq) {
            blue = poolsq[num][i-1][0];
            white = poolsq[num][i-1][1];
        } else if (sysd) {
            blue = poolsd[num][i-1][0];
            white = poolsd[num][i-1][1];
        } else {
            blue = pools[num][i-1][0];
            white = pools[num][i-1][1];
        }

        if (pm->m[i].ignore)
            continue;

        if (COMP_1_PTS_WIN(pm->m[i]) && pm->yes[blue] == TRUE && pm->yes[white] == TRUE) {
            pm->wins[blue]++;
	    pm->tim[blue] += pm->m[i].match_time;
            pm->pts[blue] += pm->m[i].blue_points;
            pm->pts[white] += pm->m[i].white_points; // team event points
            pm->mw[blue][white] = TRUE;
            pm->mw[white][blue] = FALSE;
        } else if (COMP_2_PTS_WIN(pm->m[i]) && pm->yes[blue] == TRUE && pm->yes[white] == TRUE) {
            pm->wins[white]++;
	    pm->tim[white] += pm->m[i].match_time;
            pm->pts[white] += pm->m[i].white_points;
            pm->pts[blue] += pm->m[i].blue_points; // team event points
            pm->mw[blue][white] = FALSE;
            pm->mw[white][blue] = TRUE;
        } else if (pm->yes[blue] == TRUE && pm->yes[white] == TRUE)
            pm->finished = FALSE;
    }

    /* special case: two competitors and three matches */
    if (num == 2 && num_matches(sys.system, num) == 3 &&
        ((COMP_1_PTS_WIN(pm->m[1]) && COMP_1_PTS_WIN(pm->m[2])) ||
         (COMP_2_PTS_WIN(pm->m[1]) && COMP_2_PTS_WIN(pm->m[2])))) {
        pm->finished = TRUE;

        db_set_comment(category, 3, COMMENT_WAIT);
    }
}

void empty_pool_struct(struct pool_matches *pm)
{
    gint i;

    for (i = 0; i < 21; i++)
        free_judoka(pm->j[i]);
}

static gint find_comp_num(struct custom_matches *cm, gint index)
{
    gint i;
    for (i = 1; i <= cm->num_competitors; i++) {
        if (cm->j[i] && index == cm->j[i]->index)
            return i;
    }
    return 0;
}

static gint find_comp_num_in_pool(struct pool *p, gint index)
{
    gint i;
    for (i = 0; i < p->num_competitors; i++) {
        if (p->competitors[i].index == index) return i;
    }
    return -1;
}

void fill_custom_struct(gint category, struct custom_matches *cm)
{
    gint i, j, k, poolnum, mnum, b3num;
    gboolean all_done;
    struct compsys sys;
    struct custom_data *cd;

    sys = get_cat_system(category);
    cd = get_custom_table(sys.table);
    if (!cd)
        return;

    memset(cm, 0, sizeof(*cm));
    cm->cd = cd;

    // read matches
    db_read_category_matches(category, cm->m);

    cm->num_matches = cd->num_matches;
    cm->num_round_robin_pools = cd->num_round_robin_pools;
    cm->num_best_of_three = cd->num_best_of_three_pairs;

    // find competitors
    cm->num_competitors = 0;
    for (mnum = 0; mnum < cd->num_matches; mnum++) {
        if (cd->matches[mnum].c1.type == COMP_TYPE_COMPETITOR) {
            if (cm->j[cd->matches[mnum].c1.num] == NULL) {
                cm->j[cd->matches[mnum].c1.num] = get_data(cm->m[mnum+1].blue);
                if (cd->matches[mnum].c1.num > cm->num_competitors)
                    cm->num_competitors = cd->matches[mnum].c1.num;
            }
        }
        if (cd->matches[mnum].c2.type == COMP_TYPE_COMPETITOR) {
            if (cm->j[cd->matches[mnum].c2.num] == NULL) {
                cm->j[cd->matches[mnum].c2.num] = get_data(cm->m[mnum+1].white);
                if (cd->matches[mnum].c2.num > cm->num_competitors)
                    cm->num_competitors = cd->matches[mnum].c2.num;
            }
        }
    }
    if (sys.numcomp != cm->num_competitors)
        g_print("ERROR: Conflicting comp num sys=%d counted=%d\n", sys.numcomp, cm->num_competitors);

    // set best of 3 pairs
    for (b3num = 0; b3num < cm->num_best_of_three; b3num++) {
        gint c1 = 0, c2 = 0, mn, no_match = 2;

        mn = cd->best_of_three_pairs[b3num].matches[0];
        if (cm->m[mn].blue >= 10)
            cm->best_of_three[b3num].competitors[0].index = cm->m[mn].blue;
        if (cm->m[mn].white >= 10)
            cm->best_of_three[b3num].competitors[1].index = cm->m[mn].white;

        for (j = 0; j < 3; j++) {
            mn = cd->best_of_three_pairs[b3num].matches[j];
            if (COMP_1_PTS_WIN(cm->m[mn])) c1++;
            else if (COMP_2_PTS_WIN(cm->m[mn])) c2++;
            else no_match = j;
        }

        if (c1 >= 2 || c2 >= 2) {
            mn = cd->best_of_three_pairs[b3num].matches[0];
            cm->best_of_three[b3num].finished = TRUE;
            cm->best_of_three[b3num].winner = c1 >= 2 ? cm->m[mn].blue : cm->m[mn].white;
            cm->best_of_three[b3num].loser = c1 >= 2 ? cm->m[mn].white : cm->m[mn].blue;
            db_set_comment(category, cd->best_of_three_pairs[b3num].matches[no_match], COMMENT_WAIT);
        }
    }

    // set pools
    for (poolnum = 0; poolnum < cm->num_round_robin_pools; poolnum++) {
        struct pool *p = &(cm->pm[poolnum]);
        round_robin_bare_t *cdpool = &(cd->round_robin_pools[poolnum]);

        p->finished = TRUE;
        p->num_competitors = cdpool->num_competitors;

        // find competitors' index
        for (k = 0; k < cdpool->num_competitors; k++) {
            if (cdpool->competitors[k].type == COMP_TYPE_COMPETITOR) {
                gint compnum = cdpool->competitors[k].num;
                struct judoka *ju = cm->j[compnum];
                if (ju)
                    p->competitors[k].index = ju->index;
            } else {
                gint n;
                for (n = 0; n < cdpool->num_rr_matches; n++) {
                    match_bare_t *cdm = &cd->matches[cdpool->rr_matches[n]-1];
                    if (memcmp(&cdm->c1, &cdpool->competitors[k], sizeof(competitor_bare_t)) == 0) {
                        struct match *m = &cm->m[cdpool->rr_matches[n]];
                        if (m->blue >= 10)
                            p->competitors[k].index = m->blue;
                        break;
                    } else if (memcmp(&cdm->c2, &cdpool->competitors[k], sizeof(competitor_bare_t)) == 0) {
                        struct match *m = &cm->m[cdpool->rr_matches[n]];
                        if (m->white >= 10)
                            p->competitors[k].index = m->white;
                        break;
                    }
                }
            }
        }

        /* check for hansoku-makes */
        for (k = 0; k < p->num_competitors; k++) {
            gint comp = p->competitors[k].index;
            gint compnum = find_comp_num(cm, comp);
            if (cm->j[compnum] == NULL || (cm->j[compnum]->deleted & HANSOKUMAKE) == 0)
                continue; /* competitor ok */

            // was penalty during the last fight?
            all_done = TRUE;
            for (j = 0; j < cdpool->num_rr_matches; j++) {
                gint comp1 = cm->m[cdpool->rr_matches[j]].blue;
                gint comp2 = cm->m[cdpool->rr_matches[j]].white;

                if ((comp1 == comp || comp2 == comp) &&
                    cm->m[cdpool->rr_matches[j]].blue_points == 0 &&
                    cm->m[cdpool->rr_matches[j]].white_points == 0) {
                    all_done = FALSE; /* undone match */
                    break;
                }
            }

            p->competitors[k].all_matched = all_done;

            if (all_done)
                continue; /* all matched, points are valid */

            for (i = 0; i < cdpool->num_rr_matches; i++) {
                gint comp1 = cm->m[cdpool->rr_matches[i]].blue;
                gint comp2 = cm->m[cdpool->rr_matches[i]].white;

                if (comp1 == comp || comp2 == comp)
                    cm->m[cdpool->rr_matches[i]].ignore = TRUE; /* hansoku-make matches are void */
            }
        } // for k

        // calculate points
        for (mnum = 0; mnum < cdpool->num_rr_matches; mnum++) {
            gint n = cdpool->rr_matches[mnum];
            gint comp1 = cm->m[n].blue;
            gint comp2 = cm->m[n].white;
            gint c1 = find_comp_num_in_pool(p, comp1);
            gint c2 = find_comp_num_in_pool(p, comp2);

            if (cm->m[n].ignore)
                continue;

            if (c1 >= 0 && c2 >= 0) {
                if (COMP_1_PTS_WIN(cm->m[n])) {
                    p->competitors[c1].wins++;
                    p->competitors[c1].pts += cm->m[n].blue_points;
                    p->competitors[c2].pts += cm->m[n].white_points; // team event points
                    p->competitors[c1].mw[c2] = TRUE;
                    p->competitors[c2].mw[c1] = FALSE;
                } else if (COMP_2_PTS_WIN(cm->m[n])) {
                    p->competitors[c2].wins++;
                    p->competitors[c2].pts += cm->m[n].white_points;
                    p->competitors[c1].pts += cm->m[n].blue_points; // team event points
                    p->competitors[c2].mw[c1] = FALSE;
                    p->competitors[c1].mw[c2] = TRUE;
                } else
                    p->finished = FALSE;
            }
        }

        // calculate positions
        if (!p->finished)
            return;

        for (i = 0; i < 21; i++) {
            p->competitors[i].position = i;
            p->competitors[i].tie = FALSE;
        }

#if 0
        // check for not defined competitors
        for (i = 0; i < p->num_competitors; i++)
            if (p->competitors[i].index == 0)
                return;
#endif
        for (i = 0; i < p->num_competitors; i++) {
            for (j = 0; j < p->num_competitors - 1; j++) {
                gint pos1 = p->competitors[j].position;
                gint pos2 = p->competitors[j+1].position;
                struct judoka *ju = cm->j[find_comp_num(cm, p->competitors[pos1].index)];
                if ((ju && (ju->deleted & HANSOKUMAKE) &&
                     p->competitors[pos1].all_matched == FALSE) ||                 /* hansoku-make */

                    p->competitors[pos1].index == 0 ||

                    (p->competitors[pos1].wins < p->competitors[pos2].wins) || /* more wins  */

                    (p->competitors[pos1].wins == p->competitors[pos2].wins &&            /* same wins, more points  */
                     p->competitors[pos1].pts < p->competitors[pos2].pts) ||

                    (p->competitors[pos1].wins == p->competitors[pos2].wins &&            /* wins same, points same */
                     p->competitors[pos1].pts  == p->competitors[pos2].pts  &&
                     p->competitors[pos2].mw[pos1])) {
                    p->competitors[j].position = pos2;
                    p->competitors[j+1].position = pos1;
                }
            } // for j
        } // for i
    } // for pools
}

void empty_custom_struct(struct custom_matches *cm)
{
    gint i;

    for (i = 1; i <= cm->num_competitors; i++)
        free_judoka(cm->j[i]);
}

gboolean pool_finished(gint numcomp, gint nummatches, gint sys, gboolean yes[], struct pool_matches *pm)
{
    gint i;

    for (i = 1; i <= nummatches; i++) {
        gint blue, white;
        if (sys == SYSTEM_QPOOL) {
            blue = poolsq[numcomp][i-1][0];
            white = poolsq[numcomp][i-1][1];
        } else if (sys == SYSTEM_DPOOL || sys == SYSTEM_DPOOL2 || sys == SYSTEM_DPOOL3) {
            blue = poolsd[numcomp][i-1][0];
            white = poolsd[numcomp][i-1][1];
        } else {
            blue = pools[numcomp][i-1][0];
            white = pools[numcomp][i-1][1];
        }
        if (yes[blue] == FALSE || yes[white] == FALSE)
            continue;
        if ((pm->j[blue]->deleted & HANSOKUMAKE) || (pm->j[white]->deleted & HANSOKUMAKE))
            continue;
        if (pm->m[i].blue_points == 0 && pm->m[i].white_points == 0)
            return FALSE;
    }

    return TRUE;
}

static void update_pool_matches(gint category, gint num)
{
    struct pool_matches pm;
    gint i, j;
    struct compsys sys = get_cat_system(category);
    gint num_pools = 1, num_knockouts = 0;
    struct category_data *catdata = avl_get_category(category);

    if (catdata)
        catdata->tie = FALSE;

    /* Number of pools and matches after the pools are finished.  */
    switch (sys.system) {
    case SYSTEM_BEST_OF_3:
    case SYSTEM_POOL: num_pools = 1; num_knockouts = 0; break;
    case SYSTEM_DPOOL: num_pools = 2; num_knockouts = 3; break;
    case SYSTEM_QPOOL: num_pools = 4; num_knockouts = 7; break;
    case SYSTEM_DPOOL2: num_pools = 2; num_knockouts = 0; break;
    case SYSTEM_DPOOL3: num_pools = 2; num_knockouts = 2; break;
    }

    if (automatic_sheet_update || automatic_web_page_update)
        current_category = category;
    //g_print("current_category = %d\n", category);

    /* Read matches in. */
    fill_pool_struct(category, num, &pm, FALSE);

    if (pm.j[1] == NULL) // category not drawn
        goto out;

    if (num_pools == 1) {
        if (pm.finished) {
            get_pool_winner(num, pm.c, pm.yes, pm.wins, pm.pts, pm.tim, pm.mw, pm.j, pm.all_matched, pm.tie);
            if (catdata)
                catdata->tie = pm.tie[0];
        }
        goto out;
    }

    gboolean yes[4][21];
    gboolean pool_done[4];
    gint c[4][21];
    gint last_match = num_matches(sys.system, num);

    gint pool_start[5];
    gint pool_size[4];
    gint size = num/num_pools;
    gint rem = num - size*num_pools;

    for (i = 0; i < num_pools; i++) {
        pool_size[i] = size;
    }

    for (i = 0; i < rem; i++)
        pool_size[i]++;

    gint start = 1;
    for (i = 0; i < num_pools; i++) {
        pool_start[i] = start;
        start += pool_size[i];
    }
    pool_start[i] = start;

    memset(yes, 0, sizeof(yes));
    memset(c, 0, sizeof(c));

    for (i = 1; i <= num; i++) {
        for (j = 0; j < num_pools; j++) {
            if (i >= pool_start[j] && i < pool_start[j+1]) {
                yes[j][i] = TRUE;
                break;
            }
        }
    }

    for (i = 0; i < num_pools; i++) {
        get_pool_winner(pool_size[i], c[i], yes[i], pm.wins, pm.pts, pm.tim, pm.mw, pm.j, pm.all_matched, pm.tie);
        pool_done[i] = pool_finished(num, last_match, sys.system, yes[i], &pm);
        if (catdata && pool_done[i])
            catdata->tie = catdata->tie || pm.tie[0];
    }

    if (sys.system == SYSTEM_DPOOL2) {
        // check if there are matches done in the final pool
        for (i = 1; i <= 6; i++)
            if (pm.m[last_match+i].blue_points || pm.m[last_match+i].white_points)
                goto out;

        // make the final matches
        struct judoka *winners[5];
        winners[1] = pm.j[c[0][2]];
        winners[2] = pm.j[c[0][1]];
        winners[3] = pm.j[c[1][1]];
        winners[4] = pm.j[c[1][2]];

        for (i = 0; i < 6; i++) {
            struct match ma;

            memset(&ma, 0, sizeof(ma));
            ma.category = category;
            ma.number = last_match+i+1;
            if (pool_done[0] == FALSE || pool_done[1] == FALSE ||
                (catdata && catdata->tie
		 /*&& prop_get_int_val(PROP_RESOLVE_3_WAY_TIES_BY_WEIGHTS) == FALSE*/)) {
                ma.blue = 0;
                ma.white = 0;
            } else {
                ma.blue = winners[pools[4][i][0]]->index;
                ma.white = winners[pools[4][i][1]]->index;

                if (prop_get_int_val(PROP_DPOOL2_WITH_CARRIED_FORWARD_POINTS)) { // preserve old results
                    gint k;
                    for (k = 1; k <= last_match; k++) {
                        if ((pm.m[k].blue == ma.blue && pm.m[k].white == ma.white) ||
                            (pm.m[k].blue == ma.white && pm.m[k].white == ma.blue)) {
                            if (pm.m[k].blue == ma.blue) {
                                ma.blue_score = pm.m[k].blue_score;
                                ma.blue_points = pm.m[k].blue_points;
                                ma.white_score = pm.m[k].white_score;
                                ma.white_points = pm.m[k].white_points;
                            } else {
                                ma.blue_score = pm.m[k].white_score;
                                ma.blue_points = pm.m[k].white_points;
                                ma.white_score = pm.m[k].blue_score;
                                ma.white_points = pm.m[k].blue_points;
                            }
                            ma.match_time = pm.m[k].match_time;
                        }
                    }
                }
            }
            set_match(&ma);
            db_set_match(&ma);
        }

        goto out;
    }

    if (sys.system == SYSTEM_DPOOL3) {
        if (MATCHED_POOL_P(last_match+1) == FALSE) {
            struct match ma;

            memset(&ma, 0, sizeof(ma));
            ma.category = category;
            ma.number = last_match+1;

            if (pool_done[0])
                ma.blue = (pm.j[c[0][2]]->deleted & HANSOKUMAKE) ? GHOST : pm.j[c[0][2]]->index;

            if (pool_done[1])
                ma.white = (pm.j[c[1][2]]->deleted & HANSOKUMAKE) ? GHOST : pm.j[c[1][2]]->index;

            set_match(&ma);
            db_set_match(&ma);
        }

        if (MATCHED_POOL_P(last_match+2) == FALSE) {
            struct match ma;

            memset(&ma, 0, sizeof(ma));
            ma.category = category;
            ma.number = last_match+2;

            if (pool_done[0])
                ma.blue = (pm.j[c[0][1]]->deleted & HANSOKUMAKE) ? GHOST : pm.j[c[0][1]]->index;

            if (pool_done[1])
                ma.white = (pm.j[c[1][1]]->deleted & HANSOKUMAKE) ? GHOST : pm.j[c[1][1]]->index;

            set_match(&ma);
            db_set_match(&ma);
        }

        goto out;
    }

    for (i = 0; i < num_pools; i++) {
        if (MATCHED_POOL_P(last_match+i+1) == FALSE) {
            struct match ma;

            memset(&ma, 0, sizeof(ma));
            ma.category = category;
            ma.number = last_match+i+1;

            if (catdata == NULL || catdata->tie == FALSE
		/* || prop_get_int_val(PROP_RESOLVE_3_WAY_TIES_BY_WEIGHTS)*/) {
                if (pool_done[i])
                    ma.blue = (pm.j[c[i][1]]->deleted & HANSOKUMAKE) ? GHOST : pm.j[c[i][1]]->index;

                if (pool_done[num_pools-i-1])
                    ma.white = (pm.j[c[num_pools-i-1][2]]->deleted & HANSOKUMAKE) ?
                        GHOST : pm.j[c[num_pools-i-1][2]]->index;
            }

            set_match(&ma);
            db_set_match(&ma);
        }
    }

    if (num_pools == 4) {
        for (i = last_match + 5; i <= last_match + 6; i++) {
            if (MATCHED_POOL_P(i))
                continue;

            gint m1 = (i == last_match + 5) ? last_match + 1 : last_match + 3;
            gint m2 = m1 + 1;
            struct match ma;

            memset(&ma, 0, sizeof(ma));
            ma.category = category;
            ma.number = i;

            if (MATCHED_POOL(m1)) {
                if (COMP_1_PTS_WIN(pm.m[m1]) || pm.m[m1].white == GHOST)
                    ma.blue = pm.m[m1].blue;
                else
                    ma.blue = pm.m[m1].white;
            }

            if (MATCHED_POOL(m2)) {
                if (COMP_1_PTS_WIN(pm.m[m2]) || pm.m[m2].white == GHOST)
                    ma.white = pm.m[m2].blue;
                else
                    ma.white = pm.m[m2].white;
            }

            set_match(&ma);
            db_set_match(&ma);
        }
    }

    if (!MATCHED_POOL_P(last_match+num_knockouts)) {
        struct match ma;

        memset(&ma, 0, sizeof(ma));
        ma.category = category;
        ma.number = last_match+num_knockouts;

        if (MATCHED_POOL(last_match+num_knockouts-2)) {
            if (COMP_1_PTS_WIN(pm.m[last_match+num_knockouts-2]) ||
                pm.m[last_match+num_knockouts-2].white == GHOST)
                ma.blue = pm.m[last_match+num_knockouts-2].blue;
            else
                ma.blue = pm.m[last_match+num_knockouts-2].white;
        }

        if (MATCHED_POOL(last_match+num_knockouts-1)) {
            if (COMP_1_PTS_WIN(pm.m[last_match+num_knockouts-1]) ||
                pm.m[last_match+num_knockouts-1].white == GHOST)
                ma.white = pm.m[last_match+num_knockouts-1].blue;
            else
                ma.white = pm.m[last_match+num_knockouts-1].white;
        }

        set_match(&ma);
        db_set_match(&ma);
    }

 out:
    empty_pool_struct(&pm);
}

static void set_repechage_16(struct match m[], gint table, gint i)
{
    gint p;
    gint sys = FRENCH_16;

    if (table == TABLE_SWE_ENKELT_AATERKVAL) {
	if (!MATCHED_FRENCH(13+i))
	    return;

	p = GET_PREV(13+i);
	COPY_PLAYER((15+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((15+i), blue, LOSER(p));
	set_match(&m[15+i]); db_set_match(&m[15+i]);
    } else if (table == TABLE_ESP_REPESCA_SIMPLE) {
	if (!MATCHED_FRENCH(13+i))
	    return;

	p = GET_PREV(13+i);
	COPY_PLAYER((15+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((15+i), blue, LOSER(p));
	set_match(&m[15+i]); db_set_match(&m[15+i]);
    } else if (table == TABLE_DEN_DOUBLE_ELIMINATION) {
        const gint reps[] = {17, 18, 15, 16};
        gint pw, pl;
	if (!MATCHED_FRENCH(9+i))
	    return;

        GET_PREV_MATCHES(9+i, pw, pl);

	COPY_PLAYER((15+i), blue, LOSER(pw));
	set_match(&m[15+i]); db_set_match(&m[15+i]);

        COPY_PLAYER((reps[i]), white, LOSER(pl));
	set_match(&m[reps[i]]); db_set_match(&m[reps[i]]);
    } else {
	if (!MATCHED_FRENCH(9+i))
	    return;

	COPY_PLAYER((13+i), white, LOSER(9+i));
	p = GET_PREV(9+i);
	COPY_PLAYER((13+i), blue, LOSER(p));
	set_match(&m[13+i]); db_set_match(&m[13+i]);
    }
}

static void set_repechage_32(struct match m[], gint table, gint i)
{
    gint p;
    gint sys = FRENCH_32;

    if (table == TABLE_SWE_ENKELT_AATERKVAL) {
	if (!MATCHED_FRENCH(29+i))
	    return;

	p = GET_PREV(29+i);
	COPY_PLAYER((33+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((31+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((31+i), blue, LOSER(p));
	set_match(&m[33+i]); db_set_match(&m[33+i]);
	set_match(&m[31+i]); db_set_match(&m[29+i]);
    } /* else if (table == TABLE_ESP_REPESCA_DOBLE_INICIO) {
	if (!MATCHED_FRENCH(25+i))
	    return;
	COPY_PLAYER((29+i), white, LOSER(25+i));
	p = GET_PREV(25+i);
	COPY_PLAYER((29+i), blue, LOSER(p));
	set_match(&m[29+i]); db_set_match(&m[29+i]);
        } */
    else if (table == TABLE_ESP_REPESCA_SIMPLE) {
	if (!MATCHED_FRENCH(29+i))
	    return;

	p = GET_PREV(29+i);
	COPY_PLAYER((31+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((31+i), blue, LOSER(p));
	set_match(&m[31+i]); db_set_match(&m[31+i]);
    } else if (table == TABLE_DEN_DOUBLE_ELIMINATION) {
	if (!MATCHED_FRENCH(25+i))
	    return;
        gint pw, pl, pww, pwl, plw, pll;
        gint o1 = 31+i*2;
        gint o2 = (i <= 1) ? (35+i*2) : (31+(i-2)*2);

        GET_PREV_MATCHES(25+i, pw, pl); // matches 17, 18
        GET_PREV_MATCHES(pw, pww, pwl); // matches 17, 18
        GET_PREV_MATCHES(pl, plw, pll); // matches 17, 18

	COPY_PLAYER(o1+8, white, LOSER(pw));
	set_match(&m[o1+8]); db_set_match(&m[o1+8]);
	COPY_PLAYER(o2+8+1, white, LOSER(pl));
	set_match(&m[o2+8+1]); db_set_match(&m[o2+8+1]);

	COPY_PLAYER(o1, blue, LOSER(pww));
	set_match(&m[o1]); db_set_match(&m[o1]);
	COPY_PLAYER((o2), white, LOSER(pwl));
	set_match(&m[o2]); db_set_match(&m[o2]);

	COPY_PLAYER((o2+1), white, LOSER(plw));
	set_match(&m[o2+1]); db_set_match(&m[o2+1]);
	COPY_PLAYER((o1+1), blue, LOSER(pll));
	set_match(&m[o1+1]); db_set_match(&m[o1+1]);
    } else {
	if (!MATCHED_FRENCH(25+i))
	    return;

	COPY_PLAYER((33+i), white, LOSER(25+i));
	p = GET_PREV(25+i);
	COPY_PLAYER((29+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((29+i), blue, LOSER(p));
	set_match(&m[33+i]); db_set_match(&m[33+i]);
	set_match(&m[29+i]); db_set_match(&m[29+i]);
    }
}

static void set_repechage_64(struct match m[], gint table, gint i)
{
    gint p;
    gint sys = FRENCH_64;

    if (table == TABLE_SWE_ENKELT_AATERKVAL) {
	if (!MATCHED_FRENCH(61+i))
	    return;

	p = GET_PREV(61+i);
	COPY_PLAYER((67+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((65+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((63+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((63+i), blue, LOSER(p));
	set_match(&m[67+i]); db_set_match(&m[69+i]);
	set_match(&m[65+i]); db_set_match(&m[65+i]);
	set_match(&m[63+i]); db_set_match(&m[61+i]);
    } /* else if (table == TABLE_ESP_REPESCA_DOBLE_INICIO) {
	if (!MATCHED_FRENCH(57+i))
	    return;
	COPY_PLAYER((61+i), white, LOSER(57+i));
	p = GET_PREV(57+i);
	COPY_PLAYER((61+i), blue, LOSER(p));
	set_match(&m[61+i]); db_set_match(&m[61+i]);
        } */
    else if (table == TABLE_ESP_REPESCA_SIMPLE) {
	if (!MATCHED_FRENCH(61+i))
	    return;

	p = GET_PREV(61+i);
	COPY_PLAYER((63+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((63+i), blue, LOSER(p));
	set_match(&m[63+i]); db_set_match(&m[63+i]);
    } else if (table == TABLE_DEN_DOUBLE_ELIMINATION) {
	if (!MATCHED_FRENCH(57+i))
	    return;
        gint pw, pl, pww, pwl, plw, pll, pwww, pwwl, pwlw, pwll, plww, plwl, pllw, plll;
        gint o1 = 63+i*4;
        gint o2 = (i <= 1) ? (71+i*4) : (63+(i-2)*4);
        gint o3 = 103+i*2;
        gint o4 = (i <= 1) ? (107+i*2) : (103+(i-2)*2);

        GET_PREV_MATCHES(57+i, pw, pl); // matches 49, 50
        GET_PREV_MATCHES(pw, pww, pwl); // matches 33, 34
        GET_PREV_MATCHES(pl, plw, pll); // matches 35, 36
        GET_PREV_MATCHES(pww, pwww, pwwl);
        GET_PREV_MATCHES(pwl, pwlw, pwll);
        GET_PREV_MATCHES(plw, plww, plwl);
        GET_PREV_MATCHES(pll, pllw, plll);

        COPY_AND_SET(o3, white, LOSER(pw));
        COPY_AND_SET(o4+1, white, LOSER(pl));

        COPY_AND_SET(o1+16, white, LOSER(pww));
        COPY_AND_SET(o2+16+1, white, LOSER(pwl));
        COPY_AND_SET(o2+16+3, white, LOSER(plw));
        COPY_AND_SET(o1+16+2, white, LOSER(pll));

        COPY_AND_SET(o1, blue, LOSER(pwww));
        COPY_AND_SET(o2, white, LOSER(pwwl));
        COPY_AND_SET(o2+1, white, LOSER(pwlw));
        COPY_AND_SET(o1+1, blue, LOSER(pwll));
        COPY_AND_SET(o2+2, white, LOSER(plww));
        COPY_AND_SET(o1+2, blue, LOSER(plwl));
        COPY_AND_SET(o1+3, blue, LOSER(pllw));
        COPY_AND_SET(o2+3, white, LOSER(plll));
    } else {
	if (!MATCHED_FRENCH(57+i))
	    return;

	COPY_PLAYER((69+i), white, LOSER(57+i));
	p = GET_PREV(57+i);
	COPY_PLAYER((65+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((61+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((61+i), blue, LOSER(p));
	set_match(&m[69+i]); db_set_match(&m[69+i]);
	set_match(&m[65+i]); db_set_match(&m[65+i]);
	set_match(&m[61+i]); db_set_match(&m[61+i]);
    }
}

static void set_repechage_128(struct match m[], gint table, gint i)
{
    gint p;
    gint sys = FRENCH_128;

    if (table == TABLE_DOUBLE_REPECHAGE) {
	if (!MATCHED_FRENCH(121+i))
	    return;

	COPY_PLAYER((137+i), white, LOSER(121+i));
	p = GET_PREV(121+i);
	COPY_PLAYER((133+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((129+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((125+i), white, LOSER(p));
	p = GET_PREV(p);
	COPY_PLAYER((125+i), blue, LOSER(p));
	set_match(&m[137+i]); db_set_match(&m[137+i]);
	set_match(&m[133+i]); db_set_match(&m[133+i]);
	set_match(&m[129+i]); db_set_match(&m[129+i]);
	set_match(&m[125+i]); db_set_match(&m[125+i]);
    }
}

static void update_french_matches(gint category, struct compsys systm)
{
    gint i;
    struct match m[NUM_MATCHES];
    gint sys = systm.system - SYSTEM_FRENCH_8;
    gint table = systm.table;

    if (automatic_sheet_update || automatic_web_page_update)
        current_category = category;
    //g_print("current_category = %d\n", category);

    memset(m, 0, sizeof(m));
    db_read_category_matches(category, m);

    if (table == TABLE_DOUBLE_REPECHAGE ||
	table == TABLE_SWE_DUBBELT_AATERKVAL ||
	table == TABLE_SWE_ENKELT_AATERKVAL ||
        /*table == TABLE_ESP_REPESCA_DOBLE_INICIO ||*/
        table == TABLE_ESP_REPESCA_SIMPLE ||
        table == TABLE_DEN_DOUBLE_ELIMINATION ||
        table == TABLE_DOUBLE_REPECHAGE_ONE_BRONZE) {
	/* Repechage */
	if (sys == FRENCH_16) {
	    set_repechage_16(m, table, 0);
	    set_repechage_16(m, table, 1);
	    if (table != TABLE_SWE_ENKELT_AATERKVAL &&
                table != TABLE_ESP_REPESCA_SIMPLE) {
		set_repechage_16(m, table, 2);
		set_repechage_16(m, table, 3);
	    }
	} else if (sys == FRENCH_32) {
	    set_repechage_32(m, table, 0);
	    set_repechage_32(m, table, 1);
	    if (table != TABLE_SWE_ENKELT_AATERKVAL &&
                table != TABLE_ESP_REPESCA_SIMPLE) {
		set_repechage_32(m, table, 2);
		set_repechage_32(m, table, 3);
	    }
	} else if (sys == FRENCH_64) {
	    set_repechage_64(m, table, 0);
	    set_repechage_64(m, table, 1);
	    if (table != TABLE_SWE_ENKELT_AATERKVAL &&
                table != TABLE_ESP_REPESCA_SIMPLE) {
		set_repechage_64(m, table, 2);
		set_repechage_64(m, table, 3);
	    }
	} else if (sys == FRENCH_128) {
	    set_repechage_128(m, table, 0);
	    set_repechage_128(m, table, 1);
	    if (table != TABLE_SWE_ENKELT_AATERKVAL &&
                table != TABLE_ESP_REPESCA_SIMPLE) {
		set_repechage_128(m, table, 2);
		set_repechage_128(m, table, 3);
	    }
	}
    }

    for (i = 1; i <= french_num_matches[table][sys]; i++) {
        if (1 || m[i].blue == 0 || m[i].white == 0) {
            gint prev_match_blue = french_matches[table][sys][i][0];
            gint prev_match_white = french_matches[table][sys][i][1];

            m[i].category = category;
            m[i].number = i;
            m[i].visible = TRUE;

            if (prev_match_blue != 0) {
                struct judoka *g = get_data(category);

                if (MATCHED_FRENCH(prev_match_blue)) {
                    if (m[i].blue &&
                        m[i].blue != WINNER_OR_LOSER(prev_match_blue) &&
                        (m[i].blue_points || m[i].white_points)) {
                        SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                     g && g->last ? g->last : "?",
                                     i, m[i].blue_points, m[i].white_points,
                                     _("canceled"));
                        m[i].blue_points = m[i].white_points = 0;
                    }
                    m[i].blue = WINNER_OR_LOSER(prev_match_blue);

                    if (db_has_hansokumake(m[i].blue) && prev_match_blue < 0 &&
			!MATCHED(i))
                        m[i].blue = GHOST;

                } else if (m[i].blue) {
                    if (MATCHED(i))
                        SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                     g && g->last ? g->last : "?",
                                     i, m[i].blue_points, m[i].white_points,
                                     _("canceled"));
                    else
                        SHOW_MESSAGE("%s %s:%d %s!", _("Match"),
                                     g && g->last ? g->last : "?", i, _("changed"));

                    m[i].blue = 0;
                }

                free_judoka(g);
            } else {
                /*
		if (db_has_hansokumake(m[i].blue) &&
		    m[i].white >= COMPETITOR &&
		    m[i].blue_points == 0 && m[i].white_points == 0)
		    m[i].white_points = 10;
                */
	    }

            if (prev_match_white != 0) {
                struct judoka *g = get_data(category);

                if (MATCHED_FRENCH(prev_match_white)) {
                    if (m[i].white &&
                        m[i].white != WINNER_OR_LOSER(prev_match_white) &&
                        (m[i].blue_points || m[i].white_points)) {
                        SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                     g && g->last ? g->last : "?",
                                     i, m[i].blue_points, m[i].white_points,
                                     _("canceled"));
                        m[i].blue_points = m[i].white_points = 0;
                    }
                    m[i].white = WINNER_OR_LOSER(prev_match_white);

                    if (db_has_hansokumake(m[i].white) && prev_match_white < 0 &&
			!MATCHED(i))
                        m[i].white = GHOST;

                } else if (m[i].white) {
                    if (MATCHED(i))
                        SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                     g && g->last ? g->last : "?",
                                     i, m[i].blue_points, m[i].white_points,
                                     _("canceled"));
                    else
                        SHOW_MESSAGE("%s %s:%d %s!", _("Match"),
                                     g && g->last ? g->last : "?", i,
                                     _("changed"));
                    m[i].white = 0;
                }

                free_judoka(g);
            } else {
                /*
		if (db_has_hansokumake(m[i].white) &&
		    m[i].blue >= COMPETITOR &&
		    m[i].blue_points == 0 && m[i].white_points == 0)
		    m[i].blue_points = 10;
                */
	    }

            set_match(&m[i]);
            db_set_match(&m[i]);
        }
    }

}

static void update_custom_matches(gint category, struct compsys systm)
{
    gint i;
    struct match *m;
    struct custom_data *cd = get_custom_table(systm.table);
    match_bare_t *t = cd->matches;
    gint n = cd->num_matches;
    struct custom_matches *cm = NULL;

#define T(_n) t[_n-1]

    if (automatic_sheet_update || automatic_web_page_update)
        current_category = category;

    cm = g_malloc(sizeof(*cm));
    if (!cm) return;
    fill_custom_struct(category, cm);
    m = cm->m;

    for (i = 1; i <= n; i++) {
        m[i].category = category;
        m[i].number = i;
        m[i].visible = TRUE;

        if (T(i).c1.type == COMP_TYPE_MATCH) {
            struct judoka *g = get_data(category);
            gint prevm = T(i).c1.num;
            gint comp = T(i).c1.pos;

            if (MATCHED_FRENCH(prevm)) {
                gint prevnum;

                for (prevnum = 0;
                     prevnum < 8 && T(i).c1.prev[prevnum] && MATCHED_FRENCH(prevm);
                     prevnum++) {
                    gint winner = WINNER(prevm);
                    if (comp == 1) {
                        if (winner == m[prevm].blue) prevm = T(prevm).c1.num;
                        else prevm = T(prevm).c2.num;
                    } else {
                        if (winner == m[prevm].blue) prevm = T(prevm).c2.num;
                        else prevm = T(prevm).c1.num;
                    }
                    comp = T(i).c1.prev[prevnum];
                }

                if (comp == 2) prevm = -prevm; // loser is negative

                if (m[i].blue &&
                    m[i].blue != WINNER_OR_LOSER(prevm) &&
                    (m[i].blue_points || m[i].white_points)) {
                    SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                 g && g->last ? g->last : "?",
                                 i, m[i].blue_points, m[i].white_points,
                                 _("canceled"));
                    m[i].blue_points = m[i].white_points = 0;
                }
                m[i].blue = WINNER_OR_LOSER(prevm);

                if (db_has_hansokumake(m[i].blue) && prevm < 0 &&
			!MATCHED(i))
                    m[i].blue = GHOST;
            } else if (m[i].blue) {
                if (MATCHED(i))
                    SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                 g && g->last ? g->last : "?",
                                 i, m[i].blue_points, m[i].white_points,
                                 _("canceled"));
                else
                    SHOW_MESSAGE("%s %s:%d %s!", _("Match"),
                                 g && g->last ? g->last : "?", i, _("changed"));

                m[i].blue = 0;
            }

            free_judoka(g);
        } // if (T(i).c1.type == COMP_TYPE_MATCH)
        else if (T(i).c1.type == COMP_TYPE_ROUND_ROBIN) {
            gint poolnum = T(i).c1.num - 1;
            gint pos = T(i).c1.pos;
            //g_print("1: poolnum=%d pos=%d fin=%d\n", poolnum, pos, cm->pm[poolnum].finished);
           if (cm->pm[poolnum].finished) {
               gint posix = cm->pm[poolnum].competitors[pos-1].position;
               gint ix = cm->pm[poolnum].competitors[posix].index;
               m[i].blue = ix;
               //g_print("1: posix=%d white=%d\n", posix, ix);
            } else
               m[i].blue = 0;
        } // if (T(i).c1.type == COMP_TYPE_ROUND_ROBIN)
        else if (T(i).c1.type == COMP_TYPE_BEST_OF_3) {
            gint pairnum = T(i).c1.num - 1;
            gint pos = T(i).c1.pos;
            g_print("1: pairnum=%d pos=%d fin=%d\n", pairnum, pos, cm->best_of_three[pairnum].finished);
            if (cm->best_of_three[pairnum].finished) {
               m[i].blue = pos == 1 ? cm->best_of_three[pairnum].winner : cm->best_of_three[pairnum].loser;
               g_print("1: white=%d\n", m[i].blue);
            } else
               m[i].blue = 0;
        } // if (T(i).c1.type == COMP_TYPE_BEST_OF_3) {

        if (T(i).c2.type == COMP_TYPE_MATCH) {
            struct judoka *g = get_data(category);
            gint prevm = T(i).c2.num;
            gint comp = T(i).c2.pos;

            if (MATCHED_FRENCH(prevm)) {
                gint prevnum;

                for (prevnum = 0;
                     prevnum < 8 && T(i).c2.prev[prevnum] && MATCHED_FRENCH(prevm);
                     prevnum++) {
                    gint winner = WINNER(prevm);
                    if (comp == 1) {
                        if (winner == m[prevm].blue) prevm = T(prevm).c1.num;
                        else prevm = T(prevm).c2.num;
                    } else {
                        if (winner == m[prevm].blue) prevm = T(prevm).c2.num;
                        else prevm = T(prevm).c1.num;
                    }
                    comp = T(i).c2.prev[prevnum];
                }

                if (comp == 2) prevm = -prevm; // loser is negative

                if (m[i].white &&
                    m[i].white != WINNER_OR_LOSER(prevm) &&
                    (m[i].white_points || m[i].white_points)) {
                    SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                 g && g->last ? g->last : "?",
                                 i, m[i].blue_points, m[i].white_points,
                                 _("canceled"));
                    m[i].blue_points = m[i].white_points = 0;
                }
                m[i].white = WINNER_OR_LOSER(prevm);

                if (db_has_hansokumake(m[i].white) && prevm < 0 &&
			!MATCHED(i))
                    m[i].white = GHOST;
            } else if (m[i].white) {
                if (MATCHED(i))
                    SHOW_MESSAGE("%s %s:%d (%d-%d) %s!", _("Match"),
                                 g && g->last ? g->last : "?",
                                 i, m[i].blue_points, m[i].white_points,
                                 _("canceled"));
                else
                    SHOW_MESSAGE("%s %s:%d %s!", _("Match"),
                                 g && g->last ? g->last : "?", i, _("changed"));

                m[i].white = 0;
            }

            free_judoka(g);
        } // if (T(i).c1.type == COMP_TYPE_MATCH)
        else if (T(i).c2.type == COMP_TYPE_ROUND_ROBIN) {
            gint poolnum = T(i).c2.num - 1;
            gint pos = T(i).c2.pos;
            //g_print("2: poolnum=%d pos=%d fin=%d\n", poolnum, pos, cm->pm[poolnum].finished);
           if (cm->pm[poolnum].finished) {
               gint posix = cm->pm[poolnum].competitors[pos-1].position;
               gint ix = cm->pm[poolnum].competitors[posix].index;
               m[i].white = ix;
               //g_print("2: posix=%d blue=%d\n", posix, ix);
            } else
               m[i].white = 0;
        } // if (T(i).c1.type == COMP_TYPE_ROUND_ROBIN)
        else if (T(i).c1.type == COMP_TYPE_BEST_OF_3) {
            gint pairnum = T(i).c2.num - 1;
            gint pos = T(i).c2.pos;
            g_print("2: pairnum=%d pos=%d fin=%d\n", pairnum, pos, cm->best_of_three[pairnum].finished);
            if (cm->best_of_three[pairnum].finished) {
               m[i].white = pos == 1 ? cm->best_of_three[pairnum].winner : cm->best_of_three[pairnum].loser;
               g_print("2: blue=%d\n", m[i].white);
            } else
               m[i].white = 0;
        } // if (T(i).c1.type == COMP_TYPE_BEST_OF_3) {

        set_match(&m[i]);
        db_set_match(&m[i]);
    } // for through matches

    empty_custom_struct(cm);
    g_free(cm);
}

gint find_match_time(const gchar *cat)
{
    if (strchr(cat, 'A') || strchr(cat, 'a'))
        return 4;
    else if (strchr(cat, 'B') || strchr(cat, 'b') ||
             strchr(cat, 'C') || strchr(cat, 'c'))
        return 3;
    else if (strchr(cat, 'D') || strchr(cat, 'd'))
        return 2;
    else if (strchr(cat, 'E') || strchr(cat, 'e'))
        return 1;
    else
        return 5;
}

gint get_match_number_flag(gint category, gint number)
{
    struct category_data *cat = avl_get_category(category);
    if (!cat)
        return 0;

    gint sys = cat->system.system - SYSTEM_FRENCH_8;
    gint table = cat->system.table;

    gint matchnum = 1000;

    switch (cat->system.system) {
    case SYSTEM_POOL:
    case SYSTEM_DPOOL2:
    case SYSTEM_QPOOL:
    case SYSTEM_BEST_OF_3:
	return 0;

    case SYSTEM_DPOOL:
        matchnum = num_matches(cat->system.system, cat->system.numcomp) + 3;

	if (number == matchnum - 2)
	    return MATCH_FLAG_SEMIFINAL_A;
	else if (number == matchnum - 1)
	    return MATCH_FLAG_SEMIFINAL_B;
	else if (number == matchnum)
	    return MATCH_FLAG_GOLD;
	return 0;

    case SYSTEM_DPOOL3:
        matchnum = num_matches(cat->system.system, cat->system.numcomp) + 2;

	if (number == matchnum - 1)
	    return MATCH_FLAG_BRONZE_A;
	else if (number == matchnum)
	    return MATCH_FLAG_GOLD;
	return 0;

    case SYSTEM_FRENCH_8:
    case SYSTEM_FRENCH_16:
    case SYSTEM_FRENCH_32:
    case SYSTEM_FRENCH_64:
    case SYSTEM_FRENCH_128:
        if (table == TABLE_MODIFIED_DOUBLE_ELIMINATION) {
            if (number == medal_matches[table][sys][1])
                return MATCH_FLAG_SILVER;
            else if (number == medal_matches[table][sys][2])
                return MATCH_FLAG_GOLD;
	} else if (number == medal_matches[table][sys][0])
	    return MATCH_FLAG_BRONZE_A;
	else if (number == medal_matches[table][sys][1])
	    return MATCH_FLAG_BRONZE_B;
	else if (number == medal_matches[table][sys][2])
	    return MATCH_FLAG_GOLD;
        else if (number == french_matches[table][sys][medal_matches[table][sys][2]][0])
	    return MATCH_FLAG_SEMIFINAL_A;
        else if (number == french_matches[table][sys][medal_matches[table][sys][2]][1])
	    return MATCH_FLAG_SEMIFINAL_B;
        else if (is_repechage(cat->system, number))
            return MATCH_FLAG_REPECHAGE;
    }

    return 0;
}

gchar *get_match_number_text(gint category, gint number)
{
    gint flags = get_match_number_flag(category, number);

    if (flags & MATCH_FLAG_GOLD)
        return _("Gold medal match");
    else if (flags & MATCH_FLAG_BRONZE_A)
        return _("Bronze match A");
    else if (flags & MATCH_FLAG_BRONZE_B)
        return _("Bronze match B");
    else if (flags & MATCH_FLAG_SEMIFINAL_A)
        return _("Semifinal A");
    else if (flags & MATCH_FLAG_SEMIFINAL_B)
        return _("Semifinal B");
    else if (flags & MATCH_FLAG_SILVER)
        return _("Silver medal match");

    return NULL;
}

#if 0
static struct message info_cache[NUM_TATAMIS][INFO_MATCH_NUM+1];
static gboolean info_cache_changed[NUM_TATAMIS][INFO_MATCH_NUM+1];
extern gint send_msg(gint fd, struct message *msg);
extern void send_info_packet(struct message *msg);

gboolean send_info_bg(gpointer user_data)
{
    static gint t = 0;
    gint j;

    for (j = 0; j <= INFO_MATCH_NUM; j++) {
	if (info_cache_changed[t][j]) {
	    send_info_packet(&info_cache[t][j]);
	    info_cache_changed[t][j] = FALSE;
	    return TRUE;
	}
    }

    if (++t >= NUM_TATAMIS)
	t = 0;

    return TRUE;
}

void send_packet_cache(struct message *msg)
{
    gint i = msg->u.match_info.tatami - 1;
    gint j = msg->u.match_info.position;

    info_cache[i][j] = *msg;
    info_cache_changed[i][j] = TRUE;
}
#endif

void fill_match_info(struct message *msg, gint tatami, gint pos, struct match *m)
{
    msg->u.match_info_11.info[pos].tatami   = tatami;
    msg->u.match_info_11.info[pos].position = pos;
    msg->u.match_info_11.info[pos].category = m->category;
    msg->u.match_info_11.info[pos].number   = m->number;
    msg->u.match_info_11.info[pos].blue     = m->blue;
    msg->u.match_info_11.info[pos].white    = m->white;

    struct category_data *cat = avl_get_category(m->category);
    if (cat) {
        msg->u.match_info_11.info[pos].flags = get_match_number_flag(m->category, m->number);

        time_t last_time1 = avl_get_competitor_last_match_time(m->blue);
        time_t last_time2 = avl_get_competitor_last_match_time(m->white);
        time_t last_time = last_time1 > last_time2 ? last_time1 : last_time2;
        gint rest_time = cat->rest_time;
        time_t now = time(NULL);

        if (now < last_time + rest_time) {
            msg->u.match_info_11.info[pos].rest_time = last_time + rest_time - now;
            if (last_time1 > last_time2)
                msg->u.match_info_11.info[pos].flags |= MATCH_FLAG_BLUE_REST | MATCH_FLAG_BLUE_DELAYED;
            else
                msg->u.match_info_11.info[pos].flags |= MATCH_FLAG_WHITE_REST | MATCH_FLAG_WHITE_DELAYED;
        } else
            msg->u.match_info_11.info[pos].rest_time = 0;
    }
    msg->u.match_info_11.info[pos].flags |= MATCH_FLAG_JUDOGI_MASK & m->deleted;

    if (avl_get_competitor_status(m->blue) & JUDOGI_OK)
        msg->u.match_info_11.info[pos].flags |= MATCH_FLAG_JUDOGI1_OK;
    if (avl_get_competitor_status(m->white) & JUDOGI_OK)
        msg->u.match_info_11.info[pos].flags |= MATCH_FLAG_JUDOGI2_OK;
}

void send_match(gint tatami, gint pos, struct match *m)
{
    struct message msg;

    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_MATCH_INFO;
    msg.u.match_info.tatami   = tatami;
    msg.u.match_info.position = pos;
    msg.u.match_info.category = m->category;
    msg.u.match_info.number   = m->number;
    msg.u.match_info.blue     = m->blue;
    msg.u.match_info.white    = m->white;

    struct category_data *cat = avl_get_category(m->category);
    if (cat) {
        msg.u.match_info.flags = get_match_number_flag(m->category, m->number);

        time_t last_time1 = avl_get_competitor_last_match_time(m->blue);
        time_t last_time2 = avl_get_competitor_last_match_time(m->white);
        time_t last_time = last_time1 > last_time2 ? last_time1 : last_time2;
        gint rest_time = cat->rest_time;
        time_t now = time(NULL);

        if (now < last_time + rest_time) {
            msg.u.match_info.rest_time = last_time + rest_time - now;
            if (last_time1 > last_time2)
                msg.u.match_info.flags |= MATCH_FLAG_BLUE_REST | MATCH_FLAG_BLUE_DELAYED;
            else
                msg.u.match_info.flags |= MATCH_FLAG_WHITE_REST | MATCH_FLAG_WHITE_DELAYED;
        } else
            msg.u.match_info.rest_time = 0;
    }
    msg.u.match_info.flags |= MATCH_FLAG_JUDOGI_MASK & m->deleted;

    if (avl_get_competitor_status(m->blue) & JUDOGI_OK)
        msg.u.match_info.flags |= MATCH_FLAG_JUDOGI1_OK;
    if (avl_get_competitor_status(m->white) & JUDOGI_OK)
        msg.u.match_info.flags |= MATCH_FLAG_JUDOGI2_OK;

    send_packet(&msg);
}

void send_matches(gint tatami)
{
    struct message msg;
    struct match *m = get_cached_next_matches(tatami);
    gint t = tatami - 1, k;

    if (t < 0 || t >= NUM_TATAMIS)
        return;

    draw_match_graph();
    draw_gategory_graph();

    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_11_MATCH_INFO;
    msg.u.match_info_11.info[0].tatami   = tatami;
    msg.u.match_info_11.info[0].position = 0;
    msg.u.match_info_11.info[0].category = next_matches_info[t][0].won_catnum;
    msg.u.match_info_11.info[0].number   = next_matches_info[t][0].won_matchnum;
    msg.u.match_info_11.info[0].blue     = next_matches_info[t][0].won_ix;
    msg.u.match_info_11.info[0].white    = 0;
    msg.u.match_info_11.info[0].flags    = get_match_number_flag(next_matches_info[t][0].won_catnum,
                                                      next_matches_info[t][0].won_matchnum);
    for (k = 0; k < INFO_MATCH_NUM; k++) {
        fill_match_info(&msg, tatami, k+1, &(m[k]));
    }

    send_packet(&msg);
}

void update_matches_small(guint category, struct compsys sys_or_tatami)
{
    if (sys_or_tatami.system == 0)
        sys_or_tatami = get_cat_system(category);

    switch (sys_or_tatami.system) {
    case SYSTEM_POOL:
    case SYSTEM_DPOOL:
    case SYSTEM_DPOOL2:
    case SYSTEM_DPOOL3:
    case SYSTEM_QPOOL:
    case SYSTEM_BEST_OF_3:
        update_pool_matches(category, sys_or_tatami.numcomp);
        break;
    case SYSTEM_FRENCH_8:
    case SYSTEM_FRENCH_16:
    case SYSTEM_FRENCH_32:
    case SYSTEM_FRENCH_64:
    case SYSTEM_FRENCH_128:
        update_french_matches(category, sys_or_tatami);
        break;
    }
}

void send_next_matches(gint category, gint tatami, struct match *nm)
{
    struct message msg;
    struct judoka *j1 = NULL, *j2 = NULL, *g = NULL;

    /* create a message for the judotimers */
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_NEXT_MATCH;
    msg.u.next_match.category = nm[0].category;
    msg.u.next_match.match = nm[0].number;
    msg.u.next_match.flags = nm[0].deleted;

    if (nm[0].number < 1000) {
        struct category_data *cat = avl_get_category(nm[0].category);

        if (cat && is_repechage(cat->system, nm[0].number))
            msg.u.next_match.flags |= MATCH_FLAG_REPECHAGE;

        if (cat && (cat->deleted & TEAM_EVENT))
            msg.u.next_match.flags |= MATCH_FLAG_TEAM_EVENT;

        if (!tatami)
            tatami = db_find_match_tatami(nm[0].category, nm[0].number);

        g = get_data(nm[0].category & MATCH_CATEGORY_MASK);
        j1 = get_data(nm[0].blue);
        j2 = get_data(nm[0].white);

        if (g && j1 && j2) {
            gchar buf[100];

            if (avl_get_competitor_status(nm[0].blue) & JUDOGI_OK)
                msg.u.next_match.flags |= MATCH_FLAG_JUDOGI1_OK;
            if (avl_get_competitor_status(nm[0].white) & JUDOGI_OK)
                msg.u.next_match.flags |= MATCH_FLAG_JUDOGI2_OK;

            if (fill_in_next_match_message_data(g->last, &msg.u.next_match)) {
                msg.u.next_match.minutes = 1<<20; //find_match_time(g->last);
                time_t last_time1 = avl_get_competitor_last_match_time(nm[0].blue);
                time_t last_time2 = avl_get_competitor_last_match_time(nm[0].white);
                time_t last_time = last_time1 > last_time2 ? last_time1 : last_time2;
                gint rest_time = msg.u.next_match.rest_time;
                time_t now = time(NULL);

                if ((nm[0].category & MATCH_CATEGORY_SUB_MASK) == 0 && now < last_time + rest_time) {
                    msg.u.next_match.rest_time = last_time + rest_time;
                    if (last_time1 > last_time2)
                        msg.u.next_match.minutes |= MATCH_FLAG_BLUE_REST;
                    else
                        msg.u.next_match.minutes |= MATCH_FLAG_WHITE_REST;
                } else
                    msg.u.next_match.rest_time = 0;
            }
            set_graph_rest_time(tatami, msg.u.next_match.rest_time, msg.u.next_match.minutes);

            msg.u.next_match.tatami = tatami;

            if (nm[0].category & MATCH_CATEGORY_SUB_MASK) {
                gint ix = find_age_index(g->last);
                if (ix >= 0 && nm[0].number > 0 && nm[0].number < NUM_CAT_DEF_WEIGHTS) {
                    snprintf(msg.u.next_match.cat_1,
                             sizeof(msg.u.next_match.cat_1),
                             "%s", category_definitions[ix].weights[nm[0].number-1].weighttext);
                }
            } else
                snprintf(msg.u.next_match.cat_1,
                         sizeof(msg.u.next_match.cat_1),
                         "%s", g->last);

            snprintf(msg.u.next_match.blue_1,
                     sizeof(msg.u.next_match.blue_1),
                     "%s\t%s\t%s\t%s", j1->last, j1->first, j1->country, j1->club);
            snprintf(msg.u.next_match.white_1, sizeof(msg.u.next_match.white_1),
                     "%s\t%s\t%s\t%s", j2->last, j2->first, j2->country, j2->club);

            snprintf(buf, sizeof(buf), "%s: %s: %s %s, %s - %s %s, %s", _("Match"),
                     g->last,
                     j1->first, j1->last, get_club_text(j1, 0),
                     j2->first, j2->last, get_club_text(j2, 0));

            if (tatami > 0 && tatami <= NUM_TATAMIS) {
                gtk_label_set_text(GTK_LABEL(next_match[tatami-1][0]), buf);
                COPY_MATCH_INFO(category, tatami, 0, g->last);
                COPY_MATCH_INFO(blue_first, tatami, 0, j1->first);
                COPY_MATCH_INFO(blue_last, tatami, 0, j1->last);
                COPY_MATCH_INFO(blue_club, tatami, 0, get_club_text(j1, 0));
                COPY_MATCH_INFO(white_first, tatami, 0, j2->first);
                COPY_MATCH_INFO(white_last, tatami, 0, j2->last);
                COPY_MATCH_INFO(white_club, tatami, 0, get_club_text(j2, 0));
                next_matches_info[tatami-1][0].catnum = nm[0].category;
                next_matches_info[tatami-1][0].matchnum = nm[0].number;
                next_matches_info[tatami-1][0].blue = nm[0].blue;
                next_matches_info[tatami-1][0].white = nm[0].white;
            }
        } else if (g) {
            msg.u.next_match.tatami = tatami;
            gtk_label_set_text(GTK_LABEL(next_match[tatami-1][0]), _("Match:"));
            snprintf(msg.u.next_match.cat_1,
                     sizeof(msg.u.next_match.cat_1),
                     "%s", g->last);
            snprintf(msg.u.next_match.blue_1, sizeof(msg.u.next_match.blue_1), "\t\t\t");
            snprintf(msg.u.next_match.white_1, sizeof(msg.u.next_match.white_1), "\t\t\t");
        }
        free_judoka(g);
        free_judoka(j1);
        free_judoka(j2);
    } else {
        if (tatami == 0 && category) {
            g = get_data(category  & MATCH_CATEGORY_MASK);
            if (g) {
                tatami = g->belt;
                free_judoka(g);
            }
        }

        if (tatami > 0 && tatami <= NUM_TATAMIS) {
            msg.u.next_match.tatami = tatami;
            gtk_label_set_text(GTK_LABEL(next_match[tatami-1][0]), _("Match:"));
            NULL_MATCH_INFO(category, tatami, 0);
            NULL_MATCH_INFO(blue_first, tatami, 0);
            NULL_MATCH_INFO(blue_last, tatami, 0);
            NULL_MATCH_INFO(blue_club, tatami, 0);
            NULL_MATCH_INFO(white_first, tatami, 0);
            NULL_MATCH_INFO(white_last, tatami, 0);
            NULL_MATCH_INFO(white_club, tatami, 0);
            next_matches_info[tatami-1][0].catnum = 0;
        }
    }

    if (nm[1].number < 1000) {
        if (!tatami)
            tatami = db_find_match_tatami(nm[1].category, nm[1].number);
        g = get_data(nm[1].category & MATCH_CATEGORY_MASK);
        j1 = get_data(nm[1].blue);
        j2 = get_data(nm[1].white);

        if (g) {
            gchar buf[100];

            if (nm[1].category & MATCH_CATEGORY_SUB_MASK) {
                gint ix = find_age_index(g->last);
                if (ix >= 0 && nm[1].number > 0 && nm[1].number < NUM_CAT_DEF_WEIGHTS) {
                    snprintf(msg.u.next_match.cat_2,
                             sizeof(msg.u.next_match.cat_2),
                             "%s", category_definitions[ix].weights[nm[1].number-1].weighttext);
                }
            } else
                snprintf(msg.u.next_match.cat_2, sizeof(msg.u.next_match.cat_2),
                         "%s", g->last);
            if (j1)
                snprintf(msg.u.next_match.blue_2, sizeof(msg.u.next_match.blue_2),
                         "%s\t%s\t%s\t%s", j1->last, j1->first, j1->country, j1->club);
            else
                snprintf(msg.u.next_match.blue_2, sizeof(msg.u.next_match.blue_2), "?");

            if (j2)
                snprintf(msg.u.next_match.white_2, sizeof(msg.u.next_match.white_2),
                         "%s\t%s\t%s\t%s", j2->last, j2->first, j2->country, j2->club);
            else
                snprintf(msg.u.next_match.white_2, sizeof(msg.u.next_match.white_2), "?");

            if (j1 && j2)
                snprintf(buf, sizeof(buf), "%s: %s: %s %s, %s - %s %s, %s", _("Preparing"),
                         g->last,
                         j1->first, j1->last, get_club_text(j1, 0),
                         j2->first, j2->last, get_club_text(j2, 0));
            else
                snprintf(buf, sizeof(buf), "%s: %s: ? - ?", _("Preparing"), g->last);

            if (tatami > 0 && tatami <= NUM_TATAMIS) {
                gtk_label_set_text(GTK_LABEL(next_match[tatami-1][1]), buf);
                COPY_MATCH_INFO(category, tatami, 1, g->last);
                COPY_MATCH_INFO(blue_first, tatami, 1, j1 ? j1->first : "?");
                COPY_MATCH_INFO(blue_last, tatami, 1, j1 ? j1->last : "?");
                COPY_MATCH_INFO(blue_club, tatami, 1, j1 ? get_club_text(j1, 0) : "?");
                COPY_MATCH_INFO(white_first, tatami, 1, j2 ? j2->first : "?");
                COPY_MATCH_INFO(white_last, tatami, 1, j2 ? j2->last : "?");
                COPY_MATCH_INFO(white_club, tatami, 1, j2 ? get_club_text(j2, 0) : "?");
                next_matches_info[tatami-1][1].catnum = nm[1].category;
                next_matches_info[tatami-1][1].matchnum = nm[1].number;
                next_matches_info[tatami-1][1].blue = nm[1].blue;
                next_matches_info[tatami-1][1].white = nm[1].white;
            }
        }
        free_judoka(g);
        free_judoka(j1);
        free_judoka(j2);
    } else {
        if (tatami == 0 && category) {
            g = get_data(category & MATCH_CATEGORY_MASK);
            if (g) {
                tatami = g->belt;
                free_judoka(g);
            }
        }

        if (tatami > 0 && tatami <= NUM_TATAMIS) {
            gtk_label_set_text(GTK_LABEL(next_match[tatami-1][1]),
                               _("Preparing:"));
            NULL_MATCH_INFO(category, tatami, 1);
            NULL_MATCH_INFO(blue_first, tatami, 1);
            NULL_MATCH_INFO(blue_last, tatami, 1);
            NULL_MATCH_INFO(blue_club, tatami, 1);
            NULL_MATCH_INFO(white_first, tatami, 1);
            NULL_MATCH_INFO(white_last, tatami, 1);
            NULL_MATCH_INFO(white_club, tatami, 1);
            next_matches_info[tatami-1][1].catnum = 0;
        }
    }

    send_packet(&msg);
}

void update_matches(guint category, struct compsys sys, gint tatami)
{
    struct match *nm;
    PROF_START;
    if (category) {
        struct category_data *catdata = avl_get_category(category);
        if (catdata && (catdata->deleted & TEAM_EVENT)) {
            db_event_matches_update(category);
        }

        category &= MATCH_CATEGORY_MASK;

        if (sys.system == 0)
            sys = db_get_system(category);

        switch (sys.system) {
        case SYSTEM_POOL:
        case SYSTEM_DPOOL:
        case SYSTEM_DPOOL2:
        case SYSTEM_DPOOL3:
        case SYSTEM_QPOOL:
        case SYSTEM_BEST_OF_3:
            update_pool_matches(category, sys.numcomp);
            break;
        case SYSTEM_FRENCH_8:
        case SYSTEM_FRENCH_16:
        case SYSTEM_FRENCH_32:
        case SYSTEM_FRENCH_64:
        case SYSTEM_FRENCH_128:
            update_french_matches(category, sys);
            break;
        case SYSTEM_CUSTOM:
            update_custom_matches(category, sys);
            break;
        }
    }
    PROF;
#if (GTKVER == 3)
    G_LOCK(next_match_mutex);
#else
    g_static_mutex_lock(&next_match_mutex);
#endif
    nm = db_next_match(tatami ? 0 : category, tatami);
    PROF;
    if (nm)
        send_next_matches(category, tatami, nm);
#if (GTKVER == 3)
    G_UNLOCK(next_match_mutex);
#else
    g_static_mutex_unlock(&next_match_mutex);
#endif
    if (category)
        update_category_status_info(category);

    PROF;
    refresh_sheet_display(FALSE);
    PROF;
    refresh_window();
    PROF;

    send_matches(tatami);
    PROF;
    PROF_END;
}

static void log_match(gint category, gint number, gint bluepts, gint whitepts)
{
    gint blue_ix, white_ix, win_ix;
    struct judoka *blue = NULL, *white = NULL, *cat = get_data(category), *winner;
    gint tatami = db_find_match_tatami(category, number);

    if (!cat)
        return;

    gchar *cmd = g_strdup_printf("select * from matches where \"category\"=%d and \"number\"=%d",
                                 category, number);
    gint numrows = db_get_table(cmd);
    g_free(cmd);

    if (numrows <= 0)
        goto error;

    blue_ix = atoi(db_get_data(0, "blue"));
    white_ix = atoi(db_get_data(0, "white"));
    blue = get_data(blue_ix);
    white = get_data(white_ix);

    if (blue == NULL || white == NULL)
        goto error;

    if (bluepts > whitepts) {
        winner = blue;
        win_ix = blue_ix;
    } else {
        winner = white;
        win_ix = white_ix;
    }

    COMP_LOG_INFO(tatami, "%s: %s (%d), %s-%s, %s: %d-%d", _("Match"),
                  cat->last, number, blue->last, white->last,
                  _("points"), bluepts, whitepts);

    if (tatami >= 1 && tatami <= NUM_TATAMIS) {
        g_strlcpy(next_matches_info[tatami-1][0].won_last,
                  winner->last, sizeof(next_matches_info[0][0].won_last));
        g_strlcpy(next_matches_info[tatami-1][0].won_first,
                  winner->first, sizeof(next_matches_info[0][0].won_first));
        g_strlcpy(next_matches_info[tatami-1][0].won_cat,
                  cat->last, sizeof(next_matches_info[0][0].won_cat));
        next_matches_info[tatami-1][0].won_catnum = category;
        next_matches_info[tatami-1][0].won_matchnum = number;
        next_matches_info[tatami-1][0].won_ix = win_ix;
    }

error:
    free_judoka(blue);
    free_judoka(white);
    free_judoka(cat);
    db_close_table();
}

struct popup_data popupdata;

void set_points(GtkWidget *menuitem, gpointer userdata)
{
    guint data = ptr_to_gint(userdata);
    guint category = popupdata.category;
    guint number = popupdata.number;
    gboolean is_blue = popupdata.is_blue;
    guint points = data & 0xf;
    struct compsys sys = db_get_system(category);
    gboolean hikiwake = points == 11;

    if (hikiwake) {
        db_set_points(category, number, 0, 1, 1, 0, 0, 0);
    } else {
        db_set_points(category, number, 0,
                      is_blue ? points : 0,
                      is_blue ? 0 : points, 0, 0, 0);
    }

    db_read_match(category, number);

    if (hikiwake)
        log_match(category, number, 1, 1);
    else
        log_match(category, number, is_blue ? points : 0, is_blue ? 0 : points);

    update_matches(category, sys, db_find_match_tatami(category, number));

    /* send points to net */
    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_SET_POINTS;
    msg.u.set_points.category     = category;
    msg.u.set_points.number       = number;
    msg.u.set_points.sys          = sys.system;
    msg.u.set_points.blue_points  = hikiwake ? 1 : (is_blue ? points : 0);
    msg.u.set_points.white_points = hikiwake ? 1 : (is_blue ? 0 : points);
    send_packet(&msg);

    db_force_match_number(category);

    make_backup();
}

void set_points_from_net(struct message *msg)
{
    struct compsys sys = db_get_system(msg->u.set_points.category);

    db_set_points(msg->u.set_points.category,
                  msg->u.set_points.number,
                  0,
                  msg->u.set_points.blue_points,
                  msg->u.set_points.white_points,
                  0, 0, 0);

    db_read_match(msg->u.set_points.category, msg->u.set_points.number);

    log_match(msg->u.set_points.category, msg->u.set_points.number,
              msg->u.set_points.blue_points, msg->u.set_points.white_points);

    update_matches(msg->u.set_points.category, sys,
                   db_find_match_tatami(msg->u.set_points.category, msg->u.set_points.number));

    db_force_match_number(msg->u.set_points.category);

    make_backup();
}

void set_points_and_score(struct message *msg)
{
    guint category = msg->u.result.category;
    guint number = msg->u.result.match;
    gint  minutes = msg->u.result.minutes;
    struct compsys sys = db_get_system(category);
    gint  winscore = 0, losescore = 0;
    gint  points = 2, blue_pts = 0, white_pts = 0;
    //gint  numcompetitors = sys & COMPETITORS_MASK;

    if (number >= 1000 || category < 10000)
        return;

    if (msg->u.result.blue_hansokumake || (msg->u.result.blue_score & 0xf) >= 4) {
        msg->u.result.blue_score &= 0xffff;
        msg->u.result.white_score |= 0x10000;
        if (msg->u.result.blue_hansokumake)
            msg->u.result.blue_score |= 8;
    } else if (msg->u.result.white_hansokumake || (msg->u.result.white_score & 0xf) >= 4) {
        msg->u.result.blue_score |= 0x10000;
        msg->u.result.white_score &= 0xffff;
        if (msg->u.result.white_hansokumake)
            msg->u.result.white_score |= 8;
    }

    if ((msg->u.result.blue_score & 0xffff0) > (msg->u.result.white_score & 0xffff0)) {
        winscore = msg->u.result.blue_score & 0xffff0;
        losescore = msg->u.result.white_score & 0xffff0;
    } else if ((msg->u.result.blue_score & 0xffff0) < (msg->u.result.white_score & 0xffff0)) {
        winscore = msg->u.result.white_score & 0xffff0;
        losescore = msg->u.result.blue_score & 0xffff0;
    } else if (prop_get_int_val(PROP_EQ_SCORE_LESS_SHIDO_WINS) &&
               (msg->u.result.blue_score & 0xf) != (msg->u.result.white_score & 0xf)) {
        if ((msg->u.result.blue_score & 0xf) > (msg->u.result.white_score & 0xf)) {
            winscore = msg->u.result.white_score;
            losescore = msg->u.result.blue_score;
        } else {
            winscore = msg->u.result.blue_score;
            losescore = msg->u.result.white_score;
        }
    } else if (msg->u.result.blue_vote == 0 && msg->u.result.white_vote == 0)
        return;

    if ((winscore & 0xffff0) != (losescore & 0xffff0)) {
        if ((winscore & 0xf0000) && (losescore & 0xf0000) == 0) points = 10;
        else if ((winscore & 0xf000) && (losescore & 0xf000) == 0) points = 7;
        else if ((winscore & 0xf00) > (losescore & 0xf00)) points = 5;
        else if ((winscore & 0xf0) > (losescore & 0xf0)) points = 3;
        else points = 2; //XXXXXXXXXXXXXXX This should never happen

        // After golden score only one point to winner if hantei bits set (Slovakia).
        if (prop_get_int_val(PROP_GS_WIN_GIVES_1_POINT) && (msg->u.result.legend & 0x100))
            points = 1;

        if ((msg->u.result.blue_score) > (msg->u.result.white_score))
            blue_pts = points;
        else
            white_pts = points;
    } else if (prop_get_int_val(PROP_EQ_SCORE_LESS_SHIDO_WINS) &&
               winscore != losescore) {
        if ((msg->u.result.blue_score & 0xf) > (msg->u.result.white_score & 0xf))
            white_pts = 1;
        else
            blue_pts = 1;
    } else if (msg->u.result.blue_vote && msg->u.result.white_vote) { // hikiwake
            blue_pts = 1;
            white_pts = 1;
    } else {
        if (msg->u.result.blue_vote > msg->u.result.white_vote)
            blue_pts = 1;
        else
            white_pts = 1;
    }

    /*** Timer cannot disqualify competitors from the tournament
    if (msg->u.result.blue_hansokumake || msg->u.result.white_hansokumake) {
        db_set_match_hansokumake(category, number,
                                 msg->u.result.blue_hansokumake,
                                 msg->u.result.white_hansokumake);
    }
    ***/

    db_set_points(category, number, minutes,
                  blue_pts, white_pts, msg->u.result.blue_score, msg->u.result.white_score,
                  msg->u.result.legend); // bits 0-7 are legend, bit #8 = golden score

    db_read_match(category, number);

    log_match(category, number, blue_pts, white_pts);

    update_matches(category, sys, db_find_match_tatami(category, number));

    db_force_match_number(category);

    make_backup();
}

void update_competitors_categories(gint competitor)
{
    GtkTreeIter iter, parent;
    struct compsys sys;
    gint category;

    if (find_iter(&iter, competitor) == FALSE)
        return;

    if (gtk_tree_model_iter_parent(current_model, &parent, &iter) == FALSE)
        return;

    gtk_tree_model_get(current_model, &parent,
                       COL_INDEX, &category,
                       -1);

    sys = db_get_system(category);

    update_matches(category, sys, 0);
}

static void expand_all(GtkWidget *widget, gpointer userdata)
{
    GtkWidget *treeview = userdata;
    gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
}

static void collapse_all(GtkWidget *widget, gpointer userdata)
{
    GtkWidget *treeview = userdata;
    gtk_tree_view_collapse_all(GTK_TREE_VIEW(treeview));
}

static void view_match_popup_menu(GtkWidget *treeview,
                                  GdkEventButton *event,
                                  guint category,
                                  guint number,
                                  gboolean visible)
{
    GtkWidget *menu, *menuitem;

    menu = gtk_menu_new();

    menuitem = gtk_menu_item_new_with_label(_("Expand All"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) expand_all, treeview);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label(_("Collapse All"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) collapse_all, treeview);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    gtk_widget_show_all(menu);

    /* Note: event can be NULL here when called from view_onPopupMenu;
     *  gdk_event_get_time() accepts a NULL argument */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   (event != NULL) ? event->button : 0,
                   gdk_event_get_time((GdkEvent*)event));
}

//void matches_refresh_1(gint tatami);

static void set_comment(GtkWidget *menuitem, gpointer userdata)
{
    guint data = ptr_to_gint(userdata);
    guint category = popupdata.category;
    guint number = popupdata.number;
    guint cmd = data & 0x3;
    struct compsys sys;
    sys = db_get_system(category);
    //gint tatami = db_get_tatami(category);

    db_set_comment(category, number, cmd);
    //db_read_matches(); too heavy and unnecessary
    update_matches(category, sys, db_find_match_tatami(category, number));
    //matches_refresh_1(tatami);

    /* send comment to net */
    struct message msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = MSG_SET_COMMENT;
    msg.u.set_comment.data = data;
    msg.u.set_comment.category = category;
    msg.u.set_comment.number = number;
    msg.u.set_comment.cmd = cmd;
    msg.u.set_comment.sys = sys.system;
    send_packet(&msg);
}

void set_comment_from_net(struct message *msg)
{
    struct compsys sys;
    sys = db_get_system(msg->u.set_comment.category);
    //gint tatami = db_get_tatami(category);

    db_set_comment(msg->u.set_comment.category, msg->u.set_comment.number, msg->u.set_comment.cmd);
    //db_read_matches(); too heavy operation
    update_matches(msg->u.set_comment.category, sys,
                   db_find_match_tatami(msg->u.set_comment.category, msg->u.set_comment.number));
    //matches_refresh_1(tatami);
}

static void view_next_match_popup_menu(GtkWidget *treeview,
                                       GdkEventButton *event,
                                       guint category,
                                       guint number,
                                       gboolean visible)
{
    GtkWidget *menu, *menuitem;

    popupdata.category = category;
    popupdata.number = number;

    menu = gtk_menu_new();

    menuitem = gtk_menu_item_new_with_label(_("Next match"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_comment, gint_to_ptr(COMMENT_MATCH_1));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label(_("Preparing"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_comment, gint_to_ptr(COMMENT_MATCH_2));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label(_("Delay the match"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_comment, gint_to_ptr(COMMENT_WAIT));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label(_("Remove comment"));
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_comment, gint_to_ptr(0));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    gtk_widget_show_all(menu);

    /* Note: event can be NULL here when called from view_onPopupMenu;
     *  gdk_event_get_time() accepts a NULL argument */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   (event != NULL) ? event->button : 0,
                   gdk_event_get_time((GdkEvent*)event));
}

static void change_competitor(gint index, gpointer data)
{
    struct match *nm;
    gint tatami = db_get_tatami(popupdata.category);

    db_change_competitor(popupdata.category, popupdata.number, popupdata.is_blue, index);
    db_read_match(popupdata.category, popupdata.number);

#if (GTKVER == 3)
    G_LOCK(next_match_mutex);
#else
    g_static_mutex_lock(&next_match_mutex);
#endif
    nm = db_next_match(popupdata.category, tatami);
    if (nm)
        send_next_matches(popupdata.category, tatami, nm);
#if (GTKVER == 3)
    G_UNLOCK(next_match_mutex);
#else
    g_static_mutex_unlock(&next_match_mutex);
#endif
    send_matches(tatami);
}

static void view_match_competitor_popup_menu(GtkWidget *treeview,
                                         GdkEventButton *event,
                                         guint category,
                                         guint number,
                                         gboolean is_blue)
{
    popupdata.category = category;
    popupdata.number = number;
    popupdata.is_blue = is_blue;

    search_competitor_args(NULL, change_competitor, &popupdata);
}

static void view_match_points_popup_menu(GtkWidget *treeview,
                                         GdkEventButton *event,
                                         guint category,
                                         guint number,
                                         gboolean is_blue)
{
    GtkWidget *menu, *menuitem;
    //guint data;

    //data = (category << 18) | (number << 5) | (is_blue ? 16: 0);
    popupdata.category = category;
    popupdata.number = number;
    popupdata.is_blue = is_blue;

    menu = gtk_menu_new();

    menuitem = gtk_menu_item_new_with_label("10");
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_points, gint_to_ptr(10));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    menuitem = gtk_menu_item_new_with_label("7");
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_points, gint_to_ptr(7));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    menuitem = gtk_menu_item_new_with_label("5");
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_points, gint_to_ptr(5));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    menuitem = gtk_menu_item_new_with_label("3");
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_points, gint_to_ptr(3));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    menuitem = gtk_menu_item_new_with_label("1");
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_points, gint_to_ptr(1));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    menuitem = gtk_menu_item_new_with_label("0");
    g_signal_connect(menuitem, "activate",
                     (GCallback) set_points, gint_to_ptr(0));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    if (category & MATCH_CATEGORY_SUB_MASK) {
        menuitem = gtk_menu_item_new_with_label("Hikiwake");
        g_signal_connect(menuitem, "activate",
                         (GCallback) set_points, gint_to_ptr(11));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    }

    gtk_widget_show_all(menu);

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
                   (event != NULL) ? event->button : 0,
                   gdk_event_get_time((GdkEvent*)event));
}

struct score {
    guint category, number;
    gboolean is_blue;
    GtkWidget *ippon, *wazaari, *yuko, *shido;
};

struct match_time {
    guint category, number;
    GtkWidget *min, *sec;
};

static void set_score(GtkWidget *widget,
                      GdkEvent *event,
                      gpointer data)
{
    struct score *s = data;

    if (ptr_to_gint(event) == GTK_RESPONSE_OK) {
        db_set_score(s->category, s->number,
                     (gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->ippon))<<16) |
                     (gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->wazaari))<<12) |
                     (gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->yuko))<<8) |
                     gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->shido)),
                     s->is_blue);

        db_read_match(s->category, s->number);
        //update_matches(category, sys, db_find_match_tatami(category, number));
        //db_force_match_number(category);
        //make_backup();
    }

    g_free(s);
    gtk_widget_destroy(widget);
}

static void set_time(GtkWidget *widget,
		     GdkEvent *event,
		     gpointer data)
{
    struct match_time *s = data;
    struct compsys sys = db_get_system(s->category);

    if (ptr_to_gint(event) == GTK_RESPONSE_OK) {
        db_set_time(s->category, s->number,
		    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->min))*60 +
		    gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(s->sec)));

        db_read_match(s->category, s->number);
        update_matches(s->category, sys, db_find_match_tatami(s->category, s->number));
        db_force_match_number(s->category);
        make_backup();
    }

    g_free(s);
    gtk_widget_destroy(widget);
}

static void view_match_score_popup_menu(GtkWidget *treeview,
                                        GdkEventButton *event,
                                        guint category,
                                        guint number,
                                        guint score,
                                        gboolean is_blue)
{
    struct score *s = g_malloc0(sizeof *s);
    GtkWidget *dialog, *table;

    s->category = category;
    s->number = number;
    s->is_blue = is_blue;

    dialog = gtk_dialog_new_with_buttons (_("Set score"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          NULL);

    s->ippon = gtk_spin_button_new_with_range(0.0, 1.0, 1.0);
    s->wazaari = gtk_spin_button_new_with_range(0.0, 2.0, 1.0);
    s->yuko = gtk_spin_button_new_with_range(0.0, 10.0, 1.0);
    s->shido = gtk_spin_button_new_with_range(0.0, 4.0, 1.0);

#if (GTKVER == 3)
    table = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("I"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("W"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("Y"), 2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("/"), 3, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("S"), 4, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), s->ippon,           0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), s->wazaari,         1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), s->yuko,            2, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("/"), 3, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), s->shido,           4, 1, 1, 1);
#else
    table = gtk_table_new(2, 6, TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new("I"), 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new("W"), 1, 2, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new("Y"), 2, 3, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new("/"), 3, 4, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new("S"), 4, 5, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), s->ippon, 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), s->wazaari, 1, 2, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), s->yuko, 2, 3, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), gtk_label_new("/"), 3, 4, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), s->shido, 4, 5, 1, 2);
#endif
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->ippon), (score>>16)&15);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->wazaari), (score>>12)&15);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->yuko), (score>>8)&15);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->shido), score&15);

#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       table, FALSE, FALSE, 0);
#else
    gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), table);
#endif
    gtk_widget_show_all(dialog);

    g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(set_score), s);
}

static void view_match_time_popup_menu(GtkWidget *treeview,
				       GdkEventButton *event,
				       guint category,
				       guint number,
				       guint tim)
{
    struct match_time *s = g_malloc0(sizeof *s);
    GtkWidget *dialog, *table;

    s->category = category;
    s->number = number;

    dialog = gtk_dialog_new_with_buttons (_("Set time"),
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          NULL);

    s->min = gtk_spin_button_new_with_range(0.0, 99.0, 1.0);
    s->sec = gtk_spin_button_new_with_range(0.0, 59.0, 1.0);

    table = gtk_grid_new();
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("Min"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), gtk_label_new("Sec"), 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(table), s->min,               0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(table), s->sec,               1, 1, 1, 1);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->min), tim/60);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(s->sec), tim%60);

    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       table, FALSE, FALSE, 0);
    gtk_widget_show_all(dialog);

    g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(set_time), s);
}


static gboolean view_onButtonPressed(GtkWidget *treeview,
                                     GdkEventButton *event,
                                     gpointer userdata)
{
    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_PRESS) {
        GtkTreePath *path;
        GtkTreeModel *model;
        GtkTreeIter iter;
        GtkTreeViewColumn *col, *blue_pts_col, *white_pts_col, *blue_score_col, *white_score_col;
        GtkTreeViewColumn *blue_col, *white_col, *time_col;
        guint category, number, blue, white, w, b, blue_score, white_score, tim;
        gboolean visible, handled = FALSE;
        guint category1, number1;

        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
                                          (gint) event->x,
                                          (gint) event->y,
                                          &path, &col, NULL, NULL)) {
            model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
            gtk_tree_model_get_iter(model, &iter, path);

            gtk_tree_model_get(model, &iter,
                               COL_MATCH_CAT, &category,
                               COL_MATCH_NUMBER, &number,
                               COL_MATCH_VIS, &visible,
                               COL_MATCH_BLUE_POINTS, &blue,
                               COL_MATCH_WHITE_POINTS, &white,
                               COL_MATCH_BLUE_SCORE, &blue_score,
                               COL_MATCH_WHITE_SCORE, &white_score,
                               COL_MATCH_BLUE, &b,
                               COL_MATCH_WHITE, &w,
                               COL_MATCH_TIME, &tim,
                               -1);
            category1 = category | (number & MATCH_CATEGORY_SUB_MASK);
            number1 = number & MATCH_CATEGORY_MASK;

            if (category == 0) {
                if (event->button == 3) {
                    view_match_popup_menu(treeview, event,
                                          category,
                                          number,
                                          visible);
                    return TRUE;
                }
                return FALSE;
            }

            current_category = category;

            blue_pts_col = gtk_tree_view_get_column (GTK_TREE_VIEW(treeview), COL_MATCH_BLUE_POINTS);
            white_pts_col = gtk_tree_view_get_column (GTK_TREE_VIEW(treeview), COL_MATCH_WHITE_POINTS);
            blue_score_col = gtk_tree_view_get_column (GTK_TREE_VIEW(treeview), COL_MATCH_BLUE_SCORE);
            white_score_col = gtk_tree_view_get_column (GTK_TREE_VIEW(treeview), COL_MATCH_WHITE_SCORE);
            blue_col = gtk_tree_view_get_column (GTK_TREE_VIEW(treeview), COL_MATCH_BLUE);
            white_col = gtk_tree_view_get_column (GTK_TREE_VIEW(treeview), COL_MATCH_WHITE);
            time_col = gtk_tree_view_get_column (GTK_TREE_VIEW(treeview), COL_MATCH_TIME);

            if (!visible && event->button == 3) {
                view_match_popup_menu(treeview, event,
                                      category1,
                                      number1,
                                      visible);
                handled = TRUE;
            } else if (visible && event->button == 3 && col == blue_col && (number & MATCH_CATEGORY_SUB_MASK)) {
                view_match_competitor_popup_menu(treeview, event,
                                                 category1,
                                                 number1,
                                                 TRUE);
                handled = TRUE;
            } else if (visible && event->button == 3 && col == white_col && (number & MATCH_CATEGORY_SUB_MASK)) {
                view_match_competitor_popup_menu(treeview, event,
                                                 category1,
                                                 number1,
                                                 FALSE);
                handled = TRUE;
            } else if (visible && event->button == 3 && col == blue_pts_col
                       /*&&
                         b != GHOST && w != GHOST*/) {
                view_match_points_popup_menu(treeview, event,
                                             category1,
                                             number1,
                                             TRUE);
                handled = TRUE;
            } else if (visible && event->button == 3 && col == white_pts_col
                       /*&&
                         b != GHOST && w != GHOST*/) {
                view_match_points_popup_menu(treeview, event,
                                             category1,
                                             number1,
                                             FALSE);
                handled = TRUE;
            } else if (visible && event->button == 3 && col == blue_score_col) {
                view_match_score_popup_menu(treeview, event,
                                            category1,
                                            number1,
                                            blue_score,
                                            TRUE);
                handled = TRUE;
            } else if (visible && event->button == 3 && col == white_score_col) {
                view_match_score_popup_menu(treeview, event,
                                            category1,
                                            number1,
                                            white_score,
                                            FALSE);
                handled = TRUE;
            } else if (visible && event->button == 3 && col == time_col) {
                view_match_time_popup_menu(treeview, event,
					   category1,
					   number1,
					   tim);
                handled = TRUE;
            } else if (visible && event->button == 3 && b != GHOST && w != GHOST) {
                view_next_match_popup_menu(treeview, event,
                                           category1,
                                           number1,
                                           visible);
                handled = TRUE;
            }

            gtk_tree_path_free(path);
        }
        return handled; /* we handled this */
    }
    return FALSE; /* we did not handle this */
}

static void view_onRowActivated (GtkTreeView        *treeview,
				 GtkTreePath        *path,
				 GtkTreeViewColumn  *col,
				 gpointer            userdata)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    gint cat;

    model = gtk_tree_view_get_model(treeview);
    if (gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_model_get(model, &iter,
                           COL_MATCH_CAT, &cat,
                           -1);

        if (cat < 10000)
            return;

        category_window(cat);
    }
}

static GtkTreeModel *create_and_fill_model (void)
{
    GtkTreeStore  *treestore;

    treestore = gtk_tree_store_new(NUM_MATCH_COLUMNS,
                                   G_TYPE_UINT, /* group */
                                   G_TYPE_UINT, /* category */
                                   G_TYPE_UINT, /* number */
                                   G_TYPE_UINT, /* blue */
                                   G_TYPE_UINT, /* blue score */
                                   G_TYPE_UINT, /* blue points */
                                   G_TYPE_UINT, /* white points */
                                   G_TYPE_UINT, /* white score */
                                   G_TYPE_UINT, /* white */
                                   G_TYPE_UINT, /* time */
                                   G_TYPE_UINT, /* comment */
                                   G_TYPE_BOOLEAN,/* visible */
                                   G_TYPE_UINT  /* status */
        );

    return GTK_TREE_MODEL(treestore);
}

static GtkWidget *create_view_and_model(void)
{
    GtkTreeViewColumn   *col;
    GtkCellRenderer     *renderer;
    GtkWidget           *view;
    GtkTreeModel        *model;
    GtkTreeSelection    *selection;
    gint col_offset;

    view = gtk_tree_view_new();

    /* --- Column group --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, _("Group"),
                                                             renderer, "text",
                                                             COL_MATCH_GROUP,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            group_cell_data_func,
                                            (gpointer)COL_MATCH_GROUP, NULL);

    /* --- Column category --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "weight", PANGO_WEIGHT_BOLD,
                 "weight-set", TRUE,
                 "xalign", 0.0,
                 NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, _("Category"),
                                                             renderer, "text",
                                                             COL_MATCH_CAT,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            category_cell_data_func,
                                            (gpointer)COL_MATCH_CAT, NULL);
    gtk_tree_view_column_set_sort_column_id(GTK_TREE_VIEW_COLUMN(col), COL_MATCH_CAT);

    /* --- Column number --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, _("Match"),
                                                             renderer, "text",
                                                             COL_MATCH_NUMBER,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            number_cell_data_func,
                                            NULL, NULL);

    /* --- Column blue --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, prop_get_int_val(PROP_WHITE_FIRST) ? _("White") : _("Blue"),
                                                             renderer, "text",
                                                             COL_MATCH_BLUE,
                                                             /*"visible",
                                                               COL_MATCH_VIS,*/
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            name_cell_data_func,
                                            (gpointer)COL_MATCH_BLUE, NULL);

    /* --- Column blue score --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.5, NULL);
    g_object_set(renderer, "editable", TRUE, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, "IWY/S",
                                                             renderer, "text",
                                                             COL_MATCH_BLUE_SCORE,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            score_cell_data_func,
                                            (gpointer)COL_MATCH_BLUE_SCORE, NULL);

    /* --- Column blue points --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, _("Points"),
                                                             renderer, "text",
                                                             COL_MATCH_BLUE_POINTS,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);

    /* --- Column white points --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.5, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, _("Points"),
                                                             renderer, "text",
                                                             COL_MATCH_WHITE_POINTS,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);

    /* --- Column white score --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.5, NULL);
    g_object_set(renderer, "editable", TRUE, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, "IWY/S",
                                                             renderer, "text",
                                                             COL_MATCH_WHITE_SCORE,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            score_cell_data_func,
                                            (gpointer)COL_MATCH_WHITE_SCORE, NULL);

    /* --- Column white --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, prop_get_int_val(PROP_WHITE_FIRST) ? _("Blue") : _("White"),
                                                             renderer, "text",
                                                             COL_MATCH_WHITE,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            name_cell_data_func,
                                            (gpointer)COL_MATCH_WHITE, NULL);

    /* --- Column time --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, _("Time"),
                                                             renderer, "text",
                                                             COL_MATCH_TIME,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            time_cell_data_func,
                                            (gpointer)COL_MATCH_TIME, NULL);

    /* --- Column comment --- */

    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "xalign", 0.0, NULL);
    col_offset = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
                                                             -1, _("Comment"),
                                                             renderer, "text",
                                                             COL_MATCH_COMMENT,
                                                             "visible",
                                                             COL_MATCH_VIS,
                                                             NULL);
    col = gtk_tree_view_get_column (GTK_TREE_VIEW (view), col_offset - 1);
    gtk_tree_view_column_set_cell_data_func(col, renderer,
                                            comment_cell_data_func,
                                            (gpointer)COL_MATCH_COMMENT, NULL);

    /*****/

    model = create_and_fill_model();

    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (view), TRUE);

    g_object_unref(model); /* destroy model automatically with view */

    g_signal_connect(view, "row-activated", (GCallback) view_onRowActivated, NULL);
    g_signal_connect(view, "button-press-event", (GCallback) view_onButtonPressed, NULL);
    //g_signal_connect(view, "popup-menu", (GCallback) view_onPopupMenu, NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_NONE);

    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model),
                                    COL_MATCH_CAT,
                                    sort_iter_compare_func_1,
                                    GINT_TO_POINTER(COL_MATCH_CAT),
                                    NULL);

    return view;
}

static GtkWidget *match_pages[NUM_TATAMIS];

void update_match_pages_visibility(void)
{
    gint i;
    for (i = 0; i < NUM_TATAMIS; i++) {
        if (match_pages[i] == NULL)
            continue;

        if (i < number_of_tatamis)
            gtk_widget_show(GTK_WIDGET(match_pages[i]));
        else
            gtk_widget_hide(GTK_WIDGET(match_pages[i]));
    }
}

void set_match_pages(GtkWidget *notebook)
{
    GtkWidget *scrolled_window;
    GtkWidget *label;
    GtkWidget *view;
    GtkWidget *match1;
    GtkWidget *match2;
    GtkWidget *vbox;

    int        i;
    char       buffer[20];

    for (i = 0; i < 10000; i++)
        competitor_positions[i] = 0;

    for (i = 0; i < NUM_TATAMIS; i++) {
#if (GTKVER == 3)
        match_pages[i] = vbox = gtk_grid_new();
#else
        match_pages[i] = vbox = gtk_vbox_new(FALSE, 1);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 1);
#endif
        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 10);

        match_view[i] = view = create_view_and_model();

#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
        gtk_container_add(GTK_CONTAINER(scrolled_window), view);
#else
        gtk_scrolled_window_add_with_viewport(
            GTK_SCROLLED_WINDOW(scrolled_window), view);
#endif
        sprintf(buffer, "Tatami %d", i+1);
        label = gtk_label_new (buffer);

        next_match[i][0] = match1 = gtk_label_new(_("Match:"));
        next_match[i][1] = match2 = gtk_label_new(_("Preparing:"));

        gtk_misc_set_alignment(GTK_MISC(match1), 0.0, 0.0);
        gtk_misc_set_alignment(GTK_MISC(match2), 0.0, 0.0);

#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(vbox), match1, 0, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(vbox), match2, 0, 1, 1, 1);
        gtk_grid_attach(GTK_GRID(vbox), scrolled_window, 0, 2, 1, 1);
        gtk_widget_set_hexpand(scrolled_window, TRUE);
        gtk_widget_set_vexpand(scrolled_window, TRUE);
#else
        gtk_box_pack_start(GTK_BOX(vbox), match1, FALSE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), match2, FALSE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
#endif
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox, label);
    }

    db_read_matches();

    for (i = 0; i <= NUM_TATAMIS; i++) {
        struct category_data *catdata = category_queue[i].next;

        while (catdata) {
            struct compsys sys = db_get_system(catdata->index);

            switch (sys.system) {
            case SYSTEM_POOL:
            case SYSTEM_DPOOL:
            case SYSTEM_DPOOL2:
            case SYSTEM_DPOOL3:
            case SYSTEM_QPOOL:
            case SYSTEM_BEST_OF_3:
                update_pool_matches(catdata->index, sys.numcomp);
                break;
            case SYSTEM_FRENCH_8:
            case SYSTEM_FRENCH_16:
            case SYSTEM_FRENCH_32:
            case SYSTEM_FRENCH_64:
            case SYSTEM_FRENCH_128:
                update_french_matches(catdata->index, sys);
                break;
            }

            catdata = catdata->next;
        }

        if (i)
            update_matches(0, (struct compsys){0,0,0,0}, i);
        /******
               g_static_mutex_lock(&next_match_mutex);
               nm = db_next_match(0, i+1);
               gint n = nm[0].number;
               g_static_mutex_unlock(&next_match_mutex);
               if (n < 1000) {
               gint sys = db_get_system(nm[0].category);
               update_matches(nm[0].category, sys);
               }
        ****/
    }
}

static GtkTreeIter found_iter;
static gboolean    iter_found;

static gboolean traverse_rows(GtkTreeModel *model,
			      GtkTreePath *path,
			      GtkTreeIter *iter,
			      gpointer data)
{
    struct matchid *id = data;
    gint category, number, group;

    gtk_tree_model_get(model, iter,
                       COL_MATCH_GROUP,  &group,
                       COL_MATCH_CAT,    &category,
                       COL_MATCH_NUMBER, &number,
                       -1);
    if (id->category == category &&
        id->number == number &&
        (id->group == group || id->group == 0)) {
        iter_found = TRUE;
        found_iter = *iter;
        return TRUE;
    }
    return FALSE;
}

static gboolean find_match_iter(GtkTreeModel *model,
                                GtkTreeIter *iter,
                                gint group,
                                gint category,
                                gint number)
{
    struct matchid id;

    id.group = group;
    id.category = category;
    id.number = number;
    iter_found = FALSE;
    gtk_tree_model_foreach(model, traverse_rows, (gpointer)&id);
    if (iter_found) {
        *iter = found_iter;
        return TRUE;
    }
    return FALSE;
}

static gboolean find_group_iter(GtkTreeModel *model,
                                GtkTreeIter *iter,
                                gint group)
{
    gboolean ok;
    gint grp;

    ok = gtk_tree_model_get_iter_first(model, iter);
    while (ok) {
        gtk_tree_model_get(model, iter,
                           COL_MATCH_GROUP, &grp,
                           -1);
        if (grp == group)
            return TRUE;

        ok = gtk_tree_model_iter_next(model, iter);
    }

    return FALSE;
}

#if (GTKVER == 3)
G_LOCK_DEFINE_STATIC(set_match_mutex);
#else
static GStaticMutex set_match_mutex = G_STATIC_MUTEX_INIT;
#endif

void set_match(struct match *m)
{
    gint i, tatami, group;
    GtkTreeIter iter, cat, grp_iter, gi, tmp_iter, ic;
    GtkTreeModel *model;
    gboolean ok, iter_set = FALSE;
    gint grp = -1, num = -1;
    struct match m1 = *m;

    set_competitor_position_drawn(m->blue);
    set_competitor_position_drawn(m->white);

#if (GTKVER == 3)
    G_LOCK(set_match_mutex);
#else
    g_static_mutex_lock(&set_match_mutex);
#endif

    struct category_data *c = avl_get_category(m->category & MATCH_CATEGORY_MASK);
    if (c && (c->deleted & TEAM_EVENT)) { // team event
        if (m->category & MATCH_CATEGORY_SUB_MASK) {
            m1.category = m->category & MATCH_CATEGORY_MASK;
            m1.number = m->number | (m->category & MATCH_CATEGORY_SUB_MASK);
        } else {
#if (GTKVER == 3)
            G_UNLOCK(set_match_mutex);
#else
            g_static_mutex_unlock(&set_match_mutex);
#endif
            return;
        }
    }
    m = &m1;

    /* Find info about category */
    if (find_iter(&cat, m->category) == FALSE) {
        g_print("CANNOT FIND CAT %d (num=%d)\n", m->category, m->number);
#if (GTKVER == 3)
        G_UNLOCK(set_match_mutex);
#else
        g_static_mutex_unlock(&set_match_mutex);
#endif
        //assert(0);
        return; /* error */
    }

    /* Match found. Look for tatami number. */
    gtk_tree_model_get(current_model, &cat,
                       COL_BELT, &tatami,
                       COL_BIRTHYEAR, &group,
                       -1);

    for (i = 0; i < NUM_TATAMIS; i++) {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(match_view[i]));

        if (find_match_iter(model,
                            &iter,
                            0,
                            m->category,
                            m->number)) {
            guint old_group;

            gtk_tree_model_get(model, &iter,
                               COL_MATCH_GROUP, &old_group,
                               -1);

            if (tatami != (i+1) || group != old_group) {
                GtkTreeIter parent, grandparent;
                gint num_categories, num_matches;

                /* Changed. */
                gtk_tree_model_iter_parent(model, &parent, &iter);
                gtk_tree_model_iter_parent(model, &grandparent, &parent);
                num_matches = gtk_tree_model_iter_n_children(model, &parent);
                num_categories = gtk_tree_model_iter_n_children(model, &grandparent);
                if (num_categories == 1 && num_matches == 1)
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &grandparent);
                else if (num_matches == 1)
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &parent);
                else
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
            } else {
                /* Same tatami. Update information. */
                goto update;
            }

            break;
        }
    }

    if (tatami == 0) {
        //g_print("TATAMI NOT SET\n");
#if (GTKVER == 3)
        G_UNLOCK(set_match_mutex);
#else
        g_static_mutex_unlock(&set_match_mutex);
#endif
        return; /* tatami not set */
    }

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(match_view[tatami-1]));

    if (find_group_iter(model, &grp_iter, group) == FALSE) {
        iter_set = FALSE;
        ok = gtk_tree_model_get_iter_first(model, &gi);
        while (ok) {
            gtk_tree_model_get(model, &gi,
                               COL_MATCH_GROUP, &grp,
                               -1);
            if (grp > group) {
                gtk_tree_store_insert_before((GtkTreeStore *)model,
                                             &grp_iter,
                                             NULL,
                                             &gi);
                iter_set = TRUE;
                break;
            }
            ok = gtk_tree_model_iter_next(model, &gi);
        }
        if (iter_set == FALSE)
            gtk_tree_store_append((GtkTreeStore *)model,
                                  &grp_iter, NULL);

        gtk_tree_store_set((GtkTreeStore *)model, &grp_iter,
                           COL_MATCH_GROUP,        group,
                           COL_MATCH_CAT,          0,
                           COL_MATCH_NUMBER,       0,
                           COL_MATCH_BLUE,         0,
                           COL_MATCH_BLUE_SCORE,   0,
                           COL_MATCH_BLUE_POINTS,  0,
                           COL_MATCH_WHITE_POINTS, 0,
                           COL_MATCH_WHITE_SCORE,  0,
                           COL_MATCH_WHITE,        0,
                           COL_MATCH_TIME,         0,
                           COL_MATCH_COMMENT,      0,
                           COL_MATCH_VIS,          FALSE,
                           COL_MATCH_STATUS,       0,
                           -1);
    }

    if (find_match_iter(model, &cat, group, m->category, 0) == FALSE) {
        iter_set = FALSE;
        ok = gtk_tree_model_iter_children(model, &ic, &grp_iter);
        while (ok) {
            gint colcat;

            gtk_tree_model_get(model, &ic,
                               COL_MATCH_CAT, &colcat,
                               -1);
            if (colcat > m->category) {
                gtk_tree_store_insert_before((GtkTreeStore *)model,
                                             &cat,
                                             NULL,
                                             &ic);
                iter_set = TRUE;
                break;
            }
            ok = gtk_tree_model_iter_next(model, &ic);
        }
        if (iter_set == FALSE)
            gtk_tree_store_append((GtkTreeStore *)model,
                                  &cat, &grp_iter);

        gtk_tree_store_set((GtkTreeStore *)model, &cat,
                           COL_MATCH_GROUP,        group,
                           COL_MATCH_CAT,          m->category,
                           COL_MATCH_NUMBER,       0,
                           COL_MATCH_BLUE,         0,
                           COL_MATCH_BLUE_SCORE,   0,
                           COL_MATCH_BLUE_POINTS,  0,
                           COL_MATCH_WHITE_POINTS, 0,
                           COL_MATCH_WHITE_SCORE,  0,
                           COL_MATCH_WHITE,        0,
                           COL_MATCH_TIME,         0,
                           COL_MATCH_COMMENT,      0,
                           COL_MATCH_VIS,          FALSE,
                           COL_MATCH_STATUS,       0,
                           -1);
    }

    iter_set = FALSE;
    ok = gtk_tree_model_iter_children(model, &tmp_iter, &cat);
    while (ok) {
        gtk_tree_model_get(model, &tmp_iter,
                           COL_MATCH_NUMBER, &num,
                           -1);
        if (num > m->number) {
            gtk_tree_store_insert_before((GtkTreeStore *)model,
                                         &iter,
                                         NULL,
                                         &tmp_iter);
            iter_set = TRUE;
            break;
        }
        ok = gtk_tree_model_iter_next(model, &tmp_iter);
    }
    if (iter_set == FALSE)
        gtk_tree_store_append((GtkTreeStore *)model,
                              &iter, &cat);
update:
    gtk_tree_store_set((GtkTreeStore *)model, &iter,
                       COL_MATCH_GROUP,        group,
                       COL_MATCH_CAT,          m->category,
                       COL_MATCH_NUMBER,       m->number,
                       COL_MATCH_BLUE,         m->blue,
                       COL_MATCH_BLUE_SCORE,   m->blue_score,
                       COL_MATCH_BLUE_POINTS,  m->blue_points,
                       COL_MATCH_WHITE_POINTS, m->white_points,
                       COL_MATCH_WHITE_SCORE,  m->white_score,
                       COL_MATCH_WHITE,        m->white,
                       COL_MATCH_TIME,         m->match_time,
                       COL_MATCH_COMMENT,      m->comment,
                       COL_MATCH_VIS,          TRUE,
                       COL_MATCH_STATUS,       m->forcedtatami,
                       -1);

#if (GTKVER == 3)
    G_UNLOCK(set_match_mutex);
#else
    g_static_mutex_unlock(&set_match_mutex);
#endif
}

static gint remove_category_from_matches(gint category)
{
    GtkTreeIter grp_iter, cat_iter;
    GtkTreeModel *model;
    gboolean ok_grp, ok_cat;
    gint cat, i;

    /* find current placement */
    for (i = 0; i < NUM_TATAMIS; i++) {
        model = gtk_tree_view_get_model(GTK_TREE_VIEW(match_view[i]));

        /* go through groups */
        ok_grp = gtk_tree_model_get_iter_first(model, &grp_iter);
        while (ok_grp) {
            /* go through categories */
            ok_cat = gtk_tree_model_iter_children(model, &cat_iter, &grp_iter);
            while (ok_cat) {
                gtk_tree_model_get(model, &cat_iter,
                                   COL_MATCH_CAT, &cat,
                                   -1);
                if (cat == category) {
                    gtk_tree_store_remove(GTK_TREE_STORE(model), &cat_iter);
                    if (gtk_tree_model_iter_n_children(model, &grp_iter) == 0)
                        gtk_tree_store_remove(GTK_TREE_STORE(model), &grp_iter);
                    return i+1; /* return tatami number */
                }

                ok_cat = gtk_tree_model_iter_next(model, &cat_iter);
            }

            ok_grp = gtk_tree_model_iter_next(model, &grp_iter);
        }

    }

    //g_print("Error: %s %d (cannot remove %d)\n", __FUNCTION__, __LINE__, category);
    return 0;
}

void category_refresh(gint category)
{
    gint tatami, group, old_tatami;
    GtkTreeIter cat_iter;
    struct compsys sys;

#if (GTKVER == 3)
    G_LOCK(set_match_mutex);
#else
    g_static_mutex_lock(&set_match_mutex);
#endif
    old_tatami = remove_category_from_matches(category);
#if (GTKVER == 3)
    G_UNLOCK(set_match_mutex);
#else
    g_static_mutex_unlock(&set_match_mutex);
#endif
    /* Find info about category */
    if (find_iter(&cat_iter, category) == FALSE) {
        g_print("Error: %s %d (cat %d)\n", __FUNCTION__, __LINE__, category);
        return; /* error */
    }

    /* Match found. Look for tatami number. */
    gtk_tree_model_get(current_model, &cat_iter,
                       COL_BELT, &tatami,
                       COL_BIRTHYEAR, &group,
                       -1);
#if 0
    struct match m;
    memset(&m, 0, sizeof(m));
    m.category = category;
    m.group = group;
    m.tatami = tatami;
    set_match(&m);
#endif
    db_read_matches_of_category(category);
    sys = db_get_system(category);
    update_matches(category, sys, 0);

    if (old_tatami && old_tatami != tatami) {
        update_matches(0, (struct compsys){0,0,0,0}, old_tatami);
        /**********
                   g_static_mutex_lock(&next_match_mutex);
                   struct match *nm = db_next_match(0, old_tatami);
                   gint n = nm[0].number;
                   gint cat = nm[0].category;
                   g_static_mutex_unlock(&next_match_mutex);
                   if (n < 1000) {
                   gint sys = db_get_system(cat);
                   update_matches(cat, sys);
                   }
        *************/
    }

    draw_gategory_graph();
    draw_match_graph();
}

void gategories_refresh(void)
{
#if 0
    GtkTreeIter cat_iter;
    gboolean ok_cat;

    ok_cat = gtk_tree_model_get_iter_first(current_model, &cat_iter);
    while (ok_cat) {
        gboolean visible;
        gint     index;

        gtk_tree_model_get(current_model, &cat_iter,
                           COL_MATCH_INDEX,   &index,
                           COL_MATCH_VIS, &visible,
                           -1);

        if (!visible) {
            g_print("refresh cat %d\n", index);
            category_refresh(index);
        }

        ok_cat = gtk_tree_model_iter_next(current_model, &cat_iter);
    }
#endif
}

void matches_refresh(void)
{
    gint i;

    clear_competitor_positions();

    for (i = 0; i < NUM_TATAMIS; i++)
        gtk_tree_store_clear(GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(match_view[i]))));

    db_read_matches();

    for (i = 0; i < NUM_TATAMIS; i++) {
        update_matches(0, (struct compsys){0,0,0,0}, i+1);
        /********************
		g_static_mutex_lock(&next_match_mutex);
                struct match *nm = db_next_match(0, i+1);
		gint n = nm[0].number;
		g_static_mutex_unlock(&next_match_mutex);
                if (n < 1000) {
                        gint sys = db_get_system(nm[0].category);
                        update_matches(nm[0].category, sys);
                } else {
			struct message msg;
			gint tatami = i+1;

			NULL_MATCH_INFO(category, tatami, 0);
			NULL_MATCH_INFO(blue_first, tatami, 0);
			NULL_MATCH_INFO(blue_last, tatami, 0);
			NULL_MATCH_INFO(blue_club, tatami, 0);
			NULL_MATCH_INFO(white_first, tatami, 0);
			NULL_MATCH_INFO(white_last, tatami, 0);
			NULL_MATCH_INFO(white_club, tatami, 0);
			next_matches_info[tatami-1][0].catnum = 0;
			NULL_MATCH_INFO(category, tatami, 1);
			NULL_MATCH_INFO(blue_first, tatami, 1);
			NULL_MATCH_INFO(blue_last, tatami, 1);
			NULL_MATCH_INFO(blue_club, tatami, 1);
			NULL_MATCH_INFO(white_first, tatami, 1);
			NULL_MATCH_INFO(white_last, tatami, 1);
			NULL_MATCH_INFO(white_club, tatami, 1);
			next_matches_info[tatami-1][1].catnum = 0;

			memset(&msg, 0, sizeof(msg));
			msg.type = MSG_NEXT_MATCH;
			msg.u.next_match.tatami = tatami;
			send_packet(&msg);
		}
        ************/
    }

    refresh_sheet_display(TRUE);
}

void matches_refresh_1(gint category, struct compsys sys)
{
    db_read_matches();
    update_matches(category, sys, 0);
    refresh_sheet_display(TRUE);
}

void matches_clear(void)
{

}

void get_match_data(GtkTreeModel *model, GtkTreeIter *iter, gint *group, gint *category, gint *number)
{
    gtk_tree_model_get(model, iter,
                       COL_MATCH_GROUP,  group,
                       COL_MATCH_CAT,    category,
                       COL_MATCH_NUMBER, number,
                       -1);
}

void set_match_col_titles(void)
{
    GtkTreeViewColumn *col;
    gint i;

    for (i = 0; i < NUM_TATAMIS; i++) {
        if (!match_view[i])
            continue;

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_GROUP);
        gtk_tree_view_column_set_title(col, _("Group"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_CAT);
        gtk_tree_view_column_set_title(col, _("Category"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_NUMBER);
        gtk_tree_view_column_set_title(col, _("Match"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_BLUE);
        gtk_tree_view_column_set_title(col, prop_get_int_val(PROP_WHITE_FIRST) ? _("White") : _("Blue"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_BLUE_POINTS);
        gtk_tree_view_column_set_title(col, _("Points"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_WHITE_POINTS);
        gtk_tree_view_column_set_title(col, _("Points"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_WHITE);
        gtk_tree_view_column_set_title(col, prop_get_int_val(PROP_WHITE_FIRST) ? _("Blue") : _("White"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_TIME);
        gtk_tree_view_column_set_title(col, _("Time"));

        col = gtk_tree_view_get_column(GTK_TREE_VIEW(match_view[i]), COL_MATCH_COMMENT);
        gtk_tree_view_column_set_title(col, _("Comment"));
    }
}
