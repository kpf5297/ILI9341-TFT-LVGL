/**
 * @file LCDController.c
 * @brief LVGL display glue for the ILI9341 driver.
 */

/*********************
 *      INCLUDES
 *********************/
#include "LCDController.h"
#include <stdbool.h>
#include "main.h"
#include "ILI9341.h"
#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    320
#define MY_DISP_VER_RES    240



#define BYTE_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565)) /*will be 2 for RGB565 */

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_display_t *disp = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    disp_init();

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
//    lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);
    /* Use two small buffers so DMA can flush while LVGL prepares the next area */
    LV_ATTRIBUTE_MEM_ALIGN static uint8_t buf1[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    LV_ATTRIBUTE_MEM_ALIGN static uint8_t buf2[MY_DISP_HOR_RES * 10 * BYTE_PER_PIXEL];
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);



}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    /*You code here*/
	ILI9341_Init();
}

volatile bool disp_flush_enabled = true;

/* Enable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_enable_update(void)
{
    disp_flush_enabled = true;
}

/* Disable updating the screen (the flushing process) when disp_flush() is called by LVGL
 */
void disp_disable_update(void)
{
    disp_flush_enabled = false;
}

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    if(!disp_flush_enabled) {
        lv_display_flush_ready(disp_drv);
        return;
    }

    //Set the drawing region
	ILI9341_SetWindow(area->x1, area->y1, area->x2, area->y2);

    int height = area->y2 - area->y1 + 1;
    int width = area->x2 - area->x1 + 1;

    lv_color_t *color_p = (lv_color_t *) px_map;

    ILI9341_DrawBitmapDMA(width, height, (uint8_t *) color_p);
    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_display_flush_ready(disp_drv);
}

