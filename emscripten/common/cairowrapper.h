#ifndef _CAIRO_WRAPPER_H_
#define _CAIRO_WRAPPER_H_

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_image.h>
#include <emscripten.h>

TTF_Font *get_font(int size, int bold);
void init_fonts(void);

/***
      @font-face { font-family: free-sans; src: url('FreeSans.ttf'); }
      h4 {font-family: free-sans }
      @font-face { font-family: free-sans-bold; src: url('FreeSansBold.ttf'); }
      h3 {font-family: free-sans-bold }
***/

#define cairo_t		SDL_Surface
#define cairo_surface_t	SDL_Surface
#define GtkWidget	void
#define GtkWindow	void
#define GTimer		void
#define GKeyFile	void
#define GdkEvent	void
#define GdkEventButton	SDL_MouseButtonEvent
#define GdkEventExpose	void
#define PangoFontDescription void
#define GdkCursor	void
#define GError		char

#define gboolean	int
#define gchar		char
#define guchar		unsigned char
#define gint		int
#define guint		unsigned int
#define gdouble		double
#define gpointer	void *
#define glong		long
#define gulong		unsigned long
#define guint64		uint64_t
#define gsize		size_t
#define gssize		ssize_t

#define FALSE		0
#define TRUE		(!FALSE)

#define G_LOCK_DEFINE_STATIC(_m) SDL_mutex *_m
#define G_LOCK(_m)	do {} while (0)
#define G_UNLOCK(_m)	do {} while (0)
#define GStaticMutex	SDL_mutex
#define g_thread_exit(_x)	do {} while (0)

#define g_print printf
#define g_usleep usleep
#define g_malloc malloc
#define g_free free
#define g_strdup strdup

#define GDK_0 SDLK_0
#define GDK_1 SDLK_1
#define GDK_2 SDLK_2
#define GDK_3 SDLK_3
#define GDK_4 SDLK_4
#define GDK_5 SDLK_5
#define GDK_6 SDLK_6
#define GDK_7 SDLK_7
#define GDK_8 SDLK_8
#define GDK_9 SDLK_9
#define GDK_a SDLK_a
#define GDK_b SDLK_b
#define GDK_c SDLK_c
#define GDK_d SDLK_d
#define GDK_e SDLK_e
#define GDK_f SDLK_f
#define GDK_g SDLK_g
#define GDK_h SDLK_h
#define GDK_i SDLK_i
#define GDK_j SDLK_j
#define GDK_k SDLK_k
#define GDK_l SDLK_l
#define GDK_m SDLK_m
#define GDK_n SDLK_n
#define GDK_o SDLK_o
#define GDK_p SDLK_p
#define GDK_q SDLK_q
#define GDK_r SDLK_r
#define GDK_s SDLK_s
#define GDK_t SDLK_t
#define GDK_u SDLK_u
#define GDK_v SDLK_v
#define GDK_w SDLK_w
#define GDK_x SDLK_x
#define GDK_y SDLK_y
#define GDK_z SDLK_z
#define GDK_F1 SDLK_F1
#define GDK_F2 SDLK_F2
#define GDK_F3 SDLK_F3
#define GDK_F4 SDLK_F4
#define GDK_F5 SDLK_F5
#define GDK_F6 SDLK_F6
#define GDK_F7 SDLK_F7
#define GDK_F8 SDLK_F8
#define GDK_F9 SDLK_F9
#define GDK_F10 SDLK_F10
#define GDK_F11 SDLK_F11
#define GDK_F12 SDLK_F12
#define GDK_space SDLK_SPACE
#define GDK_Return SDLK_RETURN
#define GDK_Down SDLK_DOWN
#define GDK_Up SDLK_UP
#define GDK_KP_Enter SDLK_KP_ENTER

#define GdkColor SDL_Color
typedef struct GdkColor GdkColor;

#define GTK_STATE_NORMAL 0

struct stack_item {
    Uint32 curcol;
    int curx, cury, curlinew;
    int fontweight, fontsize;
    char curr, curg, curb, cura;
};
#define SX stack[sp]

extern struct stack_item stack[8];
extern int sp;

#define CAIRO_FONT_WEIGHT_NORMAL 0
#define CAIRO_FONT_WEIGHT_BOLD   1
#define CAIRO_FONT_SLANT_NORMAL  0
#define CAIRO_FONT_SLANT_ITALIC  1

#define cairo_save(_c)					\
    do {						\
	sp++;						\
	if (sp > 8) printf("STACK SP=%d line=%d\n", sp, __LINE__);	\
	SX = stack[sp-1];				\
    } while (0)

#define cairo_restore(_c)						\
    do {								\
	sp--;								\
	if (sp < 0) printf("STACK SP=%d line=%d\n", sp, __LINE__);	\
    } while (0)

#define cairo_stroke(_c) do {} while (0)

#define cairo_fill(_c) do {} while (0)

#define cairo_select_font_face(_c, _ff, _n, _bold) \
    do { \
	SX.fontweight = _bold; \
    } while (0)

#define cairo_set_font_size(_c, _s) \
    do { \
	SX.fontsize = _s; \
    } while (0)

#define cairo_set_line_width(_c, _w) do { SX.curlinew = _w; } while (0)

#define cairo_rectangle(_c, _x, _y, _w, _h)				\
    do {								\
	SDL_Rect rect; rect.x = _x; rect.y = _y;			\
	rect.w = _w; rect.h = _h;					\
	SDL_FillRect(_c, &rect, SX.curcol);				\
    } while (0)

#define cairo_set_source_rgb(_c, _r, _g, _b)			\
	do {							\
		SX.curr = _r; SX.curg = _g; SX.curb = _b;	\
		SX.curcol = SDL_MapRGB(_c->format, _r, _g, _b);	\
	} while (0)

#define cairo_set_source_rgba(_c, _r, _g, _b, _a)			\
	do {							\
		SX.curr = _r; SX.curg = _g; SX.curb = _b;	\
		SX.cura = _a;					\
		SX.curcol = SDL_MapRGB(_c->format, _r, _g, _b);	\
	} while (0)

#define cairo_move_to(_c, _x, _y) do {SX.curx = _x; SX.cury = _y; } while (0)

#define cairo_line_to(_c, _x, _y) \
    do {								\
	int x, y, w, h;							\
	if (_x > SX.curx) { x = SX.curx; w = _x - SX.curx; }		\
	else { x = _x; w = SX.curx - _x; }				\
	if (_y > SX.cury) { y = SX.cury; h = _y - SX.cury; }		\
	else { y = _y; h = SX.cury - _y; }				\
	if (w < SX.curlinew) w = SX.curlinew;				\
	if (h < SX.curlinew) h = SX.curlinew;				\
	cairo_rectangle(_c, x, y, w, h);				\
	SX.curx = _x; SX.cury = _y;					\
    } while(0)

#ifdef JUDOTIMER
#define FONT get_font(SX.fontsize, SX.fontweight)
#else
#define FONT SX.fontweight ? font_bold : font
#endif

#define cairo_show_text(_c, _txt)					\
    do {								\
	SDL_Color color;						\
	color.r = SX.curr; color.g = SX.curg;				\
	color.b = SX.curb;						\
	SDL_Surface *text = TTF_RenderText_Solid(			\
	    FONT, _txt, color);						\
	SDL_Rect dest; dest.x = SX.curx; dest.y = SX.cury; dest.w = dest.h = 0; \
	SDL_BlitSurface(text, NULL, _c, &dest);				\
	SDL_FreeSurface(text);						\
    } while (0)

typedef struct { int width; int height; int y_bearing; } cairo_text_extents_t;

#define cairo_text_extents(_c, _txt, _e)		\
    do {						\
	(_e)->height = SX.fontsize;			\
	(_e)->width = strlen(_txt)*SX.fontsize;		\
	(_e)->y_bearing = 0;				\
    } while (0)						\



static inline
gchar    *g_key_file_to_data                (GKeyFile             *key_file,
					     gsize                *length,
					     GError              **error)
{ return NULL; }

static inline
gboolean g_file_set_contents (const gchar *filename,
                              const gchar *contents,
                              gssize         length,
                              GError       **error)
{ return 0; }

static inline void g_key_file_free(GKeyFile *key_file){ }
static inline void gtk_main_quit (void) { }

static inline
char *g_strconcat(gchar *a, ...)
{
	char *s, buf[128];
	int n = 0;

	n = sprintf(buf, "%s", a);

	va_list args;
	va_start(args, a);
	do {
		s = va_arg(args, char *);
		if (s)
			n += sprintf(&buf[n], "%s", s);
	} while (s);
	va_end(args);

	return strdup(buf);
}

#define GTK_WINDOW(_w) _w
#define gtk_window_set_title(_a, _b) do {} while (0)
#define gtk_window_set_titlebar(_a, _b) do {} while (0)
#define cairo_create(_a) _a
#define cairo_show_page(_c) do {} while (0)
#define cairo_destroy(_c) do {} while (0)
#define cairo_clip(_c) do {} while (0)
#define cairo_set_operator(_c, _a) do {} while (0)
#define cairo_surface_destroy(_c) SDL_FreeSurface(_c)
#define gtk_widget_queue_draw_area(_z, _a, _b, _c, _d) do {} while (0)
#define gtk_widget_get_allocated_width(_c) 640
#define gtk_widget_get_allocated_height(_c) 480
#define gtk_widget_show(_a) do {} while (0)
#define gtk_widget_hide(_a) do {} while (0)
#define gtk_window_set_decorated(_a, _b) do {} while (0)
#define gtk_window_set_keep_above(_a, _b) do {} while (0)
#define gtk_window_fullscreen(_a) do {} while (0)
#define gtk_widget_set_size_request(_a, _b, _c) do {} while (0)
#define GTK_WIDGET(_a) _a
#define g_key_file_set_boolean(_a, _b, _c, _d) do {} while (0)
#define gtk_window_unfullscreen(_a) do {} while (0)
#define gtk_window_resize(_win, _w, _h) emscripten_set_canvas_size(_w, _h)
#define g_key_file_set_integer(_a, _b, _c, _d) do {} while (0)
#define g_key_file_set_string(_a, _b, _c, _d) do {} while (0)
//#define a(_a, _b, _c, _d) do {} while (0)
//#define a(_a, _b, _c, _d) do {} while (0)
//#define a(_a, _b, _c, _d) do {} while (0)

static inline
double g_timer_elapsed(void *timer, void *usecs) {
    struct timeval tp;
    struct timezone tzp;
    (void)timer;
    (void)usecs;

    gettimeofday(&tp, &tzp);
    return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
}


#endif
