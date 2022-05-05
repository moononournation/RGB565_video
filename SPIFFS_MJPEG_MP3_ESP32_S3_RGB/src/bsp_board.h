
#ifndef _BSP_BOARD_H_
#define _BSP_BOARD_H_

#include "driver/gpio.h"

/**
 * @brief ESP32-S3 LCD GPIO defination and config
 * 
 */
#define FUNC_LCD_EN     (1)
#define LCD_WIDTH       (480)
#define LCD_HEIGHT      (480)

#define GPIO_LCD_BL     (GPIO_NUM_38)
#define GPIO_LCD_RST    (GPIO_NUM_NC)
#define GPIO_LCD_CS     (GPIO_NUM_39)
#define GPIO_LCD_SDA    (GPIO_NUM_47)
#define GPIO_LCD_SCK    (GPIO_NUM_48)

#define GPIO_LCD_DE     (GPIO_NUM_18)
#define GPIO_LCD_VSYNC  (GPIO_NUM_17)
#define GPIO_LCD_HSYNC  (GPIO_NUM_16)
#define GPIO_LCD_PCLK   (GPIO_NUM_21)

#define GPIO_LCD_R0    (GPIO_NUM_4)
#define GPIO_LCD_R1    (GPIO_NUM_3)
#define GPIO_LCD_R2    (GPIO_NUM_2)
#define GPIO_LCD_R3    (GPIO_NUM_1)
#define GPIO_LCD_R4    (GPIO_NUM_0)

#define GPIO_LCD_G0    (GPIO_NUM_10)
#define GPIO_LCD_G1    (GPIO_NUM_9)
#define GPIO_LCD_G2    (GPIO_NUM_8)
#define GPIO_LCD_G3    (GPIO_NUM_7)
#define GPIO_LCD_G4    (GPIO_NUM_6)
#define GPIO_LCD_G5    (GPIO_NUM_5)

#define GPIO_LCD_B0    (GPIO_NUM_15)
#define GPIO_LCD_B1    (GPIO_NUM_14)
#define GPIO_LCD_B2    (GPIO_NUM_13)
#define GPIO_LCD_B3    (GPIO_NUM_12)
#define GPIO_LCD_B4    (GPIO_NUM_11)

/**
 * @brief ESP32-S3 I2C GPIO defineation
 * 
 */
#define FUNC_I2C_EN     (1)
#define GPIO_I2C_SCL    (GPIO_NUM_41)
#define GPIO_I2C_SDA    (GPIO_NUM_40)

/**
 * @brief ESP32-S3 I2S GPIO defination
 * 
 */
#define FUNC_I2S_EN      (1)
#define GPIO_I2S_MCLK    (GPIO_NUM_42)
#define GPIO_I2S_SCLK    (GPIO_NUM_46)
#define GPIO_I2S_LRCK    (GPIO_NUM_45)
#define GPIO_I2S_DIN     (GPIO_NUM_44)
#define GPIO_I2S_DOUT    (GPIO_NUM_43)

/**
 * @brief ESP32-S3 SDMMC GPIO defination
 * 
 */
#define SDMMC_BUS_WIDTH (1)
#define GPIO_SDMMC_CLK  (GPIO_NUM_36)
#define GPIO_SDMMC_CMD  (GPIO_NUM_37)
#define GPIO_SDMMC_D0   (GPIO_NUM_35)
#define GPIO_SDMMC_D1   (GPIO_NUM_NC)
#define GPIO_SDMMC_D2   (GPIO_NUM_NC)
#define GPIO_SDMMC_D3   (GPIO_NUM_NC)
#define GPIO_SDMMC_DET  (GPIO_NUM_NC)

#endif /* CONFIG_ESP32_S3_HMI_DEVKIT_BOARD */

