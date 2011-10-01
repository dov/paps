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
 */


#include "libpaps.h"

#include <pango/pango.h>
#include <pango/pangoft2.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* The following def should be provided by pango! */
#ifndef PANGO_GLYPH_EMPTY
#define PANGO_GLYPH_EMPTY ((PangoGlyph)0x0FFFFFFF)
#endif

// The dpi is not used as dpi but only determines the number of significant
// digits used in the definition of the outlines. In the PostScript all
// distances are scaled and only integer values are given for compactness.
// By decreasing this value the PostScript becomes smaller. But I believe
// there are better ways to compact the postscript...
#define PAPS_DPI 1000
#define FT2PS (72.0/64.0)

typedef struct {
  GString *header;
  GHashTable *glyph_cache;
  int last_char_idx;
  double last_pos_y;
  double last_pos_x;
  double scale_x;
  double scale_y;
} paps_private_t;


// Forward declarations
static void add_postscript_prologue(paps_private_t *paps);
static gchar *get_next_char_id_strdup(paps_private_t *paps);
static void add_line_to_postscript(paps_private_t *paps,
				   GString *line_str,
				   double x_pos,
				   double y_pos,
				   PangoLayoutLine *line);

paps_t *paps_new()
{
    paps_private_t *paps = g_new0(paps_private_t, 1);
    paps->header = g_string_new("");
    paps->glyph_cache = g_hash_table_new_full(g_str_hash,
					      g_str_equal,
					      g_free,
					      g_free);
    // Make sure the cache will be invalid...
    paps->last_pos_x = -1e67;
    paps->last_pos_y = -1e67;
    paps->last_char_idx = 0;
    paps->scale_x = 1.0;
    paps->scale_y = 1.0;

    add_postscript_prologue(paps);

    return paps;
}

void
paps_set_scale(paps_t  *paps_,
	       gdouble  scale_x,
	       gdouble  scale_y)
{
    paps_private_t *paps = (paps_private_t *)paps_;

    paps->scale_x = scale_x;
    paps->scale_y = scale_y;
#if 0
    // Why would you want to do such a thing???
    g_string_erase(paps->header, 0, -1);
    add_postscript_prologue(paps);
#endif
}

PangoContext *paps_get_pango_context()
{
  return pango_ft2_get_context (PAPS_DPI, PAPS_DPI);
}

gchar *paps_get_postscript_header_strdup(paps_t *paps_)
{
    paps_private_t *paps = (paps_private_t*)paps_;

    /* Add end of header string, create a strdup, and then erase the
       end of line. */
    int old_len = paps->header->len;
    gchar *ret_str;
    g_string_append_printf(paps->header,
			   "end end\n"
			   "%%%%EndPrologue\n"
			   );
    ret_str = g_strdup(paps->header->str);
    g_string_truncate(paps->header, old_len);

    return ret_str;
}

typedef struct {
  PangoLayoutLine *pango_line;
  PangoRectangle logical_rect;
  PangoRectangle ink_rect;
} LineLink;

/* Information passed in user data when drawing outlines */
typedef struct _OutlineInfo OutlineInfo;
struct _OutlineInfo {
  paps_private_t *paps;
  GString *out_string;
  FT_Vector glyph_origin;
  int dpi;
  int is_empty; // Flag for optimization 
};

static void draw_contour(paps_private_t *paps,
			 GString *line_str,
			 PangoLayoutLine *pango_line,
			 double line_start_pos_x,
			 double line_start_pos_y
			 );
void draw_bezier_outline(paps_private_t *paps,
			 GString *layout_str,
                         FT_Face face,
                         PangoGlyphInfo *glyph_info,
                         double pos_x,
                         double pos_y
                         );
/* Countour traveling functions */
static int paps_ps_move_to( const FT_Vector* to,
                            void *user_data);
static int paps_ps_line_to( const FT_Vector*  to,
                            void *user_data);
static int paps_ps_conic_to( const FT_Vector*  control,
                             const FT_Vector*  to,
                             void *user_data);
static int paps_ps_cubic_to( const FT_Vector*  control1,
                             const FT_Vector*  control2,
                             const FT_Vector*  to,
                             void *user_data);
static void get_glyph_hash_string(FT_Face face,
                                  PangoGlyphInfo *glyph_info,
                                  // output
                                  gchar *hash_string
                                  );

// Fonts are three character symbols in an alphabet composing of
// the following characters:
//
//   First character:  a-zA-Z@_.,!-~`'"
//   Rest of chars:    like first + 0-9
//
// Care is taken that no keywords are overwritten, e.g. def, end.

#define CASE(s) if (strcmp(S_, s) == 0)

/* Split a list of paragraphs into a list of lines.
 */
gchar *paps_layout_to_postscript_strdup(paps_t *paps_,
					double pos_x,
					double pos_y,
					PangoLayout *layout)
{
  paps_private_t *paps = (paps_private_t*)paps_;
  GString *layout_str = g_string_new("");
  gchar *ret_str;
  int para_num_lines, line_idx;
  double scale = 72.0 / PANGO_SCALE  / PAPS_DPI;

  para_num_lines = pango_layout_get_line_count(layout);

  for (line_idx=0; line_idx<para_num_lines; line_idx++)
    {
      PangoRectangle logical_rect, ink_rect;
      PangoLayoutLine *pango_line = pango_layout_get_line(layout, line_idx);

      pango_layout_line_get_extents(pango_line,
				    &ink_rect, &logical_rect);

      
      add_line_to_postscript(paps,
			     layout_str,
			     pos_x,
			     pos_y,
			     pango_line);

      pos_y -= logical_rect.height * scale;
    }

  ret_str = layout_str->str;
  g_string_free(layout_str, FALSE);

  return ret_str;
}

gchar *paps_layout_line_to_postscript_strdup(paps_t *paps_,
					     double pos_x,
					     double pos_y,
					     PangoLayoutLine *layout_line)
{
  paps_private_t *paps = (paps_private_t*)paps_;
  GString *layout_str = g_string_new("");
  gchar *ret_str;

  add_line_to_postscript(paps,
			 layout_str,
			 pos_x,
			 pos_y,
			 layout_line);

  ret_str = layout_str->str;
  g_string_free(layout_str, FALSE);

  return ret_str;
}

static void
add_postscript_prologue(paps_private_t *paps)
{
  g_string_append_printf(paps->header,
			 "%%%%BeginProlog\n"
			 "/papsdict 1 dict def\n"
			 "papsdict begin\n"
			 "\n"
			 );
  
  /* Outline support */
  g_string_append_printf(paps->header,
			 "/conicto {\n"
			 "    /to_y exch def\n"
			 "    /to_x exch def\n"
			 "    /conic_cntrl_y exch def\n"
			 "    /conic_cntrl_x exch def\n"
			 "    currentpoint\n"
			 "    /p0_y exch def\n"
			 "    /p0_x exch def\n"
			 "    /p1_x p0_x conic_cntrl_x p0_x sub 2 3 div mul add def\n"
			 "    /p1_y p0_y conic_cntrl_y p0_y sub 2 3 div mul add def\n"
			 "    /p2_x p1_x to_x p0_x sub 1 3 div mul add def\n"
			 "    /p2_y p1_y to_y p0_y sub 1 3 div mul add def\n"
			 "    p1_x p1_y p2_x p2_y to_x to_y curveto\n"
			 "} bind def\n"
			 "/start_ol { gsave } bind def\n"
			 "/end_ol { closepath fill grestore } bind def\n"
			 /* Specify both x and y. */
			 "/draw_char { fontdict begin gsave %f dup scale last_x last_y translate load exec end grestore} def\n"
			 "/goto_xy { fontdict begin /last_y exch def /last_x exch def end } def\n"
			 "/goto_x { fontdict begin /last_x exch def end } def\n"
			 "/fwd_x { fontdict begin /last_x exch last_x add def end } def\n"
			 "/c /curveto load def\n"
			 "/x /conicto load def\n"
			 "/l /lineto load def\n"
			 "/m /moveto load def\n"
			 "end\n",
			 // The scaling is a combination of the scaling due
			 // to the dpi and the difference in the coordinate
			 // systems of postscript and freetype2.
			 1.0 / PAPS_DPI
			 );

  // The following is a dispatcher for an encoded string that contains
  // a packed version of the pango layout data. Currently it just executes
  // the symbols corresponding to the encoded characters, but in the future
  // it will also contain some meta data, e.g. the size of the layout.
  g_string_append_printf(paps->header,
			 "/paps_exec {\n"
			 "  1 dict begin\n"
			 "  /ps exch def\n"
			 "  /len ps length def\n"
			 "  /pos 0 def\n"
			 "\n"
			 "  %% Loop over all the characters of the string\n"
			 "  {\n"
			 "    pos len eq {exit} if\n"
			 "\n"
			 "    %% Get character at pos\n"
			 "    /ch ps pos 1 getinterval def\n"
			 "    \n"
			 "    %% check for +\n"
			 "    (+) ch eq {\n"
			 "      /pos 1 pos add def\n"
			 "      /xp ps pos 8 getinterval cvi def\n"
			 "      /yp ps pos 8 add 8 getinterval cvi def\n"
			 "      /pos 16 pos add def\n"
			 "      papsdict begin xp yp goto_xy end\n"
			 "    } {\n"
			 "      (*) ch eq {\n"
			 "        /pos 1 pos add def\n"
			 "        /xp ps pos 8 getinterval cvi def\n"
			 "        /pos 8 pos add def\n"
			 "        papsdict begin xp goto_x end\n"
			 "      } { (>) ch eq {\n"
			 "          /pos 1 pos add def\n"
			 "          /xp ps pos 4 getinterval cvi def\n"
			 "          /pos 4 pos add def\n"
			 "          papsdict begin xp 2 mul fwd_x end\n"
			 "        } { (-) ch eq {\n"
			 "            /pos 1 pos add def\n"
			 "            /xp ps pos 4 getinterval cvi def\n"
			 "            /pos 4 pos add def\n"
			 "            papsdict begin xp neg 2 mul fwd_x end\n"
			 "          } {\n"
			 "              %% Must be a 3 char sym. Load and exec\n"
			 "              /name ps pos 3 getinterval def\n"
			 "              papsdict begin name draw_char end\n"
			 "              /pos 3 pos add def\n"
			 "            } ifelse\n"
			 "          } ifelse\n"
			 "        } ifelse\n"
			 "    } ifelse\n"
			 "  } loop\n"
			 "  end\n"
			 "} def\n"
			 );

  /* Open up dictionaries */
  g_string_append(paps->header,
		  "/fontdict 1 dict def\n"
		  "papsdict begin fontdict begin\n");
}


static void
add_line_to_postscript(paps_private_t *paps,
		       GString *line_str,
		       double x_pos,
		       double y_pos,
		       PangoLayoutLine *line)
{
  PangoRectangle ink_rect, logical_rect;

  pango_layout_line_get_extents(line,
                                &ink_rect,
                                &logical_rect);




#if 0
  // TBD - Handle RTL scripts
  if (paps->pango_dir == PANGO_DIRECTION_RTL) {
      x_pos += page_layout->column_width  - logical_rect.width / (page_layout->pt_to_pixel * PANGO_SCALE);
  }
#endif

  draw_contour(paps, line_str, line, x_pos, y_pos);
}

/* draw_contour() draws all of the contours that make up a line.
   to access the ft font information out of the pango font info.
 */
static void draw_contour(paps_private_t *paps,
			 GString *layout_str,
			 PangoLayoutLine *pango_line,
			 double line_start_pos_x,
			 double line_start_pos_y
			 )
{
  GSList *runs_list;
  double scale = 72.0 / PANGO_SCALE  / PAPS_DPI; 

  g_string_append(layout_str, "(");
  
  /* Loop over the runs and output font info */
  runs_list = pango_line->runs;
  double x_pos = line_start_pos_x;
  while(runs_list)
    {
      PangoLayoutRun *run = runs_list->data;
      PangoItem *item = run->item;
      PangoGlyphString *glyphs = run->glyphs;
      PangoAnalysis *analysis = &item->analysis;
      PangoFont *font = analysis->font;
      FT_Face ft_face = pango_ft2_font_get_face(font);
      int num_glyphs = glyphs->num_glyphs;
      int glyph_idx;
      
      for (glyph_idx=0; glyph_idx<num_glyphs; glyph_idx++)
        {
          PangoGlyphGeometry geometry = glyphs->glyphs[glyph_idx].geometry;
          double glyph_pos_x, glyph_pos_y;

          glyph_pos_x = x_pos + 1.0*geometry.x_offset * scale;
          glyph_pos_y = line_start_pos_y - 1.0*geometry.y_offset * scale;

	  x_pos += geometry.width * scale * paps->scale_x;

          if (glyphs->glyphs[glyph_idx].glyph == PANGO_GLYPH_EMPTY)
            continue;

	  draw_bezier_outline(paps,
			      layout_str,
			      ft_face,
			      &glyphs->glyphs[glyph_idx],
			      glyph_pos_x,
			      glyph_pos_y);
        }

      runs_list = runs_list->next;
    }
  
  g_string_append(layout_str, ")paps_exec\n");
  
}

void draw_bezier_outline(paps_private_t *paps,
			 GString *layout_str,
                         FT_Face face,
                         PangoGlyphInfo *glyph_info,
                         double pos_x,
                         double pos_y)
{
  static gchar glyph_hash_string[100];
  double scale = 72.0 / PANGO_SCALE  / PAPS_DPI;
  double epsilon = 1e-2;
  double glyph_width = glyph_info->geometry.width * scale * paps->scale_x;
  gchar *id = NULL;

  /* Output outline */
  static FT_Outline_Funcs ps_outlinefunc = 
    {
      (FT_Outline_MoveToFunc)paps_ps_move_to,
      (FT_Outline_LineToFunc )paps_ps_line_to,
      (FT_Outline_ConicToFunc)paps_ps_conic_to,
      (FT_Outline_CubicToFunc)paps_ps_cubic_to
    };
  FT_Outline_Funcs *outlinefunc;
  OutlineInfo outline_info;

  get_glyph_hash_string(face,
                        glyph_info,
                        // output
                        glyph_hash_string);

  // Look up the key in the hash table
  if (!(id = g_hash_table_lookup(paps->glyph_cache,
                                 glyph_hash_string)))
    {
      FT_Int load_flags = FT_LOAD_DEFAULT;
      FT_Glyph glyph;
      GString *glyph_def_string;
      FT_Error ft_ret;

      ft_ret = FT_Load_Glyph(face, glyph_info->glyph, load_flags);

      if (ft_ret != 0
          // Sorry - No support for bitmap glyphs at the moment. :-(
          || face->glyph->format == FT_GLYPH_FORMAT_BITMAP)
        ;
      else
        {
          glyph_def_string = g_string_new("");
          
          // The key doesn't exist. Define the outline
          id = get_next_char_id_strdup(paps);
          
          // Create the outline
          outlinefunc = &ps_outlinefunc;
          outline_info.paps = paps;
          outline_info.glyph_origin.x = pos_x;
          outline_info.is_empty = 1;
          outline_info.glyph_origin.y = pos_y;
          outline_info.out_string = glyph_def_string;
          
          
          g_string_append(glyph_def_string,
                          "start_ol\n");
          
          FT_Get_Glyph (face->glyph, &glyph);
          FT_Outline_Decompose (&(((FT_OutlineGlyph)glyph)->outline),
                                outlinefunc, &outline_info);
          
          g_string_append_printf(glyph_def_string,
                                 "%.0f fwd_x\n"
                                 "end_ol\n",
                                 glyph_info->geometry.width * scale * paps->scale_x * PAPS_DPI
                                 );
          
          // TBD - Check if the glyph_def_string is empty. If so, set the
          // id to the character to "" and don't draw it.
          if (outline_info.is_empty
              || glyph_info->glyph == 0) 
            id[0] = '*';
          else
            // Put the font in the font def dictionary
            g_string_append_printf(paps->header,
                                   "/%s { %s } def\n",
                                   id,
                                   glyph_def_string->str);
          
          g_hash_table_insert(paps->glyph_cache,
                              g_strdup(glyph_hash_string),
                              id);
          
          g_string_free(glyph_def_string, TRUE);
          FT_Done_Glyph (glyph);
        }
    }

  if (id && id[0] != '*')
    {
      glyph_width *= PAPS_DPI;
      pos_x *=PAPS_DPI;
      pos_y *= PAPS_DPI;

      // Output codes according to the ps_exec string encoding!
      if (fabs (pos_y - paps->last_pos_y ) > epsilon)
	{
	  g_string_append_printf(layout_str,
				 "+%8.0f%8.0f",
				 pos_x, pos_y
				 );
	}
      else if (fabs(pos_x - paps->last_pos_x) > epsilon)
	{
	  int dx = (int)(pos_x - paps->last_pos_x+0.5);

	  // The extra 2 factor makes the small dx encoding trigger
	  // more often...
	  if (dx > 0 && dx < 20000) 
	    g_string_append_printf(layout_str,
				   ">%4d", dx/2
				   );
	  else if (dx < 0 && dx > -20000) 
	    g_string_append_printf(layout_str,
				   "-%4d", abs(dx)/2
				   );
	  else if (dx != 0)
	    g_string_append_printf(layout_str,
				   "*%8.0f", pos_x
				   );
	}
      
      // Put character on page
      g_string_append_printf(layout_str,
			     "%s",
			     id
			     );

    }

  paps->last_pos_y = pos_y;
  paps->last_pos_x = pos_x+glyph_width; // glyph_with is added by the outline def
}

/*======================================================================
//  outline traversing functions.
//----------------------------------------------------------------------*/
static int paps_ps_move_to( const FT_Vector* to,
                            void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  g_string_append_printf(outline_info->out_string,
                         "%d %d m\n",
                         (int)(to->x * FT2PS*outline_info->paps->scale_x) ,
                         (int)(to->y * FT2PS*outline_info->paps->scale_y ));
  return 0;
}

static int paps_ps_line_to( const FT_Vector*  to,
                            void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  g_string_append_printf(outline_info->out_string,
                         "%d %d l\n",
                         (int)(to->x * FT2PS * outline_info->paps->scale_x) ,
                         (int)(to->y * FT2PS * outline_info->paps->scale_y) );
  outline_info->is_empty = 0;
  return 0;
}

static int paps_ps_conic_to( const FT_Vector*  control,
                             const FT_Vector*  to,
                             void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  g_string_append_printf(outline_info->out_string,
                         "%d %d %d %d x\n",
                         (int)(control->x * FT2PS*outline_info->paps->scale_x)  ,
                         (int)(control->y * FT2PS*outline_info->paps->scale_y) ,
                         (int)(to->x * FT2PS*outline_info->paps->scale_x),
                         (int)(to->y * FT2PS*outline_info->paps->scale_y));
  outline_info->is_empty = 0;
  return 0;
}

static int paps_ps_cubic_to( const FT_Vector*  control1,
                             const FT_Vector*  control2,
                             const FT_Vector*  to,
                             void *user_data)
{
  OutlineInfo *outline_info = (OutlineInfo*)user_data;
  g_string_append_printf(outline_info->out_string,
                         "%d %d %d %d %d %d c\n",
                         (int)(control1->x * FT2PS*outline_info->paps->scale_x) , 
                         (int)(control1->y * FT2PS*outline_info->paps->scale_y) ,
                         (int)(control2->x * FT2PS*outline_info->paps->scale_x) ,
                         (int)(control2->y * FT2PS*outline_info->paps->scale_y) ,
                         (int)(to->x * FT2PS*outline_info->paps->scale_x) ,
                         (int)(to->y * FT2PS*outline_info->paps->scale_y) );
  outline_info->is_empty = 0;
  return 0;
}

static gchar *get_next_char_id_strdup(paps_private_t *paps)
{
  int i;
  int index = paps->last_char_idx++;
  gchar *char_id = g_new0(gchar, 4);
  
  // Just use a-zA-Z for the three chars
  for (i=0; i<3; i++)
    {
      int p = index % 52;
      index /= 52;

      if (p < 26)
        p += 'A';
      else
        p += 'a'-26;

      char_id[i] = p;
    }
  char_id[3] = 0;
  return char_id;
}

static void get_glyph_hash_string(FT_Face face,
                                  PangoGlyphInfo *glyph_info,
                                  // output
                                  gchar *hash_string
                                  )
{
  sprintf(hash_string, "%08x-%d-%d",
	  (unsigned int)face,
	  glyph_info->glyph,
	  glyph_info->geometry.width
	  );
}

double paps_postscript_points_to_pango(double points)
{
  return points * 1.0 / 72 * PAPS_DPI * PANGO_SCALE;
}

