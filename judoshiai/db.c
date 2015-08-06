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

#include "sqlite3.h"
#include "judoshiai.h"

//#define TATAMI_DEBUG 3

static gint db_changes;
gchar database_name[200] = {0};
const char *db_name;

G_LOCK_DEFINE(db);

guint64 cumul_db_time = 0;
gint    cumul_db_count = 0;

gint db_exec(const char *dbn, char *cmd, void *data, void *dbcb)
{
    sqlite3 *db;
    char *zErrMsg = 0;
    int rc;
    guint64 start, stop;

    RDTSC(start);
    if (db_name == dbn)
	G_LOCK(db);

    rc = sqlite3_open(dbn, &db);
    if (rc) {
	fprintf(stderr, "Can't open database: %s\n", 
		sqlite3_errmsg(db));
	sqlite3_close(db);
	if (db_name == dbn)
	    G_UNLOCK(db);
	exit(1);
    }

    db_changes = 0;
#ifdef FAST_DB
    rc = sqlite3_exec(db, "PRAGMA synchronous=OFF", NULL, NULL, &zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("SQL PRAGMA sync error: %s\n", zErrMsg);
    }
    rc = sqlite3_exec(db, "PRAGMA default_cache_size=10000", NULL, NULL, &zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("SQL PRAGMA cache error: %s\n", zErrMsg);
    }
#endif
    //g_print("\nSQL: %s:\n  %s\n", db_name, cmd);
    rc = sqlite3_exec(db, cmd, dbcb, data, &zErrMsg);

    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("SQL error: %s\n%s\n", zErrMsg, cmd);
    }

    if (zErrMsg)
	sqlite3_free(zErrMsg);

    db_changes = sqlite3_changes(db);

    sqlite3_close(db);

    if (db_name == dbn)
	G_UNLOCK(db);

    RDTSC(stop);
    cumul_db_time += stop - start;
    cumul_db_count++;

    return rc;
}

void db_exec_str(void *data, void *dbcb, gchar *format, ...)
{
    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);
    db_exec(db_name, text, data, dbcb);
    g_free(text);
}

static sqlite3 *maindb;

void db_close(void)
{
    if (!maindb) {
	g_print("%s: maindb was not open!\n", __FUNCTION__);
	return;
    }
#if 0
    char *zErrMsg = NULL;
    gint rc = sqlite3_exec(maindb, "END TRANSACTION", NULL, NULL, &zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("SQL END TRANSACTION error: %s\n", zErrMsg);
    }
#endif
    sqlite3_close(maindb);
    maindb = NULL;
    G_UNLOCK(db);
}

gint db_open(void)
{
    char *zErrMsg = NULL;

    if (maindb) {
	g_print("%s: maindb was already open!\n", __FUNCTION__);
	return -1;
    }

    if (!db_name) {
	g_print("%s: db_name is null!\n", __FUNCTION__);
	return -2;
    }

    G_LOCK(db);

    gint rc = sqlite3_open(db_name, &maindb);

    if (rc) {
	g_print("%s: cannot open maindb (%s)!\n", __FUNCTION__, 
		sqlite3_errmsg(maindb));
	sqlite3_close(maindb);
	maindb = NULL;
	G_UNLOCK(db);
    }
#ifdef FAST_DB
    rc = sqlite3_exec(maindb, "PRAGMA synchronous=OFF", NULL, NULL, &zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("SQL PRAGMA sync error: %s\n", zErrMsg);
    }
    rc = sqlite3_exec(maindb, "PRAGMA default_cache_size=10000", NULL, NULL, &zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("SQL PRAGMA cache error: %s\n", zErrMsg);
    }
#endif
#if 0
    rc = sqlite3_exec(maindb, "BEGIN TRANSACTION", NULL, NULL, &zErrMsg);
    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("SQL BEGIN TRANSACTION error: %s\n", zErrMsg);
    }
#endif
    return rc;
}

gint db_cmd(void *data, void *dbcb, gchar *format, ...)
{
    char *zErrMsg = NULL;
    va_list args;
    va_start(args, format);
    gchar *text = g_strdup_vprintf(format, args);
    va_end(args);

    if (!maindb) {
	g_print("db_cmd SQL error: maindb is null\n");
        g_free(text);
	return -1;
    }

    gint rc = sqlite3_exec(maindb, text, dbcb, data, &zErrMsg);

    if (rc != SQLITE_OK && rc != SQLITE_ABORT && zErrMsg) {
	g_print("db_cmd SQL error: %s\n%s\n", zErrMsg, text);
    }

    if (zErrMsg)
	sqlite3_free(zErrMsg);

    g_free(text);

    return rc;
}

static int db_info_cb(void *data, int argc, char **argv, char **azColName)
{
    init_property(argv[0], argv[1]);
    return 0;
}

static gboolean tatami_exists, number_exists, country_exists, 
    id_exists, numcomp_exists, seeding_exists, comp_comment_exists, match_date_exists;

static int db_callback_tables(void *data, int argc, char **argv, char **azColName)
{
    int i;

    for (i = 0; i < argc; i++) {
        if (IS(sql)) {
            if (strstr(argv[i], "matches") && strstr(argv[i], "forcedtatami"))
                tatami_exists = TRUE;
            if (strstr(argv[i], "matches") && strstr(argv[i], "forcednumber"))
                number_exists = TRUE;
            if (strstr(argv[i], "matches") && strstr(argv[i], "date"))
                match_date_exists = TRUE;
            if (strstr(argv[i], "competitors") && strstr(argv[i], "country"))
                country_exists = TRUE;
            if (strstr(argv[i], "competitors") && strstr(argv[i], "\"id\""))
                id_exists = TRUE;
            if (strstr(argv[i], "categories") && strstr(argv[i], "numcomp"))
                numcomp_exists = TRUE;
            if (strstr(argv[i], "competitors") && strstr(argv[i], "seeding"))
                seeding_exists = TRUE;
            if (strstr(argv[i], "competitors") && strstr(argv[i], "comment"))
                comp_comment_exists = TRUE;
        }
    }

    return 0;
}

gint db_init(const char *dbname)
{
    gint r = 0;
    
    // try to open db
    FILE *f = fopen(dbname, "rb");
    if (f)
        fclose(f);
    else
        return -1;

    // check the number of columns
    sqlite3 *db;
    gchar *zErrMsg = NULL;
    gchar **tablep = NULL;
    gint tablerows, compcols, catcols, matchcols, infocols, catdefcols;
    gint rc = sqlite3_open(dbname, &db);
    if (rc)
        return rc;
    
    rc = sqlite3_get_table(db, "select * from competitors limit 1", 
                           &tablep, &tablerows, &compcols, &zErrMsg);
    if (tablep) sqlite3_free_table(tablep);
    rc = sqlite3_get_table(db, "select * from categories limit 1", 
                           &tablep, &tablerows, &catcols, &zErrMsg);
    if (tablep) sqlite3_free_table(tablep);
    rc = sqlite3_get_table(db, "select * from matches limit 1", 
                           &tablep, &tablerows, &matchcols, &zErrMsg);
    if (tablep) sqlite3_free_table(tablep);
    rc = sqlite3_get_table(db, "select * from info limit 1", 
                           &tablep, &tablerows, &infocols, &zErrMsg);
    if (tablep) sqlite3_free_table(tablep);
    rc = sqlite3_get_table(db, "select * from catdef limit 1", 
                           &tablep, &tablerows, &catdefcols, &zErrMsg);
    if (tablep) sqlite3_free_table(tablep);

    sqlite3_close(db);

    if (compcols   > 17 ||
        catcols    > 17 || 
        matchcols  > 15 ||
        infocols   > 2  ||
        catdefcols > 13) {
        SHOW_MESSAGE("%s", _("Cannot handle: Database created with newer JudoShiai version."));
        g_print("Number of columns: %d %d %d %d %d\n", compcols, catcols, matchcols, 
                infocols, catdefcols);
        return -2;
    }

    db_name = dbname;

    read_custom_from_db();

    db_matches_init();
	
    init_trees();

    db_exec(db_name, "SELECT * FROM \"info\"", NULL, db_info_cb);
    reset_props_1(NULL, NULL, TRUE); // initialize not initialized values
    props_save_to_db();

    set_configuration();

    snprintf(hello_message.u.hello.info_competition, 
             sizeof(hello_message.u.hello.info_competition), 
             "%s", prop_get_str_val(PROP_NAME));
    snprintf(hello_message.u.hello.info_date, 
             sizeof(hello_message.u.hello.info_date), 
             "%s", prop_get_str_val(PROP_DATE));
    snprintf(hello_message.u.hello.info_place, 
             sizeof(hello_message.u.hello.info_place), 
             "%s", prop_get_str_val(PROP_PLACE));

    read_cat_definitions();

    tatami_exists = number_exists = country_exists = id_exists = 
        numcomp_exists = seeding_exists = comp_comment_exists = match_date_exists = FALSE;

    db_exec(db_name, 
            "SELECT sql FROM sqlite_master", 
            NULL, db_callback_tables);
    if (!tatami_exists) {
        g_print("forcedtatami does not exist, add one\n");
        db_exec(db_name, "ALTER TABLE matches ADD \"forcedtatami\" INTEGER", NULL, NULL);
        db_exec(db_name, "UPDATE matches SET 'forcedtatami'=0", NULL, NULL);
    }
    if (!number_exists) {
        g_print("forcednumber does not exist, add one\n");
        db_exec(db_name, "ALTER TABLE matches ADD \"forcednumber\" INTEGER", NULL, NULL);
        db_exec(db_name, "UPDATE matches SET 'forcednumber'=0", NULL, NULL);
    }
    if (!country_exists) {
        g_print("country does not exist, add one\n");
        db_exec(db_name, "ALTER TABLE competitors ADD \"country\" TEXT", NULL, NULL);
    }
    if (!id_exists) {
        g_print("id does not exist, add one\n");
        db_exec(db_name, "ALTER TABLE competitors ADD \"id\" TEXT", NULL, NULL);
    }

    if (!seeding_exists) {
        g_print("seeding does not exist, add one\n");
        db_exec(db_name, "ALTER TABLE competitors ADD \"seeding\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE competitors ADD \"clubseeding\" INTEGER", NULL, NULL);
        db_exec(db_name, "UPDATE competitors SET 'seeding'=(\"deleted\">>2)&7 ", NULL, NULL);
        db_exec(db_name, "UPDATE competitors SET 'deleted'=(\"deleted\")&3 ", NULL, NULL);
        db_exec(db_name, "UPDATE competitors SET 'clubseeding'=0 ", NULL, NULL);
    }

    if (!numcomp_exists) {
        g_print("numcomp does not exist, add one\n");
        db_exec(db_name, "ALTER TABLE categories ADD \"numcomp\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"table\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"wishsys\" INTEGER", NULL, NULL);
        db_exec(db_name, "UPDATE categories SET 'table'=(\"system\">>20)&15 ", NULL, NULL);
        db_exec(db_name, "UPDATE categories SET 'numcomp'=\"system\"&255 ", NULL, NULL);
        db_exec(db_name, "UPDATE categories SET 'wishsys'=(\"system\">>24)&15 ", NULL, NULL);
        db_exec(db_name, "UPDATE categories SET 'system'=(\"system\">>16)&15 ", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos1\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos2\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos3\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos4\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos5\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos6\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos7\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE categories ADD \"pos8\" INTEGER", NULL, NULL);
    }

    if (!comp_comment_exists) {
        g_print("comment does not exist, add one\n");
        db_exec(db_name, "ALTER TABLE competitors ADD \"comment\" TEXT", NULL, NULL);
        db_exec(db_name, "ALTER TABLE competitors ADD \"coachid\" TEXT", NULL, NULL);
        db_exec(db_name, "UPDATE competitors SET \"comment\"=\"\", \"coachid\"=\"\" ", NULL, NULL);
    }

    if (!match_date_exists) {
        g_print("date and legend do not exist, add one\n");
        db_exec(db_name, "ALTER TABLE matches ADD \"date\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE matches ADD \"legend\" INTEGER", NULL, NULL);
        db_exec(db_name, "UPDATE matches SET \"date\"=0, \"legend\"=0 ", NULL, NULL);
    }

    if (!tatami_exists || !number_exists || !country_exists || !id_exists || !numcomp_exists || !seeding_exists ||
        !comp_comment_exists || !match_date_exists) {
        r = 55555;
        SHOW_MESSAGE("%s", _("Database tables updated."));
    }

    if (prop_get_int_val(PROP_THREE_MATCHES_FOR_TWO)) {
        init_property("three_matches_for_two", "0");
        props_save_to_db();

        db_exec_str(NULL, NULL, "UPDATE categories SET \"system\"=%d, \"wishsys\"=%d WHERE "
                    "\"system\"=%d AND \"numcomp\"=2", 
                    SYSTEM_BEST_OF_3, CAT_SYSTEM_BEST_OF_3,
                    SYSTEM_POOL);
    }
    

    set_menu_active();

    return r;
}

void db_new(const char *dbname)
{
    FILE *f;

    char *cmd1 = "CREATE TABLE competitors ("
        "\"index\" INTEGER, \"last\" TEXT, \"first\" TEXT, \"birthyear\" INTEGER, "
        "\"belt\" INTEGER, \"club\" TEXT, \"regcategory\" TEXT, "
        "\"weight\" INTEGER, \"visible\" INTEGER, "
        "\"category\" TEXT, \"deleted\" INTEGER, "
        "\"country\" TEXT, \"id\" TEXT, \"seeding\" INTEGER, \"clubseeding\" INTEGER, "
        "\"comment\" TEXT, \"coachid\" TEXT )";
    char *cmd3 = "CREATE TABLE categories ("
        "\"index\" INTEGER, \"category\" TEXT, \"tatami\" INTEGER, "
        "\"deleted\" INTEGER, \"group\" INTEGER, \"system\" INTEGER, "
        "\"numcomp\" INTEGER, \"table\" INTEGER, \"wishsys\" INTEGER, "
        "\"pos1\" INTEGER, \"pos2\" INTEGER, \"pos3\" INTEGER, \"pos4\" INTEGER, "
        "\"pos5\" INTEGER, \"pos6\" INTEGER, \"pos7\" INTEGER, \"pos8\" INTEGER"
        ")";
    char *cmd4 = "CREATE TABLE matches ("
        "\"category\" INTEGER, \"number\" INTEGER,"
        "\"blue\" INTEGER, \"white\" INTEGER,"
        "\"blue_score\" INTEGER, \"white_score\" INTEGER, "
        "\"blue_points\" INTEGER, \"white_points\" INTEGER, "
        "\"time\" INTEGER, \"comment\" INTEGER, \"deleted\" INTEGER, "
        "\"forcedtatami\" INTEGER, \"forcednumber\" INTEGER, "
        "\"date\" INTEGER, \"legend\" INTEGER )";
    char *cmd5 = "CREATE TABLE \"info\" ("
        "\"item\" TEXT, \"value\" TEXT )";
	
    db_name = dbname;
    if ((f = fopen(db_name, "rb"))) {
        /* exists */
        fclose(f);
        return;
    }

    db_exec(db_name, cmd1, NULL, NULL);
    db_exec(db_name, cmd3, NULL, NULL);
    db_exec(db_name, cmd4, NULL, NULL);
    db_exec(db_name, cmd5, NULL, NULL);
}

void db_set_info(gchar *name, gchar *value) 
{
    db_exec_str(NULL, NULL, 
                "UPDATE \"info\" SET "
                "\"value\"=\"%s\" WHERE \"item\"=\"%s\"", value, name);

    if (db_changes == 0)
        db_exec_str(NULL, NULL, 
                    "INSERT INTO \"info\" VALUES (\"%s\", \"%s\")",
                    name, value);
}

void db_save_config(void)
{
}

/*********************************************************/

gchar *db_sql_command(const gchar *command)
{
    sqlite3 *db;
    char *zErrMsg = NULL;
    gint rc, row, col;
    gchar **table = NULL;
    gint rows, cols;
    gchar *ret = NULL, *tmp;

    if ((rc = sqlite3_open(db_name, &db)))
        return NULL;

    rc = sqlite3_get_table(db, command, &table, &rows, &cols, &zErrMsg);
    if (rc != SQLITE_OK) {
        if (zErrMsg) {
            ret = g_strdup(zErrMsg);
            sqlite3_free(zErrMsg);
        } else
            ret = g_strdup("UNKNOWN ERROR!");
        goto out;
    }
        
    ret = g_strdup("");
    for (row = 0; row <= rows; row++) {
        for (col = 0; col < cols; col++) {
            if (col == 0) {
                if (cols == 1)
                    tmp = g_strconcat(ret, table[row*cols + col], "\n", NULL);
                else
                    tmp = g_strconcat(ret, table[row*cols + col], NULL);
            } else if (col == cols - 1)
                tmp = g_strconcat(ret, " | ", table[row*cols + col], "\n", NULL);
            else
                tmp = g_strjoin(" | ", ret, table[row*cols + col], NULL);
            g_free(ret);
            ret = tmp;
        }
    }
out:
    sqlite3_free_table(table);
    sqlite3_close(db);

    return ret;
}

void db_create_cat_def_table(void)
{
    char *cmd = "CREATE TABLE \"catdef\" ("
        "\"age\" INTEGER, \"agetext\" TEXT, "
        "\"flags\" INTEGER, "
        "\"weight\" INTEGER, \"weighttext\" TEXT, "
        "\"matchtime\" INTEGER, \"pintimekoka\" INTEGER, "
        "\"pintimeyuko\" INTEGER, \"pintimewazaari\" INTEGER, \"pintimeippon\" INTEGER, "
        "\"resttime\" INTEGER, \"gstime\" INTEGER, \"reptime\" INTEGER)";
    db_exec(db_name, cmd, 0, 0);
}

void db_delete_cat_def_table_data(void)
{
    char *cmd = "DELETE FROM \"catdef\"";
    db_exec(db_name, cmd, 0, 0);
}

void db_insert_cat_def_table_data_begin(void)
{
    db_open();
    db_cmd(NULL, NULL, "begin transaction");
}

void db_insert_cat_def_table_data_end(void)
{
    db_cmd(NULL, NULL, "commit");
    db_close();
}

void db_insert_cat_def_table_data(struct cat_def *def)
{
    char buf[1024];
    sprintf(buf, "INSERT INTO catdef VALUES ("
            "%d, \"%s\", %d, %d, \"%s\", %d, %d, %d, %d, %d, %d, %d, %d)",
            def->age, def->agetext, def->gender,
            def->weights[0].weight, def->weights[0].weighttext,
            def->match_time, def->pin_time_koka, def->pin_time_yuko, 
            def->pin_time_wazaari, def->pin_time_ippon, def->rest_time, def->gs_time, def->rep_time); 
    db_cmd(NULL, NULL, buf);
    //db_exec(db_name, buf, 0, 0);
}

void db_cat_def_table_done(void)
{
    avl_set_category_rest_times();
}

void xxdb_add_colums_to_cat_def_table(void)
{
    db_exec(db_name,
            "CREATE TABLE catdef_backup ("
            "\"age\" INTEGER, \"agetext\" TEXT, "
            "\"flags\" INTEGER, "
            "\"weight\" INTEGER, \"weighttext\" TEXT)",
            0, 0);

    db_exec(db_name,
            "INSERT INTO catdef_backup SELECT * FROM catdef",
            0, 0);

    db_exec(db_name,
            "DROP TABLE catdef",
            0, 0);

    db_create_cat_def_table();

    db_exec(db_name,
            "INSERT INTO catdef SELECT * FROM catdef_backup",
            0, 0);

    db_exec(db_name,
            "DROP TABLE catdef_backup",
            0, 0);
}

static gboolean catdef_exists, matchtime_exists, gstime_exists, reptime_exists;

static int db_callback_catdef(void *data, int argc, char **argv, char **azColName)
{
    int i;

    for (i = 0; i < argc; i++) {
        if (IS(name)) {
            if (strcmp("catdef", argv[i]) == 0) {
                catdef_exists = TRUE;
                return 0;
            }
        } else if (IS(sql)) {
            if (strstr(argv[i], "matchtime"))
                matchtime_exists = TRUE;
            if (strstr(argv[i], "gstime"))
                gstime_exists = TRUE;
            if (strstr(argv[i], "reptime"))
                reptime_exists = TRUE;
        }
    }

    return 0;
}

gboolean catdef_needs_init(void)
{
    catdef_exists = matchtime_exists = gstime_exists = reptime_exists = FALSE;
    

    db_exec(db_name, 
            "SELECT name FROM sqlite_master WHERE type='table'", 
            NULL, db_callback_catdef);
	
    db_exec(db_name, 
            "SELECT sql FROM sqlite_master", 
            NULL, db_callback_catdef);
	
    if (!catdef_exists) {
        g_print("catdef does not exist, create one\n");
        db_create_cat_def_table();
        return TRUE;
    }

    if (!matchtime_exists) {
        g_print("catdef matchtime does not exist, add one\n");

        db_exec(db_name, "ALTER TABLE catdef ADD \"matchtime\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE catdef ADD \"pintimekoka\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE catdef ADD \"pintimeyuko\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE catdef ADD \"pintimewazaari\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE catdef ADD \"pintimeippon\" INTEGER", NULL, NULL);
        db_exec(db_name, "ALTER TABLE catdef ADD \"resttime\" INTEGER", NULL, NULL);

        db_exec(db_name, "UPDATE catdef SET 'matchtime'=300", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'matchtime'=120 WHERE \"age\"<=12", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'matchtime'=180 WHERE \"age\"<=16 AND \"age\">12", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'matchtime'=240 WHERE \"age\"<=19 AND \"age\">16", NULL, NULL);

        db_exec(db_name, "UPDATE catdef SET 'resttime'=300", NULL, NULL);

        db_exec(db_name, "UPDATE catdef SET 'pintimekoka'=10", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'pintimeyuko'=15", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'pintimewazaari'=20", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'pintimeippon'=25", NULL, NULL);

        db_exec(db_name, "UPDATE catdef SET 'pintimekoka'=0 WHERE \"age\"<=10", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'pintimeyuko'=5 WHERE \"age\"<=10", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'pintimewazaari'=10 WHERE \"age\"<=10", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'pintimeippon'=15 WHERE \"age\"<=10", NULL, NULL);
    }	

    if (!gstime_exists) {
        g_print("catdef gstime does not exist, add one\n");

        db_exec(db_name, "ALTER TABLE catdef ADD \"gstime\" INTEGER", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'gstime'=300", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'gstime'=120 WHERE \"age\"<=16", NULL, NULL);
    }

    if (!reptime_exists) {
        g_print("catdef reptime does not exist, add one\n");

        db_exec(db_name, "ALTER TABLE catdef ADD \"reptime\" INTEGER", NULL, NULL);
        db_exec(db_name, "UPDATE catdef SET 'reptime'=0", NULL, NULL);
    }

    return FALSE;
}

int db_create_blob_table(void)
{
    sqlite3 *db;
    int rc = sqlite3_open(db_name, &db);
    if (rc) {
	fprintf(stderr, "%s: Can't open database: %s\n", __FUNCTION__, 
		sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    const char *zSql = "CREATE TABLE blobs(key INTEGER PRIMARY KEY, value BLOB)";
    rc = sqlite3_exec(db, zSql, 0, 0, 0);

    sqlite3_close(db);
    return rc;
}

int db_write_blob (
  int key,
  const unsigned char *zBlob,    /* Pointer to blob of data */
  int nBlob                      /* Length of data pointed to by zBlob */
)
{
    sqlite3 *db;
    int rc = sqlite3_open(db_name, &db);
    if (rc) {
	fprintf(stderr, "%s: Can't open database: %s\n", __FUNCTION__, 
		sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    const char *zSql = "INSERT INTO blobs(key, value) VALUES(?, ?)";
    sqlite3_stmt *pStmt;

    do {
        rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
        if( rc!=SQLITE_OK ) {
            sqlite3_close(db);
            return rc;
        }

        sqlite3_bind_int(pStmt, 1, key);
        sqlite3_bind_blob(pStmt, 2, zBlob, nBlob, SQLITE_STATIC);

        rc = sqlite3_step(pStmt);
        assert( rc!=SQLITE_ROW );

        rc = sqlite3_finalize(pStmt);
    } while ( rc==SQLITE_SCHEMA );

    sqlite3_close(db);
    return rc;
}

/*
** Read a blob from database db. Return an SQLite error code.
*/ 
int db_read_blob(
  int key,
  unsigned char **pzBlob,    /* Set *pzBlob to point to the retrieved blob */
  int *pnBlob                /* Set *pnBlob to the size of the retrieved blob */
)
{
    sqlite3 *db;
    int rc = sqlite3_open(db_name, &db);
    if (rc) {
	fprintf(stderr, "%s: Can't open database: %s\n", __FUNCTION__, 
		sqlite3_errmsg(db));
        sqlite3_close(db);
        return -1;
    }

    const char *zSql = "SELECT value FROM blobs WHERE key = ?";
    sqlite3_stmt *pStmt;

    *pzBlob = 0;
    *pnBlob = 0;

    do {
        rc = sqlite3_prepare(db, zSql, -1, &pStmt, 0);
        if( rc!=SQLITE_OK ){
            sqlite3_close(db);
            return rc;
        }

        sqlite3_bind_int(pStmt, 1, key);

        rc = sqlite3_step(pStmt);
        if( rc==SQLITE_ROW ){
            *pnBlob = sqlite3_column_bytes(pStmt, 0);
            *pzBlob = (unsigned char *)malloc(*pnBlob);
            memcpy(*pzBlob, sqlite3_column_blob(pStmt, 0), *pnBlob);
        }

        rc = sqlite3_finalize(pStmt);
    } while( rc==SQLITE_SCHEMA );

    sqlite3_close(db);
    return rc;
}

void db_delete_blob_line(int key)
{
    db_exec_str(NULL, NULL, "DELETE FROM blobs WHERE \"key\"=%d", key);
}

void db_free_blob(unsigned char *zBlob)
{
    free(zBlob);
}

