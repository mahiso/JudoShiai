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

G_LOCK_EXTERN(db);

static char **tablep = NULL;
static int tablerows, tablecols;

#if (GTKVER == 3)
G_LOCK_DEFINE_STATIC(table_mutex);
#else
static GStaticMutex table_mutex = G_STATIC_MUTEX_INIT;
#endif

static void utf8error(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    switch (sqlite3_value_type(argv[0]) ) {
    case SQLITE_TEXT: {
        const unsigned char *tVal = sqlite3_value_text(argv[0]);
        if (g_utf8_validate((gchar *)tVal, -1, NULL))
            sqlite3_result_int(context, 0);
        else
            sqlite3_result_int(context, 1);
        break;
    }
    default:
        sqlite3_result_null(context);
        break;
    }
}

static void getweight(sqlite3_context *context, int argc, sqlite3_value **argv)
{
    switch (sqlite3_value_type(argv[0]) ) {
    case SQLITE_TEXT: {
        const unsigned char *tVal = sqlite3_value_text(argv[0]);
        const guchar *p = tVal;
        gint weight = 0;
        while (*p) {
            if (*p >= '0' && *p <= '9') {
                weight = 10*weight + 1000*(*p - '0');
            } else if (*p == '.' || *p == ',') {
                p++;
                if (*p >= '0' && *p <= '9') {
                    weight += 100*(*p - '0');
                    p++;
                    if (*p >= '0' && *p <= '9') {
                        weight += 10*(*p - '0');
                    }
                }
                break;
            } else
                weight = 0;
            p++;
        }
        sqlite3_result_int(context, weight);
        break;
    }
    default:
        sqlite3_result_null(context);
        break;
    }
}

int db_get_table(char *command)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;

#if (GTKVER == 3)
    G_LOCK(table_mutex);
#else
    g_static_mutex_lock(&table_mutex);
#endif
    if (tablep) {
        g_print("ERROR: TABLE NOT CLOSED BEFORE OPENING!\n");
        sqlite3_free_table(tablep);
    }
    tablep = NULL;

    G_LOCK(db);

    rc = sqlite3_open(db_name, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", 
                sqlite3_errmsg(db));
        G_UNLOCK(db);
#if (GTKVER == 3)
        G_UNLOCK(table_mutex);
#else
        g_static_mutex_unlock(&table_mutex);
#endif
        return -1;
    }

    sqlite3_create_function(db, "utf8error", 1, SQLITE_UTF8, NULL, utf8error, NULL, NULL);
    sqlite3_create_function(db, "getweight", 1, SQLITE_UTF8, NULL, getweight, NULL, NULL);

    //g_print("\nSQL: %s:\n  %s\n", db_name, cmd);
    rc = sqlite3_get_table(db, command, &tablep, &tablerows, &tablecols, &zErrMsg);
    if (rc != SQLITE_OK && zErrMsg) {
        g_print("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        G_UNLOCK(db);
#if (GTKVER == 3)
        G_UNLOCK(table_mutex);
#else
        g_static_mutex_unlock(&table_mutex);
#endif
        return -1;
    }

    sqlite3_close(db);
    G_UNLOCK(db);

    return tablerows;
}

void db_close_table(void)
{
    if (tablep)
        sqlite3_free_table(tablep);
    else
        g_print("ERROR: TABLE CLOSED TWICE!\n");
    tablep = NULL;
#if (GTKVER == 3)
    G_UNLOCK(table_mutex);
#else
    g_static_mutex_unlock(&table_mutex);
#endif
}

char *db_get_data(int row, char *name)
{
    return db_get_col_data(tablep, tablecols, row, name);
}

gchar *db_get_col_data(gchar **data, gint numcols, gint row, gchar *name)
{
    int col;

    for (col = 0; col < numcols; col++)
        if (strcmp(data[col], name) == 0)
            break;
    
    if (col >= numcols)
        return NULL;
    
    return data[(row+1)*numcols + col];
}

gchar *db_get_row_col_data(gint row, gint col)
{
    gchar **data = tablep;

    if (col >= tablecols)
        return NULL;
    
    return data[(row+1)*tablecols + col];
}

char **db_get_table_copy(char *command, int *tablerows1, int *tablecols1)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    char **tablep1 = NULL;

    G_LOCK(db);

    rc = sqlite3_open(db_name, &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", 
                sqlite3_errmsg(db));
        G_UNLOCK(db);
        return NULL;
    }

    sqlite3_create_function(db, "utf8error", 1, SQLITE_UTF8, NULL, utf8error, NULL, NULL);
    sqlite3_create_function(db, "getweight", 1, SQLITE_UTF8, NULL, getweight, NULL, NULL);

    //g_print("\nSQL: %s:\n  %s\n", db_name, cmd);
    rc = sqlite3_get_table(db, command, &tablep1, tablerows1, tablecols1, &zErrMsg);
    if (rc != SQLITE_OK && zErrMsg) {
        g_print("SQL error: %s\n", zErrMsg);
        sqlite3_free(zErrMsg);
        sqlite3_close(db);
        G_UNLOCK(db);
        return NULL;
    }

    sqlite3_close(db);
    G_UNLOCK(db);

    return tablep1;
}

void db_close_table_copy(char **tablep)
{
    sqlite3_free_table(tablep);
}

