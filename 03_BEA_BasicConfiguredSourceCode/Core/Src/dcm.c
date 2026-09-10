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

/* Buffer tĩnh an toàn trong SRAM */
static uint8_t s_reqPayload[256];
static uint8_t s_respPayload[256];
static uint8_t s_outFrame[256];

/* Thời điểm xử lý UDS gần nhất để tránh in đè log CAN */
static uint32_t s_lastDiagActivity = 0;

void Dcm_Init(void)
{
    s_currentCANID = 0x0012;
    s_pendingCANID = 0x0012;
    s_lastDiagActivity = 0;

    memset(s_reqPayload, 0, sizeof(s_reqPayload));
    memset(s_respPayload, 0, sizeof(s_respPayload));
    memset(s_outFrame, 0, sizeof(s_outFrame));

    /* Khởi tạo phân hệ Security Access */
    DCM_SECA_Init();
}

uint16_t Dcm_GetCurrentCANID(void)
{
    return s_currentCANID;
}

uint16_t Dcm_GetPendingCANID(void)
{
    return s_pendingCANID;
}

uint8_t Dcm_GetTemperature(void)
{
    extern uint16_t g_TemperatureSensorRawValue_u16[1];
    uint16_t adcRaw = g_TemperatureSensorRawValue_u16[0];
    if (adcRaw == 0)
    {
        return 27; /* Nhiệt độ phòng danh định ban đầu */
    }

    /* V_REF = 3300 mV, 12-bit ADC = 4095 */
    int32_t vsense_mv = ((int32_t)adcRaw * 3300) / 4095;
    /* Temp = ((vsense - 760) * 10 / 25) + 25 */
    int32_t temp_c = (((vsense_mv - 760) * 10) / 25) + 25;

    if (temp_c < 0)   temp_c = 0;
    if (temp_c > 125) temp_c = 125;

    return (uint8_t)temp_c;
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

void Dcm_SendUDSCanFrame(CAN_HandleTypeDef *hcan, uint32_t canId, const uint8_t *payload, uint16_t len)
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

    /* Byte 0: Độ dài của bản tin UDS (CAN-TP Single Frame SF_DL) */
    txData[0] = (uint8_t)len;

    /* Các byte tiếp theo: Nội dung bản tin UDS */
    for (uint8_t i = 0; i < len && i < 7; i++)
    {
        txData[i + 1] = payload[i];
    }

    /* Padding các byte chưa dùng bằng 0x55 theo đề bài Bosch (tránh bit-padding trên bus) */
    for (uint8_t i = (uint8_t)(len + 1); i < 8; i++)
    {
        txData[i] = 0x55;
    }

    /* Nếu không có mailbox trống, hủy mailbox cũ nhất để không bị nghẽn */
    if (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0)
    {
        HAL_CAN_AbortTxRequest(hcan, CAN_TX_MAILBOX0 | CAN_TX_MAILBOX1 | CAN_TX_MAILBOX2);
    }

    HAL_CAN_AddTxMessage(hcan, &txHeader, txData, &mailbox);
}

void Dcm_OnCAN2RequestReceived(const uint8_t *canData, uint8_t dlc)
{
    (void)canData;
    (void)dlc;
}

void Dcm_OnCAN1ResponseReceived(const uint8_t *canData, uint8_t dlc)
{
    (void)canData;
    (void)dlc;
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

        s_lastDiagActivity = HAL_GetTick();
        memset(s_respPayload, 0, sizeof(s_respPayload));

        /* ============================================================
         * 1. SƠ ĐỒ TOPOLOGY (Phát CAN Request ID 0x712):
         * Đóng gói UDS Request nhận từ UART3 thành bản tin CAN ID 0x712
         * và phát lên CAN bus để máy hiện sóng hoặc analyzer phân tích.
         * ============================================================ */
        Dcm_SendUDSCanFrame(&hcan1, DCM_DIAG_CAN_REQ_ID, s_reqPayload, payloadLen);

        /* ============================================================
         * 2. ĐIỀU PHỐI DỊCH VỤ UDS TRỰC TIẾP (Xử lý duy nhất 1 lần):
         * ============================================================ */
        uint8_t sid = s_reqPayload[0];

        switch (sid)
        {
            case 0x22: /* ReadDataByIdentifier (0x0124 Nhiệt độ, 0x0123 CANID) */
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
         * 3. PHÁT BẢN TIN PHẢN HỒI LÊN CAN BUS (Response ID 0x7A2):
         * Đóng gói kết quả thành bản tin phản hồi ID 0x7A2 phát qua CAN2!
         * ============================================================ */
        Dcm_SendUDSCanFrame(&hcan2, DCM_DIAG_CAN_RESP_ID, s_respPayload, respLen);

        /* ============================================================
         * 4. GỬI PHẢN HỒI DUY NHẤT VỀ PC QUA UART3:
         * Phản hồi lập tức và chính xác, không qua vòng lặp trễ hay fallback
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

void Dcm_MainFunction(void)
{
    /* Cập nhật trạng thái thời gian bảo mật và LED-0 (PB0) */
    DCM_SECA_MainFunction();

    /* Quét và xử lý gói tin UDS nhận qua UART3 từ PC */
    Dcm_ProcessUART();
}

uint8_t Dcm_IsDiagActive(void)
{
    /* Báo chẩn đoán đang hoạt động trong vòng 500ms sau request UDS gần nhất */
    return (HAL_GetTick() - s_lastDiagActivity < 500) ? 1 : 0;
}
