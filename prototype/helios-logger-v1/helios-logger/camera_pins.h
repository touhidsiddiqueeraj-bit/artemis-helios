/**
 * camera_pins.h — OV2640 GPIO mapping
 *
 * Configured for Seeed XIAO ESP32S3 Sense (embedded OV2640).
 * If you are using a different board (e.g. AI-Thinker ESP32-CAM),
 * swap the pin numbers below for your board's pinout.
 *
 * XIAO ESP32S3 Sense OV2640 pinout:
 * https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
 */

#pragma once

// XIAO ESP32S3 Sense
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    10
#define SIOD_GPIO_NUM    40
#define SIOC_GPIO_NUM    39

#define Y9_GPIO_NUM      48
#define Y8_GPIO_NUM      11
#define Y7_GPIO_NUM      12
#define Y6_GPIO_NUM      14
#define Y5_GPIO_NUM      16
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM      17
#define Y2_GPIO_NUM      15
#define VSYNC_GPIO_NUM   38
#define HREF_GPIO_NUM    47
#define PCLK_GPIO_NUM    13

/**
 * IF USING AI-THINKER ESP32-CAM, comment out above and use:
 *
 * #define PWDN_GPIO_NUM   32
 * #define RESET_GPIO_NUM  -1
 * #define XCLK_GPIO_NUM    0
 * #define SIOD_GPIO_NUM   26
 * #define SIOC_GPIO_NUM   27
 * #define Y9_GPIO_NUM     35
 * #define Y8_GPIO_NUM     34
 * #define Y7_GPIO_NUM     39
 * #define Y6_GPIO_NUM     36
 * #define Y5_GPIO_NUM     21
 * #define Y4_GPIO_NUM     19
 * #define Y3_GPIO_NUM     18
 * #define Y2_GPIO_NUM      5
 * #define VSYNC_GPIO_NUM  25
 * #define HREF_GPIO_NUM   23
 * #define PCLK_GPIO_NUM   22
 */
