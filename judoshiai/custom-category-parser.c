/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */ 

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <locale.h>

#ifdef WIN32
#include <process.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <sys/resource.h>
#endif

#include "custom-category.h"

/*
  Description file format:

  Match definition:
  <ident>: c<num> c<num>
  <ident>: <ident>.<num>[.p.<num>]* <ident>.<num>[.p.<num>]*

  rr <ident>: c<num> [c<num>]*

  ko <ident>: c<num> c<num> [c<num> c<num>]*

  pos<num> <ident>.<num>


  Examples:
  -----
  ko a_lohko: c1 c2 c3 c4
  ko b_lohko: c5 c6 c7 c8
  pronssi: a_lohko.2 b_lohko.2
  finaali: a_lohko.1 b_lohko.1

  pos1 finaali.1
  pos2 finaali.2
  pos3 pronssi.1
  -----
  rr a_pooli: c1 c2 c3
  rr b_pooli: c4 c5 c6
  semari_a: a_pooli.1 b_pooli.2
  semari_b: a_pooli.2 b_pooli.1
  finaali: semari_a.1 semari_b.1

  pos1 finaali.1
  pos2 finaali.2
  pos3 semari_a.2
  pos3 semari_b.2 3
  -----
 */

#define expect(_s) do { expect1(_s); if (stop) { /*fprintf(stderr, "%s:%d\n", __FUNCTION__, __LINE__); */ return 0; }} while (0)

static int block(void);

static FILE *f;


typedef enum { 
    dummy, rr, ko, b3, match, dot, colon, prev, ident, number, 
    competitor, eol, eof, pos, dash, lparen, rparen, order,
    page, svg, info, err, id, group
} Symbol;

static char *labels[] = {
    "", "rr", "ko", "b3", "match", "p", "colon", "prev", "ident", "number", 
    "competitor", "eol", "eof", "pos", "dash", "lparen", "rparen", "order",
    "page", "svg", "info", "error", "id", "group"
};

typedef struct symbol {
    char *name;
    int   type;
    int   value;
} sym_t;

typedef struct player {
    int    comp;
    sym_t *match;
    int    pos;
    int    prev[8];
} competitor_t;

typedef struct fight {
    sym_t *name;
    int   type;
#define COMP_STAT_MATCHED 1
    int   status;
    int   level;
    int   pos;
    int   ordernum;
    int   number;
    int   matchord;
    competitor_t c1;
    competitor_t c2;
    int   page;
    double x, y, y1, y2;
    sym_t *reference;
#define FLAG_LONG_NAME 1
#define FLAG_LAST      2
    int flags;
} match_t;

typedef struct position {
    sym_t *match;
    int    pos;
    int    page;
    int    real_contest_pos;
} position_t;

static position_t positions[NUM_CUST_POS]; 

typedef struct round_robin {
    sym_t *name;
    sym_t *rr_matches[NUM_RR_MATCHES];
    int   num_rr_matches;
    competitor_t competitors[NUM_COMPETITORS];
    int   num_competitors;
} round_robin_t;

static round_robin_t round_robin_pools[NUM_ROUND_ROBIN_POOLS];
static int num_round_robin_pools = 0;

typedef struct best_of_three {
    sym_t *name;
    sym_t *matches[3];
} best_of_three_t;

static best_of_three_t best_of_three_pairs[NUM_BEST_OF_3_PAIRS];
static int num_best_of_three_pairs = 0;

typedef struct group {
    int competitors[NUM_COMPETITORS];
    int num_competitors;
} group_t;

static group_t groups[NUM_GROUPS];
static int num_groups = 0;
static int groups_type = 0;
static int group_set = 0;

#define NUM_SYMBOLS 1024 
static struct symbol symbols[NUM_SYMBOLS];
static int num_symbols = 0;

static match_t matches[NUM_CUSTOM_MATCHES];
static int num_matches = 0;
static int match_list[NUM_CUSTOM_MATCHES];

static Symbol sym;
static int value, numvalue, ordernum, pagenum = 1;
static char strvalue[256];
static int linenum = 1;
static char line[256];
static int n = 0;

static competitor_t compvalue;
static int stop = 0;

static int max_comp = 0;

static struct {
    competitor_t first, second;
} match_order[NUM_CUSTOM_MATCHES];
static int num_ord = 0;

static char name_long[64];
static char name_short[16];
static int  competitors_min, competitors_max;

static char errstr[128];
#define ERR_SYM(_test, _str...) \
    do { if (_test) { snprintf(errstr, sizeof(errstr), _str); sym = err; goto out; } } while (0)

static char *readname = NULL;

static sym_t *get_sym(char *name) {
    int i;
    for (i = 0; i < num_symbols; i++) {
        if (!strcmp(symbols[i].name, name))
            return &symbols[i];
    }

    symbols[num_symbols].name = strdup(name);
    return &symbols[num_symbols++];
}

static match_t *get_match_or_create(char *name) {
    int i;

    for (i = 0; i < num_matches; i++) {
        if (!strcmp(matches[i].name->name, name)) {
            return &matches[i];
        }
    }
    if (num_matches < NUM_CUSTOM_MATCHES) {
        matches[num_matches].name = get_sym(name); 
        return &matches[num_matches++];
    }
    return NULL;
}

static match_t *get_match(char *name) {
    int i;

    for (i = 0; i < num_matches; i++) {
        if (!strcmp(matches[i].name->name, name)) {
            return &matches[i];
        }
    }

    return NULL;
}

static void getsym(void)
{
    if (stop) {
        sym = eof;
        goto out;
    }

    int c = fgetc(f);
    while (c == ' ') c = fgetc(f);

    if (c == '\n') {
        linenum++;
        sym = eol;
        goto out;
    }

    if (c < 0 || c > 255) {
        sym = eof;
        goto out;
    }

    if (c == '.') {
        sym = dot;
        goto out;
    }

    if (c == ':') {
        sym = colon;
        goto out;
    }

    if (c == '-') {
        sym = dash;
        goto out;
    }

    if (c == '(') {
        sym = lparen;
        goto out;
    }

    if (c == ')') {
        sym = rparen;
        goto out;
    }

    if (c >= '0' && c <= '9') {
        value = c - '0';
        c = fgetc(f);
        while (c >= '0' && c <= '9') {
            value = 10*value + c - '0';
            c = fgetc(f);
        }
        ungetc(c, f);
        sym = number;
        goto out;
    }

    if (c == '"') {
        n = 0;
        c = fgetc(f);
        while (n < sizeof(line)-1 && c != '"') {
            ERR_SYM((c < ' '), "Invalid character");
            if (c == '\\') c = fgetc(f);
            line[n++] = c;
            c = fgetc(f);
        }
        line[n] = 0;
        sym = ident;
        get_sym(line);
        goto out;
    }

    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        n = 0;
        line[n++] = c;
        c = fgetc(f);
        while (n < sizeof(line)-1 && 
               ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            line[n++] = c;
            c = fgetc(f);
        }
        line[n] = 0;
        ungetc(c, f);

        if (!strcmp(line, "id")) {
            sym = id;
            goto out;
        }

        if (!strcmp(line, "rr")) {
            sym = rr;
            goto out;
        }

        if (!strcmp(line, "ko")) {
            sym = ko;
            goto out;
        }

        if (!strcmp(line, "b3")) {
            sym = b3;
            goto out;
        }

        if (!strcmp(line, "p")) {
            sym = prev;
            goto out;
        }

        if (!strcmp(line, "order")) {
            sym = order;
            goto out;
        }

        if (!strcmp(line, "page")) {
            sym = page;
            goto out;
        }

        if (!strcmp(line, "svg")) {
            sym = svg;
            goto out;
        }

        if (!strcmp(line, "info")) {
            sym = info;
            goto out;
        }

        if (!strncmp(line, "pos", 3) && line[3] >= '0' && line[3] <= '9') {
            sym = pos;
            value = atoi(&line[3]);
            goto out;
        }

        if (!strncmp(line, "group", 5) && line[5] >= '0' && line[5] <= '9') {
            sym = group;
            value = atoi(&line[5]);
            goto out;
        }

        if (line[0] == 'c' && line[1] >= '0' && line[1] <= '9') {
            value = atoi(&line[1]);
            sym = competitor;
            goto out;
        }

        if (line[0] == 'M' && line[1] >= '0' && line[1] <= '9') {
            value = atoi(&line[1]);
            sym = match;
            goto out;
        }

        get_sym(line);
        sym = ident;
        goto out;
    }

 out:
        if (sym == err)
            fprintf(stderr, "Error: %s\n", errstr);
}

/*
static void error(const char msg[])
{
    fprintf(stderr, "line %d: %s %s\n", linenum, msg, labels[sym]);
}
*/

static int accept(Symbol s) {
    if (sym == s) {
        numvalue = value;
        strcpy(strvalue, line);
        getsym();
        return 1;
    }
    return 0;
}

static char message[128];
 
static int expect1(Symbol s) {
    if (accept(s))
        return 1;
    snprintf(message, sizeof(message), "%s line %d:\n  Unexpected symbol \"%s\" (expected \"%s\")", 
            readname, linenum, labels[sym], labels[s]);
    stop = 1;
    return 0;
}

static int player(void) {
    int p = 0;
    memset(&compvalue, 0, sizeof(compvalue));
    if (accept(competitor)) {
        compvalue.comp = numvalue;
        if (max_comp < numvalue)
            max_comp = numvalue;
        return 1;
    } else if (accept(ident)) {
        compvalue.match = get_sym(strvalue);
        expect(dot);
        expect(number);
        compvalue.pos = numvalue;
        while (accept(dot)) {
            expect(prev);
            expect(dot);
            expect(number);
            compvalue.prev[p++] = numvalue;
        }
        return 2;
    }
    return 0;
}

#define checkval(exp, errstr...) do { if (!exp) { stop = 1; snprintf(message, sizeof(message), errstr); return 0; }} while(0) 

static int block(void)
{
    if (stop) return 0;

    if (accept(number)) {
        ordernum = numvalue;
    } else {
        ordernum = 0;
    }

    if (accept(info)) {
        expect(ident);
        strncpy(name_long, strvalue, sizeof(name_long)-1);
        expect(ident);
        strncpy(name_short, strvalue, sizeof(name_short)-1);
        expect(number);
        competitors_min = numvalue;
        expect(number);
        competitors_max = numvalue;
        expect(eol);
        return 1;
    }

    if (accept(svg)) {
        if (accept(page)) {
            expect(number);
            pagenum = numvalue;
        }
        expect(eol);
        return 1;
    }

    if (accept(ident)) {
        match_t *m = get_match_or_create(strvalue);
        m->ordernum = ordernum;
        m->number = linenum;
        m->page = pagenum;
        expect(colon);
        player();
        m->c1 = compvalue;
        player();
        m->c2 = compvalue;
        expect(eol);
        return 1;
    }

    if (accept(b3)) {
        char buf[64];
        match_t *m[3];
        int i;
        checkval((num_best_of_three_pairs < NUM_BEST_OF_3_PAIRS), 
                 "Too many best of three pools, max = %d", NUM_BEST_OF_3_PAIRS);
        expect(ident);
        best_of_three_pairs[num_best_of_three_pairs].name = get_sym(strvalue);
        for (i = 0; i < 3; i++) {
            snprintf(buf, sizeof(buf), "%s_%d", strvalue, i+1);
            m[i] = get_match_or_create(buf);
            m[i]->ordernum = ordernum;
            m[i]->number = linenum;
            m[i]->page = pagenum;
            best_of_three_pairs[num_best_of_three_pairs].matches[i] = get_sym(buf);
        }
        expect(colon);
        player();
        m[0]->c1 = m[1]->c1 = m[2]->c1 = compvalue;
        player();
        m[0]->c2 = m[1]->c2 = m[2]->c2 = compvalue;
        expect(eol);
        num_best_of_three_pairs++;
        return 1;
    }

    if (accept(rr)) {
        round_robin_t *pool = &round_robin_pools[num_round_robin_pools];
        competitor_t c[20];
        int num_players = 0, i, j;
        char buf[64];
        checkval((num_round_robin_pools < NUM_ROUND_ROBIN_POOLS), 
                 "Too many round robin pools, max = %d", NUM_ROUND_ROBIN_POOLS);
        expect(ident);
        pool->name = get_sym(strvalue);
        /*
        match_t *m1 = get_match(strvalue);
        m1->ordernum = ordernum;
        m1->number = linenum;
        m1->page = pagenum;
        m1->type = rr;
        */
        expect(colon);
        while (player()) {
            c[num_players++] = compvalue;
            pool->competitors[pool->num_competitors++] = compvalue;
        }
        // Create round robin matches
        for (i = 0; i < num_players-1; i++) {
            for (j = i+1; j < num_players; j++) {
                checkval((pool->num_rr_matches < NUM_RR_MATCHES), 
                         "Too many round robin matches, max = %d", NUM_RR_MATCHES);

                sprintf(buf, "%s_%d_%d", round_robin_pools[num_round_robin_pools].name->name, i, j);
                match_t *m = get_match_or_create(buf);
                m->ordernum = ordernum;
                m->number = linenum;
                m->page = pagenum;
                m->c1 = c[i];
                m->c2 = c[j];
                pool->rr_matches[pool->num_rr_matches++] = get_sym(buf);
            }
        }
        expect(eol);
        num_round_robin_pools++;
        return 1;
    }

    if (accept(ko)) {
        int num_players = 0;
        char buf[64];
        expect(ident);
        sym_t *name = get_sym(strvalue), *reference = NULL;
        expect(colon);
        int i = 1, n, lev = 1;
        while (player()) {
            checkval((num_matches < NUM_CUSTOM_MATCHES), 
                     "Too many matches, max = %d", NUM_CUSTOM_MATCHES);

            sprintf(buf, "%s_%d_%d", name->name, lev, i);
            match_t *m = get_match_or_create(buf);
            if (i == 1) reference = get_sym(buf);
            else {
                m->reference = reference;
                //m->x = 0.0; m->y = y_shift*(i - 1);
            }
            m->ordernum = ordernum;
            m->number = linenum;
            m->page = pagenum;
            m->c1 = compvalue;
            if (!player()) {
                fprintf(stderr, "Line %d: missing player\n", linenum);
                return 1;
            }
            m->c2 = compvalue;
            m->flags |= FLAG_LONG_NAME;
            num_players += 2;
            i++;
        }
        // Create knock out matches
        for (n = num_players/4; n >= 1; n = n/2) {
            lev++;
            for (i = 0; i < n; i++) {
                if (n == 1)
                    sprintf(buf, "%s", name->name);
                else
                    sprintf(buf, "%s_%d_%d", name->name, lev, i+1);
                match_t *m = get_match_or_create(buf), *m1, *m2;

                if (n == 1) m->flags |= FLAG_LAST;

                m->ordernum = ordernum;
                m->number = linenum;
                m->page = pagenum;

                sprintf(buf, "%s_%d_%d", name->name, lev-1, 2*i+1);
                m->c1.match = get_sym(buf);
                m->c1.pos = 1;

                sprintf(buf, "%s_%d_%d", name->name, lev-1, 2*i+2);
                m->c2.match = get_sym(buf);
                m->c2.pos = 1;

                checkval((num_matches < NUM_CUSTOM_MATCHES-1), 
                         "Too many matches, max = %d", NUM_CUSTOM_MATCHES);
                m1 = get_match_or_create(m->c1.match->name);
                m2 = get_match_or_create(m->c2.match->name);
                if (m1 && m2) {
                    m->reference = reference;
                    //m->x = x_shift*(lev-1); 
                    m->y = (m1->y + m2->y)/2;
                    m->y1 = m1->y - m->y;
                    m->y2 = m2->y - m->y;
                }
            }
        } 
        expect(eol);
        return 1;
    }

    if (accept(order)) {
        while (player()) {
            checkval((num_ord < NUM_CUSTOM_MATCHES), 
                     "Too many matches in order, max = %d", NUM_CUSTOM_MATCHES);
            match_order[num_ord].first = compvalue;
            expect(dash);
            if (player()) {
                match_order[num_ord++].second = compvalue;
            }
        }
         
        expect(eol);
        return 1;
    }

    if (accept(pos)) {
        int posval = numvalue-1;
        checkval((posval >= 0 && posval < NUM_CUST_POS), 
                 "Wrong position number, values 1-%d are valid", NUM_CUST_POS);

        expect(ident);
        sym_t *s = get_sym(strvalue);
        positions[posval].match = s;
        expect(dot);
        expect(number);
        positions[posval].pos = numvalue;
        positions[posval].page = pagenum;
        if (accept(number)) {
            positions[posval].real_contest_pos = numvalue;
        }
        expect(eol);
        return 1;
    }

    if (accept(group)) {
        int grpval = numvalue-1;
        checkval((grpval >= 0 && grpval < NUM_GROUPS), "Wrong group index, 1-%d are valid", NUM_GROUPS);
        if (accept(rr)) {
            groups_type = 1;
        }
        groups[grpval].num_competitors = 0;
        while (player()) {
            if (compvalue.comp)
                groups[grpval].competitors[groups[grpval].num_competitors++] = compvalue.comp;
        }        
        if (grpval+1 > num_groups) num_groups = grpval+1;
        group_set = 1;
        expect(eol);
        return 1;
    }

    accept(eol);
    return 1;
}

static void solve(sym_t *mname, int level, int pos) {
    match_t *m;
    
    if (!mname)
        return;

    m = get_match(mname->name);
    if (!m) { // round robin
        return;
    }

    if (!m->level) {
        m->level = level;
        m->pos = pos;
    }

    if (m->c1.comp && m->c2.comp) {
        //printf("%s: comp%d comp%d\n", m->name, m->c1.comp, m->c2.comp);
        return;
    }
    if (m->c1.comp) {
        solve(m->c2.match, level+1, pos);
        //printf("%s: comp%d match=%s.%d\n", m->name, m->c1.comp, m->c2.match, m->c2.pos);
        return;
    }
    if (m->c2.comp) {
        solve(m->c1.match, level+1, pos);
        //printf("%s: match=%s.%d comp%d\n", m->name, m->c1.match, m->c1.pos, m->c2.comp);
        return;
    }
    solve(m->c1.match, level+1, pos);
    solve(m->c2.match, level+1, pos);
    //printf("%s: match=%s.%d match=%s.%d\n", m->name, m->c1.match, m->c1.pos, m->c2.match, m->c2.pos);
}

static void sort_matches(void) {
    int i, j, i1, j1;

    // find explicite match numbers 
    for (i = 0; i < num_matches; i++) {
        for (j = 0; j < num_ord; j++) {
            if (!memcmp(&matches[i].c1, &match_order[j].first.comp, sizeof(competitor_t)) &&
                !memcmp(&matches[i].c2, &match_order[j].second.comp, sizeof(competitor_t))) {
                matches[i].matchord = j+1;
                break;
            }
        }
    }

    for (i1 = 0; i1 < num_matches-1; i1++) {
        for (j1 = i1+1; j1 < num_matches; j1++) {
            i = match_list[i1];
            j = match_list[j1];
            int order = matches[i].ordernum > matches[j].ordernum && 
                matches[i].ordernum && matches[j].ordernum;
            int level = matches[i].level < matches[j].level &&
                matches[i].level && matches[j].level;
            int number = matches[i].level == matches[j].level && 
                matches[i].number > matches[j].number;
            if (order || level || number) {
                /*printf("sort: %d:%d order=%d level=%d number=%d type=%d\n", i, j,
                  order, level, number, typ);*/
                int tmp = match_list[i1];
                match_list[i1] = match_list[j1];
                match_list[j1] = tmp;
                /*
                match_t m = matches[i];
                matches[i] = matches[j];
                matches[j] = m;
                */
            }
        }
    }

    for (i1 = 0; i1 < num_matches-1; i1++) {
        for (j1 = i1+1; j1 < num_matches; j1++) {
            i = match_list[i1];
            j = match_list[j1];
            if (matches[i].matchord && matches[j].matchord &&
                matches[i].matchord > matches[j].matchord) {
                //printf("ord sort: %d:%d\n", i, j);
                int tmp = match_list[i1];
                match_list[i1] = match_list[j1];
                match_list[j1] = tmp;
                /*
                match_t m = matches[i];
                matches[i] = matches[j];
                matches[j] = m;
                */
            }
        }
    }
}

static void program(void) {
    getsym();
    while (sym != eof) {
        if (block() == 0) return;
    }
}


static int get_match_num(char *name) {
    int i;
    for (i = 0; i < num_matches; i++) {
        if (!strcmp(matches[match_list[i]].name->name, name))
            return i+1;
    }
    return 0;
}

static int get_rr_num(char *name) {
    int i;
    for (i = 0; i < num_round_robin_pools; i++) {
        if (!strcmp(round_robin_pools[i].name->name, name))
            return i+1;
    }
    return 0;
}

static int get_b3_num(char *name) {
    int i;
    for (i = 0; i < num_best_of_three_pairs; i++) {
        if (!strcmp(best_of_three_pairs[i].name->name, name))
            return i+1;
    }
    return 0;
}

static struct player_bare get_palyer_bare(struct player *c)
{
    struct player_bare b;

    if (c->comp) {
        b.type = COMP_TYPE_COMPETITOR;
        b.num = c->comp;
    } else if (get_rr_num(c->match->name)) {
        b.type = COMP_TYPE_ROUND_ROBIN;
        b.num = get_rr_num(c->match->name);
        b.pos = c->pos;
    } else if (get_b3_num(c->match->name)) {
        b.type = COMP_TYPE_BEST_OF_3;
        b.num = get_b3_num(c->match->name);
        b.pos = c->pos;
    } else {
        int j;
        b.type = COMP_TYPE_MATCH;
        b.num = get_match_num(c->match->name);
        b.pos = c->pos;
        for (j = 0; j < 8; j++)
            b.prev[j] = c->prev[j];
    }

    return b;
}

#define checkval2(_exp, _msg...) do { if (!_exp) { snprintf(message, sizeof(message), _msg); return message;}} \
    while (0)


char *read_custom_category(char *name, struct custom_data *data)
{
    int i;

    readname = name;

    memset(&matches, 0, sizeof(matches));
    memset(&match_list, 0, sizeof(match_list));
    memset(&positions, 0, sizeof(positions));
    memset(&round_robin_pools, 0, sizeof(round_robin_pools));
    memset(&symbols, 0, sizeof(symbols));
    memset(&match_order, 0, sizeof(match_order));
    memset(&best_of_three_pairs, 0, sizeof(best_of_three_pairs));
    num_round_robin_pools = 0;
    num_symbols = 0;
    num_matches = 0;
    num_best_of_three_pairs = 0;
    num_groups = 0;
    group_set = 0;
    groups_type = 0;
    linenum = 1;
    stop = 0;
    max_comp = 0;
    num_ord = 0;
    competitors_min = competitors_max = 0;
    value = numvalue = ordernum = n = max_comp = stop = 0;
    pagenum = linenum = 1;

    for (i = 0; i < NUM_CUSTOM_MATCHES; i++)
        match_list[i] = i;

    f = fopen(name, "r");
    if (!f) {
        snprintf(message, sizeof(message), "Cannot read %s", name);
        return message;
    }

    program();

    fclose(f);

    if (stop)
        return message;

    for (i = 1; i < NUM_CUST_POS ; i++) {
        if (!positions[i].match)
            continue;
        
        solve(positions[i].match, 1, i);
    }

    sort_matches();

    data->num_round_robin_pools = 0;
    data->num_matches = 0;
    data->num_positions = 0;

    // round robin
    for (i = 0; i < num_round_robin_pools; i++) {
        int j;
        for (j = 0; j < round_robin_pools[i].num_rr_matches; j++) {
            data->round_robin_pools[i].rr_matches[j] = 
                get_match_num(round_robin_pools[i].rr_matches[j]->name);
            checkval2((data->round_robin_pools[i].rr_matches[j]), "Pool %s: match %s doesn't exist", 
                     round_robin_pools[i].name->name,
                     round_robin_pools[i].rr_matches[j]->name);
        } // for
        for (j = 0; j < round_robin_pools[i].num_competitors; j++) {
            data->round_robin_pools[i].competitors[j] = 
                get_palyer_bare(&round_robin_pools[i].competitors[j]);
        }

        data->round_robin_pools[i].num_rr_matches = round_robin_pools[i].num_rr_matches;
        data->round_robin_pools[i].num_competitors = round_robin_pools[i].num_competitors;
        snprintf(data->round_robin_pools[i].name, 16, "%s", round_robin_pools[i].name->name);
    }
    data->num_round_robin_pools = num_round_robin_pools;

    // best of three
    for (i = 0; i < num_best_of_three_pairs; i++) {
        int j;
        for (j = 0; j < 3; j++) {
            data->best_of_three_pairs[i].matches[j] = 
                get_match_num(best_of_three_pairs[i].matches[j]->name);
            checkval2((data->best_of_three_pairs[i].matches[j]), "Best of three %s: match %s doesn't exist", 
                     best_of_three_pairs[i].name->name,
                     best_of_three_pairs[i].matches[j]->name);
        }
        snprintf(data->best_of_three_pairs[i].name, 16, "%s", best_of_three_pairs[i].name->name);
    }
    data->num_best_of_three_pairs = num_best_of_three_pairs;

    // matches
    for (i = 0; i < num_matches; i++) {
        match_t *m = &matches[match_list[i]];
        data->matches[i].c1 = get_palyer_bare(&m->c1);
        data->matches[i].c2 = get_palyer_bare(&m->c2);
    }
    data->num_matches = num_matches;

    // positions
    for (i = 0; positions[i].match; i++) {
        if (get_rr_num(positions[i].match->name)) {
            data->positions[i].type = COMP_TYPE_ROUND_ROBIN;
            data->positions[i].match = get_rr_num(positions[i].match->name);
        } else if (get_b3_num(positions[i].match->name)) {
            data->positions[i].type = COMP_TYPE_BEST_OF_3;
            data->positions[i].match = get_b3_num(positions[i].match->name);
        } else {
            data->positions[i].type = COMP_TYPE_MATCH;
            data->positions[i].match = get_match_num(positions[i].match->name);
        }
        checkval2((data->positions[i].match > 0), "Wrong match name \"%s\" in pos%d", 
                  positions[i].match->name, positions[i].pos);
        data->positions[i].pos = positions[i].pos;
        data->positions[i].real_contest_pos = positions[i].real_contest_pos;
    }
    data->num_positions = i;

    // groups
    if (num_groups == 0) {
        // create default groups
            
        // find best of 3
        for (i = 0; i < data->num_best_of_three_pairs && num_groups < NUM_GROUPS; i++) {
            int n = data->best_of_three_pairs[i].matches[0];
            if (data->matches[n].c1.type == COMP_TYPE_COMPETITOR) {
                groups[num_groups].competitors[0] = data->matches[n].c1.num;
                groups[num_groups].competitors[1] = data->matches[n].c2.num;
                groups[num_groups].num_competitors = 2;
                num_groups++;
            }
        }

        // round robin
        for (i = 0; i < data->num_round_robin_pools && num_groups < NUM_GROUPS; i++) {
            if (data->round_robin_pools[i].competitors[0].type == COMP_TYPE_COMPETITOR) {
                int j;
                for (j = 0; j < data->round_robin_pools[i].num_competitors; j++)
                    groups[num_groups].competitors[j] = data->round_robin_pools[i].competitors[j].num;
                num_groups++;
                groups_type = 1;
            }
        }

        // matches
        if (num_groups == 0) {
            // assume we have a knock out system
            int n = competitors_max/4;
            for (i = 0; i < 4; i++) {
                int j;
                for (j = 0; j < n; j++) {
                    groups[i].competitors[j] = i*n + j + 1;
                }
                groups[i].num_competitors = n;
            }
        }
    }

    for (i = 0; i < num_groups; i++) {
        int j;
        for (j = 0; j < groups[i].num_competitors; j++)
            data->groups[i].competitors[j] = groups[i].competitors[j];
        data->groups[i].num_competitors = groups[i].num_competitors;
    }
    data->num_groups = num_groups;

    // info
    checkval2((competitors_min >= 1 && competitors_min <= competitors_max &&
               name_long[0] && name_short[0]), "Wrong info");
    data->competitors_min = competitors_min;
    data->competitors_max = competitors_max;
    strncpy(data->name_long, name_long, sizeof(data->name_long)-1);
    strncpy(data->name_short, name_short, sizeof(data->name_short)-1);

    return 0;
}
