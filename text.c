#include <pthread.h>
#include <time.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "text_font.h"

#include "text.h"

#define TEXT_LINE_HEIGHT 34
#define TEXT_START_X 12
#define TEXT_BASELINE_Y 33
#define TEXT_BACKGROUND_X 7
#define TEXT_BACKGROUND_Y 7
#define TEXT_BACKGROUND_EXTRA_WIDTH 10

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

static bool load_library(FT_Library *plibrary, FT_Face *pface) {
    int error = FT_Init_FreeType(plibrary);
    if (error) {
        set_error("FT_Init_FreeType() failed");
        return false;
    }

    error = FT_New_Memory_Face(*plibrary, text_font_ttf, sizeof(text_font_ttf),
                               0, pface);
    if (error) {
        FT_Done_FreeType(*plibrary);
        set_error("FT_New_Memory_Face() failed");
        return false;
    }

    error = FT_Set_Pixel_Sizes(*pface, 25, 25);
    if (error) {
        FT_Done_Face(*pface);
        FT_Done_FreeType(*plibrary);
        set_error("FT_Set_Pixel_Sizes() failed");
        return false;
    }

    return true;
}

static char *normalize_newlines(char *text) {
    char *dest = text;

    for (const char *src = text; *src != 0x00; src++) {
        if (*src == '\r' && src[1] == '\n') {
            src++;
        }
        *dest++ = *src;
    }

    *dest = 0x00;

    return text;
}

static char *trim_newlines(char *text) {
    while (*text == '\n') {
        text++;
    }

    char *end = text + strlen(text) - 1;
    while (end >= text && *end == '\n') {
        *end = 0x00;
        end--;
    }

    return text;
}

typedef struct {
    pthread_mutex_t mutex;
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

    textp->text_overlay = strdup(params->text_overlay);
    textp->text_overlay = normalize_newlines(textp->text_overlay);
    textp->text_overlay = trim_newlines(textp->text_overlay);
    textp->stride = stride;
    textp->height = params->height;

    if (params->text_overlay_enable) {
        bool ok = load_library(&textp->library, &textp->face);
        if (!ok) {
            goto failed;
        }
    }

    pthread_mutex_init(&textp->mutex, NULL);

    return true;

failed:
    free(textp->text_overlay);
    free(textp);

    return false;
}

static void draw_rect(uint8_t *buf, int stride, int height, int x, int y,
                      unsigned int rect_width, unsigned int rect_height) {
    if (x < 0 || x >= stride || y < 0 || y >= height) {
        return;
    }
    if (rect_width > (unsigned int)(stride - x)) {
        rect_width = stride - x;
    }
    if (rect_height > (unsigned int)(height - y)) {
        rect_height = height - y;
    }

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
                int dest_x = x + src_x;
                int dest_y = y + src_y;
                if (dest_x < 0 || dest_x >= stride || dest_y < 0 ||
                    dest_y >= height) {
                    continue;
                }
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

static int get_text_width(FT_Face face, const char *text, int *lines) {
    int width = 0;
    int max_width = 0;
    *lines = 1;

    for (const char *ptr = text; *ptr != 0x00; ptr++) {
        if (*ptr == '\n') {
            if (width > max_width) {
                max_width = width;
            }
            width = 0;
            (*lines)++;
            continue;
        }

        int error = FT_Load_Char(face, *ptr, FT_LOAD_RENDER);
        if (error) {
            continue;
        }

        width += face->glyph->advance.x >> 6;
    }

    return width > max_width ? width : max_width;
}

void text_reload_params(text_t *text, const parameters_t *params) {
    text_priv_t *textp = (text_priv_t *)text;

    pthread_mutex_lock(&textp->mutex);

    free(textp->text_overlay);
    textp->text_overlay = strdup(params->text_overlay);
    textp->text_overlay = normalize_newlines(textp->text_overlay);
    textp->text_overlay = trim_newlines(textp->text_overlay);

    if (params->text_overlay_enable && textp->library == NULL) {
        bool ok = load_library(&textp->library, &textp->face);
        if (!ok) {
            textp->face = NULL;
            textp->library = NULL;
        }
    } else if (!params->text_overlay_enable && textp->library != NULL) {
        FT_Done_Face(textp->face);
        FT_Done_FreeType(textp->library);
        textp->face = NULL;
        textp->library = NULL;
    }

    pthread_mutex_unlock(&textp->mutex);
}

void text_destroy(text_t *text) {
    text_priv_t *textp = (text_priv_t *)text;

    if (textp->library != NULL) {
        FT_Done_Face(textp->face);
        FT_Done_FreeType(textp->library);
    }

    free(textp->text_overlay);
    pthread_mutex_destroy(&textp->mutex);
    free(textp);
}

void text_draw(text_t *text, uint8_t *buf, uint64_t ntp) {
    text_priv_t *textp = (text_priv_t *)text;

    pthread_mutex_lock(&textp->mutex);

    if (textp->library != NULL) {
        struct timeval tv = {
            .tv_sec = ntp / 1000000,
            .tv_usec = ntp % 1000000,
        };

        char buffer[256];
        extended_strftime(buffer, 256, textp->text_overlay, &tv);

        int lines;
        int width = get_text_width(textp->face, buffer, &lines);
        draw_rect(buf, textp->stride, textp->height, TEXT_BACKGROUND_X,
                  TEXT_BACKGROUND_Y, width + TEXT_BACKGROUND_EXTRA_WIDTH,
                  lines * TEXT_LINE_HEIGHT);

        int x = TEXT_START_X;
        int y = TEXT_BASELINE_Y;

        for (const char *ptr = buffer; *ptr != 0x00; ptr++) {
            if (*ptr == '\n') {
                x = TEXT_START_X;
                y += TEXT_LINE_HEIGHT;
                continue;
            }

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

    pthread_mutex_unlock(&textp->mutex);
}
