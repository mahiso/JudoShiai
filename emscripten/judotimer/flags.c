/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <emscripten.h>

#include "avl.h"

struct flag_data {
    SDL_Surface *img;
    char name[16];
};

avl_tree *flag_tree = NULL;

static int flag_avl_compare(void *compare_arg, void *a, void *b)
{
    struct flag_data *a1 = a;
    struct flag_data *b1 = b;
    return strncmp(a1->name, b1->name, 3);
}

static int free_avl_key(void *key)
{
    struct flag_data *x = key;
    SDL_FreeSurface(x->img);
    free(key);
    return 1;
}

SDL_Surface *get_flag(const char *name)
{
    struct flag_data data;
    struct flag_data *data1;

    strncpy(data.name, name, 3);
    data.name[3] = 0;

    if (avl_get_by_key(flag_tree, &data, (void **)&data1) == 0) {
	//printf("get_flag '%s' = %p\n", data1->name, data1->img);
        return data1->img;
    }

    return NULL;
}

void insert_flag(const char *name, SDL_Surface *img)
{
    struct flag_data *data1;

    if (get_flag(name))
	return;

    data1 = malloc(sizeof(struct flag_data));
    strncpy(data1->name, name, 3);
    data1->name[3] = 0;
    data1->img = img;
    //printf("insert_flag '%s' = %p\n", data1->name, data1->img);
    avl_insert(flag_tree, data1);
}

void init_flags(void)
{
    if (flag_tree)
        avl_tree_free(flag_tree, free_avl_key);
    flag_tree = avl_tree_new(flag_avl_compare, NULL);
}
