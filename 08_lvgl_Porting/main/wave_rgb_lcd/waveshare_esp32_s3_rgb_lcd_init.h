/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#ifndef _WAVESHARE_ESP32_S3_RGB_LCD_INIT_H_
#define _WAVESHARE_ESP32_S3_RGB_LCD_INIT_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the Waveshare ESP32-S3 RGB LCD
 *
 * This function initializes the RGB LCD panel and touch controller,
 * then initializes the LVGL port.
 *
 * @return
 *     - ESP_OK: Success
 *     - Others: Fail
 */
esp_err_t waveshare_esp32_s3_rgb_lcd_init(void);

/**
 * @brief Turn on the LCD backlight
 *
 * @return
 *     - ESP_OK: Success
 *     - Others: Fail
 */
esp_err_t wavesahre_rgb_lcd_bl_on(void);

/**
 * @brief Turn off the LCD backlight
 *
 * @return
 *     - ESP_OK: Success
 *     - Others: Fail
 */
esp_err_t wavesahre_rgb_lcd_bl_off(void);

/**
 * @brief Create example LVGL demo UI
 *
 */
void example_lvgl_demo_ui(void);

#ifdef __cplusplus
}
#endif

#endif /* _WAVESHARE_ESP32_S3_RGB_LCD_INIT_H_ */
