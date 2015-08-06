/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include "sqlite3.h"
#include "judoshiai.h"

#define switch_comp(_a, _b) do {                \
        struct comp_s tmp = comp[_a];           \
        comp[_a] = comp[_b];                    \
        comp[_b] = tmp;                         \
    } while (0)

enum draw_dialog_buttons {
    BUTTON_NEXT = 1000,
    BUTTON_REST,
    BUTTON_SHOW,
    BUTTON_ADD
};

guint french_matches_blue[64] = {
    1, 33, 17, 49,  9, 41, 25, 57,  5, 37, 21, 53, 13, 45, 29, 61, 3, 35, 19, 51, 11, 43, 27, 59, 7, 39, 23, 55, 15, 47, 31, 63,
    2, 34, 18, 50, 10, 42, 26, 58,  6, 38, 22, 54, 14, 46, 30, 62, 4, 36, 20, 52, 12, 44, 28, 60, 8, 40, 24, 56, 16, 48, 32, 64
};
guint french_matches_white_offset[NUM_FRENCH] = {4, 8, 16, 32, 64};
guint french_mul[NUM_FRENCH] = {16, 8, 4, 2, 1};

#define MAX_MATES 100000

struct mdata {
    struct mcomp {
        gint index;
        gint pos;
        gint seeded;
        gint clubseeded;
        gint num_mates;
        gchar *club;
        gchar *country;
        GtkWidget *eventbox, *label;
	gchar distance[NUM_COMPETITORS+1];
    } mcomp[NUM_COMPETITORS+1];

    struct mpos {
        gint judoka;
        GtkWidget *eventbox, *label;
    } mpos[NUM_COMPETITORS+1];

    gint mjudokas, mpositions, mcategory_ix;
    gint selected, mfrench_sys;
    gboolean hidden, edit;
    gint drawn, seeded1, seeded2, seeded3;
    struct compsys sys;
    struct custom_data *custom_table;
    guint64 mask;
};

GtkWidget *draw_one_category_manually_1(GtkTreeIter *parent, gint competitors,
                                        struct mdata *mdata);
static void make_manual_matches_callback(GtkWidget *widget,
                                         GdkEvent *event,
                                         void *data);
static gboolean select_competitor(GtkWidget *eventbox, GdkEventButton *event, void *param);
static gint get_competitor_number(gint pos, struct mdata *mdata);
static gint get_rr_by_comp(struct custom_data *ct, gint cnum);

#if 0
static gint get_free_pos(gint pos[], gint num_competitors, gint group)
{
    guint p = rand()%32;
    gint i, j, pos1 = group;
    while (p > 0) {
        pos1 += 4;
        if (pos1 >= num_competitors)
            pos1 = group;
        p--;
    }

    for (i = 0; i < 4; i++) {
        gint pos2 = pos1 + i;
        if (pos2 >= num_competitors)
            pos2 = (group+i)%4;
        for (j = 0; j < 16; j++) {
            if (pos[pos2] == -1)
                return pos2;
            pos2 += 4;
            if (pos2 >= num_competitors)
                pos2 = (group+i)%4;
        }
    }
    return -1;
}
#endif

static gint cmp_clubs(struct mcomp *m1, struct mcomp *m2)
{
    gint result = 0;

    // club name is the same
    if (m1->club && m2->club &&
	m1->club[0] && m2->club[0] &&
	strcmp(m1->club, m2->club) == 0)
	result |= CLUB_TEXT_CLUB;

    // country name is the same
    if (m1->country && m2->country &&
	m1->country[0] && m2->country[0] &&
	strcmp(m1->country, m2->country) == 0)
	result |= CLUB_TEXT_COUNTRY;

    return result;
}

void draw_one_category(GtkTreeIter *parent, gint competitors)
{
    struct mdata *mdata;
    GtkWidget *dialog;

    mdata = malloc(sizeof(*mdata));
    memset(mdata, 0, sizeof(*mdata));
    mdata->hidden = TRUE;

    dialog = draw_one_category_manually_1(parent, competitors, mdata);

    if (dialog) {
        make_manual_matches_callback(dialog, (gpointer)BUTTON_REST, mdata);
        make_manual_matches_callback(dialog, (gpointer)GTK_RESPONSE_OK, mdata);
    }
}

void draw_one_category_manually(GtkTreeIter *parent, gint competitors)
{
    struct mdata *mdata;

    mdata = malloc(sizeof(*mdata));
    memset(mdata, 0, sizeof(*mdata));

    draw_one_category_manually_1(parent, competitors, mdata);
}

void edit_drawing(GtkTreeIter *parent, gint competitors)
{
    struct mdata *mdata;

    mdata = malloc(sizeof(*mdata));
    memset(mdata, 0, sizeof(*mdata));
    mdata->edit = TRUE;

    draw_one_category_manually_1(parent, competitors, mdata);
}

static void free_mdata(struct mdata *mdata)
{
    gint i;
    for (i = 1; i <= NUM_COMPETITORS; i++) {
        g_free(mdata->mcomp[i].club);
        g_free(mdata->mcomp[i].country);
    }
    free(mdata);
}

static gint last_selected = 0;

/* Competitors are drawed in the following order:
 * - seeded.
 * - their clubmates.
 * - competitors from the biggest club.
 * - competitors from the same country.
 * - competitors from the 2nd biggest club.
 * - competitors from the same country.
 * - etc.
 */
static gint get_next_comp(struct mdata *mdata)
{
    gint i, seed, x = (rand()%mdata->mjudokas) + 1;
    gint highest_val = -1, highest_num = 0, same_club = 0, same_country = 0;
    gint club_seeding = 0, same_club_seeding = 0, same_country_seeding = 0;

    // find seeded in order
    for (seed = 1; seed <= NUM_SEEDED; seed++) {
        for (i = 0; i < mdata->mjudokas; i++) {
            if (mdata->mcomp[x].pos == 0 &&
                mdata->mcomp[x].index &&
                mdata->mcomp[x].seeded == seed)
                return x;

            if (++x > mdata->mjudokas)
                x = 1;
        }
    }

    // find ordinary competitors
    // find from the same club as previous
    if (last_selected) {
        for (i = 1; i <= mdata->mjudokas; i++) {
            if (mdata->mcomp[x].pos == 0 && mdata->mcomp[x].index) {
                    gint c = cmp_clubs(&mdata->mcomp[x], &mdata->mcomp[last_selected]);
                    if (c & CLUB_TEXT_CLUB)
                        if ((same_club_seeding && mdata->mcomp[x].clubseeded &&
                             same_club_seeding > mdata->mcomp[x].clubseeded) ||
                            (same_club_seeding == 0)) {
                            same_club_seeding = mdata->mcomp[x].clubseeded;
                            same_club = x;
                        }
                    if (c & CLUB_TEXT_COUNTRY)
                        if ((same_country_seeding && mdata->mcomp[x].clubseeded &&
                             same_country_seeding > mdata->mcomp[x].clubseeded) ||
                            (same_country_seeding == 0)) {
                            same_country_seeding = mdata->mcomp[x].clubseeded;
                            same_country = x;
                        }
            }

            if (++x > mdata->mjudokas)
                x = 1;
        }
    }

    if (same_club) {
        last_selected = same_club;
        return same_club;
    }
    if (same_country) {
        last_selected = same_country;
        return same_country;
    }

    // find from the rest
    for (i = 0; i < mdata->mjudokas; i++) {
	if (mdata->mcomp[x].pos == 0 && mdata->mcomp[x].index) {
            // not yet drawed
	    if (mdata->mcomp[x].num_mates >= highest_val) {
                if (mdata->mcomp[x].num_mates > highest_val) {
                    // select from club who has the most competitors
                    highest_val = mdata->mcomp[x].num_mates;
                    highest_num = x;
                    club_seeding = mdata->mcomp[x].clubseeded;
                }

                if ((club_seeding && mdata->mcomp[x].clubseeded && club_seeding > mdata->mcomp[x].clubseeded) ||
                    (club_seeding == 0 && mdata->mcomp[x].clubseeded)) {
                    club_seeding = mdata->mcomp[x].clubseeded;
                    highest_num = x;
                }
	    }
	}
	if (++x > mdata->mjudokas)
	    x = 1;
    }

    last_selected = highest_num;

    return last_selected;
}

static gint get_section(gint num, struct mdata *mdata)
{
    gint i, sec_siz = mdata->mpositions/NUM_SEEDED;
    gint sec_max = sec_siz;

    // for pool & double pool section is competitor number
    if (mdata->mfrench_sys == POOL_SYS)
        return (num - 1);

    // for french system section is A1, A2, B1, B2, C1, C2, D1 or D2.
    for (i = 0; i < NUM_SEEDED; i++) {
        if (num <= sec_max)
            return i;
        sec_max += sec_siz;
    }
    //g_print("%s:%d: num=%d\n", __FUNCTION__, __LINE__, num);
    return 0;
}

static gint club_mate_in_range(gint start, gint len, struct mdata *mdata)
{
    gint i, result = 0, cmp;

    for (i = start; i < start+len; i++) {
        gint other = mdata->mpos[i].judoka;
        if (other == 0)
            continue;

        if ((cmp = cmp_clubs(&mdata->mcomp[mdata->selected], &mdata->mcomp[other]))) {
            gint val = 0;

	    if (cmp & CLUB_TEXT_CLUB)
		val++;
	    if (cmp & CLUB_TEXT_COUNTRY)
		val++;
            if (mdata->mcomp[other].seeded)
                val++;

            if (val > result)
                result = val;
        }
    }
    return result;
}

static void calc_place_values(gint *place_values, struct mdata *mdata)
{
    gint num, cnt, x, j, s;
    gint q1 = 0, q2 = 0, q3 = 0, q4 = 0;

    switch (mdata->mfrench_sys) {
    case FRENCH_8:   num = 8;  break;
    case FRENCH_16:  num = 16; break;
    case FRENCH_32:  num = 32; break;
    case FRENCH_64:  num = 64; break;
#if (NUM_COMPETITORS > 64)
    case FRENCH_128: num = 128; break;
#endif
    case CUST_SYS: num = mdata->mpositions; break;
    default: return;
    }

    if (mdata->mfrench_sys != CUST_SYS) {
	// add one to place value if fight has the other competitor
	// add also a seeding based number if the competitor is seeded
	for (x = 1; x <= mdata->mpositions-1; x += 2) {
	    if (mdata->mpos[x].judoka) {
		place_values[x+1]++;
		if (mdata->mcomp[mdata->mpos[x].judoka].seeded)
		    place_values[x+1] += NUM_SEEDED + 1 - mdata->mcomp[mdata->mpos[x].judoka].seeded;
	    }
	    if (mdata->mpos[x+1].judoka) {
		place_values[x]++;
		if (mdata->mcomp[mdata->mpos[x+1].judoka].seeded)
		    place_values[x] += NUM_SEEDED + 1 - mdata->mcomp[mdata->mpos[x+1].judoka].seeded;
	    }
	}
	// calculate number of competitors in each quarter
	for (x = 1; x <= mdata->mpositions; x++) {
	    if (mdata->mpos[x].judoka == 0)
		continue;

	    if (x <= mdata->mpositions/4)
		q1++;
	    else if (x <= mdata->mpositions/2)
		q2++;
	    else if (x <= mdata->mpositions*3/4)
		q3++;
	    else
		q4++;
	}

	// balance halfs
	if (q1 + q2 > q3 + q4) {
	    for (x = 1; x <= mdata->mpositions/2; x++)
		place_values[x] += 2;
	} else if (q1 + q2 < q3 + q4) {
	    for (x = mdata->mpositions/2 + 1; x <= mdata->mpositions; x++)
		place_values[x] += 2;
	}

	// balance 1st and 2nd quarters
	if (q1 > q2) {
	    for (x = 1; x <= mdata->mpositions/4; x++)
		place_values[x]++;
	} else if (q1 < q2) {
	    for (x = mdata->mpositions/4 + 1; x <= mdata->mpositions/2; x++)
		place_values[x]++;
	}

	// balance 3rd and 4th quarters
	if (q3 > q4) {
	    for (x = mdata->mpositions/2 + 1; x <= mdata->mpositions*3/4; x++)
		place_values[x]++;
	} else if (q3 < q4) {
	    for (x = mdata->mpositions*3/4 + 1; x <= mdata->mpositions; x++)
		place_values[x]++;
	}

	cnt = 2;
	while (cnt < num) {
	    for (x = 1; x <= mdata->mpositions; x += cnt) {
		if ((s = club_mate_in_range(x, cnt, mdata))) {
		    for (j = x; j < x+cnt; j++)
			place_values[j] += 4*s;
		}
	    }
	    cnt *= 2;
	}
    } else {
	// cust sys
	for (x = 1; x <= mdata->mpositions; x++) {
	    gint cmp, y;

	    for (y = 1; y <= mdata->mpositions; y++) {
		if (x == y)
		    continue;

		if (mdata->mpos[x].judoka)
		    continue;

		gint other = mdata->mpos[y].judoka;
		if (other == 0)
		    continue;

		if ((cmp = cmp_clubs(&mdata->mcomp[mdata->selected], &mdata->mcomp[other]))) {
		    gint val = 0;

		    if (cmp & CLUB_TEXT_CLUB)
			val++;
		    if (cmp & CLUB_TEXT_COUNTRY)
			val++;
		    if (mdata->mcomp[other].seeded)
			val++;

		    gint dist = mdata->mcomp[x].distance[y];
		    if (dist > 0)
			place_values[x] += val*16/dist;
		    else
			place_values[x] += val*32;
		}
	    }
	}
    } // cust sys
    /***
    g_print("\nplace values: ");
    for (x = 1; x <= mdata->mpositions; x++)
        g_print("%d=%d ", x, place_values[x]);
    g_print("\n");
    ***/
}

static gint get_free_pos_by_mask(gint mask, struct mdata *mdata)
{
    gint place_values[NUM_COMPETITORS+1], selected[NUM_COMPETITORS];
    gint i, x, valid_place = 0, valid_place_seeded = 0;
    gint min = mdata->mpositions, max = 1;
    gint best_value = 10000;
    gint seeded = mdata->mcomp[mdata->selected].seeded;

    if (mask == 0)
        return 0;

    if (mdata->mfrench_sys == POOL_SYS) {
        // pool or double pool
        x = rand()%mdata->mpositions + 1;
        for (i = 1; i <= mdata->mpositions; i++) {
            gint currmask = 1<<(x-1);
            if ((currmask & mask) && mdata->mpos[x].judoka == 0)
                return x;

            if (++x > mdata->mpositions)
                x = 1;
        }

        //g_print("%s:%d: mask=%x\n", __FUNCTION__, __LINE__, mask);
        return 0;
    }

    // find minimum valid position
    for (i = 0; i < NUM_SEEDED; i++) {
        if (mask & (1<<i)) {
            min = mdata->mpositions*i/NUM_SEEDED + 1;
            break;
        }
    }

    // find maximum valid position
    for (i = NUM_SEEDED - 1; i >= 0; i--) {
        if (mask & (1<<i)) {
            max = mdata->mpositions*(i+1)/NUM_SEEDED;
            break;
        }
    }

    // random number between min and max
    x = rand()%(max - min + 1) + min;

    // calculate avoiding points for the positions
    memset(place_values, 0, sizeof(place_values));
    memset(selected, 0, sizeof(selected));
    calc_place_values(place_values, mdata);

    for (i = 1; i <= mdata->mpositions; i++) {
        // is this position in a valid mask section
        gint currmask = 1<<get_section(x, mdata);
        if (((!mdata->mask && (currmask & mask)) ||
	     (mdata->mask & (1<<(x-1)))) &&
	    mdata->mpos[x].judoka == 0
            /*XXX && get_competitor_number(x, mdata) <= mdata->mjudokas*/) {
            // update the lowest avoid value
            if (place_values[x] < best_value) {
                best_value = place_values[x];
                valid_place = x;
            }
        } else
            place_values[x] = 10001;

        if (++x > mdata->mpositions)
            x = 1;
    }

    if (valid_place == 0) {
        g_print("%s:%d: no valid position for competitor!\n",
                __FUNCTION__, __LINE__);
        return 0;
    }

    if (seeded) { // ensure seeded can wear white judogi
        for (i = 1; i <= mdata->mpositions; i++) {
            if ((place_values[i] == best_value && (i & 1))) {
                valid_place_seeded = i;
                break;
            }
        }
    }

    x = 0;
    for (i = 1; i <= mdata->mpositions; i++) {
        if ((place_values[i] == best_value && valid_place_seeded == 0) ||
            (place_values[i] == best_value && (i & 1)))
            selected[x++] = i;
    }

    i = rand()%x;
    valid_place = selected[i];

    return valid_place;
}

// return sections with seedings
static gint get_seeded_mask(gint mask, struct mdata *mdata)
{
    gint i, res = 0, seed = 0;

    for (seed = 0; seed < NUM_SEEDED; seed++) {
        if ((mask & (1<<seed)) == 0)
            continue;

        for (i = 1; i <= mdata->mpositions; i++) {
            if (mdata->mpos[i].judoka &&
                mdata->mcomp[mdata->mpos[i].judoka].seeded == seed + 1) {
                res |= 1<<get_section(i, mdata);
            }
        }
    }

    return res;
}

// return sections who have competitors from the same club or country
static gint get_club_mask(struct mdata *mdata)
{
    gint i, res = 0;

    if (mdata->selected == 0)
        return 0;

    for (i = 1; i <= mdata->mpositions; i++) {
        gint comp = mdata->mpos[i].judoka;
        if (comp &&
            cmp_clubs(&mdata->mcomp[comp], &mdata->mcomp[mdata->selected])) {
            res |= 1<<get_section(i, mdata);
        }
    }

    return res;
}

// return sections who have competitors from the same club
static gint get_club_only_mask(struct mdata *mdata)
{
    gint i, res = 0;

    if (mdata->selected == 0)
        return 0;

    for (i = 1; i <= mdata->mpositions; i++) {
        gint comp = mdata->mpos[i].judoka;
        if (comp &&
            (cmp_clubs(&mdata->mcomp[comp], &mdata->mcomp[mdata->selected]) & CLUB_TEXT_CLUB)) {
            res |= 1<<get_section(i, mdata);
        }
    }

    return res;
}

#if 0
static gint get_competitor_group(gint comp, struct mdata *mdata)
{
    gint pos = mdata->mcomp[comp].pos;
    return ((pos - 1)*4)/mdata->mpositions;
}
#endif

// return competitor's number based on position
gint get_drawed_number(gint pos, gint sys)
{
    if (sys >= 0 && (pos&1))
        return french_matches_blue[(pos-1)/2 * french_mul[sys]];
    else if (sys >= 0 && (pos&1) == 0)
        return french_matches_blue[(pos-1)/2 * french_mul[sys]]
            + french_matches_white_offset[sys];
    return pos;
}

// return competitor's number based on position
static gint get_competitor_number(gint pos, struct mdata *mdata)
{
    return get_drawed_number(pos, mdata->mfrench_sys);
}

// manually select position for the competitor
static gboolean select_number(GtkWidget *eventbox, GdkEventButton *event, void *param)
{
#if (GTKVER == 3)
    GdkRGBA bg;
#else
    GdkColor bg;
#endif
    gchar buf[200];
    gint num = 0, i, nxt;
    struct mdata *mdata = param;

    for (i = 1; i <= NUM_COMPETITORS; i++)
        if (eventbox == mdata->mpos[i].eventbox) {
            num = i;
            break;
        }

    // undraw competitor
    if (num && mdata->mpos[num].judoka) {
#if (GTKVER == 3)
        gdk_rgba_parse(&bg, "#000000");
        gtk_widget_override_color(mdata->mcomp[mdata->mpos[num].judoka].label,
                                  GTK_STATE_FLAG_NORMAL, &bg);
#else
        gdk_color_parse("#000000", &bg);
        gtk_widget_modify_fg(mdata->mcomp[mdata->mpos[num].judoka].label,
                             GTK_STATE_NORMAL, &bg);
#endif
        sprintf(buf, "%2d.", get_competitor_number(num, mdata));
        gtk_label_set_text(GTK_LABEL(mdata->mpos[num].label), buf);
        mdata->mcomp[mdata->mpos[num].judoka].pos = 0;
        mdata->mpos[num].judoka = 0;

        if (mdata->drawn > 0)
            mdata->drawn--;

        return TRUE;
    }

    if (mdata->selected == 0 || num == 0) {
        g_print("error %d\n", __LINE__);
        return FALSE;
    }

    for (i = 1; i <= mdata->mpositions; i++) {
        if (mdata->mpos[i].judoka == mdata->selected) {
            mdata->mpos[i].judoka = 0;
            sprintf(buf, "%2d.", get_competitor_number(i, mdata));
            gtk_label_set_text(GTK_LABEL(mdata->mpos[i].label), buf);
        }
    }

    if (mdata->mcomp[mdata->selected].index) {
        mdata->mpos[num].judoka = mdata->selected;
        mdata->mcomp[mdata->selected].pos = num;
    } else {
        mdata->mpos[num].judoka = 0;
        mdata->mcomp[mdata->selected].pos = 0;
    }

    snprintf(buf, sizeof(buf), "%2d. %s", get_competitor_number(num, mdata),
             gtk_label_get_text(GTK_LABEL(mdata->mcomp[mdata->selected].label)));
    gtk_label_set_text(GTK_LABEL(mdata->mpos[num].label), buf);

#if (GTKVER == 3)
    gdk_rgba_parse(&bg, "#7F7F7F");
    gtk_widget_override_color(mdata->mcomp[mdata->selected].label, GTK_STATE_FLAG_NORMAL, &bg);
    gdk_rgba_parse(&bg, "#FFFFFF");
    gtk_widget_override_background_color(mdata->mcomp[mdata->selected].eventbox, GTK_STATE_FLAG_NORMAL, &bg);
#else
    gdk_color_parse("#7F7F7F", &bg);
    gtk_widget_modify_fg(mdata->mcomp[mdata->selected].label, GTK_STATE_NORMAL, &bg);
    gdk_color_parse("#FFFFFF", &bg);
    gtk_widget_modify_bg(mdata->mcomp[mdata->selected].eventbox, GTK_STATE_NORMAL, &bg);
#endif
    nxt = get_next_comp(mdata);
    if (nxt)
        select_competitor(mdata->mcomp[nxt].eventbox, NULL, mdata);
    //g_signal_emit_by_name(mdata->mcomp[nxt].eventbox, "button_press_event", NULL, &retval);
    else
        mdata->selected = 0;

    return TRUE;
}

// manually select next competitor to be drawn
static gboolean select_competitor(GtkWidget *eventbox, GdkEventButton *event, void *param)
{
    guint index = 0;
#if (GTKVER == 3)
    GdkRGBA bg;
#else
    GdkColor bg;
#endif
    gint i;
    struct mdata *mdata = param;

    for (i = 1; i <= NUM_COMPETITORS; i++)
        if (eventbox == mdata->mcomp[i].eventbox) {
            index = i;
            break;
        }

    if (index == 0) {
        g_print("error %d\n", __LINE__);
        return FALSE;
    }

#if (GTKVER == 3)
    gdk_rgba_parse(&bg, "#FFFFFF");
    for (i = 1; i <= mdata->mjudokas; i++)
        gtk_widget_override_background_color(mdata->mcomp[i].eventbox, GTK_STATE_FLAG_NORMAL, &bg);

    gdk_rgba_parse(&bg, "#E0E0FF");
    gtk_widget_override_background_color(mdata->mcomp[index].eventbox, GTK_STATE_FLAG_NORMAL, &bg);
#else
    gdk_color_parse("#FFFFFF", &bg);
    for (i = 1; i <= mdata->mjudokas; i++)
        gtk_widget_modify_bg(mdata->mcomp[i].eventbox, GTK_STATE_NORMAL, &bg);

    gdk_color_parse("#E0E0FF", &bg);
    gtk_widget_modify_bg(mdata->mcomp[index].eventbox, GTK_STATE_NORMAL, &bg);
#endif
    mdata->selected = index;

    return TRUE;
}

static gboolean remove_competitor(GtkWidget *eventbox, GdkEventButton *event, void *param)
{
    struct mdata *mdata = param;

    if (mdata->selected &&
        mdata->mcomp[mdata->selected].pos == 0) {
        mdata->mcomp[mdata->selected].index = 0;
        gtk_label_set_text(GTK_LABEL(mdata->mcomp[mdata->selected].label), "- - -");
        if (mdata->sys.numcomp > 0)
            mdata->sys.numcomp--;
    }
    return TRUE;
}

static gint seedposq[21] = {0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0x1fe, 0x3f6, // 0 - 10
                            0x7b6, 0xdb6, 0x1b66, 0x3666, 0x6666, 0x6666, 0xccd2, 0x19a52, 0x34a52, 0x94a52}; // 11 - 20

// get mask for example from half length one quarter = section C
// = create_mask(1, 2, 4)
// half from beginning = create_mask(0, 1, 2)
static gint create_mask(gint start_dividend, gint start_divisor, gint length_divisor)
{
    gint mask = 0, i;
    gint start = start_dividend*NUM_SEEDED/start_divisor;
    gint end = start + NUM_SEEDED/length_divisor;
    for (i = start; i < end; i++)
        mask |= 1<<i;
    return mask;
}

static gboolean draw_one_comp(struct mdata *mdata)
{
    gint comp = mdata->selected;
    gint found = 0, mask;
    gint sys = mdata->sys.system;

    if (mdata->selected == 0)
        return 0;

    mdata->mask = 0;

    if (mdata->mfrench_sys == CUST_SYS) { // custom
	struct custom_data *ct = mdata->custom_table;
	gint rr = get_rr_by_comp(ct, 1);

	if (rr) {
	    gint seeded = mdata->mcomp[comp].seeded;
	    gint num_start_pools = 0, r;
	    guint64 first_match_mask = 0;
	    guint64 last_match_mask = 0;

	    // Calculate number of starter pools.
	    // Find first match and last match masks.
	    for (r = 0; r < ct->num_round_robin_pools; r++) {
		round_robin_bare_t *rr = &ct->round_robin_pools[r];
		gint mnum = rr->rr_matches[0] - 1;
		match_bare_t *m = &ct->matches[mnum];
		competitor_bare_t *c1 = &m->c1;
		competitor_bare_t *c2 = &m->c2;
		if (c1->type == COMP_TYPE_COMPETITOR &&
		    c2->type == COMP_TYPE_COMPETITOR)
		    num_start_pools++;
		if (c1->type == COMP_TYPE_COMPETITOR)
		    first_match_mask |= 1<<(c1->num-1);
		if (c2->type == COMP_TYPE_COMPETITOR)
		    first_match_mask |= 1<<(c2->num-1);

		mnum = rr->rr_matches[rr->num_rr_matches-1] - 1;
		m = &ct->matches[mnum];
		c1 = &m->c1;
		c2 = &m->c2;
		if (c1->type == COMP_TYPE_COMPETITOR)
		    last_match_mask |= 1<<(c1->num-1);
		if (c2->type == COMP_TYPE_COMPETITOR)
		    last_match_mask |= 1<<(c2->num-1);
	    }

	    if (seeded) {
		mdata->mask = last_match_mask;
		found = get_free_pos_by_mask(create_mask(0,1,1), mdata);
		mdata->mask = 0;
	    }

	    // Most probably club mates go to different pools. If not (too many)
	    // then they should have the first match with each other.
	    if (!found && !seeded &&
		(mdata->mcomp[comp].num_mates & 7) > num_start_pools) {
		// Competitor has many club mates. Try to select the first match.
		mdata->mask = first_match_mask;
		found = get_free_pos_by_mask(create_mask(0,1,1), mdata);
		mdata->mask = 0;
	    }
	} else {
	    gint seeded = mdata->mcomp[comp].seeded;
	    if (seeded > 1) {
		gint d = 0, k = 2;
		gint test_mask = create_mask(d,NUM_SEEDED,2); // AB

		do {
		    mask = get_seeded_mask(1<<(seeded-k), mdata); // look for the previous seeded
		    k++;
		} while (mask == 0 && (seeded-k >= 0));

		if (seeded & 1) {
		    // put to same half as the previous
		    if ((mask & test_mask) == 0)
			d = NUM_SEEDED/2; // prev seeded in CD half

		    test_mask = create_mask(d,NUM_SEEDED,4);
		    if (mask & test_mask)
			d += NUM_SEEDED/4;

		    test_mask = create_mask(d,NUM_SEEDED,4);
		} else {
		    // put to different half as the previous
		    if (mask & test_mask)
			d = NUM_SEEDED/2; // prev seeded in AB half

		    test_mask = create_mask(d,NUM_SEEDED,2);
		}

		found = get_free_pos_by_mask((test_mask ^ mask) & test_mask, mdata);
	    }
	} // french cust

	if (!found)
            found = get_free_pos_by_mask(create_mask(0,1,1), mdata); // look everywhere
    } else if (mdata->mfrench_sys >= 0) { // french
        if (prop_get_int_val(PROP_SEEDED_TO_FIXED_PLACES) &&
            mdata->mcomp[comp].seeded > 0 && mdata->mcomp[comp].seeded <= NUM_SEEDED) {
            gint x;
            switch (mdata->mcomp[comp].seeded) {
            case 1: case 5: x = 0; break;
            case 2: case 6: x = 2; break;
            case 3: case 7: x = 1; break;
            case 4: case 8: x = 3; break;
            }
            found = mdata->mpositions*(x)/4 +
                mdata->mcomp[comp].seeded > 4 ? mdata->mpositions/8 + 1 : 1;

            if (mdata->mpos[found].judoka)
                found = 0;
        }

        if (!found) {
            gint seeded = mdata->mcomp[comp].seeded;
            if (seeded > 1) {
                gint d = 0, k = 2;
                gint test_mask = create_mask(d,NUM_SEEDED,2); // AB

                do {
                    mask = get_seeded_mask(1<<(seeded-k), mdata); // look for the previous seeded
                    k++;
                } while (mask == 0 && (seeded-k >= 0));

                if (seeded & 1) {
                    // put to same half as the previous
                    if ((mask & test_mask) == 0)
                        d = NUM_SEEDED/2; // prev seeded in CD half

                    test_mask = create_mask(d,NUM_SEEDED,4);
                    if (mask & test_mask)
                        d += NUM_SEEDED/4;

                    test_mask = create_mask(d,NUM_SEEDED,4);
                } else {
                    // put to different half as the previous
                    if (mask & test_mask)
                        d = NUM_SEEDED/2; // prev seeded in AB half

                    test_mask = create_mask(d,NUM_SEEDED,2);
                }

                found = get_free_pos_by_mask((test_mask ^ mask) & test_mask, mdata);
            }
        }

        if (!found)
            found = get_free_pos_by_mask(create_mask(0,1,1), mdata); // look everywhere
    } else if (sys != SYSTEM_QPOOL) { // pool and dpool
        gint getmask;

        if (mdata->mcomp[comp].seeded) {
            mask = get_seeded_mask(0xf, mdata); // look for any seeded

            switch (mdata->mpositions) {
            case 2:
                getmask = 0x3; // 1 and 2
                break;
            case 3:
                getmask = 0x6; // 2 and 3
                break;
            case 4:
                getmask = 0x6; // 2 and 3
                break;
            case 5:
                getmask = 0x12; // 2 and 5
                break;
            case 6:
                if (sys == SYSTEM_POOL) {
                    getmask = 0x22; // 2 and 6
                } else {
                    if (mdata->drawn == 1 || mdata->drawn == 2 ) {
                        if (mdata->seeded1 <= 3)
                            getmask = 0x30;
                        else
                            getmask = 0x06;
                        break;
                    } else
                        getmask = 0x36;
                }
                break;
            case 7:
            case 8:
                if (sys == SYSTEM_POOL && mdata->mpositions == 7) {
                    getmask = 0x50; // 5 and 7
                } else {
                    if (mdata->drawn == 1 || mdata->drawn == 2 ) {
                        if (mdata->seeded1 <= 4)
                            getmask = 0x60;
                        else
                            getmask = 0x06;
                        break;
                    } else
                        getmask = 0x66;
                }
                break;
            case 9:
                if (mdata->drawn == 1 || mdata->drawn == 2 ) {
                    if (mdata->seeded1 <= 5)
                        getmask = 0xc0;
                    else
                        getmask = 0x12;
                    break;
                } else
                    getmask = 0xd2;
                break;
            case 10:
                if (mdata->drawn == 1 || mdata->drawn == 2 ) {
                    if (mdata->seeded1 <= 5)
                        getmask = 0x240;
                    else
                        getmask = 0x12;
                    break;
                } else
                    getmask = 0x252;
                break;
            case 11:
                if (mdata->drawn == 1 || mdata->drawn == 2 ) {
                    if (mdata->seeded1 <= 6)
                        getmask = 0x480;
                    else
                        getmask = 0x22;
                    break;
                } else
                    getmask = 0x4a2;
                break;
            case 12:
                if (mdata->drawn == 1 || mdata->drawn == 2 ) {
                    if (mdata->seeded1 <= 6)
                        getmask = 0x880;
                    else
                        getmask = 0x22;
                    break;
                } else
                    getmask = 0x8a2;
                break;
            }
            found = get_free_pos_by_mask((mask ^ getmask) & getmask, mdata);
        } else { // not seeded
            gint a_pool = 0, b_pool = 0, a_pool_club = 0, b_pool_club = 0,
                a_mask = 0, b_mask = 0;
            mask = get_club_mask(mdata);
            gint clubmask = get_club_only_mask(mdata);

            if (sys == SYSTEM_POOL || sys == SYSTEM_BEST_OF_3) { // pool or best of 3
                switch (mdata->mpositions) {
                case 2:
                    getmask = 0x3;
                    break;
                case 3:
                    if (mdata->mcomp[comp].num_mates &&
                        mdata->mpos[1].judoka == 0 && mdata->mpos[2].judoka == 0)
                        getmask = 0x3;
                    else if (mask & 0x3)
                        getmask = 0x3;
                    else
                        getmask = 0x7;
                    break;
                case 4:
                    if (mdata->mcomp[comp].num_mates &&
                        ((mdata->mpos[1].judoka == 0 && mdata->mpos[2].judoka == 0) ||
                         ((mdata->mpos[1].judoka == 0 || mdata->mpos[2].judoka == 0) &&
                          (mask & 0x3))))
                        getmask = 0x3;
                    else if (mask & 0x6)
                        getmask = 0x9;
                    else
                        getmask = 0xf;
                    break;
                case 5:
                    if (mdata->mcomp[comp].num_mates &&
                        ((mdata->mpos[4].judoka == 0 && mdata->mpos[5].judoka == 0) ||
                         ((mdata->mpos[4].judoka == 0 || mdata->mpos[5].judoka == 0) &&
                          (mask & 0x18))))
                        getmask = 0x18;
                    else if (mdata->mcomp[comp].num_mates &&
                        ((mdata->mpos[1].judoka == 0 && mdata->mpos[2].judoka == 0) ||
                         ((mdata->mpos[1].judoka == 0 || mdata->mpos[2].judoka == 0) &&
                          (mask & 0x3))))
                        getmask = 0x3;
                    else if (mask & 0x12)
                        getmask = 0x0d;
                    else
                        getmask = 0x1f;
                    break;
                case 6:
                    if (mdata->mcomp[comp].num_mates &&
                        ((mdata->mpos[1].judoka == 0 && mdata->mpos[2].judoka == 0) ||
                         ((mdata->mpos[1].judoka == 0 || mdata->mpos[2].judoka == 0) &&
                          (mask & 0x3))))
                        getmask = 0x3;
                    else if (mdata->mcomp[comp].num_mates &&
                        ((mdata->mpos[3].judoka == 0 && mdata->mpos[5].judoka == 0) ||
                         ((mdata->mpos[3].judoka == 0 || mdata->mpos[5].judoka == 0) &&
                          (mask & 0x14))))
                        getmask = 0x14;
                    else if (mask & 0x22)
                        getmask = 0x1d;
                    else
                        getmask = 0x3f;
                    break;
                case 7:
                    if (mdata->mcomp[comp].num_mates &&
                        ((mdata->mpos[1].judoka == 0 && mdata->mpos[2].judoka == 0) ||
                         ((mdata->mpos[1].judoka == 0 || mdata->mpos[2].judoka == 0) &&
                          (mask & 0x3))))
                        getmask = 0x3;
                    else if (mdata->mcomp[comp].num_mates &&
                        ((mdata->mpos[3].judoka == 0 && mdata->mpos[5].judoka == 0) ||
                         ((mdata->mpos[3].judoka == 0 || mdata->mpos[5].judoka == 0) &&
                          (mask & 0x14))))
                        getmask = 0x14;
                    else if (mask & 0x50)
                        getmask = 0x2f;
                    else
                        getmask = 0x7f;
                    break;
                default:
                    getmask = 0xfff;
                }
            } else { // double pool
                gint i;
                for (i = 0; i < mdata->mpositions; i++) { // count mates in a and b pool
                    if (mask & (1<<i)) {
                        if (i < (mdata->mpositions + 1)/2)
                            a_pool++;
                        else
                            b_pool++;
                    }

                    if (clubmask & (1<<i)) {
                        if (i < (mdata->mpositions + 1)/2)
                            a_pool_club++;
                        else
                            b_pool_club++;
                    }

                    if (i < (mdata->mpositions + 1)/2)
                        a_mask |= 1<<i;
                    else
                        b_mask |= 1<<i;
                }

                if (a_pool_club < b_pool_club)
                    getmask = a_mask;
                else if (a_pool_club > b_pool_club)
                    getmask = b_mask;
                else if (a_pool < b_pool)
                    getmask = a_mask;
                else if (a_pool > b_pool)
                    getmask = b_mask;
                else
                    getmask = a_mask | b_mask;
            } // end dpool

            found = get_free_pos_by_mask(getmask, mdata);
        }

        if (!found)
            found = get_free_pos_by_mask(0xfff, mdata);
    } else { // QPOOL
        gint getmask;

        if (mdata->mcomp[comp].seeded) {
            mask = get_seeded_mask(0xfffff, mdata); // look for any seeded
            getmask = seedposq[mdata->mpositions];
            found = get_free_pos_by_mask((mask ^ getmask) & getmask, mdata);
        } else {
            gint i, j;
            gint mates[4], clubmates[4];
            gint clubmask = get_club_only_mask(mdata);
            gint pool_start[4];
            gint pool_size[4];
            gint pool_mask[4];
            gint size = mdata->mpositions/4;
            gint rem = mdata->mpositions - size*4;

            mask = get_club_mask(mdata);

            for (i = 0; i < 4; i++) {
                pool_size[i] = size;
                mates[i] = clubmates[i] = pool_mask[i] = 0;
            }

            for (i = 0; i < rem; i++)
                pool_size[i]++;

            gint start = 0;
            for (i = 0; i < 4; i++) {
                pool_start[i] = start;
                start += pool_size[i];
            }

            for (i = 0; i < mdata->mpositions; i++) { // count mates in pools
                if (mask & (1<<i)) {
                    for (j = 3; j >= 0; j--) {
                        if (i >= pool_start[j]) {
                            mates[j]++;
                            break;
                        }
                    }
                }

                if (clubmask & (1<<i)) {
                    for (j = 3; j >= 0; j--) {
                        if (i >= pool_start[j]) {
                            clubmates[j]++;
                            break;
                        }
                    }
                }

                for (j = 3; j >= 0; j--) {
                    if (i >= pool_start[j]) {
                        pool_mask[j] |= 1<<i;
                        break;
                    }
                }
            }

            gint min_mates = 10000, min_clubmates = 10000;
            gint min_mates_ix = 0;
            for (i = 0; i < 4; i++) {
                if (mates[i] < min_mates) {
                    min_mates = mates[i];
                    min_mates_ix = i;
                }
                if (clubmates[i] < min_clubmates) {
                    min_clubmates = clubmates[i];
                }
            }

            getmask = pool_mask[min_mates_ix];

            found = get_free_pos_by_mask(getmask, mdata);
        } // !seeded

        if (!found)
            found = get_free_pos_by_mask(0xfffff, mdata);
    } // QPOOL

    if (found)
        select_number(mdata->mpos[found].eventbox, NULL, mdata);

    mdata->drawn++;
    if (mdata->mcomp[comp].seeded && mdata->drawn == 1)
        mdata->seeded1 = found;

    return (found > 0);
}

static void make_manual_matches_callback(GtkWidget *widget,
                                         GdkEvent *event,
                                         void *data)
{
    gint i;
    struct mdata *mdata = data;

    if (ptr_to_gint(event) == BUTTON_NEXT) {
        draw_one_comp(mdata);

        return;
    }

    if (ptr_to_gint(event) == BUTTON_REST) {
        while (draw_one_comp(mdata))
            ;

        return;
    }

    if (ptr_to_gint(event) != GTK_RESPONSE_OK)
        goto out;

    /* check drawing is complete */
    for (i = 1; i <= mdata->mjudokas; i++) {
        if (mdata->mcomp[i].index && mdata->mcomp[i].pos == 0) {
            show_message(_("Drawing not finished!"));
            return;
        }
    }

    db_remove_matches(mdata->mcategory_ix);

    if (mdata->mfrench_sys == CUST_SYS) { // custom system
        struct custom_data *ct =  get_custom_table(mdata->sys.table);

        for (i = 0; i < ct->num_matches; i++) {
            struct match m;
            gint c;

            memset(&m, 0, sizeof(m));
            m.category = mdata->mcategory_ix;
            m.number = i+1;
            if (ct->matches[i].c1.type == COMP_TYPE_COMPETITOR) {
                c = mdata->mpos[ct->matches[i].c1.num].judoka;
                m.blue = c ? mdata->mcomp[c].index : GHOST;
            }
            if (ct->matches[i].c2.type == COMP_TYPE_COMPETITOR) {
                c = mdata->mpos[ct->matches[i].c2.num].judoka;
                m.white = c ? mdata->mcomp[c].index : GHOST;
            }

            set_match(&m);
            db_set_match(&m);
        }
    } else if (mdata->mfrench_sys == POOL_SYS) {
        for (i = 0; i < num_matches(mdata->sys.system, mdata->mjudokas); i++) {
            struct match m;

            memset(&m, 0, sizeof(m));
            m.category = mdata->mcategory_ix;
            m.number = i+1;
            if (mdata->sys.system == SYSTEM_QPOOL) {
                m.blue = mdata->mcomp[ mdata->mpos[ poolsq[mdata->mjudokas][i][0] ].judoka ].index;
                m.white = mdata->mcomp[ mdata->mpos[ poolsq[mdata->mjudokas][i][1] ].judoka ].index;
            } else if (mdata->sys.system == SYSTEM_DPOOL ||
                       mdata->sys.system == SYSTEM_DPOOL2 ||
                       mdata->sys.system == SYSTEM_DPOOL3) {
                m.blue = mdata->mcomp[ mdata->mpos[ poolsd[mdata->mjudokas][i][0] ].judoka ].index;
                m.white = mdata->mcomp[ mdata->mpos[ poolsd[mdata->mjudokas][i][1] ].judoka ].index;
            } else {
                m.blue = mdata->mcomp[ mdata->mpos[ pools[mdata->mjudokas][i][0] ].judoka ].index;
                m.white = mdata->mcomp[ mdata->mpos[ pools[mdata->mjudokas][i][1] ].judoka ].index;
            }

            set_match(&m);
            db_set_match(&m);
        }
    } else {
        for (i = 1; i < mdata->mpositions; i += 2) {
            struct match m;
            gint b, w;

            memset(&m, 0, sizeof(m));
            m.category = mdata->mcategory_ix;
            m.number = ((i - 1)/2) + 1;
            b = mdata->mpos[i].judoka;
            w = mdata->mpos[i+1].judoka;
            m.blue = b ? mdata->mcomp[b].index : GHOST;
            m.white = w ? mdata->mcomp[w].index : GHOST;

            set_match(&m);
            db_set_match(&m);
        }
    }

    db_set_system(mdata->mcategory_ix, mdata->sys);

    if (mdata->edit)
        matches_refresh();
out:

    update_matches(mdata->mcategory_ix, (struct compsys){0,0,0,0}, 0);
    category_refresh(mdata->mcategory_ix);
    //update_category_status_info_all();
    update_category_status_info(mdata->mcategory_ix);

    if (mdata->hidden == FALSE && ptr_to_gint(event) == GTK_RESPONSE_OK)
        category_window(mdata->mcategory_ix);

    free_mdata(mdata);

    if (widget)
        gtk_widget_destroy(widget);
}

struct compsys get_system_for_category(gint index, gint competitors)
{
    struct compsys systm = get_cat_system(index);
    gint wishsys = systm.wishsys;
    gint table = systm.table;
    struct category_data *cat = NULL;
    gint sys = 0;

    if (systm.system)
        return systm;

    if (wishsys == CAT_SYSTEM_CUSTOM) {
        table = get_custom_table_number_by_competitors(competitors);
        if (table == 0) wishsys = CAT_SYSTEM_DEFAULT;
        else {
            sys = SYSTEM_CUSTOM;
        }
    }

    if (wishsys == CAT_SYSTEM_DEFAULT) {
        cat = avl_get_category(index);
        if (cat) {
            gint age = 0;
            gint aix = find_age_index(cat->category);
            if (aix >= 0)
                age = category_definitions[aix].age;
            wishsys = props_get_default_wishsys(age, competitors);
        }
    }

    if (wishsys == CAT_SYSTEM_CUSTOM) {
    } else if (competitors == 2 && (wishsys == CAT_SYSTEM_BEST_OF_3 || wishsys == CAT_SYSTEM_DEFAULT)) {
        sys = SYSTEM_BEST_OF_3;
    } else if (competitors <= 5 && (wishsys == CAT_SYSTEM_POOL || wishsys == CAT_SYSTEM_DEFAULT)) {
        sys = SYSTEM_POOL;
    } else if (competitors <= 7 && wishsys == CAT_SYSTEM_POOL) {
        sys = SYSTEM_POOL;
    } else if (competitors > 5 && competitors <= 12 && wishsys == CAT_SYSTEM_DPOOL) {
        sys = SYSTEM_DPOOL;
    } else if (competitors > 5 && competitors <= 12 && wishsys == CAT_SYSTEM_DPOOL2) {
        sys = SYSTEM_DPOOL2;
    } else if (competitors > 5 && competitors <= 12 && wishsys == CAT_SYSTEM_DPOOL3) {
        sys = SYSTEM_DPOOL3;
    } else if (competitors >= 8 && competitors <= 20 && wishsys == CAT_SYSTEM_QPOOL) {
        sys = SYSTEM_QPOOL;
    } else if (competitors > 5 && competitors <= 10 && wishsys == CAT_SYSTEM_DEFAULT &&
               (draw_system == DRAW_SPANISH || draw_system == DRAW_NORWEGIAN)) {
            sys = SYSTEM_DPOOL;
    } else if (competitors > 5 && competitors <= 7 && wishsys == CAT_SYSTEM_DEFAULT) {
        if (draw_system == DRAW_INTERNATIONAL || draw_system == DRAW_ESTONIAN) {
            if (draw_system == DRAW_INTERNATIONAL)
                table = cat_system_to_table[CAT_IJF_DOUBLE_REPECHAGE];
            sys = SYSTEM_FRENCH_8;
        } else {
            sys = SYSTEM_DPOOL;
        }
    } else if (competitors > 16 && wishsys == CAT_ESP_DOBLE_PERDIDA) {
        table = cat_system_to_table[CAT_ESP_REPESCA_DOBLE];
        if (competitors <= 32)
            sys = SYSTEM_FRENCH_32;
        else
            sys = SYSTEM_FRENCH_64;
    } else if (competitors > 64) { // only 2 tables supported
        if (wishsys != CAT_SYSTEM_REPECHAGE &&
            wishsys != CAT_IJF_DOUBLE_REPECHAGE) {
            if (draw_system == DRAW_FINNISH)
                wishsys = CAT_SYSTEM_REPECHAGE;
            else
                wishsys = CAT_IJF_DOUBLE_REPECHAGE;
        }

        table = cat_system_to_table[wishsys];
        sys = SYSTEM_FRENCH_128;
    } else {
        if (wishsys == CAT_SYSTEM_DEFAULT) {
            switch (draw_system) {
            case DRAW_INTERNATIONAL:
                wishsys = CAT_IJF_DOUBLE_REPECHAGE;
                break;
            case DRAW_FINNISH:
                break;
            case DRAW_SWEDISH:
                wishsys = CAT_SYSTEM_DIREKT_AATERKVAL;
                break;
            case DRAW_ESTONIAN:
                /* double elimination is used for under 11 years old */
                cat = avl_get_category(index);
                if (cat) {
                    gint aix = find_age_index(cat->category);
                    if (aix >= 0) {
                        if (category_definitions[aix].age <= 10)
                            wishsys = CAT_SYSTEM_EST_D_KLASS;
                    }
                }
                break;
            case DRAW_SPANISH:
                wishsys = CAT_ESP_REPESCA_DOBLE;
                cat = avl_get_category(index);
                if (cat) {
                    gint aix = find_age_index(cat->category);
                    if (aix >= 0 && competitors <= 16) {
                        if (category_definitions[aix].age <= 10)
                            wishsys = CAT_ESP_DOBLE_PERDIDA;
                    }
                }
                break;
            case DRAW_NORWEGIAN:
                wishsys = CAT_SYSTEM_DUBBELT_AATERKVAL;
                break;
            case DRAW_BRITISH:
                wishsys = CAT_SYSTEM_GBR_KNOCK_OUT;
                break;
            }
        }

        table = cat_system_to_table[wishsys];

        if (competitors <= 8) {
            sys = SYSTEM_FRENCH_8;
        } else if (competitors <= 16) {
            sys = SYSTEM_FRENCH_16;
        } else if (competitors <= 32) {
            sys = SYSTEM_FRENCH_32;
        } else if (competitors <= 64) {
            sys = SYSTEM_FRENCH_64;
        } else {
            sys = SYSTEM_FRENCH_128;
        }
    }

    systm.system  = sys;
    systm.numcomp = competitors;
    systm.table   = table;
    systm.wishsys = wishsys;

    return systm;
}

static gint get_rr_by_match(struct custom_data *ct, gint mnum)
{
    gint r, f;

    if (!ct)
	return 0;

    for (r = 0; r < ct->num_round_robin_pools; r++) {
	round_robin_bare_t *rr = &ct->round_robin_pools[r];
	for (f = 0; f < rr->num_rr_matches; f++) {
	    if (rr->rr_matches[f] == mnum)
		return r + 1;
	}
    }
    return 0;
}

static gint get_rr_by_comp(struct custom_data *ct, gint cnum)
{
    gint r, f;

    if (!ct)
	return 0;

    for (r = 0; r < ct->num_round_robin_pools; r++) {
	round_robin_bare_t *rr = &ct->round_robin_pools[r];
	for (f = 0; f < rr->num_competitors; f++) {
	    competitor_bare_t *c = &rr->competitors[f];
	    if (c->type == COMP_TYPE_COMPETITOR &&
		c->num == cnum)
		return r + 1;
	}
    }
    return 0;
}

static void calc_distances(struct mdata *mdata)
{
    struct custom_data *ct = mdata->custom_table;
    gchar mlist[NUM_COMPETITORS+1][16];
    gint c, i, j, k, n;

    memset(&mlist, 0, sizeof(mlist));

#if 0
    g_print("MATCHES:\n");
    for (c = 0; c < ct->num_matches; c++) {
	match_bare_t *m = &ct->matches[c];
	competitor_bare_t *c1 = &m->c1;
	competitor_bare_t *c2 = &m->c2;
	g_print("%d: type=%d num=%d pos=%d - type=%d num=%d pos=%d\n",
	  c, c1->type, c1->num, c1->pos, c2->type, c2->num, c2->pos);
    }
#endif

    for (c = 1; c <= mdata->mpositions; c++) {
	gint depth = 0;
	competitor_bare_t cmp;
	cmp.type = COMP_TYPE_COMPETITOR;
	cmp.num = c;
	cmp.pos = 0;
	//g_print("\n-- competitor %d\n", c);
	while (1) {
	    gboolean found = FALSE;

	    //g_print("cmp=%d %d:%d:%d\n", c, cmp.type, cmp.num, cmp.pos);

	    for (i = 0; i < ct->num_matches && !found; i++) {
		gint mnum = i + 1;
		match_bare_t *m = &ct->matches[i];
		competitor_bare_t *c1 = &m->c1;
		competitor_bare_t *c2 = &m->c2;
		if ((c1->type == cmp.type && c1->num == cmp.num &&
		     c1->prev[0] == 0 && (cmp.pos == 0 || cmp.pos == c1->pos)) ||
		    (c2->type == cmp.type && c2->num == cmp.num &&
		     c2->prev[0] == 0 && (cmp.pos == 0 || cmp.pos == c2->pos))) {
		    cmp.type = COMP_TYPE_MATCH;
		    cmp.num = i + 1;
		    cmp.pos = 1;
		    found = TRUE;

		    // is this match part of round robin?
		    gint rrnum = get_rr_by_match(ct, i + 1);
		    if (rrnum) {
			mnum = ct->round_robin_pools[rrnum-1].rr_matches[0];
			cmp.type = COMP_TYPE_ROUND_ROBIN;
			cmp.num = rrnum;
		    }

		    if (depth < 16)
			mlist[c][depth++] = mnum;
		    break;
		}
	    }

	    if (!found)
		break;
	} // while
    }

    for (i = 1; i <= mdata->mpositions; i++) {
	for (j = 1; j <= mdata->mpositions; j++) {
	    gboolean found = FALSE;
	    if (i == j)
		continue;
	    for (k = 0; k < 16 && mlist[i][k] && !found; k++) {
		for (n = 0; n < 16 && mlist[j][n]; n++) {
		    if (mlist[i][k] == mlist[j][n]) {
			mdata->mcomp[i].distance[j] = k + n;
			found = TRUE;
			break;
		    }
		}
	    }
	}
    }
#if 0
    g_print("\n");
    for (c = 1; c <= mdata->mpositions; c++) {
	g_print("comp=%d:", c);
	for (i = 0; i < 16 && mlist[c][i]; i++)
	    g_print(" %d", mlist[c][i]);
	g_print("\n");
    }

    for (i = 1; i <= mdata->mpositions; i++) {
	g_print("i=%d", i);
	for (j = 1; j <= mdata->mpositions; j++) {
	    g_print(" %d", mdata->mcomp[i].distance[j]);
	}
	g_print("\n");
    }
#endif
}

GtkWidget *draw_one_category_manually_1(GtkTreeIter *parent, gint competitors,
					struct mdata *mdata)
{
    gint i;
    GtkWidget *dialog = NULL, *hbox, *vbox1, *vbox2, *label, *eventbox, *tmp;
#if (GTKVER == 3)
    GdkRGBA bg, bg1, bg2;
#else
    GdkColor bg, bg1, bg2;
#endif
    gchar *catname = NULL;
    gchar buf[200];
    struct custom_data *custom_table = NULL;

    mdata->drawn = 0;
    mdata->mjudokas = competitors;

    // colours for the tables
#if (GTKVER == 3)
    gdk_rgba_parse(&bg, "#FFFFFF");
    gdk_rgba_parse(&bg1, "#AFAF9F");
    gdk_rgba_parse(&bg2, "#AFAFBF");
#else
    gdk_color_parse("#FFFFFF", &bg);
    gdk_color_parse("#AFAF9F", &bg1);
    gdk_color_parse("#AFAFBF", &bg2);
#endif

    // find the index of the category
    gtk_tree_model_get(current_model, parent,
                       COL_INDEX, &(mdata->mcategory_ix),
                       COL_LAST_NAME, &catname,
                       -1);

    mdata->sys = get_system_for_category(mdata->mcategory_ix, competitors);
    mdata->sys.numcomp = competitors;

    switch (mdata->sys.system) {
    case SYSTEM_POOL:
    case SYSTEM_DPOOL:
    case SYSTEM_DPOOL2:
    case SYSTEM_DPOOL3:
    case SYSTEM_QPOOL:
    case SYSTEM_BEST_OF_3:
        mdata->mpositions = competitors;
        mdata->mfrench_sys = -1;
        break;
    case SYSTEM_CUSTOM:
        mdata->mfrench_sys = -2;
        custom_table = get_custom_table(mdata->sys.table);
	mdata->custom_table = custom_table;
        mdata->mpositions = custom_table->competitors_max;
	calc_distances(mdata);
        break;
    case SYSTEM_FRENCH_8:
        mdata->mpositions = 8;
        mdata->mfrench_sys = FRENCH_8;
        break;
    case SYSTEM_FRENCH_16:
        mdata->mpositions = 16;
        mdata->mfrench_sys = FRENCH_16;
        break;
    case SYSTEM_FRENCH_32:
        mdata->mpositions = 32;
        mdata->mfrench_sys = FRENCH_32;
        break;
    case SYSTEM_FRENCH_64:
        mdata->mpositions = 64;
        mdata->mfrench_sys = FRENCH_64;
        break;
    case SYSTEM_FRENCH_128:
        mdata->mpositions = 128;
        mdata->mfrench_sys = FRENCH_128;
        break;
    }

    struct category_data *catdata = avl_get_category(mdata->mcategory_ix);
    if ((catname && (catname[0] == '?' || catname[0] == '_')) ||
        (catdata && (catdata->deleted & TEAM))) {
        //SHOW_MESSAGE("Cannot draw %s", catname);
        g_free(catname);
        g_free(mdata);
        return NULL;
    }

    g_free(catname);

    if ((db_category_match_status(mdata->mcategory_ix) & SYSTEM_DEFINED /*REAL_MATCH_EXISTS*/) && mdata->edit == FALSE) {
        // Cannot draw again.
#if 0
        struct judoka *j = get_data(mdata->mcategory_ix);
        SHOW_MESSAGE("%s %s. %s.",
                     _("Matches matched in category"),
                     j ? j->last : "?", _("Clear the results first"));
        free_judoka(j);
#endif
        g_free(mdata);
        return dialog;
    }

    // Clean up the database.
    //struct compsys cs;
    //cs = get_cat_system(mdata->mcategory_ix);
    //cs.system = cs.numcomp = cs.table = 0; // leave wishsys as is

    // moved to OK button
    //db_set_system(mdata->mcategory_ix, cs);
    //db_remove_matches(mdata->mcategory_ix);

    // Get data of all the competitors.
    for (i = 0; i < competitors; i++) {
        guint index;
        gchar *club, *country;
        gint deleted;
        gint seeding, clubseeding;
        GtkTreeIter iter;
        struct judoka *j;

        gtk_tree_model_iter_nth_child(current_model,
                                      &iter,
                                      parent,
                                      i);
        gtk_tree_model_get(current_model, &iter,
                           COL_INDEX, &index,
                           COL_CLUB, &club,
                           COL_COUNTRY, &country,
                           COL_DELETED, &deleted,
                           COL_SEEDING, &seeding,
                           COL_CLUBSEEDING, &clubseeding,
                           -1);

        j = get_data(index);
	if (j && j->club && j->club[0] && j->country && j->country[0])
	    snprintf(buf, sizeof(buf), "%s %s, %s/%s",
		     j->first, j->last, j->club, j->country);
	else if (j && j->club && j->club[0])
	    snprintf(buf, sizeof(buf), "%s %s, %s",
		     j->first, j->last, j->club);
	else if (j && j->country && j->country[0])
	    snprintf(buf, sizeof(buf), "%s %s, %s",
		     j->first, j->last, j->country);
	else
	    snprintf(buf, sizeof(buf), "%s %s", j->first, j->last);

        mdata->mcomp[i+1].label = gtk_label_new(buf);
        gtk_label_set_width_chars(GTK_LABEL(mdata->mcomp[i+1].label), 20);
        mdata->mcomp[i+1].index = index;
        mdata->mcomp[i+1].club = club;
        mdata->mcomp[i+1].country = country;
        mdata->mcomp[i+1].seeded = seeding;
        mdata->mcomp[i+1].clubseeded = clubseeding;
        free_judoka(j);
    }

    // Create the dialog.
    dialog = gtk_dialog_new_with_buttons (_("Move the competitors"), //XXXX
                                          NULL,
                                          GTK_DIALOG_DESTROY_WITH_PARENT,
                                          _("Next"), BUTTON_NEXT,
                                          _("Rest"), BUTTON_REST,
                                          GTK_STOCK_OK, GTK_RESPONSE_OK,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          NULL);

    gtk_widget_set_size_request(GTK_WIDGET(dialog), FRAME_WIDTH, FRAME_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

#if (GTKVER == 3)
    hbox = gtk_grid_new();
    vbox1 = gtk_grid_new();
    vbox2 = gtk_grid_new();
    gint r1 = 0, r2 = 0;
#else
    hbox = gtk_hbox_new(FALSE, 5);
    vbox1 = gtk_vbox_new(FALSE, 5);
    vbox2 = gtk_vbox_new(FALSE, 5);
#endif
    gtk_container_set_border_width(GTK_CONTAINER(vbox1), 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 10);
#if (GTKVER == 3)
    tmp = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(hbox), tmp, 0, 0, 1, 1);
    gtk_widget_set_hexpand(tmp, TRUE);
    gtk_grid_attach(GTK_GRID(hbox), vbox2, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(hbox), vbox1, 2, 0, 1, 1);
    tmp = gtk_label_new("");
    gtk_grid_attach(GTK_GRID(hbox), tmp, 3, 0, 1, 1);
    gtk_widget_set_hexpand(tmp, TRUE);

    gtk_grid_attach(GTK_GRID(vbox1), gtk_label_new(_("Number")),     0, r1++, 1, 1);
    gtk_grid_attach(GTK_GRID(vbox2), gtk_label_new(_("Competitor")), 0, r2++, 1, 1);
#else
    gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox2);
    gtk_box_pack_start_defaults(GTK_BOX(hbox), vbox1);

    gtk_box_pack_start(GTK_BOX(vbox1), gtk_label_new(_("Number")), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), gtk_label_new(_("Competitor")), FALSE, FALSE, 0);
#endif
    // Create the positions table.
    for (i = 1; i <= mdata->mpositions; i++) {
        sprintf(buf, "%2d.                    ", get_competitor_number(i, mdata));
        mdata->mpos[i].eventbox = eventbox = gtk_event_box_new();
        mdata->mpos[i].label = label = gtk_label_new(buf);
        gtk_label_set_width_chars(GTK_LABEL(label), 20);
        gtk_container_add(GTK_CONTAINER(eventbox), label);
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(vbox1), eventbox, 0, r1++, 1, 1);
#else
        gtk_box_pack_start(GTK_BOX(vbox1), eventbox, FALSE, FALSE, 0);
#endif
        g_signal_connect(G_OBJECT(eventbox), "button_press_event",
                         G_CALLBACK(select_number), mdata);
        g_object_set(label, "xalign", 0.0, NULL);

        if (mdata->mfrench_sys == CUST_SYS) {
	    int rr = get_rr_by_comp(mdata->custom_table, i);
	    if (rr) {
		if (!(rr & 1))
		    gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &bg1);
		else
		    gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &bg2);
	    } else if ((i > mdata->mjudokas/4 && i <= mdata->mjudokas/2) ||
		       (mdata->mfrench_sys == CUST_SYS && i > mdata->mjudokas*3/4))
		gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &bg1);
	    else
		gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &bg2);
	} else if ((mdata->mfrench_sys == POOL_SYS && i > (mdata->mjudokas - mdata->mjudokas/2)) ||
            (mdata->mfrench_sys == 0 && i > 2 && i <= 4) ||
            (mdata->mfrench_sys == 0 && i > 6) ||
            (mdata->mfrench_sys == 1 && i > 4 && i <= 8) ||
            (mdata->mfrench_sys == 1 && i > 12) ||
            (mdata->mfrench_sys == 2 && i > 8 && i <= 16) ||
            (mdata->mfrench_sys == 2 && i > 24) ||
            (mdata->mfrench_sys == 3 && i > 16 && i <= 32) ||
            (mdata->mfrench_sys == 3 && i > 48) ||
            (mdata->mfrench_sys == 4 && i > 32 && i <= 64) ||
            (mdata->mfrench_sys == 4 && i > 96))
#if (GTKVER == 3)
            gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &bg1);
        else
            gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &bg2);
#else
            gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &bg1);
        else
            gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &bg2);
#endif
        if (mdata->mfrench_sys >= 0 && i < mdata->mpositions) {
#if (GTKVER == 3)
            if ((i & 1) == 0)
                gtk_grid_attach(GTK_GRID(vbox1), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, r1++, 1, 1);
            if ((i & 3) == 0)
                gtk_grid_attach(GTK_GRID(vbox1), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, r1++, 1, 1);
            if ((i & 7) == 0)
                gtk_grid_attach(GTK_GRID(vbox1), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, r1++, 1, 1);
#else
            if ((i & 1) == 0)
                gtk_box_pack_start(GTK_BOX(vbox1),
                                   gtk_hseparator_new(), FALSE, FALSE, 0);
            if ((i & 3) == 0)
                gtk_box_pack_start(GTK_BOX(vbox1),
                                   gtk_hseparator_new(), FALSE, FALSE, 0);
            if ((i & 7) == 0)
                gtk_box_pack_start(GTK_BOX(vbox1),
                                   gtk_hseparator_new(), FALSE, FALSE, 0);
#endif
        }
    }

    // cut button
    if (mdata->edit && mdata->mfrench_sys >= 0) {
        eventbox = gtk_event_box_new();
#if (GTKVER == 3)
        GtkWidget *delete = gtk_image_new_from_icon_name("edit-cut", 24);
#else
        GtkWidget *delete = gtk_image_new_from_stock(GTK_STOCK_CUT, GTK_ICON_SIZE_LARGE_TOOLBAR);
#endif
        gtk_container_add(GTK_CONTAINER(eventbox), delete);
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(vbox1), eventbox, 0, r1++, 1, 1);
#else
        gtk_box_pack_start(GTK_BOX(vbox1), eventbox, FALSE, FALSE, 0);
#endif
        g_signal_connect(G_OBJECT(eventbox), "button_press_event",
                         G_CALLBACK(remove_competitor), mdata);
    }

    // find club mates of seeded
    for (i = 1; i <= mdata->mjudokas; i++)
        if (mdata->mcomp[i].seeded) {
            gint j;

            mdata->mcomp[i].num_mates = MAX_MATES - mdata->mcomp[i].seeded;

            for (j = 1; j <= mdata->mjudokas; j++) {
                if (i == j)
                    continue;

                if (cmp_clubs(&mdata->mcomp[i], &mdata->mcomp[j]))
                    mdata->mcomp[j].num_mates =
                        MAX_MATES - mdata->mcomp[i].seeded;
            }
        }


    // find club mates
    for (i = 1; i <= mdata->mjudokas; i++) {
        gint j, r;

        if (mdata->mcomp[i].num_mates)
            continue;

        for (j = 1; j <= mdata->mjudokas; j++) {
            if (i == j)
                continue;

            if ((r = cmp_clubs(&mdata->mcomp[i], &mdata->mcomp[j]))) {
                if (r & CLUB_TEXT_COUNTRY)
                    mdata->mcomp[i].num_mates += 8;
                if (r & CLUB_TEXT_CLUB)
                    mdata->mcomp[i].num_mates++;
            }
        }
    }

    // Create competitor table.
    for (i = 1; i <= mdata->mjudokas; i++) {
        mdata->mcomp[i].eventbox = eventbox = gtk_event_box_new();
        label = mdata->mcomp[i].label;
        gtk_container_add(GTK_CONTAINER(eventbox), label);
#if (GTKVER == 3)
        gtk_grid_attach(GTK_GRID(vbox2), eventbox, 0, r2++, 1, 1);
#else
        gtk_box_pack_start(GTK_BOX(vbox2), eventbox, FALSE, FALSE, 0);
#endif
        g_signal_connect(G_OBJECT(eventbox), "button_press_event",
                         G_CALLBACK(select_competitor), mdata);

#if (GTKVER == 3)
        gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &bg);
#else
        gtk_widget_modify_bg(eventbox, GTK_STATE_NORMAL, &bg);
#endif
        g_object_set(label, "xalign", 0.0, NULL);
    }

#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       gtk_label_new(_("Select first a competitor and then a number.")), FALSE, FALSE, 0);
#else
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                       gtk_label_new(_("Select first a competitor and then a number.")),
                       FALSE, FALSE, 0);
#endif
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    //gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 10);
#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), hbox);
#else
    gtk_scrolled_window_add_with_viewport(
        GTK_SCROLLED_WINDOW(scrolled_window), hbox);
#endif
#if (GTKVER == 3)
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       scrolled_window, TRUE, TRUE, 0);
#else
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolled_window,
                       TRUE, TRUE, 0);
#endif
    if (mdata->hidden == FALSE)
        gtk_widget_show_all(dialog);

    g_signal_connect(G_OBJECT(dialog), "response",
                     G_CALLBACK(make_manual_matches_callback), mdata);

    last_selected = 0;
    gboolean retval = FALSE;
    g_signal_emit_by_name(mdata->mcomp[get_next_comp(mdata)].eventbox, "button_press_event",
                          NULL, &retval);

    if (mdata->edit) {
        struct match matches[NUM_MATCHES];
        memset(matches, 0, sizeof(matches));
        db_read_category_matches(mdata->mcategory_ix, matches);

        for (i = 1; i <= mdata->mpositions; i++) {
            gint comp = 0;
            if (mdata->mfrench_sys >= 0) {
                if (i & 1) comp = matches[(i+1)/2].blue;
                else comp = matches[(i+1)/2].white;
            } else {
                gint j = 0;
                typedef guint pair_t[2];
                pair_t *fights;

                switch (mdata->sys.system) {
                case SYSTEM_POOL:
                case SYSTEM_BEST_OF_3:
                    fights = (pair_t *)&pools[mdata->mjudokas];
                    break;
                case SYSTEM_DPOOL:
                case SYSTEM_DPOOL2:
                case SYSTEM_DPOOL3:
                    fights = (pair_t *)&poolsd[mdata->mjudokas];
                    break;
                case SYSTEM_QPOOL:
                    fights = (pair_t *)&poolsq[mdata->mjudokas];
                    break;
                }
                for (j = 0; comp == 0 && fights[j][0]; j++) {
                    if (fights[j][0] == i)
                        comp = matches[j+1].blue;
                    else if (fights[j][1] == i)
                        comp = matches[j+1].white;
                }
            }

            if (comp >= 10) {
                gint j, cnum = 0;
                for (j = 1; j <= mdata->mjudokas; j++) {
                    if (mdata->mcomp[j].index == comp) {
                        cnum = j;
                        break;
                    }
                }

                if (cnum) {
                    mdata->mpos[i].judoka = cnum;
                    mdata->mcomp[cnum].pos = i;
                    snprintf(buf, sizeof(buf), "%2d. %s", get_competitor_number(i, mdata),
                             gtk_label_get_text(GTK_LABEL(mdata->mcomp[cnum].label)));
                    gtk_label_set_text(GTK_LABEL(mdata->mpos[i].label), buf);

#if (GTKVER == 3)
                    gdk_rgba_parse(&bg, "#7F7F7F");
                    gtk_widget_override_color(mdata->mcomp[cnum].label, GTK_STATE_FLAG_NORMAL, &bg);
                    gdk_rgba_parse(&bg, "#FFFFFF");
                    gtk_widget_override_background_color(mdata->mcomp[cnum].eventbox,
                                                         GTK_STATE_FLAG_NORMAL, &bg);
#else
                    gdk_color_parse("#7F7F7F", &bg);
                    gtk_widget_modify_fg(mdata->mcomp[cnum].label, GTK_STATE_NORMAL, &bg);
                    gdk_color_parse("#FFFFFF", &bg);
                    gtk_widget_modify_bg(mdata->mcomp[cnum].eventbox, GTK_STATE_NORMAL, &bg);
#endif
                }
            }
        }
        mdata->selected = 0;
    }

    return dialog;
}

void draw_all(GtkWidget *w, gpointer data)
{
    GtkTreeIter iter;
    gint i;

    gint num_cats = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(current_model), NULL);
    gint cnt = 0;

#if (GTKVER == 3)
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), wait_cursor);
#else
    gdk_window_set_cursor(GTK_WIDGET(main_window)->window, wait_cursor);
#endif
    for (i = 0; i <= number_of_tatamis; i++) {
        struct category_data *catdata = category_queue[i].next;
        while (catdata) {
            if (find_iter(&iter, catdata->index)) {
                gint n = gtk_tree_model_iter_n_children(current_model, &iter);
                if (n >= 1 && n <= NUM_COMPETITORS)
                    draw_one_category(&iter, n);

                cnt++;
                if (num_cats > 4)
                    progress_show(1.0*cnt/num_cats, "");
            }
            catdata = catdata->next;
        }
    }

#if (GTKVER == 3)
    gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(main_window)), NULL);
#else
    gdk_window_set_cursor(GTK_WIDGET(main_window)->window, NULL);
#endif
    SYS_LOG_INFO(_("All the categories has been drawn"));

    for (i = 1; i <= NUM_TATAMIS; i++) {
        progress_show(1.0*i/NUM_TATAMIS, "");
        update_matches(0, (struct compsys){0,0,0,0}, i);
    }

    progress_show(0.0, "");
}
