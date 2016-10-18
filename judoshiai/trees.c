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

#include "judoshiai.h"

struct competitor_hash_data {
    gint hash;
    time_t last_match_time;
};

struct html_data {
    gint utf8;
    gchar *html;
};

avl_tree *categories_tree = NULL;

avl_tree *competitors_tree = NULL;
avl_tree *competitors_hash_tree = NULL;

avl_tree *club_tree = NULL;
avl_tree *club_name_tree = NULL;

avl_tree *html_tree = NULL;

struct category_data category_queue[NUM_TATAMIS+1];

static int categories_avl_compare(void *compare_arg, void *a, void *b)
{
    struct category_data *a1 = a;
    struct category_data *b1 = b;
    return a1->index - b1->index;
}

static int competitors_avl_compare(void *compare_arg, void *a, void *b)
{
    struct competitor_data *a1 = a;
    struct competitor_data *b1 = b;
    return a1->index - b1->index;
}

static int competitors_hash_avl_compare(void *compare_arg, void *a, void *b)
{
    struct competitor_hash_data *a1 = a;
    struct competitor_hash_data *b1 = b;
    return a1->hash - b1->hash;
}

static int club_avl_compare(void *compare_arg, void *a, void *b)
{
    struct club_data *a1 = a;
    struct club_data *b1 = b;

    if (a1 == NULL || a1->name == NULL)
        return -1;
    if (b1 == NULL || b1->name == NULL)
        return 1;

    if (club_text == CLUB_TEXT_CLUB)
	return sort_by_name(a1->name, b1->name);

    gint ret = sort_by_name(a1->country, b1->country);
    if (club_text == CLUB_TEXT_COUNTRY)
	return ret;

    if (ret == 0)
	return sort_by_name(a1->name, b1->name);

    return ret;
}

static int club_name_avl_compare(void *compare_arg, void *a, void *b)
{
    struct club_name_data *a1 = a;
    struct club_name_data *b1 = b;

    if (a1 == NULL || a1->name == NULL)
        return -1;
    if (b1 == NULL || b1->name == NULL)
        return 1;

    return sort_by_name(a1->name, b1->name);
}

static int html_avl_compare(void *compare_arg, void *a, void *b)
{
    struct html_data *a1 = a;
    struct html_data *b1 = b;
    return a1->utf8 - b1->utf8;
}

static int free_avl_key(void *key)
{
    g_free(key);
    return 1;
}

static int free_club_avl_key(void *key)
{
    struct club_data *key1 = key;
    g_free(key1->name);
    g_free(key1->country);
    g_free(key);
    return 1;
}

static int free_club_name_avl_key(void *key)
{
    struct club_name_data *key1 = key;
    g_free(key1->name);
    g_free(key1->abbreviation);
    g_free(key1->address);
    g_free(key);
    return 1;
}

static gint compare_categories_order(struct category_data *cat1, struct category_data *cat2)
{
    if (cat1->group < cat2->group)
        return -1;
    if (cat1->group > cat2->group)
        return 1;

    if (cat1->index < cat2->index)
        return -1;
    if (cat1->index > cat2->index)
        return 1;

    return 0;
}

void set_category_to_queue(struct category_data *data)
{
    struct category_data *queue;

    /* Remove from the old queue */
    if (data->prev) {
        struct category_data *p = data->prev;
        if (data->next) {
            struct category_data *n = data->next;
            p->next = n;
            n->prev = p;
        } else {
            p->next = NULL;
        }
    }

    if (data->deleted & DELETED) {
        data->next = data->prev = NULL;
        return;
    }

    queue = &category_queue[data->tatami];

    while (queue) {
        if (queue->next == NULL) {
            queue->next = data;
            data->prev = queue;
            data->next = NULL;
            return;
        } 

        if (compare_categories_order(data, queue->next) < 0) {
            data->prev = queue;
            data->next = queue->next;
            queue->next->prev = data;
            queue->next = data;
            return;
        }

        queue = queue->next;
    }
}

void avl_set_category(gint index, const gchar *category, gint tatami, 
                      gint group, struct compsys system, gint deleted)
{
    struct category_data *data;
    void *data1;

    if (!categories_tree)
        return;

    data = g_malloc(sizeof(*data));
    memset(data, 0, sizeof(*data));
        
    data->index = index;
    strncpy(data->category, category, sizeof(data->category)-1);
    data->tatami = tatami;
    data->group = group;
    data->system = system;
    data->rest_time = get_category_rest_time(category);
    data->deleted = deleted;

    if (avl_get_by_key(categories_tree, data, &data1) == 0) {
        /* data exists */
        avl_delete(categories_tree, data, free_avl_key);
    }
    avl_insert(categories_tree, data);

    set_category_to_queue(data);
}

struct name_status {
    const gchar *name;
    gint status;
};

static int iter_categories_2(void *key, void *iter_arg)
{
    struct category_data *data = key;
    struct name_status *arg = iter_arg;
    if (strcmp(data->category, arg->name) == 0) {
        arg->status = data->match_status;
        return 1;
    }
    return 0;
}

gint avl_get_category_status_by_name(const gchar *name)
{
    struct name_status arg;
    arg.name = name;
    arg.status = 0;

    if (!categories_tree)
        return 0;

    avl_iterate_inorder(categories_tree, iter_categories_2, &arg);

    return arg.status;
}

static int iter_categories(void *key, void *iter_arg)
{
    struct category_data *data = key;
    data->rest_time = get_category_rest_time(data->category);
    return 0;
}

void avl_set_category_rest_times(void)
{
    if (!categories_tree)
        return;

    avl_iterate_inorder(categories_tree, iter_categories, NULL);
}

struct category_data *avl_get_category(gint index)
{
    struct category_data data;
    void *data1;

    if (!categories_tree)
        return NULL;

    data.index = index & MATCH_CATEGORY_MASK;
    if (avl_get_by_key(categories_tree, &data, &data1) == 0) {
        return data1;
    }
    if (index) {
        //g_print("cannot find category index %d!\n", index);
        //assert(0);
    }
    return NULL;
}

#if 0
struct category_dataxxx!!! *avl_get_competitor(gint index)
{
    struct competitor_data data;
    void *data1;

    if (!competitors_tree)
        return NULL;

    data.index = index;
    if (avl_get_by_key(competitors_tree, &data, &data1) == 0) {
        return data1;
    }
    g_print("cannot find competitor index %d!\n", index);
    //assert(0);
    return NULL;
}
#endif

gint avl_get_competitor_hash(gint index)
{
    struct competitor_data data;
    void *data1;

    if (index == 0)
        return 0;

    if (!competitors_tree)
        return 0;

    data.index = index;
    if (avl_get_by_key(competitors_tree, &data, &data1) == 0) {
        return ((struct competitor_data *)data1)->hash;
    }

    g_print("cannot find index %d!\n", index);
    return 0;
}

void avl_set_competitor(gint index, struct judoka *j)
{
    const gchar *first = j->first;
    const gchar *last = j->last;
    struct competitor_data *data;
    struct competitor_hash_data *datah;
    void *data1;

    if (!competitors_tree)
        return;

    if (!competitors_hash_tree)
        return;

    data = g_malloc(sizeof(*data));
    memset(data, 0, sizeof(*data));
	
    datah = g_malloc(sizeof(*datah));
    memset(datah, 0, sizeof(*datah));
	
    data->index = index;
    data->status = j->deleted;
    gint crc1 = pwcrc32((guchar *)last, strlen(last));
    gint crc2 = pwcrc32((guchar *)first, strlen(first));
    data->hash = crc1 ^ crc2;
	
    datah->hash = data->hash;
    datah->last_match_time = 0;

    if (avl_get_by_key(competitors_tree, data, &data1) == 0) {
        /* data exists */
        avl_delete(competitors_tree, data, free_avl_key);
    }
    avl_insert(competitors_tree, data);

    if (avl_get_by_key(competitors_hash_tree, datah, &data1) == 0) {
        /* data exists */
        free_avl_key(datah);
    } else {
        avl_insert(competitors_hash_tree, datah);
    }

    // data for coach display
    write_competitor_for_coach_display(j);
}

void avl_set_competitor_last_match_time(gint index)
{
    struct competitor_hash_data data, *data1;

    if (!competitors_hash_tree)
        return;

    gint hash = avl_get_competitor_hash(index);
    if (hash == 0)
        return;

    data.hash = hash;
    if (avl_get_by_key(competitors_hash_tree, &data, (void **)&data1) == 0) {
        data1->last_match_time = time(NULL);
    }
}

void avl_reset_competitor_last_match_time(gint index)
{
    struct competitor_hash_data data, *data1;

    if (!competitors_hash_tree)
        return;

    gint hash = avl_get_competitor_hash(index);
    if (hash == 0)
        return;

    data.hash = hash;
    if (avl_get_by_key(competitors_hash_tree, &data, (void **)&data1) == 0) {
        data1->last_match_time = 0;
    }
}

void avl_set_competitor_status(gint index, gint status)
{
    struct competitor_data data, *data1;

    if (!competitors_tree)
        return;

    data.index = index;
    if (avl_get_by_key(competitors_tree, &data, (void **)&data1) == 0) {
        data1->status = status;
    }
}

gint avl_get_competitor_status(gint index)
{
    struct competitor_data data, *data1;

    if (!competitors_tree)
        return 0;

    data.index = index;
    if (avl_get_by_key(competitors_tree, &data, (void **)&data1) == 0) {
        return data1->status;
    }

    return 0;
}

void avl_set_competitor_position(gint index, gint position)
{
    struct competitor_data data, *data1;

    if (!competitors_tree)
        return;

    data.index = index;
    if (avl_get_by_key(competitors_tree, &data, (void **)&data1) == 0) {
        data1->position = position;
    }
}

gint avl_get_competitor_position(gint index)
{
    struct competitor_data data, *data1;

    if (!competitors_tree)
        return 0;

    data.index = index;
    if (avl_get_by_key(competitors_tree, &data, (void **)&data1) == 0) {
        return data1->position;
    }

    return 0;
}

static int iter_competitors(void *key, void *iter_arg)
{
    struct competitor_data *data = key;
    data->position = 0;
    return 0;
}

void avl_init_competitor_position(void)
{
    if (!competitors_tree)
        return;

    avl_iterate_inorder(competitors_tree, iter_competitors, NULL);
}

time_t avl_get_competitor_last_match_time(gint index)
{
    struct competitor_hash_data data, *data1;

    if (!competitors_hash_tree)
        return 0;

    gint hash = avl_get_competitor_hash(index);
    if (hash == 0)
        return 0;

    data.hash = hash;
    if (avl_get_by_key(competitors_hash_tree, &data, (void **)&data1) == 0) {
        return data1->last_match_time;
    }

    return 0;
}

const struct {
    gint utf8;
    gchar *html;
} utf2html[] = {
    {0xc2a0,	"&#160;"},
    {0xc2a1,	"&#161;"},
    {0xc2a2,	"&#162;"},
    {0xc2a3,	"&#163;"},
    {0xc2a4,	"&#164;"},
    {0xc2a5,	"&#165;"},
    {0xc2a6,	"&#166;"},
    {0xc2a7,	"&#167;"},
    {0xc2a8,	"&#168;"},
    {0xc2a9,	"&#169;"},
    {0xc2aa,	"&#170;"},
    {0xc2ab,	"&#171;"},
    {0xc2ac,	"&#172;"},
    {0xc2ad,	"&#173;"},
    {0xc2ae,	"&#174;"},
    {0xc2af,	"&#175;"},
    {0xc2b0,	"&#176;"},
    {0xc2b1,	"&#177;"},
    {0xc2b2,	"&#178;"},
    {0xc2b3,	"&#179;"},
    {0xc2b4,	"&#180;"},
    {0xc2b5,	"&#181;"},
    {0xc2b6,	"&#182;"},
    {0xc2b7,	"&#183;"},
    {0xc2b8,	"&#184;"},
    {0xc2b9,	"&#185;"},
    {0xc2ba,	"&#186;"},
    {0xc2bb,	"&#187;"},
    {0xc2bc,	"&#188;"},
    {0xc2bd,	"&#189;"},
    {0xc2be,	"&#190;"},
    {0xc2bf,	"&#191;"},
    {0xc380,	"&#192;"},
    {0xc381,	"&#193;"},
    {0xc382,	"&#194;"},
    {0xc383,	"&#195;"},
    {0xc384,	"&#196;"},
    {0xc385,	"&#197;"},
    {0xc386,	"&#198;"},
    {0xc387,	"&#199;"},
    {0xc388,	"&#200;"},
    {0xc389,	"&#201;"},
    {0xc38a,	"&#202;"},
    {0xc38b,	"&#203;"},
    {0xc38c,	"&#204;"},
    {0xc38d,	"&#205;"},
    {0xc38e,	"&#206;"},
    {0xc38f,	"&#207;"},
    {0xc390,	"&#208;"},
    {0xc391,	"&#209;"},
    {0xc392,	"&#210;"},
    {0xc393,	"&#211;"},
    {0xc394,	"&#212;"},
    {0xc395,	"&#213;"},
    {0xc396,	"&#214;"},
    {0xc397,	"&#215;"},
    {0xc398,	"&#216;"},
    {0xc399,	"&#217;"},
    {0xc39a,	"&#218;"},
    {0xc39b,	"&#219;"},
    {0xc39c,	"&#220;"},
    {0xc39d,	"&#221;"},
    {0xc39e,	"&#222;"},
    {0xc39f,	"&#223;"},
    {0xc3a0,	"&#224;"},
    {0xc3a1,	"&#225;"},
    {0xc3a2,	"&#226;"},
    {0xc3a3,	"&#227;"},
    {0xc3a4,	"&#228;"},
    {0xc3a5,	"&#229;"},
    {0xc3a6,	"&#230;"},
    {0xc3a7,	"&#231;"},
    {0xc3a8,	"&#232;"},
    {0xc3a9,	"&#233;"},
    {0xc3aa,	"&#234;"},
    {0xc3ab,	"&#235;"},
    {0xc3ac,	"&#236;"},
    {0xc3ad,	"&#237;"},
    {0xc3ae,	"&#238;"},
    {0xc3af,	"&#239;"},
    {0xc3b0,	"&#240;"},
    {0xc3b1,	"&#241;"},
    {0xc3b2,	"&#242;"},
    {0xc3b3,	"&#243;"},
    {0xc3b4,	"&#244;"},
    {0xc3b5,	"&#245;"},
    {0xc3b6,	"&#246;"},
    {0xc3b7,	"&#247;"},
    {0xc3b8,	"&#248;"},
    {0xc3b9,	"&#249;"},
    {0xc3ba,	"&#250;"},
    {0xc3bb,	"&#251;"},
    {0xc3bc,	"&#252;"},
    {0xc3bd,	"&#253;"},
    {0xc3be,	"&#254;"},
    {0xc3bf,	"&#255;"},
    {0, NULL}
};

static void init_html_tree(void)
{
    int i;

    if (html_tree)
        avl_tree_free(html_tree, free_avl_key);
    html_tree = avl_tree_new(html_avl_compare, NULL);

    if (!html_tree)
        return;
	
    for (i = 0; utf2html[i].utf8; i++) {
        struct html_data *data = g_malloc(sizeof(*data));
        if (!data) return;
        data->utf8 = utf2html[i].utf8;
        data->html = utf2html[i].html;
        avl_insert(html_tree, data);
    }
}

#if 0
static gchar *avl_get_html(gint utf8)
{
    struct html_data data, *data1;
    static gchar buf[16];

    if (!html_tree)
        return "";

    data.utf8 = utf8;
    if (avl_get_by_key(html_tree, &data, (void **)&data1) == 0) {
        return data1->html;
    }

    sprintf(buf, "|%04x|", utf8);
    return buf;
}
#endif

const gchar *utf8_to_html(const gchar *txt)
{
    if (!txt)
        return "";

    /* Cyrillic text would generate far too long strings.
       Output is UTF-8 based from version 2.2 onwards. */
    if (g_utf8_validate(txt, -1, NULL) == FALSE) {
        SYS_LOG_WARNING("Not UTF-8 string: %s\n", txt);
        return txt;
    }
    return txt;

#if 0
    if (g_utf8_validate(txt, -1, NULL) == FALSE) {
        SYS_LOG_WARNING("Not UTF-8 string: %s\n", txt);
        return txt;
    }

    buffers[sel][0] = 0;

    while (*p) {
        if (*p < 128) {
            if (n < 63)
                buffers[sel][n++] = *p;
            buffers[sel][n] = 0;
        } else {
            n += snprintf(&buffers[sel][n], 64 - n,
                          "%s", avl_get_html((*p << 8) | *(p+1)));
            p++;
            if (*p == 0)
                break;
        }
        p++;
    }

    ret = buffers[sel];
    sel++;
    if (sel >= 16)
        sel = 0;

    return ret;
#endif
}

void init_trees(void)
{
    memset(&category_queue, 0, sizeof(category_queue));

    if (categories_tree)
        avl_tree_free(categories_tree, free_avl_key);
    categories_tree = avl_tree_new(categories_avl_compare, NULL);

    if (competitors_tree)
        avl_tree_free(competitors_tree, free_avl_key);
    competitors_tree = avl_tree_new(competitors_avl_compare, NULL);

    if (competitors_hash_tree)
        avl_tree_free(competitors_hash_tree, free_avl_key);
    competitors_hash_tree = avl_tree_new(competitors_hash_avl_compare, NULL);

    init_html_tree();
}


void init_club_tree(void)
{
    if (club_tree)
        avl_tree_free(club_tree, free_club_avl_key);
    club_tree = avl_tree_new(club_avl_compare, NULL);
}

void init_club_name_tree(void)
{
    if (club_name_tree)
        avl_tree_free(club_name_tree, free_club_name_avl_key);
    club_name_tree = avl_tree_new(club_name_avl_compare, NULL);
}

void club_stat_add(const gchar *club, const gchar *country, gint num)
{
    struct club_data key;
    struct club_data *data;

    if (!club_tree)
        return;

    key.name = (gchar *)club;
    key.country = (gchar *)country;

    if (avl_get_by_key(club_tree, &key, (void *)&data)) {
        data = g_malloc(sizeof(*data));
        memset(data, 0, sizeof(*data));
        data->name = g_strdup(club);
        data->country = g_strdup(country);
        avl_insert(club_tree, data);
    }

    switch (num) {
    case 0: data->competitors++; break;
    case 1: data->gold++; break;
    case 2: data->silver++; break;
    case 3: data->bronze++; break;
    case 4: data->fourth++; break;
    case 5: data->fifth++; break;
    case 6: data->sixth++; break;
    case 7: data->seventh++; break;
    }
}

void club_name_set(const gchar *club, 
		   const gchar *abbr,
		   const gchar *address)
{
    struct club_name_data key;
    struct club_name_data *data;

    if (!club_name_tree)
        return;

    key.name = (gchar *)club;

    if (avl_get_by_key(club_name_tree, &key, (void *)&data)) {
        data = g_malloc(sizeof(*data));
        memset(data, 0, sizeof(*data));
        data->name = g_strdup(club);
        data->abbreviation = g_strdup(abbr);
        data->address = g_strdup(address);
        avl_insert(club_name_tree, data);
    } else {
	g_free(data->abbreviation);
	g_free(data->address);
        data->abbreviation = g_strdup(abbr);
        data->address = g_strdup(address);
    }
}

struct club_name_data *club_name_get(const gchar *club)
{
    struct club_name_data key;
    struct club_name_data *data = NULL;

    if (!club_name_tree)
        return NULL;

    key.name = (gchar *)club;

    if (avl_get_by_key(club_name_tree, &key, (void *)&data))
	return NULL;

    return data;
}

static struct club_data clubs, *tail = NULL;

static gint compare_medals(struct club_data *data1, struct club_data *data2)
{
    if (!data1)
        return -1;
    if (!data2)
        return 1;

    if (data1->gold   > data2->gold) return -1;
    if (data1->gold   < data2->gold) return 1;
    if (data1->silver > data2->silver) return -1;
    if (data1->silver < data2->silver) return 1;
    if (data1->bronze > data2->bronze) return -1;
    if (data1->bronze < data2->bronze) return 1;
#if 0
    if (data1->fourth  > data2->fourth) return -1;
    if (data1->fourth  < data2->fourth) return 1;
    if (data1->fifth  > data2->fifth) return -1;
    if (data1->fifth  < data2->fifth) return 1;
#endif
    return 0;
}

#if 0
static gint compare_positions(struct club_data *data1, struct club_data *data2)
{
    gint res = compare_medals(data1, data2);
    if (res == 0)
        return club_avl_compare(NULL, data1, data2);
    return 0;
}
#endif

static int iter_clubs(void *key, void *iter_arg)
{
    struct club_data *data = key, *p, *last;

    if (clubs.next == NULL) {
        clubs.next = data;
        clubs.gold = 1000;
        tail = data;
        return 0;
    }

    for (p = &clubs; p->next; p = p->next) {
        last = p->next;
        if (compare_medals(data, p->next) < 0) {
            data->next = p->next;
            p->next = data;
            return 0;
        }
    }

    last->next = data;

    return 0;
}

void club_stat_print(FILE *f)
{
    struct club_data *p, *prev = NULL;
    gint pos = 0, prpos = 0;

    if (!club_tree)
        return;

    fprintf(f, "<td valign=\"top\"><table class=\"medals\">"
            "<tr><td class=\"medalhdr\" colspan=\"6\"><h2>%s</h2></td></tr>"
            "<tr><th></th><th>%s</th><th class=\"medalcnt\">I</th>"
            "<th class=\"medalcnt\">II</th><th class=\"medalcnt\">III</th><th class=\"medalcnt\">%s</th></tr>\n",
            _T(medals), _T(club), _T(competitor));

    clubs.next = NULL;
    avl_iterate_inorder(club_tree, iter_clubs, NULL);

    for (p = clubs.next; p; p = p->next) {
        pos++;
        if (prev == NULL || compare_medals(p, prev))
            prpos = pos;
        prev = p;

        struct judoka j;
        j.club = p->name;
        j.country = p->country;

        fprintf(f, "<tr><td>%d.</td><td>%s</td><td class=\"medalcnt\">%d</td>"
                "<td class=\"medalcnt\">%d</td><td class=\"medalcnt\">%d</td>"
                "<td class=\"medalcnt\">%d</td></tr>\n", 
                prpos, utf8_to_html(get_club_text(&j, CLUB_TEXT_ADDRESS)), 
                p->gold, p->silver, p->bronze, p->competitors);
    }

    fprintf(f, "</table></td>");
}
