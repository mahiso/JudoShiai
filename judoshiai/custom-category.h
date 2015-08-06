#ifndef _CUSTOM_CATEGORY_H_
#define _CUSTOM_CATEGORY_H_

/** Custom brackets */

#ifndef NUM_COMPETITORS
  #define NUM_COMPETITORS 128
#endif

#define NUM_CUSTOM_MATCHES   512
#define NUM_CUST_POS          16
#define NUM_ROUND_ROBIN_POOLS 16
#define NUM_RR_MATCHES        64
#define NUM_BEST_OF_3_PAIRS   16
#define NUM_GROUPS             8

typedef struct player_bare {
#define COMP_TYPE_COMPETITOR  1
#define COMP_TYPE_MATCH       2
#define COMP_TYPE_ROUND_ROBIN 3
#define COMP_TYPE_BEST_OF_3   4
    int    type;
    int    num; // competitor num or match num or rr num
    int    pos; // winner = 1, loser = 2, or rr position
    char   prev[8];
} competitor_bare_t;

typedef struct fight2 {
    competitor_bare_t c1;
    competitor_bare_t c2;
} match_bare_t;

typedef struct position_bare {
    int    type;
    int    match;
    int    pos;
    int    real_contest_pos;
} position_bare_t;

typedef struct round_robin_bare {
    char name[16];
    int rr_matches[NUM_RR_MATCHES];
    int num_rr_matches;
    competitor_bare_t competitors[NUM_COMPETITORS];
    int num_competitors;
} round_robin_bare_t;

typedef struct best_of_three_bare {
    char name[16];
    int matches[3];
} best_of_three_bare_t;

typedef struct group_bare {
    int competitors[NUM_COMPETITORS];
    int num_competitors;
    int type; // 0 = knock out, 1 = round robin
} group_bare_t;

struct custom_data {
    char name_long[64];
    char name_short[16];
    int  competitors_min, competitors_max;

    round_robin_bare_t round_robin_pools[NUM_ROUND_ROBIN_POOLS];
    int num_round_robin_pools;

    best_of_three_bare_t best_of_three_pairs[NUM_BEST_OF_3_PAIRS];
    int num_best_of_three_pairs;

    match_bare_t matches[NUM_CUSTOM_MATCHES];
    int num_matches;

    position_bare_t positions[NUM_CUST_POS];
    int num_positions;

    group_bare_t groups[NUM_GROUPS];
    int num_groups;
};

extern char *read_custom_category(char *name, struct custom_data *data);
extern int make_svg_file(int argc, char *argv[]);

#endif
