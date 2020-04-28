/*
 * font.h
 *
 * Defines a font entry
 */
#ifndef __fonts_h_included
#define __fonts_h_included

#include <stdbool.h>
#include <sys/types.h>

typedef struct {
    uint8_t *base;
    bool    fixed;    /* True if fixed; otherwise a table of pointers to variable length glyphs (NYI) */
    int     first_point;
    int     last_point;
    int     space_point;
    int     width;
    int     height;
} font_t;

/*
 * Return address of the row of font glyph bytes
 */
uint8_t *font_char(const font_t *font, int ch, int *pwidth, int *pheight);
int font_char_width(const font_t *font, int ch);
int font_char_height(const font_t *font, int ch);
void font_metrics(const font_t* font, const char* text, int *pwidth, int *pheight);

#endif /* __fonts_h_included */

