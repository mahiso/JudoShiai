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
//#include "judotimer.h"

struct font_data {
    TTF_Font *font;
    int size;
    char bold;
};

avl_tree *font_tree = NULL;

static int font_avl_compare(void *compare_arg, void *a, void *b)
{
    struct font_data *a1 = a;
    struct font_data *b1 = b;
    if (a1->size == b1->size)
	return (a1->bold - b1->bold);
    return (a1->size - b1->size);
}

static int free_avl_key(void *key)
{
    struct font_data *x = key;
    TTF_CloseFont(x->font);
    free(key);
    return 1;
}

TTF_Font *get_font(int size, int bold)
{
    struct font_data data;
    struct font_data *data1;

    data.size = size;
    data.bold = bold;

    if (avl_get_by_key(font_tree, &data, (void **)&data1) == 0) {
	//printf("get_font (old) = %p\n", data1->font);
        return data1->font;
    }

    data.font = bold ?
	TTF_OpenFont("free-sans-bold", size) :
	TTF_OpenFont("free-sans", size);

    data1 = malloc(sizeof(struct font_data));
    memcpy(data1, &data, sizeof(struct font_data));
    avl_insert(font_tree, data1);

    //printf("get_font (new) = %p\n", data.font);
    return data.font;
}

void init_fonts(void)
{
    if (font_tree)
        avl_tree_free(font_tree, free_avl_key);
    font_tree = avl_tree_new(font_avl_compare, NULL);
}
