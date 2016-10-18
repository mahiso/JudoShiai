/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <math.h>
#include <locale.h>

#include <cairo.h>
#include <cairo-pdf.h>
#include <librsvg/rsvg.h>

#include "sqlite3.h"
#include "judoshiai.h"

static GtkTextBuffer *message_window(void);

//#define LOG(_f...) custlog(_f)
#define LOG(_f...) do { } while (0)

static void custlog(gchar *format, ...);

#define NUM_CUSTOM_BRACKETS 32
static gchar *custom_directory = NULL;
static struct custom_data *custom_brackets[NUM_CUSTOM_BRACKETS];
static guint hash_values[NUM_CUSTOM_BRACKETS];
static gint num_custom_brackets;

enum data_types {
    CD_VERSION = 0,     // 0
    CD_NAME_LONG,
    CD_NAME_SHORT,
    CD_COMPETITORS_MIN,
    CD_COMPETITORS_MAX,

    CD_NUM_RR_POOLS,    // 5
    CD_RR_POOL,
    CD_RR_POOL_NUM_MATCHES,
    CD_RR_POOL_MATCH,
    CD_NUM_MATCHES,

    CD_MATCH,           // 10
    CD_MATCH_COMP_1,
    CD_MATCH_COMP_2,
    CD_COMP_TYPE,
    CD_COMP_NUM,

    CD_COMP_POS,        // 15
    CD_COMP_PREV,
    CD_NUM_POSITIONS,
    CD_POSITIONS,
    CD_POSITIONS_TYPE,

    CD_POSITIONS_MATCH, // 20
    CD_POSITIONS_POS,
    CD_POSITIONS_REAL_POS,
    CD_NUM_B3_POOLS,
    CD_BEST_OF_3_PAIR,

    CD_NUM_GROUPS,      // 25
    CD_GROUP_NUM_COMP,
    CD_RR_POOL_NUM_COMPETITORS,
    CD_COMP
};

#define ENC_BUF_SIZE (1<<14)

static gchar enc_buffer[ENC_BUF_SIZE];
static gint enc_buf_cnt = 0;

#define ERROR(_f...) do { custlog(_f); return 0; } while (0)

static gchar *encode_custom_data(struct custom_data *d, gint *len)
{
    gint i, j;

#define PUT_BUF(_x) do { if (enc_buf_cnt < ENC_BUF_SIZE) enc_buffer[enc_buf_cnt++] = _x; else return NULL; } while (0)

#define PUT_SHORT(_x) do { if (enc_buf_cnt < ENC_BUF_SIZE-1) {          \
            enc_buffer[enc_buf_cnt++] = _x >> 8; enc_buffer[enc_buf_cnt++] = _x & 0xff; } \
        else return NULL; } while (0)

    enc_buf_cnt = 0;
    PUT_BUF(CD_VERSION);
    PUT_BUF(0);

    PUT_BUF(CD_NAME_LONG);
    PUT_BUF(64);
    memcpy(&enc_buffer[enc_buf_cnt], d->name_long, 64);
    enc_buf_cnt += 64;

    PUT_BUF(CD_NAME_SHORT);
    PUT_BUF(16);
    memcpy(&enc_buffer[enc_buf_cnt], d->name_short, 16);
    enc_buf_cnt += 16;

    PUT_BUF(CD_COMPETITORS_MIN);
    PUT_SHORT(d->competitors_min);

    PUT_BUF(CD_COMPETITORS_MAX);
    PUT_SHORT(d->competitors_max);

    if (d->num_round_robin_pools) {
        PUT_BUF(CD_NUM_RR_POOLS);
        PUT_BUF(d->num_round_robin_pools);
        for (i = 0; i < d->num_round_robin_pools; i++) {
            PUT_BUF(CD_RR_POOL);
            PUT_BUF(16);
            memcpy(&enc_buffer[enc_buf_cnt], d->round_robin_pools[i].name, 16);
            enc_buf_cnt += 16;
            PUT_BUF(CD_RR_POOL_NUM_MATCHES);
            PUT_SHORT(d->round_robin_pools[i].num_rr_matches);
            for (j = 0; j < d->round_robin_pools[i].num_rr_matches; j++) {
                PUT_BUF(CD_RR_POOL_MATCH);
                PUT_SHORT(d->round_robin_pools[i].rr_matches[j]);
            }

            PUT_BUF(CD_RR_POOL_NUM_COMPETITORS);
            PUT_SHORT(d->round_robin_pools[i].num_competitors);
            for (j = 0; j < d->round_robin_pools[i].num_competitors; j++) {
                gint k;
                PUT_BUF(CD_COMP);
                PUT_BUF(CD_COMP_TYPE);
                PUT_BUF(d->round_robin_pools[i].competitors[j].type);
                PUT_BUF(CD_COMP_NUM);
                PUT_SHORT(d->round_robin_pools[i].competitors[j].num);
                PUT_BUF(CD_COMP_POS);
                PUT_BUF(d->round_robin_pools[i].competitors[j].pos);
                PUT_BUF(CD_COMP_PREV);
                for (k = 0; k < 8; k++)
                    PUT_BUF(d->round_robin_pools[i].competitors[j].prev[k]);
            }
        }
    }

    if (d->num_best_of_three_pairs) {
        PUT_BUF(CD_NUM_B3_POOLS);
        PUT_BUF(d->num_best_of_three_pairs);
        for (i = 0; i < d->num_best_of_three_pairs; i++) {
            PUT_BUF(CD_BEST_OF_3_PAIR);
            PUT_BUF(16);
            memcpy(&enc_buffer[enc_buf_cnt], d->best_of_three_pairs[i].name, 16);
            enc_buf_cnt += 16;
            for (j = 0; j < 3; j++) {
                PUT_SHORT(d->best_of_three_pairs[i].matches[j]);
            }
        }
    }

    if (d->num_matches) {
        PUT_BUF(CD_NUM_MATCHES);
        PUT_SHORT(d->num_matches);
        for (i = 0; i < d->num_matches; i++) {
            PUT_BUF(CD_MATCH);

            PUT_BUF(CD_MATCH_COMP_1);
            PUT_BUF(CD_COMP_TYPE);
            PUT_BUF(d->matches[i].c1.type);
            PUT_BUF(CD_COMP_NUM);
            PUT_SHORT(d->matches[i].c1.num);
            PUT_BUF(CD_COMP_POS);
            PUT_BUF(d->matches[i].c1.pos);
            PUT_BUF(CD_COMP_PREV);
            for (j = 0; j < 8; j++)
                PUT_BUF(d->matches[i].c1.prev[j]);

            PUT_BUF(CD_MATCH_COMP_2);
            PUT_BUF(CD_COMP_TYPE);
            PUT_BUF(d->matches[i].c2.type);
            PUT_BUF(CD_COMP_NUM);
            PUT_SHORT(d->matches[i].c2.num);
            PUT_BUF(CD_COMP_POS);
            PUT_BUF(d->matches[i].c2.pos);
            PUT_BUF(CD_COMP_PREV);
            for (j = 0; j < 8; j++)
                PUT_BUF(d->matches[i].c2.prev[j]);
        }
    }

    if (d->num_positions) {
        PUT_BUF(CD_NUM_POSITIONS);
        PUT_SHORT(d->num_positions);
        for (i = 0; i < d->num_positions; i++) {
            PUT_BUF(CD_POSITIONS);
            PUT_BUF(CD_POSITIONS_TYPE);
            PUT_BUF(d->positions[i].type);
            PUT_BUF(CD_POSITIONS_MATCH);
            PUT_SHORT(d->positions[i].match);
            PUT_BUF(CD_POSITIONS_POS);
            PUT_BUF(d->positions[i].pos);
            PUT_BUF(CD_POSITIONS_REAL_POS);
            PUT_BUF(d->positions[i].real_contest_pos);
        }
    }

    if (d->num_groups) {
        PUT_BUF(CD_NUM_GROUPS);
        PUT_BUF(d->num_groups);
        for (i = 0; i < d->num_groups; i++) {
            PUT_BUF(CD_GROUP_NUM_COMP);
            PUT_BUF(d->groups[i].type);
            PUT_SHORT(d->groups[i].num_competitors);
            for (j = 0; j < d->groups[i].num_competitors; j++)
                PUT_SHORT(d->groups[i].competitors[j]);
        }
    }

    *len = enc_buf_cnt;
    return enc_buffer;
}

#define GET_BUF(_x) do { _x = buf[ix++]; } while (0)
#define GET_SHORT(_x) do { _x = (buf[ix] << 8) | buf[ix+1]; ix += 2; } while (0)
#define CHECK_BUF(_x) do { gint a = buf[ix++]; if (a != _x) ERROR("Expected %d, got %d", _x, a); } while (0)

#define get8 b[0]
#define get16 ((b[0] << 8) | b[1])

static gint get_competitor(unsigned char *b, competitor_bare_t *c, gboolean *err);

static gint get_byte(unsigned char *b, gint *v) {
    *v = get8;
    return 1;
}

static gint get_string(unsigned char *b, gchar *s) {
    gint len = get8; b++;
    memcpy(s, b, len);
    LOG("  string %s");
    return len + 1;
}

static gint get_rr_pool(unsigned char *b, round_robin_bare_t *pool, gboolean *err) {
    gint r = 0, i;
    
    gint ret = get_string(b, pool->name);
    b += ret; r += ret; 
    LOG("POOL NAME = %s", pool->name);
    gint t = get8; b++; r++;
    if (t != CD_RR_POOL_NUM_MATCHES)
        ERROR("Expected CD_RR_POOL_NUM_MATCHES, got %d", t);

    pool->num_rr_matches = get16; b += 2; r += 2;

    for (i = 0; i < pool->num_rr_matches; i++) {
        t = get8; b++; r++;
        if (t == CD_RR_POOL_MATCH) {
            pool->rr_matches[i] = get16;
            b += 2; r += 2;
            LOG("rr match %d: %d", i, pool->rr_matches[i]);
        } else ERROR("Expected CD_RR_POOL_MATCH, got %d", t);
    }

    t = get8; b++; r++;
    if (t != CD_RR_POOL_NUM_COMPETITORS)
        ERROR("Expected CD_RR_POOL_NUM_COMPETITORS, got %d", t);

    pool->num_competitors = get16; b += 2; r += 2;

    for (i = 0; i < pool->num_competitors; i++) {
        t = get8; b++; r++;
        if (t == CD_COMP) {
            gint ret = get_competitor(b, &pool->competitors[i], err);
            b += ret; r += ret;
        } else ERROR("Expected CD_COMP, got %d", t);
    }

    LOG("num rr matches = %d, num comps = %d", pool->num_rr_matches, pool->num_competitors);
    return r;
}

static gint get_rr_pools(unsigned char *b, struct custom_data *d, gboolean *err) {
    gint i, r = 0;

    d->num_round_robin_pools = get8; b++; r++;

    for (i = 0; i < d->num_round_robin_pools; i++) {
        gint t = get8; b++; r++;
        if (t == CD_RR_POOL) {
            round_robin_bare_t *pool = &(d->round_robin_pools[i]);
            gint n = get_rr_pool(b, pool, err);
            b += n; r += n;
        }
    }
    LOG("num rr pools = %d", d->num_round_robin_pools);
    return r;
}

static gint get_competitor(unsigned char *b, competitor_bare_t *c, gboolean *err) {
    gint r = 0, i;

    while (TRUE) {
        gint t = get8; b++; r++;
        switch (t) {
        case CD_COMP_TYPE:
            c->type = get8; b++; r++;
            break;
        case CD_COMP_NUM:
            c->num = get16; b += 2; r += 2;
            break;
        case CD_COMP_POS:
            c->pos = get8; b++; r++;
            break;
        case CD_COMP_PREV:
            for (i = 0; i < 8; i++) {
                c->prev[i] = get8; b++; r++;
            }
            break;
        default:
            b--; r--;
            LOG("comp type=%d num=%d pos=%d (1)", c->type, c->num, c->pos);
            return r;
        }
    }

    LOG("comp type=%d num=%d pos=%d (2)", c->type, c->num, c->pos);
    return 0;
}

static gint get_matches(unsigned char *b, struct custom_data *d, gboolean *err) {
    gint i, r = 0;

    d->num_matches = get16; b += 2; r += 2;

    for (i = 0; i < d->num_matches; i++) {
        gint t = get8; b++; r++;
        if (t != CD_MATCH) ERROR("Expected CD_MATCH, got %d", t);
        t = get8; b++; r++;
        if (t != CD_MATCH_COMP_1) ERROR("Expected CD_MATCH_COMP_1, got %d", t);
        gint ret = get_competitor(b, &(d->matches[i].c1), err);
        b += ret; r += ret;
        t = get8; b++; r++;
        if (t != CD_MATCH_COMP_2) ERROR("Expected CD_MATCH_COMP_2, got %d", t);
        ret = get_competitor(b, &(d->matches[i].c2), err);
        b += ret; r += ret;
    }

    LOG("num matches = %d", d->num_matches);
    return r;
}

static gint get_positions(unsigned char *b, struct custom_data *d, gboolean *err) {
    gint i, r = 0;

    d->num_positions = get16; b += 2; r += 2;

    for (i = 0; i < d->num_positions; i++) {
        gboolean run = TRUE;

        gint t = get8; b++; r++;
        if (t != CD_POSITIONS) ERROR("Expected CD_POSITIONS_TYPE, got %d", t);

        while (run) {
            t = get8; b++; r++;
            switch(t) {
            case CD_POSITIONS_TYPE:
                d->positions[i].type = get8; b++; r++;
                break;
            case CD_POSITIONS_MATCH:
                d->positions[i].match = get16; b += 2; r += 2;
                break;
            case CD_POSITIONS_POS:
                d->positions[i].pos = get8; b++; r++;
                break;
            case CD_POSITIONS_REAL_POS:
                d->positions[i].real_contest_pos = get8; b++; r++;
                break;
            default:
                b--; r--;
                run = FALSE;
            }
        }
        LOG("position type=%d match=%d pos=%d\n", d->positions[i].type,
            d->positions[i].match, d->positions[i].pos);
    }

    LOG("num positions %d", d->num_positions);
    return r;
}

static gint get_best_of_three_pairs(unsigned char *b, struct custom_data *d, gboolean *err) {
    gint i, j, r = 0;

    d->num_best_of_three_pairs = get8; b++; r++;

    for (i = 0; i < d->num_best_of_three_pairs; i++) {
        gint t = get8; b++; r++;
        if (t != CD_BEST_OF_3_PAIR) ERROR("Expected CD_BEST_OF_3_PAIR, got %d", t);
        gint ret = get_string(b, d->best_of_three_pairs[i].name);
        b += ret; r += ret; 
        LOG("PAIR NAME = %s", d->best_of_three_pairs[i].name);
        for (j = 0; j < 3; j++) {
            d->best_of_three_pairs[i].matches[j] = get16; b += 2; r += 2;
        }
    }

    LOG("num best of 3 %d", d->num_best_of_three_pairs);
    return r;
}

static gint get_groups(unsigned char *b, struct custom_data *d, gboolean *err) {
    gint i, j, r = 0;

    d->num_groups = get8; b++; r++;

    for (i = 0; i < d->num_groups; i++) {
        gint t = get8; b++; r++;
        if (t != CD_GROUP_NUM_COMP) ERROR("Expected CD_GROUP_NUM_COMP, got %d", t);
        d->groups[i].type = get8; b++; r++;
        d->groups[i].num_competitors = get16; b += 2; r += 2;
        for (j = 0; j < d->groups[i].num_competitors; j++) {
            d->groups[i].competitors[j] = get16; b += 2; r += 2;
            LOG("group %d: comp %d = %d", i, j, d->groups[i].competitors[j]);
        }
    }

    LOG("num groups %d, type %d", d->num_groups, d->groups[0].type);
    return r;
}

static struct custom_data *decode_custom_data(unsigned char *buf, int len, struct custom_data *d)
{
    gint ix = 0;
    gboolean err = FALSE;
    LOG("%s: len=%d", __FUNCTION__, len);

    while (ix < len && !err) {
        LOG("buf[%d]=0x%02x=%d", ix, buf[ix], buf[ix]);

        switch (buf[ix++]) {
        case CD_VERSION: { 
            gint ver;
            ix += get_byte(&(buf[ix]), &ver);
            if (ver != 0) ERROR("Wrong version %d", ver);
        }
            break;
        case CD_NAME_LONG:
            LOG("CD_NAME_LONG");
            ix += get_string(&(buf[ix]), d->name_long);
            break;
       case CD_NAME_SHORT:
            LOG("CD_NAME_SHORT");
            ix += get_string(&(buf[ix]), d->name_short);
            break;
        case CD_COMPETITORS_MIN:
            GET_SHORT(d->competitors_min);
            LOG("CD_COMPETITORS_MIN = %d", d->competitors_min);
            break;
        case CD_COMPETITORS_MAX:
            GET_SHORT(d->competitors_max);
            LOG("CD_COMPETITORS_MAX = %d", d->competitors_max);
            break;
        case CD_NUM_RR_POOLS:
            LOG("CD_NUM_RR_POOLS");
            ix += get_rr_pools(&(buf[ix]), d, &err);
            break;
        case CD_NUM_MATCHES:
            LOG("CD_NUM_MATCHES");
            ix += get_matches(&(buf[ix]), d, &err);
            break;
        case CD_NUM_POSITIONS:
            LOG("CD_NUM_POSITIONS");
            ix += get_positions(&(buf[ix]), d, &err);
            break;
        case CD_NUM_B3_POOLS:
            ix += get_best_of_three_pairs(&(buf[ix]), d, &err);
            break;
        case CD_NUM_GROUPS:
            ix += get_groups(&(buf[ix]), d, &err);
            break;
        default:
            g_print("Unknown CD type %d\n", buf[ix-1]);
            return NULL;
        }
    }

    if (err) return NULL;
    return d;
}

/**************************************************************/

struct custom_data *get_custom_table(guint table)
{
    gint i;

    for (i = 0; i < num_custom_brackets; i++)
        if (hash_values[i] == table)
            return custom_brackets[i];

    return NULL;
}

guint get_custom_table_number_by_competitors(gint num_comp)
{
    gint i;
    for (i = 0; i < num_custom_brackets; i++) {
        if (custom_brackets[i]->competitors_min <= num_comp &&
            custom_brackets[i]->competitors_max >= num_comp)
            return hash_values[i];
    }
    return 0;
}

void show_msg(GtkTextBuffer *buf, gchar *tagname, gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);

    if (tagname) {
        GtkTextIter end;
        gtk_text_buffer_get_iter_at_offset (buf, &end, -1);
        gtk_text_buffer_insert_with_tags_by_name(buf, &end, text, -1, tagname, NULL);
    } else {
        gtk_text_buffer_insert_at_cursor(buf, text, -1);
    }
    g_free(text);
}

void read_custom_files(void)
{
    gint i, n = 0;
    gchar *fullname;

    if (custom_directory == NULL)
        return;

    struct custom_data *d = g_malloc0(sizeof(struct custom_data));
    GDir *dir = g_dir_open(custom_directory, 0, NULL);

    GtkTextBuffer *msgbuf = message_window();
    show_msg(msgbuf, NULL, "Reading directory %s\n", custom_directory);

    if (dir) {
        db_create_blob_table();
        const gchar *fname = g_dir_read_name(dir);
        while (fname) {
            char *p = strstr(fname, ".txt");
            fullname = g_build_filename(custom_directory, fname, NULL);
            if (p && p[4] == 0) {
                LOG("\n---------- %s ------------", fname);
                show_msg(msgbuf, "bold", "Reading file %s\n", fname);

                char *msg = read_custom_category(fullname, d);
                if (msg) {
                    g_free(fullname);
                    fname = g_dir_read_name(dir);
                    show_msg(msgbuf, "red", "  %s\n", msg);
                    //show_message(msg);
                    continue;
                }
                guint hash = g_str_hash(d->name_short) << 5;
                db_delete_blob_line(hash);
                gint length;
                gchar *encoded = encode_custom_data(d, &length);
		if (length == 0)
		    show_msg(msgbuf, "red", "  Problem with encoding!\n");
                db_write_blob(hash, (void *)encoded, length);

                gchar buf[64];
                snprintf(buf, sizeof(buf) - 8, "%s", fname);
                p = strstr(buf, ".txt");
                if (p) *p = 0;
                else p = buf + strlen(buf);
                i = 1;
                while (1) {
                    sprintf(p, "-%d.svg", i);
                    gchar *svgname = g_build_filename(custom_directory, buf, NULL);
                    void *data = NULL;
                    gsize datalen;
                    GError *error = NULL;
                    gboolean ok = g_file_get_contents(svgname, (gchar **)&data, &datalen, &error);
                    if (!ok) {
                        if (i == 1) {
                            show_msg(msgbuf, "red", "  %s\n", error->message);
                            show_msg(msgbuf, NULL, "  Trying to read an example file\n");
                            sprintf(p, ".svg");
                            g_free(svgname);
                            svgname = g_build_filename(custom_directory, buf, NULL);
                            g_error_free(error);
                            error = NULL;
                            ok = g_file_get_contents(svgname, (gchar **)&data, &datalen, &error);
                            if (!ok) {
                                show_msg(msgbuf, "red", "  %s\n", error->message);
                                show_msg(msgbuf, NULL, "  Creating example file\n");
                                char *args[2] = {"", fullname};

                                // obtain the existing locale name for numbers    
                                char *old_locale = setlocale(LC_NUMERIC, NULL);
                                // set locale that uses points as decimal separator
                                setlocale(LC_NUMERIC, "en_US.UTF-8");
                                make_svg_file(2, args);
                                // set the locale back
                                setlocale(LC_NUMERIC, old_locale);

                                g_error_free(error);
                                error = NULL;
                                ok = g_file_get_contents(svgname, (gchar **)&data, &datalen, &error);
                                if (!ok) show_msg(msgbuf, "red", "  %s: %s\n", error->message);
                            }
                        }
                        if (error) g_error_free(error);
                        g_free(svgname);
                        svgname = NULL;
                        if (!ok) 
                            break;
                    }
                    g_free(svgname);
                    db_delete_blob_line(hash | i);
                    db_write_blob(hash | i, data, datalen);
                    g_free(data);
                    data = NULL;
                    i++;
                }
                show_msg(msgbuf, NULL, "  %d SVG pages found\n", i-1);
                n++;
            } // if
            g_free(fullname);
            fname = g_dir_read_name(dir);
        } // while fname

        g_dir_close(dir);
    } // if dir

    g_free(d);

    read_custom_from_db();
}

void read_custom_from_db(void)
{
    gint n, page, rows;
    int len;
    unsigned char *data;

    num_custom_brackets = 0;

    // find out the blob keys
    if ((rows = db_get_table("SELECT \"key\" FROM blobs")) > 0) {
        for (n = 0; n < rows; n++) {
            guint a = atoi(db_get_data(n, "key"));
            if ((a & 0x1f) == 0 && num_custom_brackets < NUM_CUSTOM_BRACKETS)
                hash_values[num_custom_brackets++] = a;
        }
    }
    if (rows >= 0) db_close_table();

    LOG("%d custom brackets", num_custom_brackets);
    for (n = 0; n < num_custom_brackets; n++) {
        unsigned char *blob = NULL;
        db_read_blob(hash_values[n], &blob, &len);
        if (blob) {
            custom_brackets[n] = g_malloc0(sizeof(struct custom_data));
            decode_custom_data(blob, len, custom_brackets[n]);
            free(blob);
        }

        if (custom_brackets[n] == NULL) {
            num_custom_brackets = n;
            g_print("ERROR: cannot read blob %d\n", n);
            break;
        }

        for (page = 1; page <= 10; page++) {
            db_read_blob(hash_values[n] | page, &data, &len);
            if (data == NULL) {
                if (page == 1) {
                    g_print("ERROR: Cannot read svg data from db\n");
                }
                break;
            }
            add_custom_svg((gchar *)data, len, hash_values[n], page);
#if 0
            gchar buf[32];
            sprintf(buf, "%x.svg", (n << 8) | page);
            if (g_file_set_contents(buf, (gchar *)data, len, NULL))
                g_print("g_file_set_contents %s OK len=%d\n", buf, len);
            else
                g_print("g_file_set_contents %s NOK len=%d\n", buf, len);
#endif
        }
    }
}

void select_custom_dir(GtkWidget *menu_item, gpointer data)
{
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new(_("Choose a directory"),
                                         NULL,
                                         GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);

    if (custom_directory)
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
                                            custom_directory);
    else {
        gchar *dirname = g_build_filename(installation_dir, "custom-examples", NULL);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dirname);
        g_free(dirname);
    }

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        return;
    }

    g_free(custom_directory);
    custom_directory = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    g_key_file_set_string(keyfile, "preferences", "customdir", custom_directory);

    gtk_widget_destroy (dialog);

    read_custom_files();
}

/* m:        Array of matches. Match #1 is in pos m[1]. m[0] is unused.
 * table:    Number of custom table.
 * pos:      Position of the competitor (1, 2, 3, 4, 5, 6...)
 * real_pos: Real position of the competitor like 1, 2, 3, 3, 5, 5...
 */

#define WINNER(_a) (COMP_1_PTS_WIN(m[_a]) ? m[_a].blue :                \
                    (COMP_2_PTS_WIN(m[_a]) ? m[_a].white :		\
		       (m[_a].blue == GHOST ? m[_a].white :		\
			(m[_a].white == GHOST ? m[_a].blue :            \
			 (db_has_hansokumake(m[_a].blue) ? m[_a].white : \
			  (db_has_hansokumake(m[_a].white) ? m[_a].blue : NO_COMPETITOR))))))

#define LOSER(_a) (COMP_1_PTS_WIN(m[_a]) ? m[_a].white :                \
                   (COMP_2_PTS_WIN(m[_a]) ? m[_a].blue :                \
		      (m[_a].blue == GHOST ? m[_a].blue :		\
		       (m[_a].white == GHOST ? m[_a].white :		\
			(db_has_hansokumake(m[_a].blue) ? m[_a].blue :  \
			 (db_has_hansokumake(m[_a].white) ? m[_a].white : NO_COMPETITOR))))))

gint get_custom_pos(struct custom_matches *cm, gint table, gint pos, gint *real_res)
{
    struct custom_data *cd = get_custom_table(table);
    struct match *m = cm->m;
    gint ix = 0;

    if (!cd || pos < 1 || pos > cd->num_positions)
        return 0;

    if (cd->positions[pos-1].type == COMP_TYPE_MATCH) {
        g_print("posarg=%d pos=%d match=%d\n", pos, cd->positions[pos-1].pos, cd->positions[pos-1].match);
        if (cd->positions[pos-1].pos == 1)
            ix = WINNER(cd->positions[pos-1].match);
        else
            ix = LOSER(cd->positions[pos-1].match);
    } else if (cd->positions[pos-1].type == COMP_TYPE_ROUND_ROBIN) {
        gint poolnum = cd->positions[pos-1].match;
        if (poolnum <= 0) return 0;
        //g_print("*** pos=%d poolnum=%d\n", pos, poolnum);
        struct pool *p = &cm->pm[poolnum-1];
        //g_print("*** finished=%d\n", p->finished);
        if (!p->finished) return 0;
        ix = p->competitors[p->competitors[pos-1].position].index;
    } else if (cd->positions[pos-1].type == COMP_TYPE_BEST_OF_3) {
        gint pairnum = cd->positions[pos-1].match;
        if (pairnum <= 0) return 0;
        struct bestof3 *p = &cm->best_of_three[pairnum-1];
        if (!p->finished) return 0;
        if (pos == 1)
            ix = p->winner;
        else
            ix = p->loser;
    }

    if (real_res && cd->positions[pos-1].real_contest_pos)
        *real_res = cd->positions[pos-1].real_contest_pos;

    if (ix < 10) ix = 0;
    return ix;
}

static void custlog(gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);
    g_print("CUSTLOG: %s\n", text);
    g_free(text);
}

#define SIZEX 400
#define SIZEY 400

static void ok_cb(GtkWidget *widget, 
                  GdkEvent *event,
                  gpointer data)
{
    GtkWidget *win = data;
    gtk_widget_destroy(win);
}
static gboolean delete_event_message( GtkWidget *widget,
                                      GdkEvent  *event,
                                      gpointer   data )
{
    return FALSE;
}

static void destroy_message( GtkWidget *widget,
                             gpointer   data )
{
}

static GtkTextBuffer *message_window(void)
{
    GtkTextBuffer *buffer;
    GtkWidget *vbox, *result, *ok;
    GtkWindow *window = GTK_WINDOW(gtk_window_new(GTK_WINDOW_TOPLEVEL));
    gtk_window_set_title(GTK_WINDOW(window), _("Messages"));
    gtk_widget_set_size_request(GTK_WIDGET(window), SIZEX, SIZEY);
    gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(window), TRUE);

    vbox = gtk_grid_new();
    gtk_container_add (GTK_CONTAINER (window), vbox);

    result = gtk_text_view_new();
    ok = gtk_button_new_with_label(_("OK"));

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 10);
#if (GTKVER == 3) && GTK_CHECK_VERSION(3,8,0)
    gtk_container_add(GTK_CONTAINER(scrolled_window), result);
#else
    gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window), result);
#endif

    gtk_grid_attach(GTK_GRID(vbox), scrolled_window, 0, 0, 1, 1);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_grid_attach(GTK_GRID(vbox), ok,            0, 2, 1, 1);
    gtk_widget_show_all(GTK_WIDGET(window));
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(result));

    PangoFontDescription *font_desc = pango_font_description_from_string("Courier 10");
    gtk_widget_override_font (GTK_WIDGET(result), font_desc);
    pango_font_description_free (font_desc);

    g_signal_connect (G_OBJECT (window), "delete_event",
                      G_CALLBACK (delete_event_message), NULL);
    g_signal_connect (G_OBJECT (window), "destroy",
                      G_CALLBACK (destroy_message), NULL);

    g_signal_connect(G_OBJECT(ok), "button-press-event", G_CALLBACK(ok_cb), window);

    gtk_text_buffer_create_tag (buffer, "red",
                                "foreground", "red", NULL);  
    gtk_text_view_set_editable(GTK_TEXT_VIEW(result), FALSE);

    gtk_text_buffer_create_tag (buffer, "bold",
                                "weight", PANGO_WEIGHT_BOLD, NULL);  
    gtk_text_buffer_create_tag (buffer, "underline",
			      "underline", PANGO_UNDERLINE_SINGLE, NULL);
    gtk_text_buffer_create_tag (buffer, "heading",
                                "weight", PANGO_WEIGHT_BOLD,
                                "size", 15 * PANGO_SCALE,
                                NULL);
    return buffer;
}

