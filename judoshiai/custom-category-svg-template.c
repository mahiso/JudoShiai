/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2015 by Hannu Jokinen
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

typedef struct allocated_space {
    double next_x, next_y, out_x, out_y;
} allocated_space_t;

static char *svg_start =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n"
    "<svg\n"
    "   xmlns=\"http://www.w3.org/2000/svg\"\n"
    "   width=\"8888px\"\n"
    "   height=\"9999px\"\n"
    "   id=\"judoshiai\"\n"
    "   version=\"1.1\"\n"
    "   xmlns:sodipodi=\"http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd\"\n"
    "   xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\">\n"
    "<style type=\"text/css\"> <![CDATA[ tspan.cmpx { fill: red; } ]]> </style>\n"
    "<sodipodi:namedview\n"
    "id=\"base\"\n"
    "pagecolor=\"#ffffff\"\n"
    "bordercolor=\"#666666\"\n"
    "borderopacity=\"1.0\"\n"
    "inkscape:pageopacity=\"0.0\"\n"
    "inkscape:pageshadow=\"2\"\n"
    "inkscape:zoom=\"1\"\n"
    "inkscape:cx=\"185.84668\"\n"
    "inkscape:cy=\"880.85263\"\n"
    "inkscape:document-units=\"px\"\n"
    "inkscape:current-layer=\"layer1\"\n"
    "inkscape:window-width=\"877\"\n"
    "inkscape:window-height=\"739\"\n"
    "inkscape:window-x=\"177\"\n"
    "inkscape:window-y=\"31\"\n"
    "showgrid=\"false\"\n"
    "inkscape:window-maximized=\"0\" />\n"
    "   <g\n"
    "      id=\"layer1\"\n"
    "      style=\"opacity:1\">\n";

char *xxx =    "<rect x='0.00' y='0.00' width='630.00' height='891.00' style='fill:white;stroke:none'/>\n";

static char *svg_end =
    "   </g>\n"
    "</svg>\n";

static char *svg_page_header_text =
    "<text\n"
    "   style=\"font-size:21.38px;font-style:normal;font-weight:bold;letter-spacing:0px;\n"
    "           word-spacing:0px;fill:#000000;fill-opacity:1;stroke:none;font-family:Arial;\n"
    "           text-anchor:middle;text-align:center\"\n"
    "   x=\"315.00\" y=\"44.55\" id=\"text1100\">\n"
    "  <tspan id=\"tspan1300\">%i-competition  %i-date  %i-place   %i-catname</tspan></text>\n";

#define svg_text \
    "<text x=\"%.2f\" y=\"%.2f\" font-family=\"Arial\" font-size=\"10\" fill=\"black\"><tspan>%s</tspan></text>\n"

static char *competitor_style  = "font-size:10px;font-family:Arial;fill:#000000";
static char *competitor_name_1 = "hm1-first-s-last-s-club";
static char *competitor_name_2 = "hm1-last";
static FILE *fout;
static double res_line = 16.0, x_init = 200.0;
static double x_shift = 100.0, y_shift = 60.0;
static struct custom_data cd;
static double start_x = 20.0, start_y = 80.0, max_x = 0, max_y = 0;
static double rr_num_w = 25.0, rr_name_w = 120.0, rr_club_w = 90.0, rr_res_w = 50;

static char match_flags[NUM_CUSTOM_MATCHES];
#define MATCH_FLAG_DRAWN 1

#define MAX(a, b) (a > b ? a : b)

static void draw_match(struct custom_data *cd,
                       double x, double y, allocated_space_t *space, int num)
{
    match_bare_t *m = &cd->matches[num-1];
    int longname = 0;
    double xc, yc;
    double xs1 = x, xs2 = x;
    double ys1 = y;
    double ys2 = y + y_shift/2.0;
    double maxy = ys2 + y_shift/2.0;
    double maxx = x;
    double r = 8.05;
    allocated_space_t sp;

    if (match_flags[num-1] & MATCH_FLAG_DRAWN) {
        space->out_x = 0;
        space->out_y = 0;
        space->next_x = x;
        space->next_y = y;
        return;
    }

    match_flags[num-1] |= MATCH_FLAG_DRAWN;

    if (m->c1.type == COMP_TYPE_MATCH) {
        draw_match(cd, x, y, &sp, m->c1.num);
        ys1 = sp.out_y;
        xs1 = sp.out_x;
        maxx = MAX(maxx, sp.next_x);
        maxy = MAX(maxy, sp.next_y);
    }

    if (m->c2.type == COMP_TYPE_MATCH) {
        draw_match(cd, x, sp.next_y, &sp, m->c2.num);
        ys2 = sp.out_y;
        xs2 = sp.out_x;
        maxx = MAX(maxx, sp.next_x);
        maxy = MAX(maxy, sp.next_y);
    }

    if (xs1 == 0 && xs2 == 0) {
        xs1 = x;
        xs2 = x;
        ys1 = y;
        ys2 = y + y_shift/2.0;
    } else if (xs1 == 0) {
        xs1 = xs2;
        ys1 = ys2 - y_shift/2.0;
    } else if (xs2 == 0) {
        xs2 = xs1;
        ys2 = ys1 + y_shift/2.0;
    }

    if (m->c1.type == COMP_TYPE_COMPETITOR) longname = 1;

    xc = MAX(xs1, xs2) + (longname ? x_init : x_shift);
    yc = (ys1 + ys2)/2.0;
    space->out_x = xc + r;
    space->out_y = yc;
    space->next_x = xc + r;
    space->next_y = maxy;

    fprintf(fout, "<g>\n");
    fprintf(fout, "  <circle cx='%.2f' cy='%.2f' r='%.2f' stroke='black' stroke-width='1' fill='white'/>\n",
            xc, yc, r);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>%d</text>\n",
            xc, yc+4.0, competitor_style, num);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-1-%s</text>\n",
            xs1 + 4.0, ys1-2.0, competitor_style, num,
            longname ? competitor_name_1 : competitor_name_2);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-2-%s</text>\n",
            xs2 + 4.0, ys2-2.0,  competitor_style, num,
            longname ? competitor_name_1 : competitor_name_2);

    fprintf(fout, "<path "
            "d='M %.2f,%.2f l %.2f,0.00 "
            "a%.2f,%.2f 0 0,1 %.2f,%.2f "
            "l 0.00,%.2f "
            "M %.2f,%.2f l %.2f,0.00 "
            "a%.2f,%.2f 0 0,0 %.2f,-%.2f "
            "l 0.00,-%.2f' "
            "style='fill:none;stroke:black;stroke-width:1' />\n",
            xs1,ys1,  xc - xs1 - r*0.5,
            r*0.5,r*0.5, r*0.5,r*0.5,
            yc - ys1 - r - 0.5*r,
            xs2,ys2,  xc - xs2 - r*0.5,
            r*0.5,r*0.5, r*0.5,r*0.5,
            ys2 - yc - r - 0.5*r);

#if 0
    if (m->flags & FLAG_LAST) {
        fprintf(out, "<path d='M %.2f,%.2f l %.2f,0' style='fill:none;stroke:black;stroke-width:1'/>\n",
                m->x + r, m->y, x_shift - r);
        fprintf(out, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-winner-%s</text>\n",
                m->x + r + 4.0, m->y,  competitor_style, num,
                longname ? competitor_name_1 : competitor_name_2);
    }
#endif

    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%dp1-1</text>\n",
            xc + 2.0, ys1 + 4.0,  competitor_style, num);

    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%dp2-1</text>\n",
            xc + 2.0, ys2,  competitor_style, num);

    fprintf(fout, "</g>\n");
}

static int draw_comp(struct custom_data *cd, competitor_bare_t *comp, double x, double y, allocated_space_t *space)
{
    if (comp->type == COMP_TYPE_MATCH) {
        if (match_flags[comp->num-1] & MATCH_FLAG_DRAWN)
            return 0;

        draw_match(cd, x, y, space, comp->num);

        fprintf(fout, "<path d='M %.2f,%.2f l %.2f,0' style='fill:none;stroke:black;stroke-width:1'/>\n",
                space->out_x, space->out_y, x_shift);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-winner-%s</text>\n",
                space->out_x + 4.0, space->out_y-2.0,  competitor_style, comp->num, competitor_name_2);

        max_x = MAX(max_x, (space->out_x + x_shift));
    }

    return 0;
}

static void draw_verical_lines(double x[], double y, double len)
{
    int i;
    double x1 = x[0];

    for (i = 1; x[i]; i++) {
        x1 += x[i];
        fprintf(fout, "<path d='"
                "M %.2f %.2f l 0,%.2f"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                x1, y, len);
    }
}

static void draw_header_texts(double x[], double y, char *text[])
{
    int i;
    double x1 = x[0];

    for (i = 1; x[i]; i++) {
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
                "text-anchor:middle;text-align:center'>%s</text>\n",
                x1 + x[i]/2.0, y, competitor_style, text[i]);
        x1 += x[i];
    }
}

static int draw_bestof3(struct custom_data *cd, best_of_three_bare_t *b3,
                        double x, double y, allocated_space_t *space)
{
    int i;
    double tot_w = 7*rr_num_w + rr_name_w + rr_club_w;
    double tot_h = 3*res_line;
    double sx = x, sy = y + res_line;

    space->next_x = sx + tot_w;
    space->next_y = sy + tot_h + 20.0;

    // competitor box
    fprintf(fout, "<path d='"
            "M %.2f %.2f"
            "l %.2f,0 l 0,%.2f l -%.2f,0 l 0,-%.2f"
            "' style='fill:none;stroke:black;stroke-width:2' />\n",
            sx, sy,
            tot_w, tot_h, tot_w, tot_h);
    // vertical lines
    fprintf(fout, "<path d='"
            "M %.2f %.2f l 0,%.2f"
            "' style='fill:none;stroke:black;stroke-width:1' />\n",
            sx + rr_num_w, sy + res_line, 2*res_line);
    fprintf(fout, "<path d='"
            "M %.2f %.2f l 0,%.2f"
            "' style='fill:none;stroke:black;stroke-width:1' />\n",
            sx + rr_num_w + rr_name_w, sy + res_line, 2*res_line);
    for (i = 0; i < 6; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l 0,%.2f"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx + rr_num_w + rr_name_w + rr_club_w + i*rr_num_w,
                sy + res_line, 2*res_line);
    // horisontal lines
    for (i = 0; i < 2; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l %.2f,0"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx, sy + (i+1)*res_line, tot_w);
    // header texts
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>%%t0</text>\n",
            sx+tot_w/2, sy-4.0, competitor_style);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>#</text>\n",
            sx+rr_num_w/2, sy+res_line-4.0, competitor_style);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>%%t6</text>\n",
            sx+rr_num_w+rr_name_w/2, sy+res_line-4.0, competitor_style);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>%%t8</text>\n",
            sx+rr_num_w+rr_name_w+rr_club_w/2, sy+res_line-4.0, competitor_style);
    for (i = 0; i < 3; i++) {
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
                "text-anchor:middle;text-align:center'>1</text>\n",
                sx+rr_num_w+rr_name_w+rr_club_w+2*i*rr_num_w+rr_num_w/2, sy+res_line-4.0,
                competitor_style);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
                "text-anchor:middle;text-align:center'>2</text>\n",
                sx+rr_num_w+rr_name_w+rr_club_w+(2*i+1)*rr_num_w+rr_num_w/2, sy+res_line-4.0,
                competitor_style);
    }

    // competitors
    int match_num = b3->matches[0];
    competitor_bare_t *c[2];
    c[0] = &cd->matches[match_num-1].c1;
    c[1] = &cd->matches[match_num-1].c2;

    for (i = 0; i < 2; i++) {
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>%d</text>\n",
                sx + rr_num_w/2.0, sy + (i + 2)*res_line - 4.0,
                competitor_style,
                c[i]->type == COMP_TYPE_COMPETITOR ? c[i]->num : i+1);

        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-%d-first-s-last</text>\n",
                sx + rr_num_w + 6.0, sy + (i + 2)*res_line - 4.0,
                competitor_style, match_num, i+1);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-%d-club</text>\n",
                sx + rr_num_w + rr_name_w + 6.0, sy + (i + 2)*res_line - 4.0,
                competitor_style, match_num, i+1);

        // points
        int j;
        double x1 = sx + rr_num_w + rr_name_w + rr_club_w;

        for (j = 0; j < 3; j++) {
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%m%dp%d</text>\n",
                    x1 + (2*j+1-i)*rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0,
                    competitor_style, b3->matches[j], i+1);
        }
    }

    // points box
    sx += tot_w;
    tot_w = 3*rr_num_w;
    space->next_x = sx + tot_w;

    fprintf(fout, "<path d='"
            "M %.2f %.2f"
            "l %.2f,0 l 0,%.2f l -%.2f,0 l 0,-%.2f"
            "' style='fill:none;stroke:black;stroke-width:2' />\n",
            sx, sy,
            tot_w, tot_h, tot_w, tot_h);
    // vertical lines
    double win_cols[] = {sx, rr_num_w, rr_num_w, rr_num_w, 0};
    draw_verical_lines(win_cols, sy + res_line, 2*res_line);
    // horisontal lines
    for (i = 0; i < 2; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l %.2f,0"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx, sy + (i+1)*res_line, tot_w);
    // header texts
    char *win_hdr_texts[] = {NULL, "Wins", "Pts", "%t22", NULL};
    draw_header_texts(win_cols, sy+res_line-4.0, win_hdr_texts);
    // points
    for (i = 0; i < 2; i++) {
        if (c[i]->type == COMP_TYPE_COMPETITOR) {
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c%dw</text>\n",
                    sx + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, c[i]->num);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c%dp</text>\n",
                    sx + rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, c[i]->num);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c%dr</text>\n",
                    sx + 2*rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, c[i]->num);
        } else {
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c-%s-%dw</text>\n",
                    sx + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, b3->name, i+1);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c-%s-%dp</text>\n",
                    sx + rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, b3->name, i+1);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c-%s-%dr</text>\n",
                    sx + 2*rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, b3->name, i+1);
        }
    }

    // matches box
    sx = x;
    sy = sy + tot_h + 20.0;
    tot_w = rr_num_w*3 + rr_name_w*2 + rr_res_w*2;
    tot_h = 4*res_line;
    space->next_x = MAX(space->next_x, sx + tot_w);
    space->next_y = sy + tot_h + 20.0;

    fprintf(fout, "<path d='"
            "M %.2f %.2f"
            "l %.2f,0 l 0,%.2f l -%.2f,0 l 0,-%.2f"
            "' style='fill:none;stroke:black;stroke-width:2' />\n",
            sx, sy,
            tot_w, tot_h, tot_w, tot_h);
    // vertical lines
    double match_cols[] = {sx, rr_num_w, rr_name_w, rr_num_w, rr_num_w, rr_name_w, rr_res_w, rr_res_w, 0};
    draw_verical_lines(match_cols, sy + res_line, 3*res_line);
    // horizontal lines
    for (i = 0; i < 3; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l %.2f,0"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx, sy + (i+1)*res_line, tot_w);
    // header texts
    char *match_hdr_texts[] = {NULL, "%t14", "%t16", "", "", "%t15", "%t17", "%t19", NULL};
    draw_header_texts(match_cols, sy+res_line-4.0, match_hdr_texts);
    // matches
    for (i = 0; i < 3; i++) {
        int match_num = b3->matches[i];

        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>%d</text>\n",
                sx+rr_num_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-1-first-s-last</text>\n",
                sx+rr_num_w+6.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-2-first-s-last</text>\n",
                sx+rr_num_w*3+rr_name_w+6.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>1</text>\n",
                sx+rr_num_w+rr_name_w+rr_num_w/2.0, sy+(i+2)*res_line-4.0, competitor_style);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>2</text>\n",
                sx+2*rr_num_w+rr_name_w+rr_num_w/2.0, sy+(i+2)*res_line-4.0, competitor_style);
        // match result
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>%%m%dp1 - %%m%dp2</text>\n",
                sx+3*rr_num_w+2*rr_name_w+rr_res_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num, match_num);
        // time
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>%%m%dtime</text>\n",
                sx+3*rr_num_w+2*rr_name_w+rr_res_w+rr_res_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
    }

    return 0;
}

static int same_comps(competitor_bare_t *c1, competitor_bare_t *c2)
{
    if (c1->type != c2->type) return 0;
    if (c1->num != c2->num) return 0;
    if (c1->type != COMP_TYPE_COMPETITOR && c1->pos != c2->pos) return 0;
    return 1;
}

static int find_comps_match(struct custom_data *cd, int num, int matches[],
                            competitor_bare_t *c1, competitor_bare_t *c2)
{
    int i;
    for (i = 0; i < num; i++) {
        match_bare_t *m = &cd->matches[matches[i]-1];
        if (same_comps(&m->c1, c1) && same_comps(&m->c2, c2))
            return matches[i];
        if (same_comps(&m->c1, c2) && same_comps(&m->c2, c1))
            return -matches[i];
    }
    return 0;
}

static int draw_round_robin(struct custom_data *cd, struct round_robin_bare *rr,
                            double x, double y, allocated_space_t *space)
{
    int i;
    double tot_w = rr_num_w + rr_name_w + rr_club_w + rr->num_competitors*rr_num_w;
    double tot_h = (rr->num_competitors+1)*res_line;
    double sx = x, sy = y + res_line;

    space->next_x = sx + tot_w;
    space->next_y = sy + tot_h + 20.0;

    // competitor box
    fprintf(fout, "<path d='"
            "M %.2f %.2f"
            "l %.2f,0 l 0,%.2f l -%.2f,0 l 0,-%.2f"
            "' style='fill:none;stroke:black;stroke-width:2' />\n",
            sx, sy,
            tot_w, tot_h, tot_w, tot_h);
    // vertical lines
    fprintf(fout, "<path d='"
            "M %.2f %.2f l 0,%.2f"
            "' style='fill:none;stroke:black;stroke-width:1' />\n",
            sx + rr_num_w, sy + res_line, rr->num_competitors*res_line);
    fprintf(fout, "<path d='"
            "M %.2f %.2f l 0,%.2f"
            "' style='fill:none;stroke:black;stroke-width:1' />\n",
            sx + rr_num_w + rr_name_w, sy + res_line, rr->num_competitors*res_line);
    for (i = 0; i < rr->num_competitors; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l 0,%.2f"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx + rr_num_w + rr_name_w + rr_club_w + i*rr_num_w,
                sy + res_line, rr->num_competitors*res_line);
    // horisontal lines
    for (i = 0; i < rr->num_competitors; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l %.2f,0"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx, sy + (i+1)*res_line, tot_w);
    // header texts
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>%%t0</text>\n",
            sx+tot_w/2, sy-4.0, competitor_style);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>#</text>\n",
            sx+rr_num_w/2, sy+res_line-4.0, competitor_style);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>%%t6</text>\n",
            sx+rr_num_w+rr_name_w/2, sy+res_line-4.0, competitor_style);
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>%%t8</text>\n",
            sx+rr_num_w+rr_name_w+rr_club_w/2, sy+res_line-4.0, competitor_style);
    for (i = 0; i < rr->num_competitors; i++)
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
                "text-anchor:middle;text-align:center'>%d</text>\n",
                sx+rr_num_w+rr_name_w+rr_club_w+i*rr_num_w+rr_num_w/2, sy+res_line-4.0,
                competitor_style, i+1);
    // competitors
    for (i = 0; i < rr->num_competitors; i++) {
        if (rr->competitors[i].type == COMP_TYPE_COMPETITOR) {
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                    "text-anchor:middle;text-align:center'>%d</text>\n",
                    sx + rr_num_w/2.0, sy + (i + 2)*res_line - 4.0,
                    competitor_style, rr->competitors[i].num);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%c%dfirst-s-last</text>\n",
                    sx + rr_num_w + 6.0, sy + (i + 2)*res_line - 4.0,
                    competitor_style, rr->competitors[i].num);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%c%dclub</text>\n",
                    sx + rr_num_w + rr_name_w + 6.0, sy + (i + 2)*res_line - 4.0,
                    competitor_style, rr->competitors[i].num);
        } else {
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                    "text-anchor:middle;text-align:center'>%d</text>\n",
                    sx + rr_num_w/2.0, sy + (i + 2)*res_line - 4.0,
                    competitor_style, i+1);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%c-%s-%dfirst-s-last</text>\n",
                    sx + rr_num_w + 6.0, sy + (i + 2)*res_line - 4.0,
                    competitor_style, rr->name, i+1);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%c-%s-%dclub</text>\n",
                    sx + rr_num_w + rr_name_w + 6.0, sy + (i + 2)*res_line - 4.0,
                    competitor_style, rr->name, i+1);
        }
        // points
        int j;
        double x1 = sx + rr_num_w + rr_name_w + rr_club_w;

        for (j = 0; j < rr->num_competitors; j++) {
            if (i == j) continue;
            int mnum = find_comps_match(cd, rr->num_rr_matches, rr->rr_matches,
                                        &rr->competitors[i], &rr->competitors[j]);
            if (mnum) {
                fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                        "%%m%dp%d</text>\n",
                        x1 + j*rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0,
                        competitor_style, mnum > 0 ? mnum : -mnum, mnum > 0 ? 1 : 2);
            }

        }
    }

    // points box
    sx += tot_w;
    tot_w = 3*rr_num_w;
    space->next_x = sx + tot_w;

    fprintf(fout, "<path d='"
            "M %.2f %.2f"
            "l %.2f,0 l 0,%.2f l -%.2f,0 l 0,-%.2f"
            "' style='fill:none;stroke:black;stroke-width:2' />\n",
            sx, sy,
            tot_w, tot_h, tot_w, tot_h);
    // vertical lines
    double win_cols[] = {sx, rr_num_w, rr_num_w, rr_num_w, 0};
    draw_verical_lines(win_cols, sy + res_line, rr->num_competitors*res_line);
    // horisontal lines
    for (i = 0; i < rr->num_competitors; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l %.2f,0"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx, sy + (i+1)*res_line, tot_w);
    // header texts
    char *win_hdr_texts[] = {NULL, "Wins", "Pts", "%t22", NULL};
    draw_header_texts(win_cols, sy+res_line-4.0, win_hdr_texts);
    // points
    for (i = 0; i < rr->num_competitors; i++) {
        if (rr->competitors[i].type == COMP_TYPE_COMPETITOR) {
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c%dw</text>\n",
                    sx + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, rr->competitors[i].num);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c%dp</text>\n",
                    sx + rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, rr->competitors[i].num);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c%dr</text>\n",
                    sx + 2*rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, rr->competitors[i].num);
        } else {
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c-%s-%dw</text>\n",
                    sx + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, rr->name, i+1);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c-%s-%dp</text>\n",
                    sx + rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, rr->name, i+1);
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>"
                    "%%c-%s-%dr</text>\n",
                    sx + 2*rr_num_w + rr_num_w/2, sy + (i + 2)*res_line - 4.0, competitor_style, rr->name, i+1);
        }
    }

    // matches box
    sx = x;
    sy = sy + tot_h + 20.0;
    tot_w = rr_num_w*3 + rr_name_w*2 + rr_res_w*2;
    tot_h = (rr->num_rr_matches + 1)*res_line;
    space->next_x = MAX(space->next_x, sx + tot_w);
    space->next_y = sy + tot_h + 20.0;

    fprintf(fout, "<path d='"
            "M %.2f %.2f"
            "l %.2f,0 l 0,%.2f l -%.2f,0 l 0,-%.2f"
            "' style='fill:none;stroke:black;stroke-width:2' />\n",
            sx, sy,
            tot_w, tot_h, tot_w, tot_h);
    // vertical lines
    double match_cols[] = {sx, rr_num_w, rr_name_w, rr_num_w, rr_num_w, rr_name_w, rr_res_w, rr_res_w, 0};
    draw_verical_lines(match_cols, sy + res_line, rr->num_rr_matches*res_line);
    // horizontal lines
    for (i = 0; i < rr->num_rr_matches; i++)
        fprintf(fout, "<path d='"
                "M %.2f %.2f l %.2f,0"
                "' style='fill:none;stroke:black;stroke-width:1' />\n",
                sx, sy + (i+1)*res_line, tot_w);
    // header texts
    char *match_hdr_texts[] = {NULL, "%t14", "%t16", "", "", "%t15", "%t17", "%t19", NULL};
    draw_header_texts(match_cols, sy+res_line-4.0, match_hdr_texts);
    // matches
    for (i = 0; i < rr->num_rr_matches; i++) {
        int match_num = rr->rr_matches[i];
        competitor_bare_t *mc1 = &cd->matches[match_num-1].c1;
        competitor_bare_t *mc2 = &cd->matches[match_num-1].c2;

        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>%d</text>\n",
                sx+rr_num_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-1-first-s-last</text>\n",
                sx+rr_num_w+6.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%m%d-2-first-s-last</text>\n",
                sx+rr_num_w*3+rr_name_w+6.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
        // find competitor numbers
        int j, comp1 = -1, comp2 = -1;
        for (j = 0; j < rr->num_competitors; j++) {
            competitor_bare_t *c = &rr->competitors[j];
            if (same_comps(c, mc1)) comp1 = j;
            if (same_comps(c, mc2)) comp2 = j;
        }
        if (comp1 >= 0)
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                    "text-anchor:middle;text-align:center'>%d</text>\n",
                    sx+rr_num_w+rr_name_w+rr_num_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                    (rr->competitors[comp1].type == COMP_TYPE_COMPETITOR) ?
                    rr->competitors[comp1].num : i+1);
        if (comp2 >= 0)
            fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                    "text-anchor:middle;text-align:center'>%d</text>\n",
                    sx+2*rr_num_w+rr_name_w+rr_num_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                    (rr->competitors[comp2].type == COMP_TYPE_COMPETITOR) ?
                    rr->competitors[comp2].num : i+1);

        // match result
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>%%m%dp1 - %%m%dp2</text>\n",
                sx+3*rr_num_w+2*rr_name_w+rr_res_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num, match_num);
        // time
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;"
                "text-anchor:middle;text-align:center'>%%m%dtime</text>\n",
                sx+3*rr_num_w+2*rr_name_w+rr_res_w+rr_res_w/2.0, sy+(i+2)*res_line-4.0, competitor_style,
                match_num);
    }

    return 0;
}

int make_svg_file(int argc, char *argv[])
{
    int i;
    char template[128];
    allocated_space_t space;
    int doc_width_off, doc_height_off;

    if (argc < 2) return 1;

    memset(&cd, 0, sizeof(cd));
    memset(&match_flags, 0, sizeof(match_flags));
    x_shift = 100.0;
    y_shift = 80.0;
    start_x = 20.0;
    start_y = 80.0;
    max_x = max_y = 0;

    char *p = strstr(svg_start, "8888px");
    if (!p) return 1;
    doc_width_off = (int)(p - svg_start);

    p = strstr(svg_start, "9999px");
    if (!p) return 1;
    doc_height_off = (int)(p - svg_start);

    //printf("doc offsets %d,%d\n", doc_width_off, doc_height_off);

    strcpy(template, argv[1]);
    p = strrchr(template, '.');
    if (p) *p = 0;
    strcat(template, ".svg");

    fout = fopen(template, "w");
    if (!fout) {
        perror(template);
        return 1;
    }

    char *msg = read_custom_category(argv[1], &cd);
    if (msg) {
        fprintf(stderr, "%s\n", msg);
        return 1;
    }

    fprintf(fout, "%s", svg_start);
    fprintf(fout, "%s", svg_page_header_text);

    // best of three
    for (i = 0; i < cd.num_best_of_three_pairs; i++) {
        draw_bestof3(&cd, &cd.best_of_three_pairs[i], start_x, start_y, &space);
        max_x = MAX(max_x, space.next_x);
        start_y = space.next_y;
    }

    // round robin
    for (i = 0; i < cd.num_round_robin_pools; i++) {
        draw_round_robin(&cd, &cd.round_robin_pools[i], start_x, start_y, &space);
        max_x = MAX(max_x, space.next_x);
        start_y = space.next_y;
    }

    // matches
    for (i = 0; i < cd.num_positions; i++) {
        competitor_bare_t c;
        c.type = cd.positions[i].type;
        c.num = cd.positions[i].match;
        c.pos = cd.positions[i].pos;
        draw_comp(&cd, &c, start_x, start_y, &space);
        start_y = space.next_y;
        max_x = MAX(max_x, space.next_x);
    }

    // results
    double res_x = 20.0, res_w = 200.0, res_line = 16.0, y = start_y, res_col1_w = 30.0;

    y += res_line;
    fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;font-weight:bold;"
            "text-anchor:middle;text-align:center'>%%t18</text>\n",
            res_x + res_w*0.5, y - 4.0, competitor_style);
    y += res_line;
    fprintf(fout, "<text x='%.2f' y='%.2f' "
            "style='%s;font-weight:bold;text-anchor:middle;text-align:center'>%%t22</text>\n",
            res_x + 12.0, y - 4.0, competitor_style);
    fprintf(fout, "<text x='%.2f' y='%.2f' "
            "style='%s;font-weight:bold;text-anchor:middle;text-align:center'>%%t6</text>\n",
            res_x + (res_w - res_col1_w)/2.0, y - 4.0, competitor_style);
    y += res_line;

    for (i = 0; i < cd.num_positions; i++) {
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s'>%%r%dhm2-first-s-last-s-club</text>\n",
                res_x + 30.0, y - 4.0, competitor_style, i+1);
        fprintf(fout, "<text x='%.2f' y='%.2f' style='%s;text-anchor:middle;text-align:center'>%d</text>\n",
                res_x + 12.0, y - 4.0, competitor_style,
                cd.positions[i].real_contest_pos ? cd.positions[i].real_contest_pos : i+1);

        y += res_line;
    }

    start_y += res_line;

    fprintf(fout, "<path "
            "d='M %.2f,%.2f "
            "l %.2f,0 l 0,%.2f l -%.2f,0 l 0,-%.2f' "
            "style='fill:none;stroke:black;stroke-width:2' />\n",
            res_x, start_y,
            res_w, (cd.num_positions + 1)*res_line,
            res_w, (cd.num_positions + 1)*res_line);

    fprintf(fout, "<path "
            "d='M %.2f,%.2f l 0,%.2f' "
            "style='fill:none;stroke:black;stroke-width:1' />\n",
            res_x + res_col1_w, start_y + res_line, cd.num_positions*res_line);

    start_y += res_line;

    for (i = 0; i < cd.num_positions; i++) {
        fprintf(fout, "<path "
                "d='M %.2f,%.2f "
                "l %.2f,0' "
                "style='fill:none;stroke:black;stroke-width:1' />\n",
                res_x, start_y + i*res_line,
                res_w);
    }

    max_y = start_y + cd.num_positions*res_line;

    fprintf(fout, "%s", svg_end);

    fseek(fout, doc_width_off, SEEK_SET);
    fprintf(fout, "%04.0f", (max_x + 20.0));
    fseek(fout, doc_height_off, SEEK_SET);
    fprintf(fout, "%04.0f", (max_y + 20.0));

    fclose(fout);
    return 0;
}

#ifndef APPLICATION

int main(int argc, char *argv[])
{
    return make_svg_file(argc, argv);
}

#endif
