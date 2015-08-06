/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <assert.h>

#include "avl.h"
#include "judojudogi.h"

avl_tree *competitors_tree = NULL;

static int competitors_avl_compare(void *compare_arg, void *a, void *b)
{
    struct name_data *a1 = a;
    struct name_data *b1 = b;
    return a1->index - b1->index;
}

static int free_avl_key(void *key)
{
    struct name_data *x = key;
    g_free(x->last);
    g_free(x->first);
    g_free(x->club);
    g_free(key);
    return 1;
}

struct name_data *avl_get_data(gint index)
{
    struct name_data data;
    void *data1;

    data.index = index;
    if (avl_get_by_key(competitors_tree, &data, &data1) == 0) {
        return data1;
    }
    //g_print("cannot find name index %d!\n", index);
    //assert(0);
    return NULL;
}

void avl_set_data(gint index, gchar *first, gchar *last, gchar *club)
{
    struct name_data *data;
    void *data1;

    data = g_malloc(sizeof(*data));
    memset(data, 0, sizeof(*data));
	
    data->index = index;
    data->last = g_strdup(last);
    data->first = g_strdup(first);
    data->club = g_strdup(club);

    if (avl_get_by_key(competitors_tree, data, &data1) == 0) {
        /* data exists */
        avl_delete(competitors_tree, data, free_avl_key);
    }
    avl_insert(competitors_tree, data);
}

void init_trees(void)
{
    if (competitors_tree)
        avl_tree_free(competitors_tree, free_avl_key);
    competitors_tree = avl_tree_new(competitors_avl_compare, NULL);

    avl_set_data(0, "", "", "");
    avl_set_data(1000, "", "", "");
}
