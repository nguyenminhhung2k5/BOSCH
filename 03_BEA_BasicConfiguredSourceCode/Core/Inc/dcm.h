/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#ifndef _DCM_H
#define _DCM_H

#include "main.h"
#include "stm32f4xx_it.h"

/* Dịch vụ UDS */
#include "dcm_rdbi.h"
#include "dcm_wdbi.h"
#include "dcm_seca.h"

/* Các mốc framing của giao thức BEA UART */
#define DCM_SOF_0                       0x0F
#define DCM_SOF_1                       0xFF
#define DCM_SOF_2                       0xF0

#define DCM_EOF_0                       0xF0
#define DCM_EOF_1                       0x00
#define DCM_EOF_2                       0x0F

/* Diagnostic CAN IDs theo tài liệu */
#define DCM_DIAG_CAN_REQ_ID             0x712
#define DCM_DIAG_CAN_RESP_ID            0x7A2

/**
 * @brief Khởi tạo hệ thống Diagnostic Communication Manager (DCM)
 */
void Dcm_Init(void);

/**
 * @brief Xử lý chu kỳ trong while(1) (cập nhật timer bảo mật, LED PB0)
 */
void Dcm_MainFunction(void);

/**
 * @brief Quét và xử lý khung tin UDS nhận được qua UART3
 */
void Dcm_ProcessUART(void);

/**
 * @brief Lấy CAN ID hiện tại của Node (DID 0x0123)
 */
uint16_t Dcm_GetCurrentCANID(void);

/**
 * @brief Lưu CAN ID mới được ghi bởi service 0x2E (chờ Ignition cycle)
 */
void Dcm_SetNewCANIDPending(uint16_t newId);

/**
 * @brief Kích hoạt chu trình đánh lửa IG OFF -> IG ON (nhấn nút PA0) để áp dụng CAN ID mới
 */
void Dcm_ApplyIgnitionCycle(void);

/**
 * @brief Đóng gói và gửi phản hồi UDS qua UART3 (bọc khung 0F FF F0 ... F0 00 0F)
 */
void Dcm_SendUDSResponseUART(const uint8_t *respPayload, uint16_t respLen);

/**
 * @brief Đóng gói và phát bản tin CAN chẩn đoán (ID 0x712 hoặc 0x7A2)
 */
void Dcm_SendUDSCanFrame(uint32_t canId, const uint8_t *payload, uint16_t len);

#endif /* _DCM_H */
