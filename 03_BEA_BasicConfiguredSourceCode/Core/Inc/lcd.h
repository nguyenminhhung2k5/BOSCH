/**
  ******************************************************************************
  * @file           : lcd.h
  * @brief          : Header for Waveshare 2.8inch SPI LCD (ST7789) on Open405R-C
  *                   Optimized for BOSCH Embedded Academy Kit
  ******************************************************************************
  */

#ifndef __LCD_H__
#define __LCD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ========================================================================== */
/*           CẤU HÌNH CHÂN THEO SƠ ĐỒ NGUYÊN LÝ OPEN405R-C & LCD 2.8"        */
/* ========================================================================== */
/* SPI Clock & Data */
#define LCD_SCK_PORT             GPIOB
#define LCD_SCK_PIN              GPIO_PIN_3     /* Chân 36: SPI1_SCK (PB3) */

#define LCD_MOSI_PORT            GPIOA
#define LCD_MOSI_PIN             GPIO_PIN_7     /* Chân 34: SPI1_MOSI (PA7) */

#define LCD_MISO_PORT            GPIOA
#define LCD_MISO_PIN             GPIO_PIN_6     /* Chân 32: SPI1_MISO (PA6) */

/* Điều khiển hiển thị LCD */
#define LCD_CS_PORT              GPIOB
#define LCD_CS_PIN               GPIO_PIN_7     /* Chân 38: LCD-CS (PB7) */

#define LCD_DC_PORT              GPIOB
#define LCD_DC_PIN               GPIO_PIN_8     /* Chân 40: LCD-RS / DC (PB8) */

#define LCD_RST_PORT             GPIOB
#define LCD_RST_PIN              GPIO_PIN_2     /* Chân 4:  LCD-RST (PB2) */

#define LCD_BL_PORT              GPIOB
#define LCD_BL_PIN               GPIO_PIN_6     /* Chân 6:  LCD_PWM / BL (PB6) - Trùng CAN2_TX */

/* Điều khiển Touch Controller XPT2046 (CÙNG BUS SPI VỚI LCD) */
#define LCD_TP_CS_PORT           GPIOB
#define LCD_TP_CS_PIN            GPIO_PIN_9     /* Chân 42: TP_CS (PB9) - BẮT BUỘC KÉO HIGH ĐỂ KHÓA TOUCH */

#define LCD_TP_IRQ_PORT          GPIOB
#define LCD_TP_IRQ_PIN           GPIO_PIN_4     /* Chân 35: TP_IRQ (PB4) */

/* Kích thước màn hình xoay dọc (Portrait: 240 x 320) */
#define LCD_WIDTH                240U
#define LCD_HEIGHT               320U

/* ========================================================================== */
/*                            BẢNG MÀU CHUẨN RGB565                           */
/* ========================================================================== */
#define LCD_COLOR_BLACK          0x0000
#define LCD_COLOR_WHITE          0xFFFF
#define LCD_COLOR_RED            0xF800
#define LCD_COLOR_GREEN          0x07E0
#define LCD_COLOR_BLUE           0x001F
#define LCD_COLOR_YELLOW         0xFFE0
#define LCD_COLOR_CYAN           0x07FF
#define LCD_COLOR_MAGENTA        0xF81F
#define LCD_COLOR_GREY           0xF7DE
#define LCD_COLOR_GRAY           0x7BEF
#define LCD_COLOR_DARKGRAY       0x39E7
#define LCD_COLOR_DARKBLUE       0x0010

/* Bí danh tương thích */
#define LCD_BLACK                LCD_COLOR_BLACK
#define LCD_WHITE                LCD_COLOR_WHITE
#define LCD_RED                  LCD_COLOR_RED
#define LCD_GREEN                LCD_COLOR_GREEN
#define LCD_BLUE                 LCD_COLOR_BLUE
#define LCD_YELLOW               LCD_COLOR_YELLOW
#define LCD_CYAN                 LCD_COLOR_CYAN
#define LCD_GRAY                 LCD_COLOR_GRAY
#define LCD_DARKGRAY             LCD_COLOR_DARKGRAY
#define LCD_DARKBLUE             LCD_COLOR_DARKBLUE

#define Black                    LCD_COLOR_BLACK
#define White                    LCD_COLOR_WHITE
#define Red                      LCD_COLOR_RED
#define Green                    LCD_COLOR_GREEN
#define Blue                     LCD_COLOR_BLUE
#define Yellow                   LCD_COLOR_YELLOW
#define Cyan                     LCD_COLOR_CYAN
#define Grey                     LCD_COLOR_GREY

#define Line0                    0
#define Line1                    1
#define Line2                    2
#define Line3                    3
#define Line4                    4
#define Line5                    5

/* ========================================================================== */
/*                          PROTOTYPES HÀM HIỂN THỊ                           */
/* ========================================================================== */
void     LCD_GPIO_Init(void);
void     LCD_Init(void);
void     LCD_Init_UI(void);
void     LCD_Clear(uint16_t Color);
void     LCD_SetTextColor(uint16_t Color);
void     LCD_SetBackColor(uint16_t Color);
uint16_t LCD_GetTextColor(void);
uint16_t LCD_GetBackColor(void);

void     LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void     LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void     LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

void     LCD_DrawChar(uint16_t x, uint16_t y, char ch);
void     LCD_DrawCharEx(uint16_t x, uint16_t y, char ch, uint16_t textColor, uint16_t bgColor);

void     LCD_DrawString(uint16_t x, uint16_t y, const char *str);
void     LCD_DrawStringEx(uint16_t x, uint16_t y, const char *str, uint16_t textColor, uint16_t bgColor);
#define  DrawString(x, y, str)   LCD_DrawString((x), (y), (str))

void     LCD_DisplayChar(uint16_t Line, uint16_t Column, uint8_t Ascii);
void     LCD_DisplayStringLine(uint16_t Line, uint8_t *ptr);
void     LCD_ClearLine(uint16_t Line);

void     LCD_DisplayCANLog(uint16_t can_id, const uint8_t *data, uint8_t is_rx);

/* Giao diện dọc (Portrait UI) chuẩn Demo Bosch */
void     LCD_Init_Portrait_UI(void);
void     LCD_Update_Portrait_UI(uint8_t rx_b0, uint8_t rx_b1, uint8_t rx_cnt,
                                uint16_t tx_id, uint8_t tx_b0, uint8_t tx_b1, uint8_t tx_b2, uint8_t tx_crc);
void     LCD_AddLogRecord(const char *log_text, uint16_t color);
void     LCD_DrawChar6x12Ex(uint16_t x, uint16_t y, char ch, uint16_t textColor, uint16_t bgColor);
void     LCD_DrawString6x12Ex(uint16_t x, uint16_t y, const char *str, uint16_t textColor, uint16_t bgColor);

/* Chuyển đổi chân PB6 giữa LCD Backlight và CAN2_TX */
void     LCD_Switch_To_LCD(void);
void     LCD_Switch_To_CAN2(void);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_H__ */
