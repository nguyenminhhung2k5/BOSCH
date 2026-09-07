/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#include "dcm.h"
#include <string.h>
#include <stdio.h>

/* Khai báo ngoại vi từ main.c */
extern UART_HandleTypeDef huart3;
extern CAN_HandleTypeDef  hcan1;
extern CAN_HandleTypeDef  hcan2;
extern CAN_TxHeaderTypeDef CAN1_pHeader;
extern uint32_t CAN1_pTxMailbox;
extern uint32_t CAN2_pTxMailbox;

extern volatile uint16_t NumBytesReq;
extern volatile uint8_t  REQ_BUFFER[4096];

/* Biến lưu CAN ID hiện tại và CAN ID đang chờ áp dụng sau chu trình IG cycle */
static uint16_t s_currentCANID = 0x0012;
static uint16_t s_pendingCANID = 0x0012;

/* Các buffer tĩnh trong SRAM để tránh tràn Stack (Stack Cortex-M chỉ có 1KB) */
static uint8_t s_reqPayload[256];
static uint8_t s_respPayload[256];
static uint8_t s_outFrame[256];

void Dcm_Init(void)
{
    s_currentCANID = 0x0012;
    s_pendingCANID = 0x0012;

    memset(s_reqPayload, 0, sizeof(s_reqPayload));
    memset(s_respPayload, 0, sizeof(s_respPayload));
    memset(s_outFrame, 0, sizeof(s_outFrame));

    /* Khởi tạo phân hệ Security Access */
    DCM_SECA_Init();
}

void Dcm_MainFunction(void)
{
    /* Cập nhật trạng thái thời gian bảo mật và LED-0 (PB0) */
    DCM_SECA_MainFunction();

    /* Quét và xử lý gói tin UDS nhận qua UART3 */
    Dcm_ProcessUART();
}

uint16_t Dcm_GetCurrentCANID(void)
{
    return s_currentCANID;
}

void Dcm_SetNewCANIDPending(uint16_t newId)
{
    s_pendingCANID = newId;
}

void Dcm_ApplyIgnitionCycle(void)
{
    /* Áp dụng CAN ID mới sau khi người dùng bấm nút PA0 (IG OFF -> ON) */
    s_currentCANID = s_pendingCANID;
}

void Dcm_SendUDSResponseUART(const uint8_t *respPayload, uint16_t respLen)
{
    uint16_t outIdx = 0;

    /* 1. Thêm Start of Frame: 0F FF F0 */
    s_outFrame[outIdx++] = DCM_SOF_0;
    s_outFrame[outIdx++] = DCM_SOF_1;
    s_outFrame[outIdx++] = DCM_SOF_2;

    /* 2. Thêm Payload dữ liệu phản hồi UDS */
    for (uint16_t i = 0; i < respLen && outIdx < 250; i++)
    {
        s_outFrame[outIdx++] = respPayload[i];
    }

    /* 3. Thêm End of Frame: F0 00 0F */
    s_outFrame[outIdx++] = DCM_EOF_0;
    s_outFrame[outIdx++] = DCM_EOF_1;
    s_outFrame[outIdx++] = DCM_EOF_2;

    /* 4. Thêm ký tự xuống dòng CRLF theo chuẩn tool Python */
    s_outFrame[outIdx++] = '\r';
    s_outFrame[outIdx++] = '\n';

    /* 5. Truyền qua UART3 */
    HAL_UART_Transmit(&huart3, s_outFrame, outIdx, 100);
}

void Dcm_SendUDSCanFrame(uint32_t canId, const uint8_t *payload, uint16_t len)
{
    CAN_TxHeaderTypeDef txHeader;
    uint8_t txData[8] = {0};
    uint32_t mailbox;

    txHeader.StdId = canId;
    txHeader.ExtId = 0x00;
    txHeader.IDE = CAN_ID_STD;
    txHeader.RTR = CAN_RTR_DATA;
    txHeader.DLC = 8;
    txHeader.TransmitGlobalTime = DISABLE;

    /* Byte 0: Độ dài của bản tin UDS (Single Frame DLC) */
    txData[0] = (uint8_t)len;

    /* Các byte tiếp theo: Nội dung bản tin UDS */
    for (uint8_t i = 0; i < len && i < 7; i++)
    {
        txData[i + 1] = payload[i];
    }

    /* Nếu là Request (0x712), phát trên CAN1 */
    if (canId == DCM_DIAG_CAN_REQ_ID)
    {
        HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &mailbox);
    }
    /* Nếu là Response (0x7A2), phát trên CAN2 (hoặc CAN1 nếu CAN2 chưa cắm) */
    else
    {
        if (HAL_CAN_AddTxMessage(&hcan2, &txHeader, txData, &mailbox) != HAL_OK)
        {
            /* Fallback phát trên CAN1 để máy hiện sóng hoặc analyzer luôn bắt được */
            HAL_CAN_AddTxMessage(&hcan1, &txHeader, txData, &mailbox);
        }
    }
}

void Dcm_ProcessUART(void)
{
    /* Kiểm tra buffer UART có đủ kích thước tối thiểu (3 SOF + ít nhất 1 payload + 3 EOF = 7 bytes) */
    if (NumBytesReq < 7)
    {
        return;
    }

    /* Tìm Start of Frame: 0F FF F0 */
    int16_t sofIndex = -1;
    for (uint16_t i = 0; i + 2 < NumBytesReq; i++)
    {
        if (REQ_BUFFER[i] == DCM_SOF_0 &&
            REQ_BUFFER[i + 1] == DCM_SOF_1 &&
            REQ_BUFFER[i + 2] == DCM_SOF_2)
        {
            sofIndex = i;
            break;
        }
    }

    if (sofIndex < 0)
    {
        /* Chưa tìm thấy SOF, nếu buffer quá đầy thì xóa bớt để tránh tràn */
        if (NumBytesReq > 3000)
        {
            NumBytesReq = 0;
        }
        return;
    }

    /* Tìm End of Frame: F0 00 0F */
    int16_t eofIndex = -1;
    for (uint16_t i = sofIndex + 3; i + 2 < NumBytesReq; i++)
    {
        if (REQ_BUFFER[i] == DCM_EOF_0 &&
            REQ_BUFFER[i + 1] == DCM_EOF_1 &&
            REQ_BUFFER[i + 2] == DCM_EOF_2)
        {
            eofIndex = i;
            break;
        }
    }

    if (eofIndex < 0)
    {
        /* Chưa nhận đủ EOF, tiếp tục chờ thêm byte */
        return;
    }

    /* Đã nhận đủ một khung UDS hoàn chỉnh! */
    uint16_t payloadStart = sofIndex + 3;
    uint16_t payloadLen = eofIndex - payloadStart;

    if (payloadLen > 0 && payloadLen < 256)
    {
        uint16_t respLen = 0;

        /* Khóa ngắt tạm thời để sao chép payload và dịch chuyển buffer an toàn */
        __disable_irq();
        memcpy(s_reqPayload, (const void *)&REQ_BUFFER[payloadStart], payloadLen);

        /* Dịch chuyển các byte còn lại trong REQ_BUFFER về đầu (nếu có byte đến sau EOF) */
        uint16_t processedBytes = eofIndex + 3;
        if (NumBytesReq > processedBytes)
        {
            uint16_t remaining = NumBytesReq - processedBytes;
            memmove((void *)REQ_BUFFER, (const void *)&REQ_BUFFER[processedBytes], remaining);
            NumBytesReq = remaining;
        }
        else
        {
            NumBytesReq = 0;
            memset((void *)REQ_BUFFER, 0, 128);
        }
        __enable_irq();

        memset(s_respPayload, 0, sizeof(s_respPayload));

        /* ============================================================
         * 1. SƠ ĐỒ TOPOLOGY (Simulated ECU 1 SW):
         * Phát bản tin chẩn đoán CAN Request ID 0x712 lên CAN BUS
         * Để máy hiện sóng (Oscilloscope) hoặc CAN Tool phân tích
         * ============================================================ */
        Dcm_SendUDSCanFrame(DCM_DIAG_CAN_REQ_ID, s_reqPayload, payloadLen);

        /* ============================================================
         * 2. ĐIỀU PHỐI DỊCH VỤ UDS THEO SID (Simulated ECU 2 SW)
         * ============================================================ */
        uint8_t sid = s_reqPayload[0];

        switch (sid)
        {
            case 0x22: /* ReadDataByIdentifier */
                DCM_RDBI_Process(s_reqPayload, payloadLen, s_respPayload, &respLen);
                break;

            case 0x27: /* SecurityAccess */
                DCM_SECA_Process(s_reqPayload, payloadLen, s_respPayload, &respLen);
                break;

            case 0x2E: /* WriteDataByIdentifier */
                DCM_WDBI_Process(s_reqPayload, payloadLen, s_respPayload, &respLen);
                break;

            default:   /* Dịch vụ không hỗ trợ */
                s_respPayload[0] = 0x7F;
                s_respPayload[1] = sid;
                s_respPayload[2] = 0x11; /* NRC: ServiceNotSupported */
                respLen = 3;
                break;
        }

        /* ============================================================
         * 3. PHÁT BẢN TIN PHẢN HỒI LÊN CAN BUS (Response ID 0x7A2)
         * ============================================================ */
        Dcm_SendUDSCanFrame(DCM_DIAG_CAN_RESP_ID, s_respPayload, respLen);

        /* ============================================================
         * 4. GỬI PHẢN HỒI VỀ PC QUA UART3 (Đóng gói 0F FF F0 ... F0 00 0F)
         * ============================================================ */
        Dcm_SendUDSResponseUART(s_respPayload, respLen);
    }
    else
    {
        __disable_irq();
        NumBytesReq = 0;
        memset((void *)REQ_BUFFER, 0, 128);
        __enable_irq();
    }
}
