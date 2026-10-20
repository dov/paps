#pragma once
#include <string>
#include <vector>
#include <pango/pangocairo.h>
#include "paragraph_markup.h"

// Make `context` draw the images attached with attach_inline_images().
void inline_images_init(PangoContext *context);

// Give each OBJECT_REPLACEMENT character in the text of `layout` the next
// image of `images`, as a Pango shape. Relative file names are looked up in
// `base_dir` (the current directory when empty). Images larger than
// max_width x max_height points are scaled down. Problems are reported on
// stderr and the image is left out.
void attach_inline_images(PangoLayout *layout,
                          const std::vector<ImageSpec> &images,
                          const std::string &base_dir,
                          double max_width,
                          double max_height);

// Release the loaded images.
void inline_images_free();
