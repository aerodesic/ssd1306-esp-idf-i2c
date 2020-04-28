/*
 * Return address of the row of font glyph bytes
 */
#include <sys/types.h>
#include "font.h"

int font_char_height(const font_t *font, int ch)
{
    int height = 0;

    if (font->fixed) {
        height = font->height;
    }

    return height;
}

int font_char_width(const font_t *font, int ch) {
    int width = 0;

    if (font->fixed) {
        width = font->width;
    }

    return width;
}

uint8_t *font_char(const font_t *font, int point, int *pwidth, int *pheight)
{
    uint8_t *glyph = NULL;

    if (point >= font->first_point && point <= font->last_point) {
        if (font->fixed) {
            glyph = (uint8_t*) font->base + ((point - font->first_point) * font->width);

            if (pwidth != NULL) {
                *pwidth = font_char_width(font, point);
            }

            if (pheight != NULL) {
                *pheight = font_char_width(font, point);
            }
        } else {
            /* Need to do something for variable pitch fonts */
        }
    }
        
    return glyph;
}

void font_metrics(const font_t *font, const char* text, int *pwidth, int *pheight)
{
    if (pwidth != NULL) {
        *pwidth = 0;
    }

    if (pheight != NULL) {
        *pheight = 0;
    }

    while (*text != 0) {
        int cwidth = font_char_width(font, *text);
        int cheight = font_char_height(font, *text);

        if (pwidth != NULL) {
            *pwidth += cwidth;
        }

        if (pheight != NULL && cheight > *pheight) {
            *pheight = cheight;
        }
    }
}

