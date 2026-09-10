/**
  ******************************************************************************
  * @file           : can_comm.h
  * @brief          : Header for CAN Communication module (Node 1 & Node 2)
  ******************************************************************************
  */

#ifndef __CAN_COMM_H__
#define __CAN_COMM_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#include "main.h"

/* Exported Defines ----------------------------------------------------------*/
#define CAN_COMM_ID_VERIFY_TX       0x0A2U  /* ID Node 2 (Verification Board) phát */
#define CAN_COMM_ID_PRACTICE_TX     0x012U  /* ID Node 1 (Practice Board) phát */
#define CAN_COMM_PAYLOAD_LEN        8U      /* Độ dài DLC cố định 8 bytes */

/* Định nghĩa các chu kỳ thời gian độc lập chuẩn theo bảng đề bài Bosch */
#define CAN_COMM_CYCLE_NODE2_MS     20U     /* Chu kỳ Node 2 phát 0x0A2: 20 ms */
#define CAN_COMM_CYCLE_NODE1_MS     50U     /* Chu kỳ Node 1 phát 0x012: BẮT BUỘC 50 ms */
#define CAN_COMM_CYCLE_LCD_MS       1000U   /* Chu kỳ làm tươi và cuộn Terminal LCD: 1000 ms (1 giây) */

/* Số dòng tối đa hiển thị trên Terminal LCD */
#define LCD_TERMINAL_MAX_LINES      11U

/* Exported Functions Prototypes ---------------------------------------------*/

/**
 * @brief  Khởi tạo các Header TX, bộ đệm và màn hình terminal cho cả 2 Node CAN.
 *         Được gọi 1 lần trong main() trước vòng lặp while(1).
 */
void CanComm_Init(void);

/**
 * @brief  Tác vụ định kỳ (Cyclic Task 20ms) cho Node 2 (Verification Node).
 *         Đóng gói dữ liệu, tăng Rolling Counter và phát khung tin 0x0A2.
 */
void CanComm_Task_Node2_20ms(void);
#define CanComm_Task_Node2_50ms CanComm_Task_Node2_20ms

/**
 * @brief  Tác vụ định kỳ (Cyclic Task 50ms) cho Node 1 (Practice Node).
 *         Lấy dữ liệu đã tính toán gần nhất trong RAM để phát khung tin 0x012.
 */
void CanComm_Task_Node1_50ms(void);

/**
 * @brief  Tác vụ định kỳ (Cyclic Task 1000ms) cập nhật dữ liệu CAN lên màn hình LCD
 *         theo dạng Terminal cuộn dòng liên tục như phần mềm Hercules.
 */
void CanComm_Update_LCD(void);

/**
 * @brief  Đảo trạng thái bật/tắt in log CAN qua UART3 (hỗ trợ nút bấm BtnU).
 */
void CanComm_ToggleLog(void);

/**
 * @brief  Callback khi Node 1 (Practice Node) nhận được khung tin 0x0A2 từ Bus.
 *         Bóc tách Byte 0, Byte 1, tính Byte 2 = Byte 0 + Byte 1,
 *         tính CRC-8 SAE J1850 cho Byte 6 và lưu vào bộ đệm RAM.
 * @param  rxData: Con trỏ trỏ tới 8 bytes dữ liệu nhận được của bản tin 0x0A2.
 */
void CanComm_Node1_OnMsgReceived(const uint8_t *rxData);

/**
 * @brief  Node 2 (Verification Node) thẩm định khung tin 0x012 nhận được từ Node 1.
 *         Kiểm tra phép cộng Byte 2 và kiểm tra mã CRC-8 SAE J1850.
 *         Điều khiển LED PC4 tương ứng khi PASS hoặc FAIL.
 * @param  rxData: Con trỏ trỏ tới 8 bytes dữ liệu nhận được của bản tin 0x012.
 */
void CanComm_Node2_VerifyResponse(const uint8_t *rxData);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_COMM_H__ */
