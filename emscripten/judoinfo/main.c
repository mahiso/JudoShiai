/* -*- mode: C; c-basic-offset: 4;  -*- */

/*
 * Copyright (C) 2006-2016 by Hannu Jokinen
 * Full copyright text is included in the software package.
 */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "judoinfo.h"

void textbox(int x1, int y1, int w1, int h1, const char *txt);
void checkbox(int x1, int y1, int size, int align, int yes);
void menuicononload(const char *str);
void menupiconload(const char *str);
void menupiconerror(const char *str);
static void show_menu(void);
static SDL_Surface *menubg, *menuicon;
static int menu_on = FALSE;
static int icontimer = 0;

extern void handle_info_msg(struct msg_match_info *input_msg);

struct stack_item stack[8];
int sp = 0;

static gboolean button_pressed(int x, int y);
static gboolean expose(void);

gchar         *program_path;
gchar          current_directory[1024] = {0};
gint           my_address;
gchar         *installation_dir = NULL;
static TTF_Font *font, *font_bold;
gchar         *conffile;
guint          current_year;
static SDL_Surface *darea = NULL;
//gint           language = LANG_EN;
gint           num_lines = NUM_LINES;
gint           display_type = NORMAL_DISPLAY;
gboolean       mirror_display = FALSE;
gboolean       white_first = TRUE;
gboolean       red_background = FALSE;
gboolean       menu_hidden = FALSE;
gchar         *filename = NULL;
time_t         next_update;
gboolean       show_tatami[NUM_TATAMIS];
static int     conf_tatamis[NUM_TATAMIS];
int            configured_tatamis = 0;

#define MY_FONT "Arial"

#define THIN_LINE     (1)
#define THICK_LINE    (2*THIN_LINE)

#define W(_w) ((_w)*paper_width)
#define H(_h) ((_h)*paper_height)

#define BOX_HEIGHT (horiz ? paper_height/(8*num_lines+2) : paper_height/(4*num_lines+1))
//#define BOX_HEIGHT (1.4*extents.height)

#define NUM_RECTANGLES 1000

static struct {
    int x1, y1, x2, y2;
    gint   tatami;
    gint   group;
    gint   category;
    gint   number;
} point_click_areas[NUM_RECTANGLES];

static gint num_rectangles;

static gboolean button_drag = FALSE;
static gchar    dragged_text[32];
static gdouble  dragged_x, dragged_y;

struct match match_list[NUM_TATAMIS][NUM_LINES];

static struct {
    gint cat;
    gint num;
} last_wins[NUM_TATAMIS];

extern void print_stat(void);

gint number_of_tatamis(void)
{
    gint i, n = 0;
    for (i = 0; i < NUM_TATAMIS; i++)
        if (show_tatami[i])
            n++;

    return n;
}

static void refresh_graph(void)
{
    //printf("graph update timeout\n");
    refresh_window();
}

static void paint(SDL_Surface *c, int paper_width, int paper_height, gpointer userdata)
{
    gint i;
    gint num_tatamis = number_of_tatamis();
    gint num_columns = num_tatamis;
    int y_pos = 0, colwidth = paper_width/num_tatamis;
    int left = 0;
    int right;
    time_t now = time(NULL);
    //gboolean update_later = FALSE;
    gboolean upper = TRUE, horiz = (display_type == HORIZONTAL_DISPLAY);
    static int last_paper_width, last_display_type;
    struct { int width; int height; } extents;

    if (paper_width != last_paper_width ||
	display_type != last_display_type) {
	last_paper_width = paper_width;
	last_display_type = display_type;
	//if (font)
	//    TTF_CloseFont(font);
	//if (font_bold)
	//    TTF_CloseFont(font_bold);
	font = get_font(BOX_HEIGHT*8/10, 0);
	font_bold = get_font(BOX_HEIGHT*8/10, 1);
	//font = TTF_OpenFont("free-sans", BOX_HEIGHT*8/10);
	//font_bold = TTF_OpenFont("free-sans-bold", BOX_HEIGHT*8/10);
    }

    extents.height = 0;
    extents.width = 100;

    if (horiz) {
        num_columns = num_tatamis/2;
        if (num_tatamis & 1)
            num_columns++;
        colwidth = paper_width/num_columns;
    }

    if (mirror_display && num_tatamis)
            left = (num_columns - 1)*colwidth;

    num_rectangles = 0;

    cairo_set_source_rgb(c, 255, 255, 255);
    cairo_rectangle(c, 0, 0, paper_width, paper_height);
    cairo_fill(c);

    cairo_set_line_width(c, THIN_LINE);
    cairo_set_source_rgb(c, 0, 0, 0);

    y_pos = BOX_HEIGHT;

    for (i = 0; i < NUM_TATAMIS; i++) {
        gchar buf[30];
        gint k;

        if (!show_tatami[i])
            continue;

        right = left + colwidth;

        if (horiz) {
            if (upper)
                y_pos = BOX_HEIGHT;
            else
                y_pos = paper_height/2 + BOX_HEIGHT;
        } else
            y_pos = BOX_HEIGHT;

        for (k = 0; k < num_lines; k++) {
            struct match *m = &match_list[i][k];
            gchar buf[20];
            int e = (k == 0) ? colwidth/2 : 0;

            if (m->number >= 1000)
                break;

            struct name_data *catdata = avl_get_data(m->category);

            cairo_save(c);
            if (k == 0) {
                if (last_wins[i].cat == m->category &&
                    last_wins[i].num == m->number)
		    cairo_set_source_rgb(c, 7*255/10, 255, 7*255/10);
                else
		    cairo_set_source_rgb(c, 0xff, 0xff, 0);
            } else
		cairo_set_source_rgb(c, 0xff, 0xff, 0xff);

            cairo_rectangle(c, left, y_pos+1, colwidth, 4*BOX_HEIGHT);
            cairo_fill(c);
            cairo_restore(c);

            cairo_save(c);
            if (k == 0) {
                cairo_move_to(c, left+5, y_pos+extents.height);
                cairo_show_text(c, _("Prev. winner:"));
            } else if (m->rest_end > now && k == 1) {
                gint t = m->rest_end - now;
                sprintf(buf, "** %d:%02d **", t/60, t%60);
                cairo_save(c);
                cairo_set_source_rgb(c, 8*255/10, 0, 0);
                cairo_move_to(c, left+5+colwidth/2, y_pos+extents.height);
                cairo_show_text(c, buf);
                cairo_restore(c);
                //update_later = TRUE;
            } else if (m->number == 1) {
                cairo_save(c);
                cairo_set_source_rgb(c, 255, 0, 0);
                cairo_move_to(c, left+5+colwidth/2, y_pos+extents.height);
                cairo_show_text(c, _("Category starts"));
                cairo_restore(c);
            }

            if (m->flags) {
                cairo_save(c);
                cairo_set_source_rgb(c, 0, 0, 255);
                cairo_move_to(c, left+5+colwidth/2, y_pos+extents.height);
                if (m->flags & MATCH_FLAG_GOLD)
                    cairo_show_text(c, _("Gold medal match"));
                else if (m->flags & MATCH_FLAG_BRONZE_A)
                    cairo_show_text(c, _("Bronze match A"));
                else if (m->flags & MATCH_FLAG_BRONZE_B)
                    cairo_show_text(c, _("Bronze match B"));
                else if (m->flags & MATCH_FLAG_SEMIFINAL_A)
                    cairo_show_text(c, _("Semifinal A"));
                else if (m->flags & MATCH_FLAG_SEMIFINAL_B)
                    cairo_show_text(c, _("Semifinal B"));
                else if (m->flags & MATCH_FLAG_SILVER)
                    cairo_show_text(c, _("Silver medal match"));
                cairo_restore(c);
            }
            cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_BOLD);
            if (k == 0)
                cairo_move_to(c, left+5, y_pos+extents.height+BOX_HEIGHT);
            else
                cairo_move_to(c, left+5, y_pos+extents.height);

            snprintf(buf, sizeof(buf), "%s #%d", catdata ? catdata->last : "?", m->number);
            cairo_show_text(c, buf);
            //cairo_show_text(c, catdata ? catdata->last : "?");
            cairo_restore(c);

            struct name_data *j = avl_get_data(m->blue);
            if (j) {
                cairo_save(c);

                if (!white_first) {
                    if (red_background)
                        cairo_set_source_rgb(c, 255, 0, 0);
                    else
                        cairo_set_source_rgb(c, 0, 0, 255);
                }
                else
                    cairo_set_source_rgb(c, 255, 255, 255);

                if (k)
                    cairo_rectangle(c, left, y_pos+BOX_HEIGHT,
                                    colwidth/2,
                                    3*BOX_HEIGHT);
                cairo_fill(c);

                if (k && !white_first)
                    cairo_set_source_rgb(c, 255, 255, 255);
                else
                    cairo_set_source_rgb(c, 0, 0, 0);

                cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_BOLD);
                cairo_move_to(c, left+5+e, y_pos+2*BOX_HEIGHT+extents.height);
                cairo_show_text(c, j->last);
                cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_NORMAL);

                cairo_move_to(c, left+5+e, y_pos+BOX_HEIGHT+extents.height);
                cairo_show_text(c, j->first);

                cairo_move_to(c, left+5+e, y_pos+3*BOX_HEIGHT+extents.height);
                cairo_show_text(c, j->club);

                cairo_restore(c);
            }
            j = avl_get_data(m->white);
            if (j && k > 0) {
                cairo_save(c);

                if (k == 0) {
                    if (last_wins[i].cat == m->category &&
                        last_wins[i].num == m->number)
                        cairo_set_source_rgb(c, 7*255/10, 255, 7*255/10);
                    else
                        cairo_set_source_rgb(c, 255, 255, 0);
                }
#if 0
                else if (k == 1 && m->flags & MATCH_FLAG_WHITE_DELAYED) {
                    if (m->flags & MATCH_FLAG_WHITE_REST)
                        cairo_set_source_rgb(c, 127, 127, 255);
                    else
                        cairo_set_source_rgb(c, 255, 127, 127);
                }
#else
                else if (white_first) {
                    if (red_background)
                        cairo_set_source_rgb(c, 255, 0, 0);
                    else
                        cairo_set_source_rgb(c, 0, 0, 255);
                }
#endif
                else
                    cairo_set_source_rgb(c, 255, 255, 255);

                cairo_rectangle(c, left+colwidth/2, y_pos+BOX_HEIGHT, colwidth/2, 3*BOX_HEIGHT);
                cairo_fill(c);

                if (k && white_first)
                    cairo_set_source_rgb(c, 255, 255, 255);
                else
                    cairo_set_source_rgb(c, 0, 0, 0);

                cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_BOLD);
                cairo_move_to(c, left+5+colwidth/2, y_pos+2*BOX_HEIGHT+extents.height);
                cairo_show_text(c, j->last);

                cairo_select_font_face(c, MY_FONT, 0, CAIRO_FONT_WEIGHT_NORMAL);
                cairo_move_to(c, left+5+colwidth/2, y_pos+BOX_HEIGHT+extents.height);
                cairo_show_text(c, j->first);

                cairo_move_to(c, left+5+colwidth/2, y_pos+3*BOX_HEIGHT+extents.height);
                cairo_show_text(c, j->club);

                cairo_restore(c);
            }

            point_click_areas[num_rectangles].category = m->category;
            point_click_areas[num_rectangles].number = m->number;
            point_click_areas[num_rectangles].group = 0;
            point_click_areas[num_rectangles].tatami = i+1;
            point_click_areas[num_rectangles].x1 = left;
            point_click_areas[num_rectangles].y1 = y_pos;
            point_click_areas[num_rectangles].x2 = right;
            point_click_areas[num_rectangles].y2 = y_pos + 4*BOX_HEIGHT;

            y_pos += 4*BOX_HEIGHT;

            if (num_rectangles < NUM_RECTANGLES-1)
                num_rectangles++;

            cairo_move_to(c, left, y_pos);
            cairo_line_to(c, right, y_pos);
            cairo_stroke(c);
        } // for (k = 0; k < num_lines; k++)

        if (horiz) {
            if (upper)
                cairo_move_to(c, 10 + left, extents.height);
            else
                cairo_move_to(c, 10 + left, paper_height/2 + extents.height);
        } else
            cairo_move_to(c, 10 + left, extents.height);

        sprintf(buf, "%s %d", _("Tatami"), i+1);
        cairo_show_text(c, buf);
#if 0
        point_click_areas[num_rectangles].category = 0;
        point_click_areas[num_rectangles].number = 0;
        point_click_areas[num_rectangles].group = 0;
        point_click_areas[num_rectangles].tatami = i+1;
        point_click_areas[num_rectangles].x1 = left;
        point_click_areas[num_rectangles].y1 = y_pos;
        point_click_areas[num_rectangles].x2 = right;
        point_click_areas[num_rectangles].y2 = paper_height;
        if (num_rectangles < NUM_RECTANGLES-1)
            num_rectangles++;
#endif
        cairo_save(c);
        cairo_set_line_width(c, THICK_LINE);
        cairo_set_source_rgb(c, 0, 0, 0);
        cairo_move_to(c, left, 0);
        cairo_line_to(c, left, paper_height);
        cairo_stroke(c);
        cairo_restore(c);

        if (horiz) {
            if (!upper) {
                if (mirror_display)
                    left -= colwidth;
                else
                    left += colwidth;
            }
        } else if (mirror_display)
            left -= colwidth;
        else
            left += colwidth;

        upper = !upper;
    } // tatamis


    cairo_save(c);
    cairo_set_line_width(c, THICK_LINE);

    cairo_set_source_rgb(c, 0, 0, 0);
    cairo_move_to(c, 0, BOX_HEIGHT);
    cairo_line_to(c, paper_width, BOX_HEIGHT);
    if (horiz) {
        cairo_move_to(c, 0, paper_height/2);
        cairo_line_to(c, paper_width, paper_height/2);
        cairo_move_to(c, 0, paper_height/2+BOX_HEIGHT);
        cairo_line_to(c, paper_width, paper_height/2+BOX_HEIGHT);
    }
    cairo_stroke(c);

    if (!horiz) {
        cairo_set_source_rgb(c, 0, 0, 255);
        cairo_move_to(c, 0, 5*BOX_HEIGHT);
        cairo_line_to(c, paper_width, 5*BOX_HEIGHT);
        cairo_move_to(c, 0, 13*BOX_HEIGHT);
        cairo_line_to(c, paper_width, 13*BOX_HEIGHT);
        cairo_stroke(c);
    }

    cairo_restore(c);
#if 0
    if (button_drag) {
        cairo_set_line_width(c, THIN_LINE);
        cairo_text_extents(c, dragged_text, &extents);
        cairo_set_source_rgb(c, 255, 255, 255);
        cairo_rectangle(c, dragged_x - extents.width/2.0, dragged_y - extents.height,
                        extents.width + 4, extents.height + 4);
        cairo_fill(c);
        cairo_set_source_rgb(c, 0, 0, 0);
        cairo_rectangle(c, dragged_x - extents.width/2.0 - 1, dragged_y - extents.height - 1,
                        extents.width + 4, extents.height + 4);
        cairo_stroke(c);
        cairo_move_to(c, dragged_x - extents.width/2.0, dragged_y);
        cairo_show_text(c, dragged_text);
    }
#endif
    next_update = time(NULL) + 3;
}

#define X_L(_x) (x > _x - 50 && x < _x)
#define X_R(_x) (x > _x && x < _x + 50)
#define Y_REG(_y) (y <= _y && y > _y - 70)
// left side buttons
#define X0 48
#define X0_1 302
#define BETWEEN(_x1, _x2) (x >= _x1 && x <= _x2)

#define L1 482
#define R1 708
#define LINE1 191
#define LINE2 280
#define LINE3 375
#define LINE4 470
#define LINE5 566
#define LINE7 755
#define TEXTBOX_W (R1 - L1)

#define X_X1 45
#define X_X2 141
#define X_Y1 840
#define X_Y2 961

const int tatami_x[] = {
    119, 209, 299, 387, 477, 567, 655, 745, 834, 925
};

const int numlines[3] = {NUM_LINES, 6, 3};
const char *dspnames[3] = {"Normal", "Small", "Horizontal"};

static int handle_menu(int x1, int y1)
{
    int x, y, a, i, j;

    /* scaled coordinates 0 - 999 */
    x = x1*1000/menubg->w;
    y = y1*1000/menubg->h;
    if (Y_REG(LINE2)) {
	for (i = 0; i < NUM_TATAMIS; i++) {
	    if (X_L(tatami_x[i])) {
		conf_tatamis[i] = !conf_tatamis[i];
		configured_tatamis = 0;

		for (j = 0; j < NUM_TATAMIS; j++)
		    if (conf_tatamis[j])
			configured_tatamis++;

		for (j = 0; j < NUM_TATAMIS; j++) {
		    if (configured_tatamis) show_tatami[j] = conf_tatamis[j];
		    else show_tatami[j] = match_list[j][1].blue && match_list[j][1].white;
		}

		return TRUE;
	    }
	}
    } else if (Y_REG(LINE3)) {
	if (X_R(R1) && display_type < 2) {
	    display_type++;
	    num_lines = numlines[display_type];
	    return TRUE;
	} else if (X_L(L1) && display_type > 0) {
	    display_type--;
	    num_lines = numlines[display_type];
	    return TRUE;
	}
    } else if (Y_REG(LINE4)) {
	if (X_R(R1) || X_L(L1))
	    red_background = !red_background;
	return TRUE;
    } else if (Y_REG(LINE5)) {
	if (X_R(L1))
	    mirror_display = !mirror_display;
	return TRUE;
    } else if (Y_REG(LINE7)) {
	if (BETWEEN(X0, X0_1)) {
	    emscripten_run_script("setfullscreen()");
	    menu_on = FALSE;
	    return TRUE;
	}
    } else if ( x > 38 && x < 119 && y > 840 && y < 960) {
	menu_on = FALSE;
	return TRUE;
    }

    return FALSE;
}

static void show_menu(void)
{
    char buf[16];
    SDL_Rect dest;
    int h = LINE2-LINE1;
    int f = 9*h/10;
    int i;

    dest.x = dest.y = dest.w = dest.h = 0;
    SDL_BlitSurface(menubg, NULL, darea, &dest);

    for (i = 0; i < 10; i++)
	checkbox(tatami_x[i], LINE2, f, -1, conf_tatamis[i]);

    textbox(L1, LINE3, TEXTBOX_W, f, dspnames[display_type]);

    textbox(L1, LINE4, TEXTBOX_W, f, red_background ? "Red" : "Blue");

    checkbox(L1, LINE5, f, 1, mirror_display);

}

static void mouse_move(void)
{
    int x, y;
    SDL_GetMouseState(&x, &y);

    if (menuicon && y < menuicon->h) {
	icontimer = 50;
    }
    if (menu_on &&
	(x > menubg->w || y > menubg->h))
	menu_on = FALSE;
}

/* This is called when we need to draw the windows contents */
static gboolean expose(void)
{
    int width, height, isFullscreen;

    emscripten_get_canvas_size(&width, &height, &isFullscreen);

    //if (paint_svg(&pd) == FALSE)
    paint(darea, width, height, NULL);

    return FALSE;
}

void onload(void *arg, void *buf, int len)
{
    char *b = buf;
    struct msg_match_info msg;
    int n;

    while (b) {
	n = sscanf(b, "%d,%d,%d,%d,%d,%d,%d,%d",
		   &msg.tatami,
		   &msg.position,
		   &msg.category,
		   &msg.number,
		   &msg.blue,
		   &msg.white,
		   &msg.flags,
		   &msg.rest_time);

	if (n == 8)
	    handle_info_msg(&msg);
	else
	    break;

	b = strchr(b, '\n');
	if (b) b++;
    }

    //refresh_window();
}

void onerror(void *a)
{
    printf("onerror %p\n", a);
}

void menuicononload(const char *str)
{
    menuicon = IMG_Load(str);
    printf("file=%s menuicon=%p\n", str, menuicon);
}

void menupiconload(const char *str)
{
    menubg = IMG_Load(str);
    printf("file=%s menubg=%p\n", str, menubg);
}

void menupiconerror(const char *str)
{
    printf("%s failed\n", str);
}

void onloadabstract(void *arg, void *buf, int len)
{
    char *b = buf;
    int n, t, crc;
    static int last_crc[NUM_TATAMIS];

    while (b) {
	n = sscanf(b, "%d,%d", &t, &crc);
	if (n == 2 && t >= 1 && t <= NUM_TATAMIS) {
	    if (last_crc[t-1] != crc) {
		char url[64];

		last_crc[t-1] = crc;
		snprintf(url, sizeof(url), "matchinfo?t=%d", t);
		emscripten_async_wget_data(url, NULL, onload, onerror);
	    }
	} else
	    break;

	b = strchr(b, '\n');
	if (b) b++;
    }
}

void EMSCRIPTEN_KEEPALIVE main_loop(void)
{
    time_t now = time(NULL);
    static time_t graph_update;
    char url[64];
    static int tatami = 1;
    static time_t forced_update;
#if 1
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
	switch(event.type) {
	case SDL_MOUSEBUTTONDOWN: {
	    SDL_MouseButtonEvent *m = (SDL_MouseButtonEvent*)&event;
	    //printf("button down: %d,%d  %d,%d\n", m->button, m->state, m->x, m->y);
	    button_pressed(m->x, m->y);
	    break;
	}
	}
    }

    if (now > next_update ||
	now > forced_update + 3 ||
	icontimer == 1) {
	refresh_window();
	forced_update = now;
    }

    timeout_ask_for_data(NULL);

    if (now > graph_update+2) {
	graph_update = now;
	emscripten_async_wget_data("matchinfo?t=0", NULL, onloadabstract, onerror);
    }

    mouse_move();

    if (icontimer > 0) {
	icontimer--;
    }

    SDL_Rect dest;
    dest.x = dest.y = dest.w = dest.h = 0;
    //dest.y = icontimer - 50;
    //if (dest.y > 0) dest.y = 0;
    if (menuicon && icontimer > 2) {
	SDL_BlitSurface(menuicon, NULL, darea, &dest);
    }

    if (menu_on && menubg) {
	icontimer = 50;
	show_menu();
    }
#endif
}

void EMSCRIPTEN_KEEPALIVE main_2(void *arg)
{
#if 0
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    darea = SDL_SetVideoMode(800, 600, 32, SDL_HWSURFACE);

    printf("Init: %d\n", TTF_Init());
#endif
    time_t     now;
    struct tm *tm;
    SDL_Thread*gth = NULL;         /* thread id */
    gboolean   run_flag = TRUE;   /* used as exit flag for threads */
    gint       i;

    printf("main2\n");
    init_trees();
    init_fonts();

    font = get_font(12, 0);
    font_bold = get_font(12, 1);

    now = time(NULL);
    tm = localtime(&now);
    current_year = tm->tm_year+1900;
    srand(now); //srandom(now);
    my_address = now + getpid()*10000;

    emscripten_async_wget("/menuicon.png", "menuicon.png", menuicononload, menupiconerror);
    emscripten_async_wget("/menupicinfo.png", "menupicinfo.png", menupiconload, menupiconerror);

    emscripten_set_main_loop(main_loop, 10, 0);
}

int main()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    darea = SDL_SetVideoMode(800, 600, 32, SDL_HWSURFACE);
    TTF_Init();
    printf("main1\n");

    emscripten_async_call(main_2, NULL, 500); // avoid startup delays and intermittent errors

    return 0;
}


gboolean this_is_shiai(void)
{
    return FALSE;
}

gint application_type(void)
{
    return APPLICATION_TYPE_INFO;
}

void refresh_window(void)
{
    expose();
}

static gint find_box(int x, int y)
{
    gint t;

    for (t = 0; t < num_rectangles; t++) {
        if (x >= point_click_areas[t].x1 &&
            x <= point_click_areas[t].x2 &&
            y >= point_click_areas[t].y1 &&
            y <= point_click_areas[t].y2) {
            return t;
        }
    }
    return -1;
}

static gboolean button_pressed(int x, int y)
{
    if (menu_on && menubg &&
	x < menubg->w &&
	y < menubg->h) {
	if (handle_menu(x, y))
	    refresh_window();
	return TRUE;
    }

    if (menuicon &&
	x < menuicon->w &&
	y < menuicon->h) {
	menu_on = TRUE;
	refresh_window();
	return TRUE;
    }

    int t = find_box(x, y);
    if (t < 0)
	return FALSE;

    int tatami = point_click_areas[t].tatami;
    last_wins[tatami-1].cat = match_list[tatami-1][0].category;
    last_wins[tatami-1].num = match_list[tatami-1][0].number;

    refresh_window();
    return TRUE;
}

/*** profiling stuff ***/
struct profiling_data prof_data[NUM_PROF];
gint num_prof_data;
guint64 prof_start;
gboolean prof_started = FALSE;

void textbox(int x1, int y1, int w1, int h1, const char *txt)
{
    SDL_Rect rect;
    SDL_Color color;
    SDL_Rect dest;
    SDL_Surface *text;
    int x, y, w, h;
    int fontsize;

    x = x1*menubg->w/1000;
    y = (y1 - h1)*menubg->h/1000;
    w = w1*menubg->w/1000;
    h = h1*menubg->h/1000;
    fontsize = 8*h/10;

    color.r = 0; color.g = 0; color.b = 0;
    text = TTF_RenderText_Solid(get_font(fontsize, 0), txt, color);
    rect.x = x; rect.y = y; rect.w = w; rect.h = h;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 255, 255, 255));
    dest.x = x + (w - text->w)/2;
    dest.y = y + (h - text->h)/2;
    dest.w = dest.h = 0;
    SDL_BlitSurface(text, NULL, darea, &dest);
    SDL_FreeSurface(text);
}

void checkbox(int x1, int y1, int h1, int align, int yes)
{
    SDL_Rect rect;
    SDL_Color color;
    SDL_Rect dest;
    int x, y, h;

    x = x1*menubg->w/1000;
    y = y1*menubg->h/1000;
    h = h1*menubg->h/1000;

    if (align == -1)
	x -= h;
    y -= h;

    rect.x = x; rect.y = y; rect.w = h; rect.h = h;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 0, 0, 0));
    rect.x++; rect.y++; rect.w -= 2; rect.h -= 2;
    SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 255, 255, 255));

    if (yes) {
	rect.x += 3; rect.y += 3; rect.w -= 6; rect.h -= 6;
	SDL_FillRect(darea, &rect, SDL_MapRGB(darea->format, 0, 0, 0));
    }
}

int EMSCRIPTEN_KEEPALIVE setscreensize(int yes)
{
    int w, h, f;

    emscripten_get_canvas_size(&w, &h, &f);
    if (1 || f == 0) {
	w = emscripten_run_script_int("window.innerWidth");
	h = emscripten_run_script_int("window.innerHeight");
	emscripten_set_canvas_size(w, h);
    }

    expose();
    return yes;
}
