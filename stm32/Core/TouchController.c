/**
 * @file TouchController.c
 * @brief LVGL input driver for the XPT2046 touch controller.
 */
#include "TouchController.h"
#include "XPT2046.h"
#include "main.h"          /* for hspi2, huart2 */
#include "lvgl.h"
#include <stdio.h>
#include <inttypes.h>      /* PRIu16 */

extern SPI_HandleTypeDef hspi2;
extern UART_HandleTypeDef huart2;

/* Retarget printf() over UART */
int __io_putchar(int ch) {
    uint8_t c = ch;
    HAL_UART_Transmit(&huart2, &c, 1, HAL_MAX_DELAY);
    return ch;
}

/* ─── RAW EDGE CONSTANTS ───
   Fill these in once you have your 4 calibration points:
   RAW_Y_MIN = raw-Y at left edge
   RAW_Y_MAX = raw-Y at right edge
   RAW_X_MIN = raw-X at top edge
   RAW_X_MAX = raw-X at bottom edge
*/
static uint16_t RAW_X_MIN = 262;
static uint16_t RAW_X_MAX = 1872;
static uint16_t RAW_Y_MIN = 230;
static uint16_t RAW_Y_MAX = 1872;

/* Map raw-Y → pixel-X (0..hor_res-1), then flip for rotation(3) */
static lv_coord_t map_x(uint16_t raw) {
    lv_coord_t w = lv_disp_get_hor_res(NULL);
    if(raw <= RAW_Y_MIN) raw = RAW_Y_MIN;
    if(raw >= RAW_Y_MAX) raw = RAW_Y_MAX;
    /* raw∈[RAW_Y_MIN..RAW_Y_MAX] → [0..w-1] then flip */
    return (lv_coord_t)((uint32_t)(raw - RAW_Y_MIN) * (w - 1) / (RAW_Y_MAX - RAW_Y_MIN));
}
/* Map raw-X → pixel-Y (0..ver_res-1) */
static lv_coord_t map_y(uint16_t raw) {
    lv_coord_t h = lv_disp_get_ver_res(NULL);
    if(raw <= RAW_X_MIN) raw = RAW_X_MIN;
    if(raw >= RAW_X_MAX) raw = RAW_X_MAX;
    return (lv_coord_t)((uint32_t)(raw - RAW_X_MIN) * (h - 1) / (RAW_X_MAX - RAW_X_MIN));
}

/* Draw a little green + at exactly (x,y) on the given screen */
static void draw_cross(lv_obj_t *scr, lv_coord_t x, lv_coord_t y) {
    /* horizontal */
    static const lv_point_t ph[] = {{-10,0},{10,0}};
    lv_obj_t *h = lv_line_create(scr);
    lv_line_set_points(h, ph, 2);
    lv_obj_set_style_line_color(h, lv_color_hex(0x00FF00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(h, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_pos(h, x, y);

    /* vertical */
    static const lv_point_t pv[] = {{0,-10},{0,10}};
    lv_obj_t *v = lv_line_create(scr);
    lv_line_set_points(v, pv, 2);
    lv_obj_set_style_line_color(v, lv_color_hex(0x00FF00), LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_style_line_width(v, 2, LV_PART_MAIN|LV_STATE_DEFAULT);
    lv_obj_set_pos(v, x, y);
}

/**
 * Run a 4-point on-screen calibration.
 * You’ll tap each green + in turn, raw values get printed,
 * then you can compute new RAW_*_MIN/MAX from those prints.
 */
void touch_calibrate(void) {
    /* make a brand-new blank screen */
    lv_obj_t *cal = lv_obj_create(NULL);
    lv_disp_load_scr(cal);
    lv_obj_clear_flag(cal, LV_OBJ_FLAG_SCROLLABLE);

    lv_coord_t w = lv_disp_get_hor_res(NULL);
    lv_coord_t h = lv_disp_get_ver_res(NULL);

    /* inset by 20px from each corner */
    const lv_point_t corners[4] = {
        {20,            20},      /* top-left  */
        {w - 21,        20},      /* top-right */
        {w - 21,        h - 21},  /* bot-right */
        {20,            h - 21}   /* bot-left  */
    };

    uint16_t raw_x[4], raw_y[4];
    for(int i = 0; i < 4; i++) {
        lv_obj_clean(cal);
        draw_cross(cal, corners[i].x, corners[i].y);

        lv_obj_t *lbl = lv_label_create(cal);
        lv_label_set_text(lbl, "Touch the green +");
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 40);

        /* wait for press */
        uint16_t rx, ry;
        do {
            lv_timer_handler();
            HAL_Delay(5);
        } while(!XPT2046_GetTouch(&rx, &ry));

        raw_x[i] = rx;
        raw_y[i] = ry;
        printf("Cal %d: raw=(%u,%u)\r\n", i, rx, ry);

        /* wait for release */
        do {
            lv_timer_handler();
            HAL_Delay(5);
        } while(XPT2046_GetTouch(&rx, &ry));
    }

    /* now raw_x/raw_y arrays hold your 4 corner measurements.
       Use them to recompute RAW_*_MIN/MAX above and rebuild. */
}

/* ———— touchpad read callback ———— */
static void touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    static uint16_t last_x = 0, last_y = 0;
    uint16_t xr, yr;

    if(XPT2046_GetTouch(&xr, &yr)) {
        last_x     = xr;
        last_y     = yr;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = map_x(last_y);
    data->point.y = map_y(last_x);
}

/* ———— initialization ———— */
void TouchController_Init(void) {
    XPT2046_Init(&hspi2);
    lv_indev_t *ind = lv_indev_create();
    lv_indev_set_type(ind, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(ind, touchpad_read);
}
/* PENIRQ EXTI handler just clears itself—LVGL will poll next tick */
void HAL_GPIO_EXTI_Callback(uint16_t pin) { (void)pin; }
