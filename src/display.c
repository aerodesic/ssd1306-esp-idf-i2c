/*
 * Generic part of the display handler.
 */
#include "sdkconfig.h" // generated by "make menuconfig"

#if CONFIG_SSD1306_I2C_ENABLED

#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"


#include "display.h"

#define TAG "DISPLAY"

static void display_lock(display_t* display)
{
    xSemaphoreTakeRecursive(display->mutex, portMAX_DELAY);
}

static void display_unlock(display_t* display)
{
    xSemaphoreGiveRecursive(display->mutex);
}

static void display_clear(display_t *display)
{
    memset(display->frame_buf, 0, display->frame_len);
}

/*
 * Put the bitmap into the frame buffer at current x and y.  x,y is the top left corner
 */
static void display_draw_bitmap(display_t *display, uint8_t *bitmap, int width, int height, bitmap_method_t method)
{
    int b_row = 0;
    int c_row = display->y;

    bool odd = true;

    display->_lock(display);

    while (height != 0) {
        /* Compute byte shift and mask */
        int shift = (b_row + c_row) % 8;  /* Starting row at this y interval */

//ESP_LOGI(TAG, "%s: height %d b_row %d c_row %d odd %s shift %d", __func__, height, b_row, c_row, odd ? "ODD left" : "EVEN right", shift);

        /* Do one row */
        for (int x = 0; x < width; ++x) {
            uint8_t value = bitmap[width * (b_row/8) + x];
            if (odd) {
                value <<= shift;
            } else {
                value >>= shift;
            }

            uint8_t *byte = &display->frame_buf[(c_row / 8) * display->width + display->x + x];

            if (method == bitmap_method_XOR) {
                *byte ^= value;
            } else {
                *byte |= value;
            }

        }

        odd = !odd;

        /* Move to the  next bitmap row */
        b_row += (8 - shift);
        c_row += (8 - shift);

        /* Remove from the heigth required */
        height -= (8 - shift);
    }

    display->_unlock(display);
}

static void display_draw_pixel(display_t *display, int x, int y, bool set)
{
    display->_lock(display);

//ESP_LOGI(TAG, "%s: %d,%d %s", __func__, x, y, set ? "SET" : "CLEAR");

    if (x >= 0 && x < display->width && y >= 0 && y < display->height) {
        uint8_t *byte = &display->frame_buf[(y/8) * display->width + x];

        if (set) {
            *byte |= 1 << (y % 8);
        } else {
            *byte &= ~(1 << (y % 8));
        }
    }

    display->_unlock(display);
}

static void display_draw_line(display_t *display, int x1, int y1, int x2, int y2, bool set)
{
//ESP_LOGI(TAG, "%s: %d,%d to %d,%d  dx %d dy %d d %d", __func__, x1, y1, x2, y2, dx, dy, d);

    display->_lock(display);

    int dx =  abs(x2-x1);
    int sx = x1<x2 ? 1 : -1;
    int dy = -abs(y2-y1);
    int sy = y1<y2 ? 1 : -1;
    int err = dx+dy;

    do {
        display_draw_pixel(display, x1, y1, set);

        if (x1 != x2 || y1 != y2) {
            int e2 = 2*err;
            if (e2 >= dy) {
                err += dy;
                x1 += sx;
            }

            if (e2 <= dx) {
                err += dx;
                y1 += sy;
            }
        }
    } while (x1 != x2 || y1 != y2);

    display->_unlock(display);
}

static void display_draw_rectangle(display_t *display, int x, int y, int width, int height, draw_flags_t flags)
{
//ESP_LOGI(TAG, "%s: x1 %d y1 %d x2 %d y2 %d flags %02x", __func__, x1, y1, x2, y2, flags);

    int x1 = x;
    int x2 = x + width - 1;
    int y1 = y;
    int y2 = y + height - 1;

    if (x1 < 0) {
        x1 = 0;
    } else if (x1 >= display->width) {
        x1 = display->width - 1;
    }

    if (x2 < 0) {
        x2 = 0;
    } else if (x2 >= display->width) {
        x2 = display->width - 1;
    }

    if (y1 < 0) {
        y1 = 0;
    } else if (y1 >= display->height) {
        y1 = display->height - 1;
    }

    if (y2 < 0) {
        y2 = 0;
    } else if (y2 >= display->height) {
        y2 = display->height - 1;
    }

    display->_lock(display);
    
    if (flags & draw_flag_border) {
        display_draw_line(display, x1, y1, x2, y1, !(flags & draw_flag_clear));
        display_draw_line(display, x2, y1, x2, y2, !(flags & draw_flag_clear));
        display_draw_line(display, x2, y2, x1, y2, !(flags & draw_flag_clear));
        display_draw_line(display, x1, y2, x1, y1, !(flags & draw_flag_clear));
        x1++;
        y1++;
        x2--;
        y2--;
    }
        
    if (flags & (draw_flag_fill | draw_flag_clear)) {
        for (int y = y1; y <= y2; ++y) {
            display_draw_line(display, x1, y, x2, y, !(flags & draw_flag_clear));
        } 
    }

    display->_unlock(display);
}

static void display_write_text(display_t *display, const char* text)
{
    display->_lock(display);

    while (*text != '\0') {
        int char_height;
        int char_width;

        uint8_t *pchar = font_char(display->font, *text, &char_width, &char_height);
        if (display->x + char_width > display->width || *text == '\n') {
            /* Advance a line */
            display->y += display->font_height;

            /* Reset X */
            display->x = 0;

            if (*text == '\n') {
                ++text;
            }
        } else if (pchar != NULL) {
            display_draw_bitmap(display, pchar, char_width, char_height, bitmap_method_XOR);
            display->x += char_width;
            ++text;
        }
    }

    display->show(display);

    display->_unlock(display);
}


static void display_set_xy(display_t* display, int x, int y)
{
    display->_lock(display);

    if (x >= 0) {
        display->x = x;
    }

    if (y >= 0) {
        display->y = y;
    }

    display->_unlock(display);
}

void display_draw_progress_bar(display_t *display, int x, int y, int width, int height, int range, int value, const char* text)
{
    /* Draw surrounding border */
    display->draw_rectangle(display, x, y, width, height, draw_flag_border);

    /* Make smaller rectangle for the moving bar */
    x += 1;
    y += 1;
    width -= 2;
    height -= 2;

    int bar = ((width - 1) * value) / range;

    /* Paint the Progress part */
    display->draw_rectangle(display, x, y, bar, height, draw_flag_fill);

    /* Paint the non-progress part */
    if (range != value) {
        display->draw_rectangle(display, x + bar, y, width - bar, height, draw_flag_clear);
    }

    if (text != NULL) {
        int cwidth, cheight;
        font_metrics(display->font, text, &cwidth, &cheight);
        display->set_xy(display, x + width/2 - cwidth/2, y + height/2 -  cheight/2);
        display->write_text(display, text);
    } else {
        display->show(display);
    }
}


static void display_get_xy(display_t* display, int *x, int *y)
{
    display->_lock(display);
    *x = display->x;
    *y = display->y;
    display->_unlock(display);
}

static void display_set_font(display_t *display, const font_t* font)
{
    display->_lock(display);

    display->font = font;

    /* Get a representative height */
    display->font_height = font_char_height(display->font, ' ');

    display->_unlock(display);
}

static const font_t *display_get_font(display_t *display)
{
    return display->font;
}

static void display_close(display_t* display)
{
    vSemaphoreDelete(display->mutex); 

    free((void*) display);
}

static void display_init(display_t* display, int width, int height, uint8_t flags)
{
    /* Create the frame buffer */
    display->frame_len            = (width * height) / 8;
    display->frame_buf            = (uint8_t *) malloc(display->frame_len);

    memset(display->frame_buf, 0, display->frame_len);

    display->width                = width;
    display->height               = height;
    display->flags                = flags;

    display->_lock                = display_lock;
    display->_unlock              = display_unlock;

    display->close                = display_close;
    display->set_xy               = display_set_xy;
    display->get_xy               = display_get_xy;
    display->set_font             = display_set_font;
    display->get_font             = display_get_font;
    display->clear                = display_clear;
    display->write_text           = display_write_text;
    display->draw_bitmap          = display_draw_bitmap;
    display->draw_rectangle       = display_draw_rectangle;
    display->draw_line            = display_draw_line;
    display->draw_pixel           = display_draw_pixel;
    display->draw_progress_bar    = display_draw_progress_bar;

    display->mutex = xSemaphoreCreateRecursiveMutex();
}

display_t *display_create(int width, int height, uint8_t flags)
{
    //ESP_LOGI(TAG, "%s: display_create %d,%d flags %02x", __func__, width, height, flags);

    display_t *display = (display_t*) malloc(sizeof(display_t));

    if (display != NULL) {
        memset(display, 0, sizeof(*display));
        display_init(display, width, height, flags);
    }

    //ESP_LOGI(TAG, "%s: returning %p", __func__, display);

    return display;
}
#endif /* CONFIG_SSD1306_I2C_DISPLAY_ENABLED */
