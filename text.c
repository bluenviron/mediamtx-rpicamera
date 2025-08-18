#include <sys/time.h>
#include <time.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "text_font.h"

#include "text.h"

static char errbuf[256];

static void set_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(errbuf, 256, format, args);
}

const char *text_get_error() { return errbuf; }

static void str_replace(char *buffer, size_t bufsize, const char *find,
                        const char *replace) {
    char *ptr = strstr(buffer, find);
    if (ptr != NULL) {
        int64_t find_len = strlen(find);
        int64_t replace_len = strlen(replace);
        int64_t after_len = strlen(ptr + find_len);

        int64_t avail1 = bufsize - (ptr - buffer) - 1;
        if (replace_len > avail1) {
            replace_len = avail1;
        }

        int64_t avail2 = avail1 - replace_len;
        if (avail2 <= 0) {
            after_len = 0;
        } else if (after_len > avail2) {
            after_len = avail2;
        }

        char tmp[256];
        memcpy(tmp, ptr + find_len, after_len);
        memcpy(ptr, replace, replace_len);
        memcpy(ptr + replace_len, tmp, after_len);
        ptr[replace_len + after_len] = 0x00;
    }
}

static void extended_strftime(char *buffer, size_t bufsize, const char *format,
                              const struct timeval *time) {
    struct tm *tm_info = localtime(&time->tv_sec);
    strftime(buffer, bufsize, format, tm_info);

    char str_usec[7];
    sprintf(str_usec, "%.6ld", time->tv_usec);

    str_replace(buffer, bufsize, "%f", str_usec);
}

typedef struct {
    bool enabled;
    char *text_overlay;
    int stride;
    int height;
    FT_Library library;
    FT_Face face;
} text_priv_t;

bool text_create(const parameters_t *params, int stride, text_t **text) {
    *text = malloc(sizeof(text_priv_t));
    text_priv_t *textp = (text_priv_t *)(*text);
    memset(textp, 0, sizeof(text_priv_t));

    textp->enabled = params->text_overlay_enable;
    textp->text_overlay = strdup(params->text_overlay);
    textp->stride = stride;
    textp->height = params->height;

    if (textp->enabled) {
        int error = FT_Init_FreeType(&textp->library);
        if (error) {
            set_error("FT_Init_FreeType() failed");
            goto failed;
        }

        error = FT_New_Memory_Face(textp->library, text_font_ttf,
                                   sizeof(text_font_ttf), 0, &textp->face);
        if (error) {
            set_error("FT_New_Memory_Face() failed");
            goto failed;
        }

        error = FT_Set_Pixel_Sizes(textp->face, 25, 25);
        if (error) {
            set_error("FT_Set_Pixel_Sizes() failed");
            goto failed;
        }
    }

    return true;

failed:
    free(textp);

    return false;
}

static void draw_rect(uint8_t *buf, int stride, int height, int x, int y,
                      unsigned int rect_width, unsigned int rect_height) {
    uint8_t *Y = buf;
    uint8_t *U = Y + stride * height;
    uint8_t *V = U + (stride / 2) * (height / 2);
    const uint8_t color[3] = {0, 128, 128};
    uint32_t opacity = 45;

    for (unsigned int src_y = 0; src_y < rect_height; src_y++) {
        for (unsigned int src_x = 0; src_x < rect_width; src_x++) {
            unsigned int dest_x = x + src_x;
            unsigned int dest_y = y + src_y;
            int i1 = dest_y * stride + dest_x;
            int i2 = dest_y / 2 * stride / 2 + dest_x / 2;

            Y[i1] = ((color[0] * opacity) + (uint32_t)Y[i1] * (255 - opacity)) /
                    255;
            U[i2] = ((color[1] * opacity) + (uint32_t)U[i2] * (255 - opacity)) /
                    255;
            V[i2] = ((color[2] * opacity) + (uint32_t)V[i2] * (255 - opacity)) /
                    255;
        }
    }
}

static void draw_bitmap(uint8_t *buf, int stride, int height,
                        const FT_Bitmap *bitmap, int x, int y) {
    uint8_t *Y = buf;
    uint8_t *U = Y + stride * height;
    uint8_t *V = U + (stride / 2) * (height / 2);

    for (unsigned int src_y = 0; src_y < bitmap->rows; src_y++) {
        for (unsigned int src_x = 0; src_x < bitmap->width; src_x++) {
            uint8_t v = bitmap->buffer[src_y * bitmap->pitch + src_x];

            if (v != 0) {
                unsigned int dest_x = x + src_x;
                unsigned int dest_y = y + src_y;
                int i1 = dest_y * stride + dest_x;
                int i2 = dest_y / 2 * stride / 2 + dest_x / 2;
                uint32_t opacity = (uint32_t)v;

                Y[i1] = (uint8_t)(((uint32_t)v * opacity +
                                   (uint32_t)Y[i1] * (255 - opacity)) /
                                  255);
                U[i2] = (uint8_t)((128 * opacity +
                                   (uint32_t)U[i2] * (255 - opacity)) /
                                  255);
                V[i2] = (uint8_t)((128 * opacity +
                                   (uint32_t)V[i2] * (255 - opacity)) /
                                  255);
            }
        }
    }
}

static int get_text_width(FT_Face face, const char *text) {
    int ret = 0;

    for (const char *ptr = text; *ptr != 0x00; ptr++) {
        int error = FT_Load_Char(face, *ptr, FT_LOAD_RENDER);
        if (error) {
            continue;
        }

        ret += face->glyph->advance.x >> 6;
    }

    return ret;
}

void text_draw(text_t *text, uint8_t *buf) {
    text_priv_t *textp = (text_priv_t *)text;

    if (textp->enabled) {
        struct timeval now;
        gettimeofday(&now, NULL);

        char buffer[256];
        extended_strftime(buffer, 256, textp->text_overlay, &now);

        draw_rect(buf, textp->stride, textp->height, 7, 7,
                  get_text_width(textp->face, buffer) + 10, 34);

        int x = 12;
        int y = 33;

        for (const char *ptr = buffer; *ptr != 0x00; ptr++) {
            int error = FT_Load_Char(textp->face, *ptr, FT_LOAD_RENDER);
            if (error) {
                continue;
            }

            draw_bitmap(buf, textp->stride, textp->height,
                        &textp->face->glyph->bitmap,
                        x + textp->face->glyph->bitmap_left,
                        y - textp->face->glyph->bitmap_top);

            x += textp->face->glyph->advance.x >> 6;
        }
    }
}
