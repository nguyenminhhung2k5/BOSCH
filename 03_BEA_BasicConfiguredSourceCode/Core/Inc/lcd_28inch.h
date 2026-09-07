/**
  ******************************************************************************
  * @file    lcd_28inch.h
  * @brief   Driver header for Waveshare 2.8inch Resistive Touch LCD on Open405R-C
  *          Supports ST7789 and HX8347D controllers via SPI
  ******************************************************************************
  */

#ifndef __LCD_28INCH_H
#define __LCD_28INCH_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* Chọn IC điều khiển: Mặc định chọn ST7789_DEVICE (phổ biến nhất)
 * Nếu màn hình của bạn là dòng cũ dùng HX8347, chuyển define sang HX8347_DEVICE */
#define ST7789_DEVICE
/* #define HX8347_DEVICE */

/* Kích thước màn hình (Chế độ nằm ngang Landscape: 320 x 240) */
#define LCD_WIDTH   320
#define LCD_HEIGHT  240

/* Định nghĩa chân kết nối trên bo mạch Open405R-C */
#define LCD_CS_PORT     GPIOB
#define LCD_CS_PIN      GPIO_PIN_7

#define LCD_DC_PORT     GPIOB
#define LCD_DC_PIN      GPIO_PIN_8

#define LCD_RST_PORT    GPIOB
#define LCD_RST_PIN     GPIO_PIN_2

#define LCD_BL_PORT     GPIOB
#define LCD_BL_PIN      GPIO_PIN_6   /* Chân này trùng với CAN2_TX */

#define LCD_SCK_PORT    GPIOB
#define LCD_SCK_PIN     GPIO_PIN_3

#define LCD_MOSI_PORT   GPIOA
#define LCD_MOSI_PIN    GPIO_PIN_7

/* Bảng màu RGB565 */
#define LCD_BLACK       0x0000
#define LCD_WHITE       0xFFFF
#define LCD_RED         0xF800
#define LCD_GREEN       0x07E0
#define LCD_BLUE        0x001F
#define LCD_YELLOW      0xFFE0
#define LCD_CYAN        0x07FF
#define LCD_MAGENTA     0xF81F
#define LCD_GRAY        0x7BEF
#define LCD_DARKGRAY    0x39E7
#define LCD_DARKBLUE    0x0010

/* Khai báo hàm điều khiển LCD */
void LCD_GPIO_Init(void);
void LCD_Init(void);
void LCD_SetBacklight(uint8_t state);
void LCD_Clear(uint16_t color);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t textColor, uint16_t bgColor);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t textColor, uint16_t bgColor);

/* Hàm giao diện và in Log CAN chuyên dụng */
void LCD_Init_UI(void);
void LCD_DisplayCANLog(uint16_t can_id, const uint8_t *data, uint8_t is_rx);

/* Hàm Re-init chuyển đổi chân PB6 giữa LCD và CAN (Giải quyết yêu cầu điểm thưởng) */
void LCD_Switch_To_LCD(void);
void LCD_Switch_To_CAN2(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_28INCH_H */
