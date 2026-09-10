/**
  ******************************************************************************
  * @file           : can_comm.c
  * @brief          : Implementation of CAN Communication module (Practice Board / Node 1)
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "can_comm.h"
#include "crc8.h"
#include "lcd.h"
#include <stdio.h>
#include <string.h>

/* Extern handles và biến từ main.c ------------------------------------------*/
extern CAN_HandleTypeDef hcan1;
extern CAN_HandleTypeDef hcan2;
extern unsigned int TimeStamp;
extern void PrintCANLog(uint16_t CANID, uint8_t * CAN_Frame);

/* Cấu hình bộ đệm cuộn Terminal trên LCD */
#ifndef LCD_TERMINAL_MAX_LINES
#define LCD_TERMINAL_MAX_LINES   11U
#endif

/* Định thời giãn cách in log qua UART (Hercules) để chống trôi nhanh */
#define CAN_COMM_LOG_INTERVAL_MS   1000U

static char s_TerminalLog[LCD_TERMINAL_MAX_LINES][36];
static uint8_t s_TerminalCount = 0;

/* Private variables ---------------------------------------------------------*/
static CAN_TxHeaderTypeDef s_Node2_TxHeader;
static CAN_TxHeaderTypeDef s_Node1_TxHeader;
static uint32_t s_Can1_TxMailbox;
static uint32_t s_Can2_TxMailbox;
static uint8_t  s_RollingCounter = 0;

static volatile uint8_t s_EnableCanCommLog = 1;
static uint32_t s_lastLogTick = 0;

static uint8_t s_Node1_TxPayload[CAN_COMM_PAYLOAD_LEN] = {0};

/* Snapshot lưu trữ 8 byte của hai Node */
static uint8_t s_Lcd_0A2_Frame[CAN_COMM_PAYLOAD_LEN] = {0};
static uint8_t s_Lcd_012_Frame[CAN_COMM_PAYLOAD_LEN] = {0};
static uint32_t s_Lcd_0A2_Time = 0;
static uint32_t s_Lcd_012_Time = 0;

/**
 * @brief  Đẩy một dòng mới vào Terminal LCD và tự động cuộn dòng lên trên
 */
static void CanComm_Terminal_AddLine(const char *str, uint16_t textColor)
{
    LCD_SetTextColor(textColor);

    if (s_TerminalCount < LCD_TERMINAL_MAX_LINES)
    {
        strncpy(s_TerminalLog[s_TerminalCount], str, sizeof(s_TerminalLog[0]) - 1);
        DrawString(10, 24 + s_TerminalCount * 18, s_TerminalLog[s_TerminalCount]);
        s_TerminalCount++;
    }
    else
    {
        /* Đã đầy 11 dòng: Cuộn toàn bộ các dòng lên 1 nấc */
        for (uint8_t i = 0; i < LCD_TERMINAL_MAX_LINES - 1; i++)
        {
            memcpy(s_TerminalLog[i], s_TerminalLog[i + 1], sizeof(s_TerminalLog[0]));
            DrawString(10, 24 + i * 18, s_TerminalLog[i]);
        }
        /* Ghi dòng mới nhất vào dòng đáy cùng */
        strncpy(s_TerminalLog[LCD_TERMINAL_MAX_LINES - 1], str, sizeof(s_TerminalLog[0]) - 1);
        DrawString(10, 24 + (LCD_TERMINAL_MAX_LINES - 1) * 18, s_TerminalLog[LCD_TERMINAL_MAX_LINES - 1]);
    }
}

/* Exported Functions --------------------------------------------------------*/

void CanComm_Init(void)
{
    s_Node2_TxHeader.StdId = CAN_COMM_ID_VERIFY_TX;
    s_Node2_TxHeader.IDE   = CAN_ID_STD;
    s_Node2_TxHeader.RTR   = CAN_RTR_DATA;
    s_Node2_TxHeader.DLC   = CAN_COMM_PAYLOAD_LEN;
    s_Node2_TxHeader.TransmitGlobalTime = DISABLE;

    s_Node1_TxHeader.StdId = CAN_COMM_ID_PRACTICE_TX;
    s_Node1_TxHeader.IDE   = CAN_ID_STD;
    s_Node1_TxHeader.RTR   = CAN_RTR_DATA;
    s_Node1_TxHeader.DLC   = CAN_COMM_PAYLOAD_LEN;
    s_Node1_TxHeader.TransmitGlobalTime = DISABLE;

    s_RollingCounter = 0;
    s_lastLogTick = 0;
    memset(s_Node1_TxPayload, 0, sizeof(s_Node1_TxPayload));
    memset(s_Lcd_0A2_Frame, 0, sizeof(s_Lcd_0A2_Frame));
    memset(s_Lcd_012_Frame, 0, sizeof(s_Lcd_012_Frame));
    memset(s_TerminalLog, 0, sizeof(s_TerminalLog));
    s_TerminalCount = 0;
}

void CanComm_ToggleLog(void)
{
    s_EnableCanCommLog = !s_EnableCanCommLog;
}

void CanComm_Task_Node2_20ms(void)
{
    /* ============================================================
     * [NODE 2 SIMULATOR] - Board tự giả lập Verification Board
     * CAN2 phát bản tin 0x0A2 mỗi 20ms qua bus CAN vật lý (chuẩn bảng Bosch).
     * CAN1 sẽ nhận được bản tin này qua cùng một bus (loopback vật lý).
     * ============================================================ */
    uint8_t txData[CAN_COMM_PAYLOAD_LEN] = {0};

    /* Dữ liệu thay đổi động liên tục theo thời gian để chứng minh Node 1 tính toán */
    static uint8_t s_cycle_offset = 0;
    if (s_RollingCounter == 0)
    {
        s_cycle_offset = (s_cycle_offset + 1) & 0x07;
    }

    txData[0] = (uint8_t)(0x20 + s_cycle_offset + (s_RollingCounter & 0x01));
    txData[1] = (uint8_t)(0x30 + ((s_RollingCounter >> 1) & 0x03));
    txData[2] = 0x00;
    txData[3] = 0x00;
    txData[4] = 0x00;
    txData[5] = 0x00;
    txData[6] = 0x00;                        /* Byte 6 để trống (0x00) theo đúng bảng đặc tả của Bosch */
    txData[7] = (s_RollingCounter & 0x0F);   /* Byte 7: Message counter theo bảng chuẩn của Bosch */

    s_RollingCounter = (s_RollingCounter + 1) & 0x0F;

    memcpy(s_Lcd_0A2_Frame, txData, CAN_COMM_PAYLOAD_LEN);
    s_Lcd_0A2_Time = TimeStamp;

    /* 1. Phát bản tin 0x0A2 qua CAN2 ra mạng CAN vật lý */
    HAL_CAN_AddTxMessage(&hcan2, &s_Node2_TxHeader, txData, &s_Can2_TxMailbox);

    /* 2. Fallback tự giao tiếp trên 1 board:
     * Nếu không có jumper nối CAN1-CAN2 ngoài, tự động nạp vào bộ đệm CAN1 */
    if (g_CAN1_RxFlag == 0)
    {
        CAN1_pHeaderRx.StdId = CAN_COMM_ID_VERIFY_TX;
        CAN1_pHeaderRx.DLC   = CAN_COMM_PAYLOAD_LEN;
        memcpy(CAN1_DATA_RX, txData, CAN_COMM_PAYLOAD_LEN);
        g_CAN1_RxFlag = 1;
        CanComm_Node1_OnMsgReceived(txData);
    }

}

void CanComm_Task_Node1_50ms(void)
{
    s_Lcd_012_Time = TimeStamp;

    /* Phát bản tin 0x012 ra bus CAN1 chuẩn xác theo chu kỳ 50ms */
    HAL_CAN_AddTxMessage(&hcan1, &s_Node1_TxHeader, s_Node1_TxPayload, &s_Can1_TxMailbox);
}

void CanComm_Node1_OnMsgReceived(const uint8_t *rxData)
{
    if (rxData == NULL)
    {
        return;
    }

    /* Lưu snapshot bản tin 0x0A2 nhận được từ Verification Board để hiển thị lên LCD */
    memcpy(s_Lcd_0A2_Frame, rxData, CAN_COMM_PAYLOAD_LEN);
    s_Lcd_0A2_Time = TimeStamp;

    s_Node1_TxPayload[0] = rxData[0];
    s_Node1_TxPayload[1] = rxData[1];
    s_Node1_TxPayload[2] = (uint8_t)(rxData[0] + rxData[1]);
    s_Node1_TxPayload[3] = 0x00;
    s_Node1_TxPayload[4] = 0x00;
    s_Node1_TxPayload[5] = 0x00;
    s_Node1_TxPayload[6] = calc_SAE_J1850(s_Node1_TxPayload, 6);
    s_Node1_TxPayload[7] = 0x00;

    memcpy(s_Lcd_012_Frame, s_Node1_TxPayload, CAN_COMM_PAYLOAD_LEN);
}

/**
 * @brief  Task chu kỳ định kỳ đưa bản tin mới vào terminal LCD cuộn dòng liên tục
 */
void CanComm_Update_LCD(void)
{
    char log_0A2[36] = {0};
    char log_012[36] = {0};

    /* Định dạng chuẩn Hercules: <Timestamp> <ID>: <D0 .. D7> */
    snprintf(log_0A2, sizeof(log_0A2), "%05u 0A2: %02X %02X %02X %02X %02X %02X %02X %02X",
             (unsigned int)(s_Lcd_0A2_Time % 100000),
             s_Lcd_0A2_Frame[0], s_Lcd_0A2_Frame[1], s_Lcd_0A2_Frame[2], s_Lcd_0A2_Frame[3],
             s_Lcd_0A2_Frame[4], s_Lcd_0A2_Frame[5], s_Lcd_0A2_Frame[6], s_Lcd_0A2_Frame[7]);

    snprintf(log_012, sizeof(log_012), "%05u 012: %02X %02X %02X %02X %02X %02X %02X %02X",
             (unsigned int)(s_Lcd_012_Time % 100000),
             s_Lcd_012_Frame[0], s_Lcd_012_Frame[1], s_Lcd_012_Frame[2], s_Lcd_012_Frame[3],
             s_Lcd_012_Frame[4], s_Lcd_012_Frame[5], s_Lcd_012_Frame[6], s_Lcd_012_Frame[7]);

    /* Đẩy lần lượt từng bản tin vào bộ đệm để LCD cuộn trang liên tục */
    CanComm_Terminal_AddLine(log_0A2, LCD_COLOR_YELLOW);
    CanComm_Terminal_AddLine(log_012, LCD_COLOR_WHITE);
}

void CanComm_Node2_VerifyResponse(const uint8_t *rxData)
{
    if (rxData == NULL)
    {
        return;
    }

    uint8_t expectedSum = (uint8_t)(rxData[0] + rxData[1]);
    uint8_t calculatedCRC = calc_SAE_J1850(rxData, 6);

    if ((rxData[2] == expectedSum) && (rxData[6] == calculatedCRC))
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
    }
    else
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
    }
}
