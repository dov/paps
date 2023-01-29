/* Pango
 * paps.cc: A postscript printing program using pango.
 *
 * Copyright (C) 2002, 2005, 2022 Dov Grobgeld <dov.grobgeld@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <glib/gstdio.h>
#include <pango/pango.h>
#include <pango/pangoft2.h>
#include <pango/pangocairo.h>
#include <cairo/cairo.h>
#include <cairo/cairo-ps.h>
#include <cairo/cairo-pdf.h>
#include <cairo/cairo-svg.h>
#include <errno.h>
#include <langinfo.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <locale.h>
#include <libgen.h>
#include <config.h>
#include <string>
#include <fmt/core.h>
#include "format_from_dict.h"
#include <vector>

using namespace std;
using namespace fmt;

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE /* for wcwidth */
// int wcwidth(wchar_t c);
#endif
#include <wchar.h>

#if ENABLE_NLS
#include <libintl.h>

#define	_(str)		gettext(str)
#ifdef gettext_noop
#define N_(str)		gettext_noop(str)
#else
#define N_(str)		(str)
#endif

#else	/* NLS is disabled */
#define _(str)		(str)
#define N_(str)		(str)
#endif

#define BUFSIZE 1024
#define DEFAULT_FONT_FAMILY     "Monospace"
#define DEFAULT_FONT_SIZE       "12"
#define HEADER_FONT_FAMILY      "Monospace Bold"
#define HEADER_FONT_SCALE       "12"
#define MAKE_FONT_NAME(f,s)     f " " s

/*
 * Cairo sets limit on the comment line for cairo_ps_surface_dsc_comment() to
 * 255 characters, including the initial percent characters.
 */
#define	CAIRO_COMMENT_MAX       255

#define	MARGIN_LEFT     36
#define	MARGIN_RIGHT    36
#define	MARGIN_TOP      36
#define	MARGIN_BOTTOM   36


typedef enum {
    PAPER_TYPE_A4 = 0,
    PAPER_TYPE_US_LETTER = 1,
    PAPER_TYPE_US_LEGAL = 2,
    PAPER_TYPE_A3 = 3
} paper_type_t ;

typedef enum {
    FORMAT_POSTSCRIPT = 0,
    FORMAT_PDF = 1,
    FORMAT_SVG = 2
} output_format_t ;

typedef struct  {
    double width;
    double height;
} paper_size_t;

static const paper_size_t paper_sizes[] = {
    { 595.28, 841.89}, /* A4 */
    { 612, 792},       /* US letter */
    { 612, 1008},      /* US legal */
    { 842, 1190}       /* A3 */
};

struct PageLayout {
  ~PageLayout() {
  };

  int column_width;
  int column_height;
  int num_columns;
  int gutter_width;  /* These are all in postscript points=1/72 inch... */
  int top_margin;  
  int bottom_margin;
  int left_margin;
  int right_margin;
  int num_pages;
  double page_width;
  double page_height;
  int header_ypos;
  int header_sep;
  int header_height;
  int footer_height;
  paper_type_t paper_type;
  gdouble scale_x;
  gdouble scale_y;
  bool do_draw_header;
  bool do_draw_footer;
  bool do_duplex;
  bool do_tumble;
  bool do_landscape;
  bool do_justify;
  bool do_show_hyphens;
  bool do_draw_separation_line;
  bool do_draw_contour;
  bool do_show_wrap;
  bool do_use_markup;
  bool do_stretch_chars;
  PangoDirection pango_dir;
  string title;
  string filename_path;
  string filename;
  char* header_left = nullptr;
  char* header_center = nullptr;
  char* header_right = nullptr;
  char* footer_left = nullptr;
  char* footer_center = nullptr;
  char* footer_right = nullptr;
  string header_font_desc;
  gdouble lpi;
  gdouble cpi;
  dict_t document_info;
};

struct LineLink {
  PangoLayoutLine *pango_line;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
  int formfeed;
  bool wrapped;   // Whether the paragraph was character wrapped
};

typedef struct _Paragraph Paragraph;

/* Structure representing a paragraph
 */
struct _Paragraph {
  const char *text;
  int length;
  int height;   /* Height, in pixels */
  int formfeed;
  bool wrapped; 
  bool clipped;   // Whether the line was clipped. Used for CPI.
  PangoLayout *layout;
};

/* Information passed in user data when drawing outlines */
static GList *split_paragraphs_into_lines  (PageLayout   *page_layout,
                                            GList           *paragraphs);
static char  *read_file                    (FILE            *file,
                                            gchar           *encoding);
static GList *split_text_into_paragraphs   (PangoContext    *pango_context,
                                            PageLayout   *page_layout,
                                            int              paint_width,
                                            const char      *text);
static int    output_pages                 (cairo_surface_t * surface,
                                            cairo_t         *cr,
                                            GList           *pango_lines,
                                            PageLayout   *page_layout,
                                            PangoContext    *pango_context);
static void   eject_column                 (cairo_t         *cr,
                                            double          title_height,
                                            PageLayout   *page_layout,
                                            int              column_idx,
                                            bool             measure_only);
static void   eject_page                   (cairo_t         *cr);
static void   start_page                   (cairo_surface_t *surface,
                                            cairo_t         *cr, 
                                            PageLayout   *page_layout,
                                            bool            measure_only);
static void   draw_line_to_page            (cairo_t         *cr,
                                            int              column_idx,
                                            int              column_pos,
                                            PageLayout   *page_layout,
                                            PangoLayoutLine *line,
                                            bool         draw_wrap_character);
static int    draw_page_header_line_to_page(cairo_t         *cr,
                                            bool         is_footer,
                                            PageLayout   *page_layout,
                                            PangoContext    *ctx,
                                            int              page,
                                            int              num_pages,
                                            dict_t&           document_info,
                                            bool             measure_only);
static void   postscript_dsc_comments      (cairo_surface_t *surface,
                                            PageLayout   *page_layout);

static void build_document_info            (PageLayout* page_layout,
                                            dict_t& document_info);

bool
copy_pango_parse_enum (GType       type,
		   const char *str,
 		   int        *value,
		   bool    warn,
		   char      **possible_values);

static char* get_encoding(void);
string get_date();
string fn_basename(const string& filename);

FILE *output_fh;
static paper_type_t paper_type = PAPER_TYPE_A4;
static bool output_format_set = false;
static output_format_t output_format = FORMAT_POSTSCRIPT;
static PangoGravity gravity = PANGO_GRAVITY_AUTO;
static PangoGravityHint gravity_hint = PANGO_GRAVITY_HINT_NATURAL;
static PangoWrapMode opt_wrap = PANGO_WRAP_WORD_CHAR;
static cairo_font_face_t *paps_glyph_face = NULL; /* Special face for paps characters, e.g. newline */
static double glyph_font_size = -1;

/* Render function for paps glyphs */
static cairo_status_t
paps_render_glyph(cairo_scaled_font_t *scaled_font G_GNUC_UNUSED,
                  unsigned long glyph,
                  cairo_t *cr,
                  cairo_text_extents_t *extents)
{
  char ch = (unsigned char)glyph;

  if (ch == 'R' || ch == 'L')
  {
    // A newline sign that I created with MetaPost
    cairo_save(cr);
    cairo_scale(cr,0.005,-0.005); // TBD - figure out the scaling.
    if (ch == 'L')
    {
      cairo_scale(cr,-1,1);
      // cairo_translate(cr,-120,0);  // Keep glyph protruding to the right.
    }
    cairo_translate(cr, 20,-50);
    cairo_move_to(cr, 0, 175);
    cairo_curve_to(cr, 25.69278, 175, 53.912, 177.59557, 71.25053, 158.75053);
    cairo_curve_to(cr, 103.52599, 123.67075, 64.54437, 77.19373, 34.99985, 34.99985);
    cairo_set_line_width(cr, 25);
    cairo_stroke(cr);

    cairo_move_to(cr,0,0);
    cairo_line_to(cr,75,0);
    cairo_line_to(cr,0,75);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_restore(cr);
  }
  return CAIRO_STATUS_SUCCESS;
}

static bool
_paps_arg_paper_cb(const char *option_name,
                   const char *value,
                   gpointer    data)
{
  bool retval = true;
  
  if (value && *value)
    {
      if (g_ascii_strcasecmp(value, "legal") == 0)
        paper_type = PAPER_TYPE_US_LEGAL;
      else if (g_ascii_strcasecmp(value, "letter") == 0)
        paper_type = PAPER_TYPE_US_LETTER;
      else if (g_ascii_strcasecmp(value, "a4") == 0)
        paper_type = PAPER_TYPE_A4;
      else if (g_ascii_strcasecmp(value, "a3") == 0)
        paper_type = PAPER_TYPE_A3;
      else {
        retval = false;
        fprintf(stderr, _("Unknown page size name: %s.\n"), value);
      }
    }
  else
    {
      fprintf(stderr, _("You must specify page size.\n"));
      retval = false;
    }
  
  return retval;
}

static bool
parse_int (const char *word,
	   int        *out)
{
  char *end;
  long val;
  int i;

  if (word == NULL)
    return false;

  val = strtol (word, &end, 10);
  i = val;

  if (end != word && *end == '\0' && val >= 0 && val == i)
    {
      if (out)
        *out = i;

      return true;
    }

  return false;
}

// A local copy of the deprecated pango_parse_enum.
bool
copy_pango_parse_enum (GType       type,
		   const char *str,
 		   int        *value,
		   bool    warn,
		   char      **possible_values)
{
  GEnumClass *klass = nullptr;
  bool ret = true;
  GEnumValue *v = nullptr;

  klass = (GEnumClass*)g_type_class_ref (type);

  if (G_LIKELY (str))
    v = g_enum_get_value_by_nick (klass, str);

  if (v)
    {
      if (G_LIKELY (value))
	*value = v->value;
    }
  else if (!parse_int (str, value))
    {
      ret = false;
      if (G_LIKELY (warn || possible_values))
	{
	  int i;
	  GString *s = g_string_new (nullptr);

	  for (i = 0, v = g_enum_get_value (klass, i); v;
	       i++  , v = g_enum_get_value (klass, i))
	    {
	      if (i)
		g_string_append_c (s, '/');
	      g_string_append (s, v->value_nick);
	    }

	  if (warn)
	    g_warning ("%s must be one of %s",
		       G_ENUM_CLASS_TYPE_NAME(klass),
		       s->str);

	  if (possible_values)
	    *possible_values = s->str;

	  g_string_free (s, possible_values ? false : true);
	}
    }

  g_type_class_unref (klass);

  return ret;
}

static bool
parse_enum (GType       type,
            int        *value,
            const char *name,
            const char *arg,
            gpointer    data G_GNUC_UNUSED,
            GError **error)
{
  char *possible_values = nullptr;
  bool ret;

  ret = copy_pango_parse_enum (type,
                               arg,
                               value,
                               false,
                               &possible_values);

  if (!ret && error)
    {
      g_set_error(error,
                  G_OPTION_ERROR,
                  G_OPTION_ERROR_BAD_VALUE,
                  _("Argument for %1$s must be one of %2$s"),
                  name,
                  possible_values);
    }

  g_free (possible_values);

  return ret;
}

static bool
parse_wrap (const char *name,
            const char *arg,
            gpointer    data,
            GError    **error)
{
  return (parse_enum (PANGO_TYPE_WRAP_MODE, (int*)(void*)&opt_wrap,
                      name, arg, data, error));
}

static bool
parse_gravity_hint (const char *name,
                    const char *arg,
                    gpointer    data,
                    GError    **error)
{
  return (parse_enum (PANGO_TYPE_GRAVITY_HINT, (int*)(void*)&gravity_hint,
                      name, arg, data, error));
}

static bool
parse_gravity (const char *name,
               const char *arg,
               gpointer    data,
               GError    **error)
{
  return (parse_enum (PANGO_TYPE_GRAVITY, (int*)(void*)&gravity,
                      name, arg, data, error));
}


static bool
_paps_arg_format_cb(const char *option_name,
                    const char *value,
                    gpointer    data)
{
  bool retval = true;
  
  if (value && *value)
    {
      output_format_set = true;
      if (g_ascii_strcasecmp(value, "pdf") == 0)
        output_format = FORMAT_PDF;
      else if (g_ascii_strcasecmp(value, "ps") == 0
               || g_ascii_strcasecmp(value, "postscript") == 0)
        output_format = FORMAT_POSTSCRIPT;
      else if (g_ascii_strcasecmp(value, "svg") == 0)
        output_format = FORMAT_SVG;
      else {
        retval = false;
        fprintf(stderr, _("Unknown output format: %s.\n"), value);
      }
    }
  else
    {
      fprintf(stderr, _("You must specify a output format.\n"));
      retval = false;
    }
  
  return retval;
}

static bool
_paps_arg_lpi_cb(const gchar *option_name,
                 const gchar *value,
                 gpointer     data)
{
  bool retval = true;
  gchar *p = nullptr;
  PageLayout *page_layout = (PageLayout*)data;

  if (value && *value)
    {
      errno = 0;
      page_layout->lpi = g_strtod(value, &p);
      if ((p && *p) || errno == ERANGE)
        {
          fprintf(stderr, _("Given LPI value was invalid.\n"));
          retval = false;
        }
    }
  else
    {
      fprintf(stderr, _("You must specify the amount of lines per inch.\n"));
      retval = false;
    }

  return retval;
}

static bool
_paps_arg_cpi_cb(const gchar *option_name,
                 const gchar *value,
                 gpointer     data)
{
  bool retval = true;
  gchar *p = nullptr;
  PageLayout *page_layout = (PageLayout*)data;
  
  if (value && *value)
    {
      errno = 0;
      page_layout->cpi = g_strtod(value, &p);
      if ((p && *p) || errno == ERANGE)
        {
          fprintf(stderr, _("Given CPI value was invalid.\n"));
          retval = false;
        }
    }
  else
    {
      fprintf(stderr, _("You must specify the amount of characters per inch.\n"));
      retval = false;
    }

  return retval;
}


/*
 * Return codeset name of the environment's locale. Use UTF8 by default
 */
static char*
get_encoding(void)
{
  static char *encoding = nullptr;

  if (encoding == nullptr)
    encoding = nl_langinfo(CODESET);

  return encoding;
}

static cairo_status_t paps_cairo_write_func(void *closure G_GNUC_UNUSED,
                                            const unsigned char *data,
                                            unsigned int length)
{
  fwrite(data,length,1,output_fh);
  return CAIRO_STATUS_SUCCESS;
}

int main(int argc, char *argv[])
{
  gboolean do_landscape = false, do_rtl = false, do_justify = false, do_show_hyphens=false, do_draw_header = false, do_draw_footer=false;
  gboolean do_stretch_chars = false;
  gboolean do_draw_separation_line = false;
  gboolean do_use_markup = false;
  gboolean do_show_wrap = false; /* Whether to show wrap characters */
  gboolean do_show_version = false; // Show version and exit
  int num_columns = 1;
  int top_margin = MARGIN_TOP, bottom_margin = MARGIN_BOTTOM,
      right_margin = MARGIN_RIGHT, left_margin = MARGIN_LEFT;
  int gutter_width = 40;
  gboolean do_fatal_warnings = false;
  const gchar *font = MAKE_FONT_NAME (DEFAULT_FONT_FAMILY, DEFAULT_FONT_SIZE);
  gchar *encoding = nullptr;
  gchar *output = nullptr;
  gchar *htitle = nullptr;
  gchar *header_left = nullptr;
  gchar *header_center = nullptr;
  gchar *header_right = nullptr;
  gchar *footer_left = nullptr;
  gchar *footer_center = nullptr;
  gchar *footer_right = nullptr;
  PageLayout page_layout;
  GOptionContext *ctxt = g_option_context_new("[text file]");
  GOptionEntry entries[] = {
    {"landscape", 0, 0, G_OPTION_ARG_NONE, &do_landscape,
     N_("Landscape output. (Default: portrait)"), nullptr},
    {"columns", 0, 0, G_OPTION_ARG_INT, &num_columns,
     N_("Number of columns output. (Default: 1)"), "NUM"},
    {"font", 0, 0, G_OPTION_ARG_STRING, &font,
     N_("Set font. (Default: Monospace 12)"), "DESC"},
    {"output", 'o', 0, G_OPTION_ARG_STRING, &output,
     N_("Output file. (Default: stdout)"), "DESC"},
    {"version", 'v', 0, G_OPTION_ARG_NONE, &do_show_version,
     N_("Current version."), nullptr},
    {"rtl", 0, 0, G_OPTION_ARG_NONE, &do_rtl,
     N_("Do right-to-left text layout."), nullptr},
    {"separation-lines", 0, 0, G_OPTION_ARG_NONE, &do_draw_separation_line,
     N_("Do draw separation lines between columns and headers and footers."), nullptr},
    {"justify", 0, 0, G_OPTION_ARG_NONE, &do_justify,
     N_("Justify the layout."), nullptr},
    {"hyphens", 0, 0, G_OPTION_ARG_NONE, &do_show_hyphens,
     N_("Show hyphens when wrapping."), nullptr},
    {"wrap", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer)&parse_wrap,
     N_("Text wrapping mode [word, char, word-char]. (Default: word-char)"), "WRAP"},
    {"show-wrap", 0, 0, G_OPTION_ARG_NONE, &do_show_wrap,
     N_("Show characters for wrapping."), nullptr},
    {"paper", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer)_paps_arg_paper_cb,
     N_("Set paper size [legal, letter, a3, a4]. (Default: a4)"), "PAPER"},
    {"gravity", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer)&parse_gravity,
     N_("Base glyph rotation [south, west, north, east, auto]. (Defaut: auto)"), "GRAVITY"},
    {"gravity-hint", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer)&parse_gravity_hint,
     N_("Base glyph orientation [natural, strong, line]. (Default: natural)"), "HINT"},
    {"format", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer)_paps_arg_format_cb,
     N_("Set output format [pdf, svg, ps]. (Default: ps)"), "FORMAT"},
    {"bottom-margin", 0, 0, G_OPTION_ARG_INT, &bottom_margin,
     N_("Set bottom margin in postscript point units (1/72 inch). (Default: 36)"), "NUM"},
    {"top-margin", 0, 0, G_OPTION_ARG_INT, &top_margin,
     N_("Set top margin. (Default: 36)"), "NUM"},
    {"right-margin", 0, 0, G_OPTION_ARG_INT, &right_margin,
     N_("Set right margin. (Default: 36)"), "NUM"},
    {"left-margin", 0, 0, G_OPTION_ARG_INT, &left_margin,
     N_("Set left margin. (Default: 36)"), "NUM"},
    {"gutter-width", 0, 0, G_OPTION_ARG_INT, &gutter_width,
     N_("Set gutter width. (Default: 40)"), "NUM"},
    {"header", 0, 0, G_OPTION_ARG_NONE, &do_draw_header,
     N_("Draw page header for each page."), nullptr},
    {"footer", 0, 0, G_OPTION_ARG_NONE, &do_draw_footer,
     "Draw page footer for each page.", nullptr},
    {"title", 0, 0, G_OPTION_ARG_STRING, &htitle,
     N_("Title string for page header (Default: filename/stdin)."), "TITLE"},
    {"header-left", 0, 0, G_OPTION_ARG_STRING, &header_left,
     N_("Left side of the header. Default is localized date."), "HEADER_LEFT"},
    {"header-center", 0, 0, G_OPTION_ARG_STRING, &header_center,
     N_("Center side of the header. Default is localized date."), "HEADER_CENTER"},
    {"header-right", 0, 0, G_OPTION_ARG_STRING, &header_right,
     N_("Right side of the header. Default is localized date."), "HEADER_RIGHT"},
    {"footer-left", 0, 0, G_OPTION_ARG_STRING, &footer_left,
     N_("Left side of the footer. Default is localized date."), "FOOTER_LEFT"},
    {"footer-center", 0, 0, G_OPTION_ARG_STRING, &footer_center,
     N_("Center side of the footer. Default is localized date."), "FOOTER_CENTER"},
    {"footer-right", 0, 0, G_OPTION_ARG_STRING, &footer_right,
     N_("Right side of the footer. Default is localized date."), "FOOTER_RIGHT"},
    {"markup", 0, 0, G_OPTION_ARG_NONE, &do_use_markup,
     N_("Interpret input text as pango markup."), nullptr},
    {"encoding", 0, 0, G_OPTION_ARG_STRING, &encoding,
     N_("Assume encoding of input text. (Default: UTF-8)"), "ENCODING"},
    {"lpi", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer)_paps_arg_lpi_cb,
     N_("Set the amount of lines per inch."), "REAL"},
    {"cpi", 0, 0, G_OPTION_ARG_CALLBACK, (gpointer)_paps_arg_cpi_cb,
     N_("Set the amount of characters per inch."), "REAL"},
    /*
     * not fixed for cairo backend: disable
     *
    {"stretch-chars", 0, 0, G_OPTION_ARG_NONE, &do_stretch_chars,
     N_("Stretch characters in y-direction to fill lines."), nullptr},
     */
    {"g-fatal-warnings", 0, 0, G_OPTION_ARG_NONE, &do_fatal_warnings,
     N_("Make all glib warnings fatal."), "REAL"},

    {nullptr}

  };
  GError *error = nullptr;
  FILE *IN = nullptr;
  GList *paragraphs;
  GList *pango_lines;
  PangoContext *pango_context;
  PangoFontDescription *font_description;
  PangoDirection pango_dir = PANGO_DIRECTION_LTR;
  PangoFontMap *fontmap;
  PangoFontset *fontset;
  PangoFontMetrics *metrics;
  int total_gutter_width;
  double page_width = paper_sizes[0].width;
  double page_height = paper_sizes[0].height;
  int do_tumble = -1;   /* -1 means not initialized */
  int do_duplex = -1;
  const gchar *header_font_desc = MAKE_FONT_NAME (HEADER_FONT_FAMILY, HEADER_FONT_SCALE);
  const gchar *filename_in;
  gchar *text;
  /* int header_sep = 20; */
  int max_width = 0, w;
  GOptionGroup *options;
  cairo_t *cr;
  cairo_surface_t *surface = nullptr;
  double surface_page_width = 0, surface_page_height = 0;

  /* Set locale from environment */
  (void) setlocale(LC_ALL, "");

  /* Setup i18n */
  textdomain(GETTEXT_PACKAGE);
  bindtextdomain(GETTEXT_PACKAGE, DATADIR "/locale");
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

  /* Setup the paps glyph face */
  paps_glyph_face = cairo_user_font_face_create();
  cairo_user_font_face_set_render_glyph_func(paps_glyph_face, paps_render_glyph);

  /* Init PageLayout parameters set by the option parsing */
  page_layout.cpi = page_layout.lpi = 0.0L;

  options = g_option_group_new("main","","",&page_layout, nullptr);
  g_option_group_add_entries(options, entries);
  g_option_group_set_translation_domain(options, GETTEXT_PACKAGE);
  g_option_context_set_main_group(ctxt, options);
#if 0
  g_option_context_add_main_entries(ctxt, entries, nullptr);
#endif
  
  /* Parse command line */
  if (!g_option_context_parse(ctxt, &argc, &argv, &error))
    {
      fprintf(stderr, _("Command line error: %s\n"), error->message);
      exit(1);
    }
  
  if (do_show_version)
    {
      printf("Version: %s\n", PACKAGE_STRING);
      exit(0);
    }
  
  if (do_fatal_warnings)
    g_log_set_always_fatal(G_LOG_LEVEL_MASK);

  if (do_rtl)
    pango_dir = PANGO_DIRECTION_RTL;

  page_layout.header_left = header_left;
  page_layout.header_center = header_center;
  page_layout.header_right = header_right;

  page_layout.footer_left = footer_left;
  page_layout.footer_center = footer_center;
  page_layout.footer_right = footer_right;

  page_layout.do_draw_header = do_draw_header;
  page_layout.do_draw_footer = do_draw_footer;

  if (argc > 1)
    {
      filename_in = argv[1];
      IN = fopen(filename_in, "r");
      if (!IN)
        {
          fprintf(stderr, _("Failed to open %s!\n"), filename_in);
          exit(1);
        }
    }
  else
    {
      filename_in = "stdin";
      IN = stdin;
    }

  // For now always write to stdout
  if (output == nullptr)
    output_fh = stdout;
  else
    {
      output_fh = fopen(output,"wb");
      if (!output_fh)
        {
          fprintf(stderr, _("Failed to open %s for writing!\n"), output);
          exit(1);
        }
    }

  /* Page layout */
  page_width = paper_sizes[(int)paper_type].width;
  page_height = paper_sizes[(int)paper_type].height;

  /* Deduce output format from file name if not explicitely set */
  if (!output_format_set && output != nullptr)
    {
      if (g_str_has_suffix(output, ".svg") || g_str_has_suffix(output, ".SVG"))
        output_format = FORMAT_SVG;
      else if (g_str_has_suffix(output, ".pdf") || g_str_has_suffix(output, ".PDF"))
        output_format = FORMAT_PDF;
      /* Otherwise keep postscript default */
    }
  
  /* Swap width and height for landscape except for postscript */
  surface_page_width = page_width;
  surface_page_height = page_height;
  if (output_format != FORMAT_POSTSCRIPT && do_landscape)
    {
      surface_page_width = page_height;
      surface_page_height = page_width;
    }
        
  if (output_format == FORMAT_POSTSCRIPT)
    surface = cairo_ps_surface_create_for_stream(&paps_cairo_write_func,
                                                 nullptr,
                                                 surface_page_width,
                                                 surface_page_height);
  else if (output_format == FORMAT_PDF)
    surface = cairo_pdf_surface_create_for_stream(&paps_cairo_write_func,
                                                  nullptr,
                                                  surface_page_width,
                                                  surface_page_height);
  else 
    surface = cairo_svg_surface_create_for_stream(&paps_cairo_write_func,
                                                  nullptr,
                                                  surface_page_width,
                                                  surface_page_height);

  cr = cairo_create(surface);

  pango_context = pango_cairo_create_context(cr);
  pango_cairo_context_set_resolution(pango_context, 72.0); /* Native postscript resolution */
  
  /* Setup pango */
  pango_context_set_base_dir (pango_context, pango_dir);
  pango_context_set_language (pango_context, pango_language_get_default ());
  pango_context_set_base_gravity (pango_context, gravity);
  pango_context_set_gravity_hint (pango_context, gravity_hint);
  
  /* create the font description */
  font_description = pango_font_description_from_string (font);
  if ((pango_font_description_get_set_fields (font_description) & PANGO_FONT_MASK_FAMILY) == 0)
    pango_font_description_set_family (font_description, DEFAULT_FONT_FAMILY);
  if ((pango_font_description_get_set_fields (font_description) & PANGO_FONT_MASK_SIZE) == 0)
    pango_font_description_set_size (font_description, atoi(DEFAULT_FONT_SIZE) * PANGO_SCALE);

  // Keep the font size for the wrap character.
  glyph_font_size = pango_font_description_get_size(font_description) / PANGO_SCALE;
  pango_context_set_font_description (pango_context, font_description);

  if (num_columns <= 0) {
    fprintf(stderr, _("%s: Invalid input: --columns=%d, using default.\n"), g_get_prgname (), num_columns);
    num_columns = 1;
  }

  if (num_columns == 1)
    total_gutter_width = 0;
  else
    total_gutter_width = gutter_width * (num_columns - 1);
  if (do_landscape)
    {
      double tmp;
      tmp = page_width;
      page_width = page_height;
      page_height = tmp;
      if (do_tumble < 0)
        do_tumble = true;
      if (do_duplex < 0)
        do_duplex = true;
    }
  else
    {
      if (do_tumble < 0)
        do_tumble = true;
      if (do_duplex < 0)
        do_duplex = true;
    }
  
  page_layout.page_width = page_width;
  page_layout.page_height = page_height;
  page_layout.paper_type = paper_type;
  page_layout.num_columns = num_columns;
  page_layout.left_margin = left_margin;
  page_layout.right_margin = right_margin;
  page_layout.gutter_width = gutter_width;
  page_layout.top_margin = top_margin;
  page_layout.bottom_margin = bottom_margin;
  page_layout.header_ypos = page_layout.top_margin;
  page_layout.header_height = 0;
  page_layout.footer_height = 0;
  page_layout.do_show_wrap = do_show_wrap;
  page_layout.scale_x = 1.0L;
  page_layout.scale_y = 1.0L;
  page_layout.header_sep = 0; // header_sep;
    
  page_layout.column_height = (int)page_height
                            - page_layout.top_margin
                            - page_layout.header_sep
                            - page_layout.bottom_margin;
  page_layout.column_width =  ((int)page_layout.page_width
                            - page_layout.left_margin - page_layout.right_margin
                            - total_gutter_width) / page_layout.num_columns;
  page_layout.do_draw_separation_line = do_draw_separation_line;
  page_layout.do_landscape = do_landscape;
  page_layout.do_justify = do_justify;
  page_layout.do_show_hyphens = do_show_hyphens;
  page_layout.do_stretch_chars = do_stretch_chars;
  page_layout.do_use_markup = do_use_markup;
  page_layout.do_tumble = do_tumble;
  page_layout.do_duplex = do_duplex;
  page_layout.pango_dir = pango_dir;
  page_layout.filename_path = filename_in;
  page_layout.filename = fn_basename(filename_in);

  if (htitle)
     page_layout.title = htitle;
  else
     page_layout.title = fn_basename(filename_in);
  page_layout.header_font_desc = header_font_desc;

  /* calculate x-coordinate scale */
  if (page_layout.cpi > 0.0L)
    {
      gint font_size;

      fontmap = pango_ft2_font_map_new ();
      fontset = pango_font_map_load_fontset (fontmap, pango_context, font_description, pango_language_get_default());
      metrics = pango_fontset_get_metrics (fontset);
      max_width = pango_font_metrics_get_approximate_char_width (metrics);
      w = pango_font_metrics_get_approximate_digit_width (metrics);
      if (w > max_width)
          max_width = w;
      page_layout.scale_x = 1.0L / page_layout.cpi * 72.0 * (gdouble)PANGO_SCALE / (gdouble)max_width;
      pango_font_metrics_unref (metrics);
      g_object_unref (G_OBJECT (fontmap));

      font_size = pango_font_description_get_size (font_description);
      // update the font size to that width
      pango_font_description_set_size (font_description, (int)(font_size * page_layout.scale_x));
      glyph_font_size = font_size * page_layout.scale_x / PANGO_SCALE;
      pango_context_set_font_description (pango_context, font_description);
    }

  page_layout.scale_x = page_layout.scale_y = 1.0;

  if (encoding == nullptr)
    encoding = get_encoding();

  text = read_file(IN, encoding);

  if (output_format == FORMAT_POSTSCRIPT)
    postscript_dsc_comments(surface, &page_layout);

  paragraphs = split_text_into_paragraphs(pango_context,
                                          &page_layout,
                                          page_layout.column_width, 
                                          text);
  pango_lines = split_paragraphs_into_lines(&page_layout, paragraphs);

  cairo_scale(cr, page_layout.scale_x, page_layout.scale_y);

  output_pages(surface,
               cr,
               pango_lines,
               &page_layout,
               pango_context);

  cairo_destroy (cr);
  cairo_surface_finish (surface);
  cairo_surface_destroy(surface);
  g_option_context_free(ctxt);

  return 0;
}


/* Read an entire file into a string
 */
static char *
read_file (FILE   *file,
           gchar  *encoding)
{
  GString *inbuf;
  char *text;
  char buffer[BUFSIZE];
  GIConv cvh = nullptr;
  gsize inc_seq_bytes = 0;


  if (encoding != nullptr)
    {
      cvh = g_iconv_open ("UTF-8", encoding);
      if (cvh == (GIConv)-1)
        {
          fprintf(stderr, _("%s: Invalid encoding: %s\n"), g_get_prgname (), encoding);
          exit(1);
        }
    }

  inbuf = g_string_new (nullptr);
  while (1)
    {
      char *ib, *ob, obuffer[BUFSIZE * 6], *bp;
      gsize iblen, oblen;

      bp = fgets (buffer+inc_seq_bytes, BUFSIZE-inc_seq_bytes-1, file);
      if (inc_seq_bytes)
        inc_seq_bytes = 0;

      if (ferror (file))
        {
          fprintf(stderr, _("%s: Error reading file.\n"), g_get_prgname ());
          g_string_free (inbuf, true);
          exit(1);
        }
      else if (bp == nullptr)
        break;

      if (cvh != nullptr)
        {
          ib = buffer;
          iblen = strlen (ib);
          ob = bp = obuffer;
          oblen = BUFSIZE * 6 - 1;
          if (g_iconv (cvh, &ib, &iblen, &ob, &oblen) == (gsize)-1)
            {
              /*
               * EINVAL - incomplete sequence at the end of the buffer. Move the
               * incomplete sequence bytes to the beginning of the buffer for
               * the next round of conversion.
               */
              if (errno == EINVAL)
                {
                  inc_seq_bytes = iblen;
                  memmove (buffer, ib, inc_seq_bytes);
                }
              else
                {
                  fprintf (stderr, _("%1$s: Error while converting input from '%2$s' to UTF-8.\n"),
                    g_get_prgname(), encoding);
                  exit(1);
                }
             }
          obuffer[BUFSIZE * 6 - 1 - oblen] = 0;
        }
      g_string_append (inbuf, bp);
    }

  fclose (file);

  /* Add a trailing new line if it is missing */
  if (inbuf->len && inbuf->str[inbuf->len-1] != '\n')
    g_string_append(inbuf, "\n");

  text = inbuf->str;
  g_string_free (inbuf, false);

  if (encoding != nullptr && cvh != nullptr)
    g_iconv_close(cvh);

  return text;
}


/* Take a UTF8 string and break it into paragraphs on \n characters
 */
static GList *
split_text_into_paragraphs (PangoContext *pango_context,
                            PageLayout *page_layout,
                            int paint_width,  /* In pixels */
                            const char *text)
{
  const char *p = text;
  char *next;
  gunichar wc;
  GList *result = nullptr;
  const char *last_para = text;
  PangoAttrList *attrs = pango_attr_list_new ();
  pango_attr_list_insert(attrs, pango_attr_insert_hyphens_new(page_layout->do_show_hyphens));

  /* If we are using markup we treat the entire text as a single paragraph.
   * I tested it and found that this is much slower than the split and
   * assign method used below. Otherwise we might as well use this single
   * chunk method always.
   *
   * I don't think we need this at all, and are better of reading one
   * line at a time...
   */
  if (0 && page_layout->do_use_markup)
    {
      Paragraph *para = g_new (Paragraph, 1);
      para->wrapped = false; /* No wrapped chars for markups */
      para->clipped = false;
      para->text = text;
      para->length = strlen(text);
      para->layout = pango_layout_new (pango_context);
      pango_layout_set_attributes (para->layout, attrs);
      pango_layout_set_markup (para->layout, para->text, para->length);
      pango_layout_set_justify (para->layout, page_layout->do_justify);
      pango_layout_set_alignment (para->layout,
                                  page_layout->pango_dir == PANGO_DIRECTION_LTR
                                      ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT);
      pango_layout_set_width (para->layout, paint_width * PANGO_SCALE);
      pango_layout_set_wrap (para->layout, opt_wrap);

      para->height = 0;
      
      result = g_list_prepend (result, para);
    }
  else
    {

      while (p != nullptr && *p)
        {
          wc = g_utf8_get_char (p);
          next = g_utf8_next_char (p);
          if (wc == (gunichar)-1)
            {
              fprintf (stderr, _("%s: Invalid character in input\n"), g_get_prgname ());
              wc = 0;
            }
          if (!*p || !wc || wc == '\r' || wc == '\n' || wc == '\f')
            {
              Paragraph *para = g_new (Paragraph, 1);
              para->wrapped = false;
              para->clipped = false;
              para->text = last_para;
              para->length = p - last_para;
              /* handle dos line breaks */
              if (wc == '\r' && *next == '\n')
                  next = g_utf8_next_char(next);
              para->layout = pango_layout_new (pango_context);

              pango_layout_set_attributes (para->layout,attrs);


              if (page_layout->cpi > 0.0L)
                {
                  /* figuring out the correct width from the pango_font_metrics_get_approximate_width()
                   * is really hard and pango_layout_set_wrap() doesn't work properly then.
                   * Those are not reliable to render the characters exactly according to the given CPI.
                   * So re-calculate the width to wrap up to be comfortable with CPI.
                   */
                  wchar_t *wtext = nullptr, *wnewtext = nullptr;
                  gchar *newtext = nullptr;
                  gsize len, col, i, wwidth = 0;
                  PangoRectangle ink_rect, logical_rect;

                  wtext = (wchar_t *)g_utf8_to_ucs4 (para->text, para->length, nullptr, nullptr, nullptr);
                  if (wtext == nullptr)
                    {
                      fprintf (stderr, _("%s: Unable to convert UTF-8 to UCS-4.\n"), g_get_prgname ());
                    fail:
                      g_free (wtext);
                      g_free (wnewtext);
                      g_free (newtext);
                      exit (1);
                    }
                  len = g_utf8_strlen (para->text, para->length);
                  /* the amount of characters that can be put on the line against CPI */
                  col = (int)(page_layout->column_width / 72.0 * page_layout->cpi);
                  if (len > col)
                    {
                      /* need to wrap them up */
                      wnewtext = g_new (wchar_t, wcslen (wtext) + 1);
                      para->clipped = true;
                      if (wnewtext == nullptr)
                        {
                          fprintf (stderr, _("%s: Unable to allocate the memory.\n"), g_get_prgname ());
                          goto fail;
                        }
                      for (i = 0; i < len; i++)
                        {
                          gssize w = wcwidth (wtext[i]);

                          if (w >= 0)
                            wwidth += w;
                          if (wwidth > col)
                            break;
                          wnewtext[i] = wtext[i];
                        }
                      wnewtext[i] = 0L;

                      newtext = g_ucs4_to_utf8 ((const gunichar *)wnewtext, i, nullptr, nullptr, nullptr);
                      if (newtext == nullptr)
                        {
                          fprintf (stderr, _("%s: Unable to convert UCS-4 to UTF-8.\n"), g_get_prgname ());
                          goto fail;
                        }
                      if (page_layout->do_use_markup)
                          pango_layout_set_markup (para->layout, newtext, -1);
                      else
                          pango_layout_set_text (para->layout, newtext, -1);
                      pango_layout_get_extents (para->layout, &ink_rect, &logical_rect);
                      paint_width = logical_rect.width / PANGO_SCALE;
                      g_free (wnewtext);
                      g_free (newtext);

                      para->length = i;
                      next = g_utf8_offset_to_pointer (para->text, para->length);
                      wc = g_utf8_get_char (g_utf8_prev_char (next));
                    }
                  else
                    {
                      pango_layout_set_text (para->layout, para->text, para->length);
                    }

                  g_free (wtext);

                  pango_layout_set_width (para->layout, -1);
                }
              else
                {
                  if (page_layout->do_use_markup)
                      pango_layout_set_markup (para->layout, para->text, para->length);
                  else
                      pango_layout_set_text (para->layout, para->text, para->length);
                  pango_layout_set_width (para->layout, paint_width * PANGO_SCALE);

                  pango_layout_set_wrap (para->layout, opt_wrap);

                  if (opt_wrap == PANGO_WRAP_CHAR)
                      para->wrapped = true;                    

                  /* Should we support truncation as well? */
                }
                  
              pango_layout_set_justify (para->layout, page_layout->do_justify);
              pango_layout_set_alignment (para->layout,
                                          page_layout->pango_dir == PANGO_DIRECTION_LTR
                                          ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT);

              para->height = 0;

              last_para = next;
            
              if (wc == '\f')
                para->formfeed = 1;
              else
                para->formfeed = 0;

              result = g_list_prepend (result, para);
            }
          if (!wc) /* incomplete character at end */
            break;
          p = next;
        }
    }

  return g_list_reverse (result);
}



/* Split a list of paragraphs into a list of lines.
 */
GList *
split_paragraphs_into_lines(PageLayout *page_layout,
                            GList         *paragraphs)
{
  GList *line_list = nullptr;
  int max_height = 0;
  /* Read the file */

  /* Now split all the pagraphs into lines */
  GList *par_list;

  par_list = paragraphs;
  while(par_list)
    {
      int para_num_lines, i;
      LineLink *line_link;
      Paragraph *para = (Paragraph*)par_list->data;

      para_num_lines = pango_layout_get_line_count(para->layout);

      for (i=0; i<para_num_lines; i++)
        {
          PangoRectangle logical_rect, ink_rect;
          
          line_link = g_new(LineLink, 1);
          line_link->formfeed = 0;
          line_link->wrapped = (para->wrapped && i < para_num_lines - 1) || (para->clipped);
          line_link->pango_line = pango_layout_get_line(para->layout, i);
          pango_layout_line_get_extents(line_link->pango_line,
                                        &ink_rect, &logical_rect);
          line_link->logical_rect = logical_rect;
          if (para->formfeed && i == (para_num_lines - 1))
              line_link->formfeed = 1;
          line_link->ink_rect = ink_rect;
          line_list = g_list_prepend(line_list, line_link);
          if (logical_rect.height > max_height)
              max_height = logical_rect.height;
        }

      par_list = par_list->next;
    }
  
  /*
   * not fixed for cairo backend: disable
   *
  if (page_layout->do_stretch_chars && page_layout->lpi > 0.0L)
      page_layout->scale_y = 1.0 / page_layout->lpi * 72.0 * PANGO_SCALE / max_height;
   */

  return g_list_reverse(line_list);
  
}


/*
 * Define PostScript document header information.
 */
void
postscript_dsc_comments(cairo_surface_t *surface, PageLayout *pl)
{
  char buf[CAIRO_COMMENT_MAX];
  int x, y;

  /*
   * Title
   */
  snprintf(buf, CAIRO_COMMENT_MAX, "%%%%Title: %s", pl->title.c_str());
  cairo_ps_surface_dsc_comment (surface, buf);

  /*
   * Orientation
   */
  if (pl->do_landscape)
    {
      cairo_ps_surface_dsc_comment (surface, "%%Orientation: Landscape");
      x = (int)pl->page_height;
      y = (int)pl->page_width;
    }
  else
    {
      cairo_ps_surface_dsc_comment (surface, "%%Orientation: Portrait");
      x = (int)pl->page_width;
      y = (int)pl->page_height;
    }

  /*
   * Redefine BoundingBox to cover the whole paper. Cairo creates the entry
   * based on the text only. This may affect further processing, such as with
   * convert(1).
   */
  snprintf(buf, CAIRO_COMMENT_MAX, "%%%%BoundingBox: 0 0 %d %d", x, y);
  cairo_ps_surface_dsc_comment (surface, buf);

  /*
   * Duplex
   */
  if (pl->do_duplex)
    {
      cairo_ps_surface_dsc_comment(surface, "%%Requirements: duplex");
      cairo_ps_surface_dsc_begin_setup(surface);

      if (pl->do_tumble)
        cairo_ps_surface_dsc_comment(surface, "%%IncludeFeature: *Duplex DuplexTumble");
      else
        cairo_ps_surface_dsc_comment(surface, "%%IncludeFeature: *Duplex DuplexNoTumble");
    }
  else
    cairo_ps_surface_dsc_begin_setup(surface);

  if (pl->paper_type == PAPER_TYPE_US_LEGAL)
    cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *PageSize Legal");
  if (pl->paper_type == PAPER_TYPE_US_LETTER)
    cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *PageSize Letter");
  if (pl->paper_type == PAPER_TYPE_A4)
    cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *PageSize A4");
  if (pl->paper_type == PAPER_TYPE_A3)
    cairo_ps_surface_dsc_comment (surface, "%%IncludeFeature: *PageSize A3");
}


int
output_pages(cairo_surface_t *surface,
             cairo_t       *cr,
             GList         *pango_lines,
             PageLayout *page_layout,
             PangoContext  *pango_context)
{
  int pango_column_height = page_layout->column_height * PANGO_SCALE;
  int height = 0;
  int num_pages = -1; // This will be calculated in the measurement pass below
  int title_height = 0;
  GList *pango_lines_start = pango_lines;
  dict_t document_info;

  // Fill in the static document info 
  build_document_info(page_layout, document_info);
  document_info["num_pages"] = 0;

  // Do twice, the first time only measure without shipping
  for (int i=0; i<2; i++)
    {
      int page_idx = 1;
      bool measure_only = i==0;
      LineLink *prev_line_link = nullptr;
      int column_idx = 0;
      int column_y_pos = 0;

      column_y_pos = 0;
      document_info["page_idx"] = page_idx;

      pango_lines = pango_lines_start;
      start_page(surface, cr, page_layout, measure_only);
    
      if (page_layout->do_draw_header)
        {
          title_height = draw_page_header_line_to_page(cr, false, page_layout, pango_context, page_idx, num_pages, document_info, measure_only);
          column_y_pos = title_height;
        }
      if (page_layout->do_draw_footer)
          draw_page_header_line_to_page(cr, true, page_layout, pango_context, page_idx, num_pages, document_info, measure_only);

      while(pango_lines)
        {
          LineLink *line_link = (LineLink*)pango_lines->data;
          PangoLayoutLine *line = line_link->pango_line;
          bool draw_wrap_character = page_layout->do_show_wrap && line_link->wrapped;
          
          /* Check if we need to move to next column */
          if ((column_y_pos + line_link->logical_rect.height
               >= pango_column_height) ||
              (prev_line_link && prev_line_link->formfeed))
            {
              column_idx++;
              column_y_pos = title_height;
              if (column_idx == page_layout->num_columns)
                {
                  column_idx = 0;
                  if (!measure_only)
                    eject_page(cr);
                  page_idx++;
                  document_info["page_idx"] = page_idx;
                  start_page(surface, cr, page_layout, measure_only);
    
                  if (page_layout->do_draw_header) {
                    title_height = draw_page_header_line_to_page(cr, false, page_layout, pango_context, page_idx, num_pages, document_info, measure_only);
                    column_y_pos = title_height;
                  }
                  if (page_layout->do_draw_footer) 
                    draw_page_header_line_to_page(cr, true, page_layout, pango_context, page_idx, num_pages, document_info, measure_only);
                }
              else
                {
                  eject_column(cr,
                               title_height/PANGO_SCALE,
                               page_layout,
                               column_idx,
                               measure_only
                               );
                }
            }
          if (page_layout->lpi > 0.0L)
            height = (int)(1.0 / page_layout->lpi * 72.0 * PANGO_SCALE);
          else
            height = line_link->logical_rect.height;
          if (!measure_only)
            draw_line_to_page(cr,
                              column_idx,
                              column_y_pos+height,
                              page_layout,
                              line,
                              draw_wrap_character);
          column_y_pos += height;
          pango_lines = pango_lines->next;
          prev_line_link = line_link;
        }
      if (!measure_only)
        eject_page(cr);

      if (measure_only)
        {
          num_pages = page_idx;
          document_info["num_pages"] = num_pages;
        }
    }
  return num_pages;
}

void eject_column(cairo_t *cr,
                  double title_height,
                  PageLayout *page_layout,
                  int column_idx,
                  bool measure_only)
{
  double x_pos, y_top, y_bot, total_gutter;

#if 0
  fprintf(stderr, "do_draw_separation_line column_idx = %d %d\n", page_layout->do_draw_separation_line, column_idx);
#endif
  if (!page_layout->do_draw_separation_line)
    return;

  if (page_layout->pango_dir == PANGO_DIRECTION_RTL)
    column_idx = (page_layout->num_columns - column_idx);

  if (column_idx == 1)
    total_gutter = 1.0 * page_layout->gutter_width /2;
  else
    total_gutter = (column_idx - 0.5) * page_layout->gutter_width;
      
  x_pos = page_layout->left_margin
        + page_layout->column_width * column_idx
      + total_gutter;

  y_top = page_layout->top_margin + page_layout->header_height + page_layout->header_sep / 2 + title_height;
  y_bot = page_layout->page_height - page_layout->bottom_margin - page_layout->footer_height;

  if (!measure_only)
    {
      cairo_move_to(cr,x_pos, y_top);
      cairo_line_to(cr,x_pos, y_bot);
      cairo_set_line_width(cr, 0.1);
      cairo_stroke(cr);
    }
}

void eject_page(cairo_t *cr)
{
  cairo_show_page(cr);
}

void start_page(cairo_surface_t *surface,
                cairo_t *cr,
                PageLayout *page_layout,
                bool measure_only)
{
  cairo_identity_matrix(cr);

  if (output_format == FORMAT_POSTSCRIPT)
    {
      char buf[CAIRO_COMMENT_MAX];
      int x, y;

      if (page_layout->do_landscape)
        {
          x = (int)page_layout->page_height;
          y = (int)page_layout->page_width;
        }
      else
        {
          x = (int)page_layout->page_width;
          y = (int)page_layout->page_height;
        }
      if (!measure_only)
        {
          cairo_ps_surface_dsc_begin_page_setup (surface);
    
          snprintf(buf, CAIRO_COMMENT_MAX, "%%%%PageBoundingBox: 0 0 %d %d", x, y);
          cairo_ps_surface_dsc_comment (surface, buf);
        }
    }

  if (page_layout->do_landscape)
    {
      if (output_format == FORMAT_POSTSCRIPT)
        {
          if (!measure_only)
            cairo_ps_surface_dsc_comment (surface, "%%PageOrientation: Landscape");
          cairo_translate(cr, 0, page_layout->page_width);
          cairo_rotate(cr, -G_PI_2);
        }
    }
  else
    {
      if (output_format == FORMAT_POSTSCRIPT)
        cairo_ps_surface_dsc_comment (surface, "%%PageOrientation: Portrait");
    }
}

void
draw_line_to_page(cairo_t *cr,
                  int column_idx,
                  int column_pos,
                  PageLayout *page_layout,
                  PangoLayoutLine *line,
                  bool draw_wrap_character)
{
  /* Assume square aspect ratio for now */
  double y_pos = page_layout->top_margin
               + page_layout->header_sep
               + column_pos / PANGO_SCALE;
  double x_pos = page_layout->left_margin
               + column_idx * (page_layout->column_width
                               + page_layout->gutter_width);
  PangoRectangle ink_rect, logical_rect;

  /* Do RTL column layout for RTL direction */
  if (page_layout->pango_dir == PANGO_DIRECTION_RTL)
    {
      x_pos = page_layout->left_margin
        + (page_layout->num_columns-1-column_idx)
        * (page_layout->column_width + page_layout->gutter_width);
    }
  
  pango_layout_line_get_extents(line,
                                &ink_rect,
                                &logical_rect);

  if (page_layout->pango_dir == PANGO_DIRECTION_RTL) {
      x_pos += page_layout->column_width  - logical_rect.width / PANGO_SCALE;
  }

  cairo_move_to(cr, x_pos, y_pos);
  pango_cairo_show_layout_line(cr, line);

  if (draw_wrap_character)
    {
      cairo_set_font_face(cr, paps_glyph_face);
      cairo_set_font_size(cr, glyph_font_size);

      if (page_layout->pango_dir == PANGO_DIRECTION_LTR)
        {
          cairo_move_to(cr, x_pos + page_layout->column_width, y_pos);
          cairo_show_text(cr, "R");
        }
      else
        {
          double left_margin = page_layout->left_margin
            + (page_layout->num_columns-1-column_idx)
            * (page_layout->column_width + page_layout->gutter_width);

          cairo_move_to(cr, left_margin, y_pos); 
          cairo_show_text(cr, "L");
        }
    }
}

/*
 * Provide date string from current locale converted to UTF-8.
 */
string
get_date()
{
  time_t t = time(nullptr);
  GIConv cvh = nullptr;
  GString *inbuf;
  char *ib, *ob, obuffer[BUFSIZE * 6], *bp;
  gsize iblen, oblen;
  static char *date_utf8 = nullptr;

  if (date_utf8 == nullptr) {
    char date[256];
    t = time(nullptr);
    strftime(date, sizeof(date), "%c", localtime(&t));

    cvh = g_iconv_open("UTF-8", get_encoding());
    if (cvh == (GIConv)-1) {
      fprintf(stderr, _("%s: Invalid encoding: %s\n"), g_get_prgname(), get_encoding());
      exit(1);
    }

    inbuf = g_string_new(nullptr);
    ib = bp = date;
    iblen = strlen(ib);
    ob = bp = obuffer;
    oblen = BUFSIZE * 6 - 1;

    if (g_iconv(cvh, &ib, &iblen, &ob, &oblen) == (gsize)-1) {
      fprintf(stderr, _("%1$s: Error while converting date string from '%2$s' to UTF-8.\n"),
        g_get_prgname(), get_encoding());
      /* Return the unconverted string. */
      g_string_free(inbuf, false);
      g_iconv_close(cvh);
      return date;
    }

    obuffer[BUFSIZE * 6 - 1 - oblen] = 0;
    g_string_append(inbuf, bp);

    date_utf8 = inbuf->str;
    g_string_free(inbuf, false);
    g_iconv_close(cvh);
  }

  return date_utf8;
}

int
draw_page_header_line_to_page(cairo_t         *cr,
                              bool             is_footer,
                              PageLayout   *page_layout,
                              PangoContext    *ctx,
                              int              page,
                              int              num_pages,
                              dict_t&          document_info,
                              bool             measure_only)
{
  PangoLayout *layout = pango_layout_new(ctx);
  PangoLayoutLine *line;
  PangoRectangle ink_rect={0,0,0,0}, logical_rect = {0,0,0,0};
  /* Assume square aspect ratio for now */
  double x_pos, y_pos;
  int height;
  gdouble line_pos;

  /* Reset gravity?? */

  // Three lines:
  //    1. Date
  //    2. Filename (title)
  //    3. Page
  string header = format("<span font_desc=\"{}\">{}</span>\n"
                  "<span font_desc=\"{}\">{}</span>\n"
                  "<span font_desc=\"{}\">{}</span>",
                  page_layout->header_font_desc,
                  get_date().c_str(),
                    page_layout->header_font_desc,
                    page_layout->title.c_str(),
                    page_layout->header_font_desc,
                    page
                    );

  vector<string> header_parts = {"","",""};
  if (is_footer)
    {
      if (page_layout->footer_left)
        header_parts[0] = page_layout->footer_left;
    
      if (page_layout->footer_center)
        header_parts[1] = page_layout->footer_center;
      
      if (page_layout->footer_right)
        header_parts[2] = page_layout->footer_right;
    }
  else
    {
      if (page_layout->header_left)
        header_parts[0]=page_layout->header_left;
      else
        header_parts[0]=get_date();
    
      if (page_layout->header_center)
        header_parts[1] = page_layout->header_center;
      else
        header_parts[1] = page_layout->filename;
      
      if (page_layout->header_right)
        header_parts[2] = page_layout->header_right;
      else
        header_parts[2] = format("{}",page);
    }

  for (auto& v : header_parts)
    v = format_from_dict(v, document_info);

  header = "";
  for (auto& hp : header_parts)
    header += format("<span font_desc=\"{}\">{}</span>\n",
                     page_layout->header_font_desc,
                     hp);

  pango_layout_set_markup(layout, header.c_str(), -1);

  /* The title is in the center */
  line = pango_layout_get_line(layout, 0);
  pango_layout_line_get_extents(line,
                                &ink_rect,
                                &logical_rect);
  x_pos = page_layout->left_margin;

  height = logical_rect.height / PANGO_SCALE /3.0;

  /* The header is placed right after the margin */
  if (is_footer)
    {
      y_pos = page_layout->page_height - page_layout->bottom_margin*0.5;
      page_layout->footer_height = height;
    }
  else
    {
      y_pos = page_layout->top_margin + height;
      page_layout->header_height = height;
    }

  cairo_move_to(cr, x_pos,y_pos);
  if (!measure_only)
    pango_cairo_show_layout_line(cr,line);

  /* output a right edge of header/footer */
  line = pango_layout_get_line(layout, 1);
  pango_layout_line_get_extents(line,
                                &ink_rect,
                                &logical_rect);
  /* pagenum_rect = logical_rect; */
  x_pos = page_layout->left_margin + (page_layout->page_width-page_layout->left_margin-page_layout->right_margin)*0.5 - 0.5*logical_rect.width/PANGO_SCALE;
  cairo_move_to(cr, x_pos,y_pos);
  if (!measure_only)
    pango_cairo_show_layout_line(cr,line);

  /* output a "center" of header/footer */
  line = pango_layout_get_line(layout, 2);
  pango_layout_line_get_extents(line,
                                &ink_rect,
                                &logical_rect);
  x_pos = page_layout->page_width - page_layout->right_margin - (logical_rect.width / PANGO_SCALE );

  //  x_pos = page_layout->page_width - page_layout->right_margin -
  //      ((logical_rect.width + pagenum_rect.width) / PANGO_SCALE + page_layout->gutter_width);
  cairo_move_to(cr, x_pos,y_pos);
  if(!measure_only)
    pango_cairo_show_layout_line(cr,line);

  g_object_unref(layout);

  /* header separator */
  if (!measure_only && page_layout->do_draw_separation_line)
    {
      if (is_footer)
        line_pos = page_layout->page_height - page_layout->bottom_margin * 1.1;
      else
        line_pos = page_layout->top_margin + page_layout->header_height + page_layout->header_sep;
      line_pos += logical_rect.height/2.0/PANGO_SCALE;
      cairo_move_to(cr, page_layout->left_margin, line_pos);
      cairo_line_to(cr,page_layout->page_width - page_layout->right_margin, line_pos);
      cairo_set_line_width(cr,0.1); // TBD
      cairo_stroke(cr);
    }

  return logical_rect.height;
}

// Build the document info hash table that may be used in the headers
// and footers
static void
build_document_info(PageLayout* page_layout,
                    dict_t& document_info)
{
  document_info["filename"] = page_layout->filename;
  document_info["path"] = page_layout->filename_path;

  GStatBuf stat_buf;
  g_stat(page_layout->filename_path.c_str(), &stat_buf);
  document_info["mtime"] = (time_t)stat_buf.st_mtime;
  document_info["now"] = time(nullptr);

  page_layout->document_info["num_pages"] = page_layout->num_pages;
}

string fn_basename(const string& filename)
{
  return basename((char*)filename.c_str());
}
