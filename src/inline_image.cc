#include <cstdio>
#include <map>
#include <glib.h>
#include "inline_image.h"

namespace {

// Loaded images by path. A null surface is an image that failed to load.
std::map<std::string, cairo_surface_t *> image_cache;

cairo_surface_t *
load_image(const std::string &path)
{
  auto it = image_cache.find(path);
  if (it != image_cache.end())
    return it->second;

  cairo_surface_t *surface = cairo_image_surface_create_from_png(path.c_str());
  if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    {
      fprintf(stderr, "%s: Failed to load %s as PNG: %s\n", g_get_prgname(),
              path.c_str(), cairo_status_to_string(cairo_surface_status(surface)));
      cairo_surface_destroy(surface);
      surface = nullptr;
    }
  image_cache[path] = surface;
  return surface;
}

void *
copy_image(gconstpointer data)
{
  return (void *)data;
}

void
render_shape(cairo_t *cr, PangoAttrShape *attr, gboolean do_path, gpointer)
{
  // The data is a surface owned by the cache, or null for a missing image
  cairo_surface_t *surface = (cairo_surface_t *)attr->data;
  double x, y;
  cairo_get_current_point(cr, &x, &y);
  // The ink rectangle is the image, relative to the baseline origin
  double left = x + (double)attr->ink_rect.x / PANGO_SCALE;
  double top = y + (double)attr->ink_rect.y / PANGO_SCALE;
  double w = (double)attr->ink_rect.width / PANGO_SCALE;
  double h = (double)attr->ink_rect.height / PANGO_SCALE;

  if (do_path)
    {
      cairo_rectangle(cr, left, top, w, h);
      return;
    }
  if (!surface || w <= 0 || h <= 0)
    return;

  cairo_save(cr);
  cairo_translate(cr, left, top);
  cairo_scale(cr, w / cairo_image_surface_get_width(surface),
              h / cairo_image_surface_get_height(surface));
  cairo_set_source_surface(cr, surface, 0, 0);
  cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BEST);
  cairo_paint(cr);
  cairo_restore(cr);
  cairo_move_to(cr, x, y);
}

}  // namespace

void
inline_images_init(PangoContext *context)
{
  pango_cairo_context_set_shape_renderer(context, render_shape, nullptr, nullptr);
}

void
attach_inline_images(PangoLayout *layout,
                     const std::vector<ImageSpec> &images,
                     const std::string &base_dir,
                     double max_width,
                     double max_height)
{
  if (images.empty())
    return;

  const char *text = pango_layout_get_text(layout);

  // Lines are drawn with the baseline at the bottom of the line box, so the
  // descenders of the line above reach into this one. Keep the image clear
  // of them by leaving a font descent above it.
  PangoContext *context = pango_layout_get_context(layout);
  PangoFontMetrics *metrics = pango_context_get_metrics(
      context, pango_context_get_font_description(context),
      pango_context_get_language(context));
  int pad = pango_font_metrics_get_descent(metrics);
  pango_font_metrics_unref(metrics);
  PangoAttrList *old = pango_layout_get_attributes(layout);
  PangoAttrList *attrs = old ? pango_attr_list_copy(old) : pango_attr_list_new();

  size_t next = 0;
  for (const char *p = text; *p && next < images.size(); p = g_utf8_next_char(p))
    {
      if (g_utf8_get_char(p) != 0xFFFC)
        continue;
      const ImageSpec &spec = images[next++];

      std::string path = spec.src;
      if (!base_dir.empty() && !g_path_is_absolute(path.c_str()))
        path = base_dir + "/" + path;
      cairo_surface_t *surface = spec.src.empty() ? nullptr : load_image(path);

      double w = 0, h = 0;
      if (surface)
        {
          double iw = cairo_image_surface_get_width(surface);
          double ih = cairo_image_surface_get_height(surface);
          w = spec.width;
          h = spec.height;
          if (w <= 0 && h <= 0)
            w = iw, h = ih;       // one pixel is one point
          else if (w <= 0)
            w = h * iw / ih;
          else if (h <= 0)
            h = w * ih / iw;
          double scale = 1.0;
          if (max_width > 0 && w > max_width)
            scale = max_width / w;
          if (max_height > 0 && h * scale > max_height)
            scale = max_height / h;
          w *= scale;
          h *= scale;
        }

      // Even a missing image takes the place of the character, as an empty box
      PangoRectangle ink = {0, (int)(-h * PANGO_SCALE),
                            (int)(w * PANGO_SCALE), (int)(h * PANGO_SCALE)};
      PangoRectangle logical = ink;
      if (surface)
        {
          logical.y -= pad;
          logical.height += pad;
        }
      PangoAttribute *attr = pango_attr_shape_new_with_data(&ink, &logical, surface, copy_image, nullptr);
      attr->start_index = p - text;
      attr->end_index = g_utf8_next_char(p) - text;
      pango_attr_list_insert(attrs, attr);
    }

  // Setting a new list makes the layout recompute its lines
  pango_layout_set_attributes(layout, attrs);
  pango_attr_list_unref(attrs);
}

void
inline_images_free()
{
  for (auto &kv : image_cache)
    if (kv.second)
      cairo_surface_destroy(kv.second);
  image_cache.clear();
}
