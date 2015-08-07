// Copyright 2015 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice (including the next
// paragraph) shall be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <alloca.h>
#include <endian.h>
#include <stdio.h>
#include <string.h>

#include <libpng16/png.h>

#include "util/log.h"
#include "util/xalloc.h"

#include "cru_image.h"

typedef struct cru_png_image cru_png_image_t;

struct cru_png_image {
    cru_image_t image;

    char *filename;
    FILE *file;

    /// Value is one of PNG_COLOR_TYPE_*.
    uint8_t png_color_type;
    uint8_t png_bit_depth;

    struct {
        /// When mapping the cru_png_image for reading, we decode the file into
        /// this pixel array.
        uint8_t *pixels;

        /// Concrete type is cru_pixel_image_t.  The image's backing storage is
        /// `cru_png_image::map.pixels`.
        cru_image_t *pixel_image;

        /// Bitmask of `CRU_IMAGE_MAP_ACCESS_*`.
        uint32_t access;
    } map;
};

static VkFormat
choose_vk_format(uint8_t png_color_type, uint8_t png_bit_depth,
                 const char *debug_filename)
{
    switch (png_color_type) {
    case PNG_COLOR_TYPE_RGB_ALPHA:
    case PNG_COLOR_TYPE_RGB:
        switch (png_bit_depth) {
        case 8:
            return VK_FORMAT_R8G8B8A8_UNORM;
        default:
            goto fail;
        }
    case PNG_COLOR_TYPE_GRAY:
        switch (png_bit_depth) {
        case 8:
            return VK_FORMAT_R8_UNORM;
        default:
            goto fail;
        }
    default:
        goto fail;
    }

fail:
    loge("unsuppoted (png_color_type, png_bit_depth) = (%u, %u)",
             png_color_type, png_bit_depth);
    loge("in PNG file %s", debug_filename);
    return VK_FORMAT_UNDEFINED;
}

static bool
cru_png_image_read_file_info(FILE *file, const char *debug_filename,
                             uint8_t *out_png_color_type,
                             uint8_t *out_bit_depth,
                             uint32_t *out_width,
                             uint32_t *out_height)
{
    bool result = false;
    png_structp png_reader = NULL;
    png_infop png_info = NULL;

    assert(file != NULL);
    assert(debug_filename != NULL);

    // FINISHME: Error callbacks for libpng
    png_reader = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                       NULL, NULL, NULL);
    if (!png_reader)
        goto fail_create_png_reader;

    png_info = png_create_info_struct(png_reader);
    if (!png_info)
        goto fail_create_png_info;

    rewind(file);
    png_init_io(png_reader, file);
    png_read_info(png_reader, png_info);

    *out_png_color_type = png_get_color_type(png_reader, png_info);
    *out_bit_depth = png_get_bit_depth(png_reader, png_info);
    *out_width = png_get_image_width(png_reader, png_info);
    *out_height = png_get_image_height(png_reader, png_info);

    result = true;

fail_create_png_info:
    png_destroy_read_struct(&png_reader, &png_info, NULL);
fail_create_png_reader:
    return result;
}

static bool
copy_direct_from_png(cru_image_t *src, cru_image_t *dest)
{
    cru_png_image_t *png_image;

    bool result = false;
    png_structp png_reader = NULL;
    png_infop png_info = NULL;

    const uint32_t width = src->width;
    const uint32_t height = src->height;
    const uint32_t stride = width * src->format_info->cpp;
    uint8_t *dest_pixels = NULL;
    uint8_t *dest_rows[height];

    assert(src->format_info == dest->format_info);
    assert(src->type == CRU_IMAGE_TYPE_PNG);
    assert(src->width == dest->width);
    assert(src->height == dest->height);

    png_image = (cru_png_image_t *) src;

    assert(!dest->read_only);
    dest_pixels = dest->map_pixels(dest, CRU_IMAGE_MAP_ACCESS_WRITE);
    if (!dest_pixels)
        return false;

    for (uint32_t y = 0; y < height; ++y) {
        dest_rows[y] = dest_pixels + y * stride;
    }

    // FINISHME: Error callbacks for libpng
    png_reader = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                       NULL, NULL, NULL);
    if (!png_reader) {
        loge("failed to create png reader");
        goto fail_create_png_reader;
    }

    png_info = png_create_info_struct(png_reader);
    if (!png_info) {
        loge("failed to create png reader info");
        goto fail_create_png_info;
    }

    rewind(png_image->file);
    png_init_io(png_reader, png_image->file);
    png_read_info(png_reader, png_info);

    // Transform the file's pixel format to the crucible image's pixel format.
    switch (png_image->png_color_type) {
    case PNG_COLOR_TYPE_RGB:
    case PNG_COLOR_TYPE_GRAY:
        if (dest->format_info->has_alpha) {
            png_set_add_alpha(png_reader, UINT32_MAX, PNG_FILLER_AFTER);
        }
        break;
    case PNG_COLOR_TYPE_RGB_ALPHA:
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        if (!dest->format_info->has_alpha) {
            png_set_strip_alpha(png_reader);
        }
        break;
    default:
        log_internal_error("bad png color type %u",
                               png_image->png_color_type);
        break;
    }

    png_read_rows(png_reader, dest_rows, NULL, height);
    png_read_end(png_reader, NULL);

    result = true;

fail_create_png_info:

    png_destroy_read_struct(&png_reader, &png_info, NULL);
fail_create_png_reader:

    if (!dest->unmap_pixels(dest)) {
        loge("failed to unmap pixel image");
        abort();
    }

    return result;
}

static bool
copy_indirect_from_png(cru_image_t *src, cru_image_t *dest)
{
    void *tmp_pixels = NULL;
    cru_image_t *tmp_image = NULL;
    bool result = false;

    tmp_pixels = xmalloc(src->format_info->cpp * src->width * src->height);

    tmp_image = cru_image_from_pixels(tmp_pixels,
                                      src->format_info->format,
                                      src->width, src->height);
    if (!tmp_image)
        goto cleanup;

    if (!copy_direct_from_png(src, tmp_image))
        goto cleanup;

    if (!cru_image_copy(dest, tmp_image))
        goto cleanup;

    result = true;

cleanup:
    cru_image_release(tmp_image);
    free(tmp_pixels);

    return result;
}

bool
cru_png_image_copy_to_pixels(cru_image_t *src, cru_image_t *dest)
{
    if (src->format_info == dest->format_info) {
        return copy_direct_from_png(src, dest);
    } else {
        return copy_indirect_from_png(src, dest);
    }
}

static uint8_t *
cru_png_image_map_pixels(cru_image_t *image, uint32_t access)
{
    cru_png_image_t *png_image = (cru_png_image_t *) image;

    const uint32_t width = image->width;
    const uint32_t height = image->height;
    cru_image_t *pixel_image = NULL;
    void *pixels = NULL;

    assert(png_image->map.access == 0);
    assert(access != 0);

    if (access & CRU_IMAGE_MAP_ACCESS_WRITE) {
        loge("crucible png images are read-only; cannot image for writing");
        goto fail;
    }

    if (png_image->map.pixels) {
        // We've already mapped the image. Since cru_png_image is read-only, we
        // keep the map alive for the image's lifetime.
        return png_image->map.pixels;
    }

    pixels = xmalloc(image->format_info->cpp * width * height);
    pixel_image = cru_image_from_pixels(pixels, image->format_info->format,
                                        width, height);
    if (!pixel_image)
        goto fail;

    if (!cru_png_image_copy_to_pixels(&png_image->image, pixel_image))
        goto fail;

    png_image->map.access = access;
    png_image->map.pixels = pixels;
    png_image->map.pixel_image = pixel_image;

    return pixels;

fail:
    cru_image_release(pixel_image);
    free(pixels);
    return NULL;
}

static bool
cru_png_image_unmap_pixels(cru_image_t *image)
{
    cru_png_image_t *png_image = (cru_png_image_t *) image;

    // PNG images are always read-only.
    assert(!(png_image->map.access & CRU_IMAGE_MAP_ACCESS_WRITE));
    assert(png_image->map.pixel_image != NULL);
    png_image->map.access = 0;

    return true;
}

static void
cru_png_image_destroy(cru_image_t *image)
{
    cru_png_image_t *png_image = (cru_png_image_t *) image;

    if (!png_image)
        return;

    if (png_image->map.pixel_image)
        cru_image_release(png_image->map.pixel_image);

    assert(png_image->file >= 0);
    fclose(png_image->file);

    free(png_image->map.pixels);
    free(png_image->filename);
    free(png_image);
}

cru_image_t *
cru_png_image_load_file(const char *filename)
{
    char *abs_filename = NULL;
    FILE *file = NULL;
    uint8_t png_color_type; // PNG_COLOR_TYPE_*
    uint8_t png_bit_depth;
    uint32_t width, height;
    VkFormat format;

    abs_filename = cru_image_get_abspath(filename);
    if (!abs_filename)
        goto fail_filename;

    file = fopen(abs_filename, "rb");
    if (!file) {
        loge("failed to open file for reading: %s", abs_filename);
        goto fail_fopen;
    }

    if (!cru_png_image_read_file_info(file, filename,
                                      &png_color_type, &png_bit_depth,
                                      &width, &height)) {
        goto fail_read_file;
    }

    format = choose_vk_format(png_color_type, png_bit_depth, filename);
    if (!format)
        goto fail_format;

    cru_png_image_t *png_image = xzalloc(sizeof(*png_image));

    if (!cru_image_init(&png_image->image,
                        CRU_IMAGE_TYPE_PNG,
                        format, width, height,
                        /*read_only*/ true)) {
        goto fail_image_init;
    }

    png_image->image.destroy = cru_png_image_destroy;
    png_image->image.map_pixels = cru_png_image_map_pixels;
    png_image->image.unmap_pixels = cru_png_image_unmap_pixels;

    png_image->filename = abs_filename;
    png_image->file = file;
    png_image->png_color_type = png_color_type;
    png_image->map.access = 0;
    png_image->map.pixels = NULL;
    png_image->map.pixel_image = NULL;

    return &png_image->image;

fail_image_init:
    free(png_image);
fail_format:
fail_read_file:
    fclose(file);
fail_fopen:
fail_filename:
    return NULL;
}

static bool
write_direct_to_png(cru_image_t *image, const string_t *filename)
{
    bool result = false;
    char *abspath = NULL;
    const uint32_t src_width = image->width;
    const uint32_t src_height = image->height;
    const uint32_t src_stride = image->format_info->cpp * src_width;
    uint8_t *src_pixels = NULL;
    uint8_t *src_rows[src_height];

    uint8_t png_color_type;
    uint8_t png_bit_depth;

    FILE *f = NULL;
    png_structp png_writer = NULL;
    png_infop png_info = NULL;

    switch (image->format_info->format) {
    case VK_FORMAT_R8_UNORM:
        png_color_type = PNG_COLOR_TYPE_GRAY;
        png_bit_depth = 8;
        break;
    case VK_FORMAT_R8G8B8A8_UNORM:
        png_color_type = PNG_COLOR_TYPE_RGBA;
        png_bit_depth = 8;
        break;
    default:
        loge("cannot write %s to PNG file", image->format_info->name);
        return false;
    }

    if (!string_endswith_cstr(filename, ".png")) {
        loge("%s: filename does have '.png' extension: %s",
                 __func__, string_data(filename));
        return false;
    }

    abspath = cru_image_get_abspath(string_data(filename));
    if (!abspath)
        goto fail_get_abspath;

    src_pixels = image->map_pixels(image, CRU_IMAGE_MAP_ACCESS_READ);
    if (!src_pixels)
        goto fail_map_pixels;

    for (uint32_t y = 0; y < src_height; ++y) {
        src_rows[y] = src_pixels + y * src_stride;
    }

    f = fopen(abspath, "wb");
    if (!f) {
        loge("failed to open file for writing: %s", abspath);
        goto fail_fopen;
    }

    // FINISHME: Error callbacks for libpng
    png_writer = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                        NULL, NULL, NULL);
    if (!png_writer) {
        loge("failed to create png writer");
        goto fail_create_png_writer;
    }

    png_info = png_create_info_struct(png_writer);
    if (!png_info) {
        loge("failed to create png writer info");
        goto fail_create_png_info;
    }

    png_init_io(png_writer, f);

    // From man:libpng(3):
    //
    //     If you call png_set_IHDR(), the call must appear before any of the
    //     other png_set_*() functions, because they might require access to
    //     some of  the  IHDR  settings.   The  remaining png_set_*()
    //     functions can be called in any order.
    //
    png_set_IHDR(png_writer, png_info,
                 src_width, src_height,
                 png_bit_depth, png_color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_writer, png_info);
    png_set_rows(png_writer, png_info, src_rows);
    png_write_png(png_writer, png_info, PNG_TRANSFORM_IDENTITY, NULL);

    result = true;

fail_create_png_info:
    png_destroy_write_struct(&png_writer, &png_info);
fail_create_png_writer:
    fclose(f);
fail_fopen:
    // Ignore the result of unmap because no write-back occurs when unmapping
    // a read-only map.
    image->unmap_pixels(image);
fail_map_pixels:
    free(abspath);
fail_get_abspath:
    return result;
}

static bool
write_indirect_to_png(cru_image_t *image, const string_t *filename)
{
    VkFormat tmp_format;
    const cru_format_info_t *tmp_format_info;
    void *tmp_pixels = NULL;
    cru_image_t *tmp_image = NULL;
    bool result = false;

    switch (image->format_info->format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8_UNORM:
        tmp_format = image->format_info->format;
        break;
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_S8_UINT:
        tmp_format = VK_FORMAT_R8_UNORM;
        break;
    default:
        loge("cannot write %s to PNG", image->format_info->name);
        return false;
    }

    tmp_format_info = cru_format_get_info(tmp_format);
    if (!tmp_format_info) {
        loge("unknown VkFormat %d", tmp_format);
        return false;
    }

    tmp_pixels = xmalloc(tmp_format_info->cpp * image->width * image->height);

    tmp_image = cru_image_from_pixels(tmp_pixels,
                                      tmp_format_info->format,
                                      image->width, image->height);
    if (!tmp_image)
        goto cleanup;

    if (!cru_image_copy(tmp_image, image))
        goto cleanup;

    if (!write_direct_to_png(tmp_image, filename))
        goto cleanup;

    result = true;

cleanup:
    cru_image_release(tmp_image);
    free(tmp_pixels);

    return result;
}

bool
cru_png_image_write_file(cru_image_t *image, const string_t *filename)
{
    switch (image->format_info->format) {
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8_UNORM:
        return write_direct_to_png(image, filename);
    default:
        return write_indirect_to_png(image, filename);
    }
}
