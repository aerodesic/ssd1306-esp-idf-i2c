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
static void display_put_bitmap(display_t *display, uint8_t *bitmap, int width, int height, bitmap_method_t method)
{
    int b_row = 0;
    int c_row = display->y;

    bool odd = true;

    while (height != 0) {
        /* Compute byte shift and mask */
        int shift = (b_row + c_row) % 8;  /* Starting row at this y interval */

ESP_LOGI(TAG, "%s: height %d b_row %d c_row %d odd %s shift %d", __func__, height, b_row, c_row, odd ? "ODD left" : "EVEN right", shift);

        /* Do one row */
        for (int x = 0; x < width; ++x) {
            uint8_t value = bitmap[width * (b_row/8) + x];
            if (odd) {
                value <<= shift;
            } else {
                value >>= shift;
            }
           
            display->frame_buf[(c_row / 8) * display->width + display->x + x] |= value;
        }

        odd = !odd;

        /* Move to the  next bitmap row */
        b_row += (8 - shift);
        c_row += (8 - shift);

        /* Remove from the heigth required */
        height -= (8 - shift);
    }
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
            display_put_bitmap(display, pchar, char_width, char_height, bitmap_method_OR);
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

static void display_get_xy(display_t* display, int *x, int *y)
{
    display->_lock(display);
    *x = display->x;
    *y = display->y;
    display->_unlock(display);
}

static void display_set_font(display_t *display, const font_t* font)
{
    display->font = font;

    /* Get a representative height */
    display->font_height = font_char_height(display->font, ' ');
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
    display->frame_len    = (width * height) / 8;
    display->frame_buf    = (uint8_t *) malloc(display->frame_len);

    memset(display->frame_buf, 0, display->frame_len);

    display->width        = width;
    display->height       = height;
    display->flags        = flags;

    display->_lock        = display_lock;
    display->_unlock      = display_unlock;

    display->close        = display_close;
    display->set_xy       = display_set_xy;
    display->get_xy       = display_get_xy;
    display->set_font     = display_set_font;
    display->get_font     = display_get_font;
    display->clear        = display_clear;
    display->write_text   = display_write_text;
    display->put_bitmap   = display_put_bitmap;

    display->mutex = xSemaphoreCreateRecursiveMutex();
}

display_t *display_create(int width, int height, uint8_t flags)
{
    ESP_LOGI(TAG, "%s: display_create %d,%d flags %02x", __func__, width, height, flags);

    display_t *display = (display_t*) malloc(sizeof(display_t));

    if (display != NULL) {
        memset(display, 0, sizeof(*display));
        display_init(display, width, height, flags);
    }

    ESP_LOGI(TAG, "%s: returning %p", __func__, display);

    return display;
}
#endif /* CONFIG_SSD1306_I2C_DISPLAY_ENABLED */