/*======================================================================
//  test-paps.c - Testing the paps library.
//
//  Dov Grobgeld <dov.weizmann@weizmann.ac.il>
//  Tue Nov  8 21:50:31 2005
//----------------------------------------------------------------------
*/

#include <pango/pango.h>
#include <pango/pangoft2.h>
#include <stdio.h>
#include "libpaps.h"

int main(int argc, char*argv[])
{
  paps_t *paps = paps_new();
  gchar *header, *ps_layout;
  PangoContext *pango_context;
  PangoFontDescription *font_description;
  PangoDirection pango_dir = PANGO_DIRECTION_LTR;
  PangoLayout *layout;
  gchar *font_family = "sans";
  int font_scale = 14;
  GString *ps_text = g_string_new("");
  
  pango_context = paps_get_pango_context();
  
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

  // Create a layout and set its properties
  layout = pango_layout_new (pango_context);
  pango_layout_set_justify (layout, TRUE);
  pango_layout_set_width (layout, paps_postscript_points_to_pango(5*72));
  
  pango_layout_set_text (layout,
			 "It was the best of times, it was the worst of times, "
			 "it was the age of wisdom, it was the age of foolishness, "
			 "it was the epoch of belief, it was the epoch of incredulity, "
			 "it was the season of Light, it was the season of Darkness, "
			 "it was the spring of hope, it was the winter of despair, "
			 "we had everything before us, we had nothing before us, "
			 "we were all going direct to Heaven, we were all going direct "
			 "the other way--in short, the period was so far like the present "
			 "period, that some of its noisiest authorities insisted on its "
			 "being received, for good or for evil, in the superlative degree "
			 "of comparison only. "
			 "\n\n"
			 "There were a king with a large jaw and a queen with a plain face, "
			 "on the throne of England; there were a king with a large jaw and "
			 "a queen with a fair face, on the throne of France.  In both "
			 "countries it was clearer than crystal to the lords of the State "
			 "preserves of loaves and fishes, that things in general were "
			 "settled for ever. "
			 "\n\n"
			 "It was the year of Our Lord one thousand seven hundred and "
			 "seventy-five.  Spiritual revelations were conceded to England at "
			 "that favoured period, as at this.  Mrs. Southcott had recently "
			 "attained her five-and-twentieth blessed birthday, of whom a "
			 "prophetic private in the Life Guards had heralded the sublime "
			 "appearance by announcing that arrangements were made for the "
			 "swallowing up of London and Westminster.  Even the Cock-lane "
			 "ghost had been laid only a round dozen of years, after rapping "
			 "out its messages, as the spirits of this very year last past "
			 "(supernaturally deficient in originality) rapped out theirs. "
			 "Mere messages in the earthly order of events had lately come to "
			 "the English Crown and People, from a congress of British subjects "
			 "in America:  which, strange to relate, have proved more important "
			 "to the human race than any communications yet received through "
			 "any of the chickens of the Cock-lane brood. "
			 , -1);

  ps_layout = paps_layout_to_postscript_strdup(paps,
					       0, 0,
					       layout);
  g_string_append_printf(ps_text,
			 "gsave\n"
			 "1 0.8 0.8 setrgbcolor\n"
			 "72 72 10 mul translate\n"
			 "3 rotate %s\n"
			 "grestore\n",
			 ps_layout);
  g_free(ps_layout);

  pango_font_description_set_size (font_description, 25 * PANGO_SCALE);
  pango_context_set_font_description (pango_context, font_description);
  pango_layout_set_width (layout, paps_postscript_points_to_pango(7*72));
  pango_layout_set_text (layout,
			 "paps by Dov Grobgeld (\327\223\327\221 \327\222\327\250\327\225\327\221\327\222\327\234\327\223\n"
			 "Printing through \316\240\316\261\316\275\350\252\236 (Pango)\n"
			 "\n"
			 "Arabic \330\247\331\204\330\263\331\204\330\247\330\271\331\204\331\212\331\203\331\205\n"
			 "Hebrew \327\251\327\201\326\270\327\234\327\225\326\271\327\235\n"
			 "Greek (\316\225\316\273\316\273\316\267\316\275\316\271\316\272\316\254)  \316\223\316\265\316\271\316\254 \317\203\316\261\317\202\n"
			 "Japanese  (\346\227\245\346\234\254\350\252\236) \343\201\223\343\202\223\343\201\253\343\201\241\343\201\257, \357\275\272\357\276\235\357\276\206\357\276\201\357\276\212\n"
			 "Chinese  (\344\270\255\346\226\207, \346\231\256\351\200\232\350\257\235,\346\261\211\350\257\255)     \344\275\240\345\245\275\n"
			 "Vietnamese	(Ti\341\272\277ng Vi\341\273\207t)	Xin Ch\303\240o\n",
			 -1
			 );
  ps_layout = paps_layout_to_postscript_strdup(paps,
					       0, 0,
					       layout);
  g_string_append_printf(ps_text,
			 "gsave\n"
			 "0.2 0.2 1.0 setrgbcolor\n"
			 "100 700 translate\n"
			 "-10 rotate %s\n"
			 "grestore\n",
			 ps_layout);
  g_string_append_printf(ps_text,
			 "gsave\n"
			 "0 0.5 0.1 setrgbcolor\n"
			 "500 300 translate\n"
			 "-5 rotate -0.5 1 scale %s\n"
			 "grestore\n",
			 ps_layout);
  g_free(ps_layout);


  header = paps_get_postscript_header_strdup(paps);
  printf("%s",header);
  g_free(header);

  printf("%s", ps_text->str);

  return 0;
}
