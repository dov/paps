/* Pango
 * paps.c: A postscript printing program using pango.
 *
 * Copyright (C) 2002, 2005 Dov Grobgeld <dov.grobgeld@gmail.com>
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
 *   * Turn into a reusable library.
 */


#include <pango/pango.h>
#include "libpaps.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFSIZE 1024

typedef enum {
    PAPER_TYPE_A4 = 0,
    PAPER_TYPE_US_LETTER = 1,
    PAPER_TYPE_US_LEGAL = 2
} paper_type_t ;

typedef struct  {
    double width;
    double height;
} paper_size_t;

const paper_size_t paper_sizes[] = {
    { 595.28, 841.89}, /* A4 */
    { 612, 792},       /* US letter */
    { 612, 1008}      /* US legal */
};

typedef struct {
  double pt_to_pixel;
  double pixel_to_pt;
  int column_width;
  int column_height;
  int num_columns;
  int gutter_width;  /* These are all in postscript points=1/72 inch... */
  int top_margin;  
  int bottom_margin;
  int left_margin;
  int right_margin;
  int page_width;
  int page_height;
  int header_ypos;
  int header_sep;
  gboolean do_draw_header;
  gboolean do_draw_footer;
  gboolean do_duplex;
  gboolean do_tumble;
  gboolean do_landscape;
  gboolean do_justify;
  gboolean do_separation_line;
  gboolean do_draw_contour;
  PangoDirection pango_dir;
} page_layout_t;

typedef struct {
  char *text;
  int length;
} para_t;

typedef struct {
  PangoLayoutLine *pango_line;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
} LineLink;

typedef struct _Paragraph Paragraph;

/* Structure representing a paragraph
 */
struct _Paragraph {
  char *text;
  int length;
  int height;   /* Height, in pixels */
  PangoLayout *layout;
};

/* Information passed in user data when drawing outlines */
GList *split_paragraphs_into_lines(GList *paragraphs);
static char *read_file (FILE *file);
static GList *
split_text_into_paragraphs (PangoContext *pango_context,
                            page_layout_t *page_layout,
                            int paint_width,
                            char *text);

int output_pages(FILE *OUT,
                 GList *pango_lines,
                 page_layout_t *page_layout);
void print_postscript_header(FILE *OUT,
                             const char *title,
                             page_layout_t *page_layout);
void print_postscript_trailer(FILE *OUT, int num_pages);
void print_svg_trailer(FILE *OUT, int num_pages);
void eject_column(FILE *OUT,
                  page_layout_t *page_layout,
                  int column_idx);
void eject_page(FILE *OUT);
void start_page(FILE *OUT, int page_idx);
void draw_line_to_page(FILE *OUT,
                      int column_idx,
                      int column_pos,
                      page_layout_t *page_layout,
                      PangoLayoutLine *line);
// Fonts are three character symbols in an alphabet composing of
// the following characters:
//
//   First character:  a-zA-Z@_.,!-~`'"
//   Rest of chars:    like first + 0-9
//
// Care is taken that no keywords are overwritten, e.g. def, end.
GString *ps_font_def_string = NULL;
GString *ps_pages_string = NULL;
int last_char_idx = 0;
double last_pos_y = -1;
double last_pos_x = -1;
paps_t *paps;

#define CASE(s) if (strcmp(S_, s) == 0)

int main(int argc, char *argv[])
{
  int argp=1;
  char *filename_in;
  char *title;
  FILE *IN, *OUT = NULL;
  page_layout_t page_layout;
  char *text;
  GList *paragraphs;
  GList *pango_lines;
  PangoContext *pango_context;
  PangoFontDescription *font_description;
  PangoDirection pango_dir = PANGO_DIRECTION_LTR;
  char *font_family = "sans";
  int font_scale = 12;
  int num_pages = 1;
  int num_columns = 1;
  int gutter_width = 40;
  int total_gutter_width;
  paper_type_t paper_type = PAPER_TYPE_A4;
  int page_width = paper_sizes[0].width;
  int page_height = paper_sizes[0].height;
  gboolean do_landscape = FALSE;
  int do_tumble = -1;   /* -1 means not initialized */
  int do_duplex = -1;
  gboolean do_draw_header = FALSE;
  gboolean do_justify = FALSE;
  gchar *paps_header = NULL;

  /* Prerequisite when using glib. */
  g_type_init();
  
  /* Parse command line */
  while(argp < argc && argv[argp][0] == '-')
    {
      char *S_ = argv[argp++];
      CASE("--help")
        {
          printf("paps - A postscript generating program using pango.\n"
                 "\n"
                 "Syntax:\n"
                 "    paps [--landscape] [--columns cl] [--font_scale fs]\n"
                 "         [--family f] [--rtl] [--paper=type]\n"
                 "\n"
                 "Description:\n"
                 "    paps reads a UTF-8 encoded file and generates a PostScript\n"
                 "    rendering of the file. The rendering is done by creating\n"
                 "    outline curves through the pango FT2 backend.\n"
                 "\n"
                 "Options:\n"
                 "    --landscape     Landscape output. Default is portrait.\n"
                 "    --columns cl    Number of columns output. Default is 1.\n"
                 "    --font_scale fs  Font scaling. Default is 12.\n"
                 "    --family f      Pango ft2 font family. Default is sans.\n"
                 "    --rtl           Do rtl layout.\n"
		 "    --paper=letter  Use US Letter page layout. Default is A4.\n"
		 "    --paper=legal   Use US Legal pge layout. Default is A4.\n"
                 );
                 
          exit(0);
        }
      CASE("--landscape") { do_landscape = TRUE; continue; }
      CASE("--columns") { num_columns = atoi(argv[argp++]); continue; }
      CASE("--font_scale") { font_scale = atoi(argv[argp++]); continue; }
      CASE("--family") { font_family = argv[argp++]; continue; }
      CASE("--rtl") { pango_dir = PANGO_DIRECTION_RTL; continue; }
      CASE("--justify") { do_justify = TRUE; continue; }
      CASE("--paper=letter") { paper_type=PAPER_TYPE_US_LETTER; continue; }
      CASE("--paper=legal") { paper_type=PAPER_TYPE_US_LEGAL; continue; }
      fprintf(stderr, "Unknown option %s!\n", S_);
      exit(0);
    }

  if (argp < argc)
    {
      filename_in = argv[argp++];
      IN = fopen(filename_in, "rb");
      if (!IN)
        {
          fprintf(stderr, "Failed to open %s!\n", filename_in);
        }
    }
  else
    {
      filename_in = "stdin";
      IN = stdin;
    }
  title = filename_in;

  paps = paps_new();
  pango_context = paps_get_pango_context (paps);
  
  /* Setup pango */
  pango_context_set_language (pango_context, pango_language_from_string ("en_US"));
  pango_context_set_base_dir (pango_context, pango_dir);

  font_description = pango_font_description_new ();
  pango_font_description_set_family (font_description, g_strdup (font_family));
  pango_font_description_set_style (font_description, PANGO_STYLE_NORMAL);
  pango_font_description_set_variant (font_description, PANGO_VARIANT_NORMAL);
  pango_font_description_set_weight (font_description, PANGO_WEIGHT_NORMAL);
  pango_font_description_set_stretch (font_description, PANGO_STRETCH_NORMAL);
  pango_font_description_set_size (font_description, font_scale * PANGO_SCALE);

  pango_context_set_font_description (pango_context, font_description);

  /* Page layout */
  page_width = paper_sizes[(int)paper_type].width;
  page_height = paper_sizes[(int)paper_type].height;

  if (num_columns == 1)
    total_gutter_width = 0;
  else
    total_gutter_width = gutter_width * (num_columns - 1);
  if (do_landscape)
    {
      int tmp;
      tmp = page_width;
      page_width = page_height;
      page_height = tmp;
      if (do_tumble < 0)
        do_tumble = TRUE;
      if (do_duplex < 0)
        do_duplex = TRUE;
    }
  else
    {
      if (do_tumble < 0)
        do_tumble = TRUE;
      if (do_duplex < 0)
        do_duplex = TRUE;
    }
  
  page_layout.page_width = page_width;
  page_layout.page_height = page_height;
  page_layout.num_columns = num_columns;
  page_layout.left_margin = 36;
  page_layout.right_margin = 36;
  page_layout.gutter_width = gutter_width;
  page_layout.top_margin = 36;
  page_layout.bottom_margin = 18;
  page_layout.header_ypos = page_layout.top_margin;
  if (do_draw_header)
    page_layout.header_sep = font_scale * 2.5;
  else
    page_layout.header_sep = 0;
    
  page_layout.column_height = page_height
                            - page_layout.top_margin
                            - page_layout.header_sep
                            - page_layout.bottom_margin;
  page_layout.column_width =  (page_layout.page_width 
                            - page_layout.left_margin - page_layout.right_margin
                            - total_gutter_width) / page_layout.num_columns;
  page_layout.pt_to_pixel = paps_postscript_points_to_pango(1)/PANGO_SCALE;
  page_layout.pixel_to_pt = 1.0/page_layout.pt_to_pixel;
  page_layout.do_separation_line = TRUE;
  page_layout.do_landscape = do_landscape;
  page_layout.do_justify = do_justify;
  page_layout.do_tumble = do_tumble;
  page_layout.do_duplex = do_duplex;
  page_layout.pango_dir = pango_dir;
  
  text = read_file(IN);
  paragraphs = split_text_into_paragraphs(pango_context,
                                          &page_layout,
                                          page_layout.column_width * page_layout.pt_to_pixel,
                                          text);
  pango_lines = split_paragraphs_into_lines(paragraphs);
  
  if (OUT == NULL)
    OUT = stdout;

  print_postscript_header(OUT, title, &page_layout);
  ps_pages_string = g_string_new("");
  
  num_pages = output_pages(OUT, pango_lines, &page_layout); 

  paps_header = paps_get_postscript_header_strdup(paps);
  fprintf(OUT, "%s", paps_header);
  g_free(paps_header);

  fprintf(OUT, "%%%%EndPrologue\n");
  fprintf(OUT, "%s", ps_pages_string->str);
  print_postscript_trailer(OUT, num_pages);

  // Cleanup
  g_string_free(ps_pages_string, TRUE);

  return 0;
}


/* Read an entire file into a string
 */
static char *
read_file (FILE *file)
{
  GString *inbuf;
  char *text;
  char buffer[BUFSIZE];

  inbuf = g_string_new (NULL);
  while (1)
    {
      char *bp = fgets (buffer, BUFSIZE-1, file);
      if (ferror (file))
        {
          fprintf(stderr, "%s: Error reading file.\n", g_get_prgname ());
          g_string_free (inbuf, TRUE);
          return NULL;
        }
      else if (bp == NULL)
        break;

      g_string_append (inbuf, buffer);
    }

  fclose (file);

  text = inbuf->str;
  g_string_free (inbuf, FALSE);

  return text;
}

/* Take a UTF8 string and break it into paragraphs on \n characters
 */
static GList *
split_text_into_paragraphs (PangoContext *pango_context,
                            page_layout_t *page_layout,
                            int paint_width,  /* In pixels */
                            char *text)
{
  char *p = text;
  char *next;
  gunichar wc;
  GList *result = NULL;
  char *last_para = text;
  
  while (*p)
    {
      wc = g_utf8_get_char (p);
      next = g_utf8_next_char (p);
      if (wc == (gunichar)-1)
        {
          fprintf (stderr, "%s: Invalid character in input\n", g_get_prgname ());
          wc = 0;
        }
      if (!*p || !wc || wc == '\n')
        {
          Paragraph *para = g_new (Paragraph, 1);
          para->text = last_para;
          para->length = p - last_para;
          para->layout = pango_layout_new (pango_context);
          pango_layout_set_text (para->layout, para->text, para->length);
          pango_layout_set_justify (para->layout, page_layout->do_justify);
          pango_layout_set_alignment (para->layout,
                                      page_layout->pango_dir == PANGO_DIRECTION_LTR
                                      ? PANGO_ALIGN_LEFT : PANGO_ALIGN_RIGHT);
          pango_layout_set_width (para->layout, paint_width * PANGO_SCALE);
          para->height = 0;

          last_para = next;
            
          result = g_list_prepend (result, para);
        }
      if (!wc) /* incomplete character at end */
        break;
      p = next;
    }

  return g_list_reverse (result);
}

/* Split a list of paragraphs into a list of lines.
 */
GList *
split_paragraphs_into_lines(GList *paragraphs)
{
  GList *line_list = NULL;
  /* Read the file */

  /* Now split all the pagraphs into lines */
  GList *par_list;

  par_list = paragraphs;
  while(par_list)
    {
      int para_num_lines, i;
      Paragraph *para = par_list->data;

      para_num_lines = pango_layout_get_line_count(para->layout);

      for (i=0; i<para_num_lines; i++)
        {
          PangoRectangle logical_rect, ink_rect;
          LineLink *line_link = g_new(LineLink, 1);
          
          line_link->pango_line = pango_layout_get_line(para->layout, i);
          pango_layout_line_get_extents(line_link->pango_line,
                                        &ink_rect, &logical_rect);
          line_link->logical_rect = logical_rect;
          line_link->ink_rect = ink_rect;
          line_list = g_list_prepend(line_list, line_link);
        }

      par_list = par_list->next;
    }

  return g_list_reverse(line_list);
  
}

int output_pages(FILE *OUT,
                 GList *pango_lines,
                 page_layout_t *page_layout)
{
  int column_idx = 0;
  int column_y_pos = 0;
  int page_idx = 1;
  int pango_column_height = page_layout->column_height * page_layout->pt_to_pixel * PANGO_SCALE;

  start_page(OUT, page_idx);

  while(pango_lines)
    {
      LineLink *line_link = pango_lines->data;
      PangoLayoutLine *line = line_link->pango_line;
      
      /* Check if we need to move to next column */
      if (column_y_pos + line_link->logical_rect.height
          >= pango_column_height)
        {
          column_idx++;
          column_y_pos = 0;
          if (column_idx == page_layout->num_columns)
            {
              column_idx = 0;
              eject_page(OUT);
              page_idx++;
              start_page(OUT, page_idx);
            }
          else
            eject_column(OUT,
                         page_layout,
                         column_idx
                         );
        }
      draw_line_to_page(OUT,
                        column_idx,
                        column_y_pos+line_link->logical_rect.height,
                        page_layout,
                        line);
      column_y_pos += line_link->logical_rect.height;
      
      pango_lines = pango_lines->next;
    }
  eject_page(OUT);
  return page_idx;
}

void print_postscript_header(FILE *OUT,
                             const char *title,
                             page_layout_t *page_layout)
{
  const char *bool_name[2] = { "false", "true" };
  const char *orientation_names[2] = { "Portrait", "Landscape" };
  int bodytop = page_layout->header_ypos + page_layout->header_sep;
  int orientation = page_layout->page_width > page_layout->page_height;
  
  fprintf(OUT,
          "%%!PS-Adobe-3.0\n"
          "%%%%Title: %s\n"
          "%%%%Creator: paps version 0.1 by Dov Grobgeld\n"
          "%%%%Pages: (atend)\n"
          "%%%%BeginProlog\n"
	  "%%%%Orientation: %s\n"
	  "/papsdict 1 dict def\n"
	  "papsdict begin\n"
          "\n"
          "/inch {72 mul} bind def\n"
          "/mm {1 inch 25.4 div mul} bind def\n"
          "\n"
          "%% override setpagedevice if it is not defined\n"
          "/setpagedevice where {\n"
          "    pop %% get rid of its dictionary\n"
          "    /setpagesize { \n"
          "       3 dict begin\n"
          "         /pageheight exch def \n"
          "         /pagewidth exch def\n"
	  "         /orientation 0 def\n"
          "         %% Exchange pagewidth and pageheight so that pagewidth is bigger\n"
          "         pagewidth pageheight gt {  \n"
          "             pagewidth\n"
          "             /pagewidth pageheight def\n"
          "             /pageheight exch def\n"
	  "             /orientation 3 def\n"
          "         } if\n"
          "         2 dict\n"
	  "         dup /PageSize [pagewidth pageheight] put\n"
	  "         dup /Orientation orientation put\n"
	  "         setpagedevice \n"
          "       end\n"
          "    } def\n"
          "}\n"
          "{\n"
          "    /setpagesize { pop pop } def\n"
          "} ifelse\n"
          "/duplex {\n"
          "    statusdict /setduplexmode known \n"
          "    { statusdict begin setduplexmode end } {pop} ifelse\n"
          "} def\n"
          "/tumble {\n"
          "    statusdict /settumble known\n"
          "   { statusdict begin settumble end } {pop} ifelse\n"
          "} def\n"
          "%% Turn the page around\n"
          "/turnpage {\n"
          "  90 rotate\n"
          "  0 pageheight neg translate\n"
          "} def\n",
          title,
	  orientation_names[orientation]
          );
  
  fprintf(OUT,
          "%% User settings\n"
          "/pagewidth %d def\n"
          "/pageheight %d def\n"
          "/column_width %d def\n"
          
          "/bodyheight %d def\n"
          "/lmarg %d def\n"
          
          "/ytop %d def\n"
          "/do_separation_line %s def\n"
          "/do_landscape %s def\n"
          
          "/do_tumble %s def\n"
          "/do_duplex %s def\n",
          page_layout->page_width,
          page_layout->page_height,
          page_layout->column_width,
          page_layout->column_height,
          page_layout->left_margin,
          page_layout->page_height - bodytop,
          bool_name[page_layout->do_separation_line>0],
          bool_name[page_layout->do_landscape>0],
          bool_name[page_layout->do_tumble>0],
          bool_name[page_layout->do_duplex>0]
          );
  
  fprintf(OUT,
          "%% Procedures to translate position to first and second column\n"
          "/lw 20 def %% whatever\n"
          "/setnumcolumns {\n"
          "    /numcolumns exch def\n"
          "    /firstcolumn { /xpos lmarg def /ypos ytop def} def\n"
          "    /nextcolumn { \n"
          "      do_separation_line {\n"
          "          xpos column_width add gutter_width 2 div add %% x start\n"
          "           ytop lw add moveto              %% y start\n"
          "          0 bodyheight lw add neg rlineto 0 setlinewidth stroke\n"
          "      } if\n"
          "      /xpos xpos column_width add gutter_width add def \n"
          "      /ypos ytop def\n"
          "    } def\n"
          "} def\n"
          "\n"
          );
  
  fprintf(OUT,
	  "pagewidth pageheight setpagesize\n"
          "%d setnumcolumns\n",
          page_layout->num_columns);
  fprintf(OUT,
          "/showline {\n"
          "    /y exch def\n"
          "    /s exch def\n"
          "    xpos y moveto \n"
          "    column_width 0 rlineto stroke\n"
          "    xpos y moveto /Helvetica findfont 20 scalefont setfont s show\n"
          "} def\n"
          );

  // The following definitions polute the global namespace. All such
  // definitions should start with paps_
  fprintf(OUT,
          "/paps_bop {  %% Beginning of page definitions\n"
	  "    papsdict begin\n"
          "    gsave\n"
          "    do_landscape {turnpage} if \n"
          "    firstcolumn\n"
	  "    end\n"
          "} def\n"
          "\n"
          "/paps_eop {  %% End of page cleanups\n"
          "    grestore    \n"
          "} def\n");

}

void print_postscript_trailer(FILE *OUT,
                             int num_pages)
{
  fprintf(OUT,
          "%%%%Pages: %d\n"
          "%%%%Trailer\n"
          "%%%%EOF\n",
          num_pages
          );
}

void eject_column(FILE *OUT,
                  page_layout_t *page_layout,
                  int column_idx)
{
  double x_pos, y_top, y_bot, total_gutter;

#if 0
  fprintf(stderr, "do_separation_line column_idx = %d %d\n", page_layout->do_separation_line, column_idx);
#endif
  if (!page_layout->do_separation_line)
    return;

  if (page_layout->pango_dir == PANGO_DIRECTION_RTL)
    column_idx = (page_layout->num_columns - column_idx);

  if (column_idx == 1)
    total_gutter = 1.0 * page_layout->gutter_width /2;
  else
    total_gutter = (column_idx + 1.5) * page_layout->gutter_width;
      
  x_pos = page_layout->left_margin
        + page_layout->column_width * column_idx
      + total_gutter;

  y_top = page_layout->page_height - page_layout->top_margin;
  y_bot = page_layout->bottom_margin;

  g_string_append_printf(ps_pages_string,
                         "%f %f moveto %f %f lineto 0 setlinewidth stroke\n",
                         x_pos, y_top,
                         x_pos, y_bot);
}

void eject_page(FILE *OUT)
{
  g_string_append_printf(ps_pages_string,
                         "paps_eop\n"
                         "showpage\n");
}

void start_page(FILE *OUT,
                int page_idx)
{
  g_string_append_printf(ps_pages_string,
                         "%%%%Page: %d %d\n"
                         "paps_bop\n",
                         page_idx, page_idx);
}

void
draw_line_to_page(FILE *OUT,
                  int column_idx,
                  int column_pos,
                  page_layout_t *page_layout,
                  PangoLayoutLine *line)
{
  /* Assume square aspect ratio for now */
  double y_pos = page_layout->page_height
               - page_layout->top_margin
               - page_layout->header_sep
               - column_pos / PANGO_SCALE * page_layout->pixel_to_pt;
  double x_pos = page_layout->left_margin
               + column_idx * (page_layout->column_width
                               + page_layout->gutter_width);
  gchar *ps_layout;

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
      x_pos += page_layout->column_width  - logical_rect.width / (page_layout->pt_to_pixel * PANGO_SCALE);
  }

  ps_layout = paps_layout_line_to_postscript_strdup(paps,
						    x_pos, y_pos,
						    line);

  g_string_append(ps_pages_string,
		  ps_layout);
  g_free(ps_layout);
}

