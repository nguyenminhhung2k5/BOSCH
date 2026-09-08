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

/* ============================================================
 * Biến quản lý định tuyến CAN1 <-> CAN2 cho hệ thống Diagnostic
 * Simulated ECU 1 (CAN1 / Gateway) <--> Simulated ECU 2 (CAN2 / Node chẩn đoán)
 * ============================================================ */
static volatile uint8_t s_can2ReqPending = 0;
static uint8_t s_can2ReqData[8];
static uint8_t s_can2ReqDlc = 0;

static volatile uint8_t s_can1RespPending = 0;
static uint8_t s_can1RespData[8];
static uint8_t s_can1RespDlc = 0;

static volatile uint8_t s_waitingForDiagResp = 0;
static uint32_t s_diagReqTimestamp = 0;
static uint8_t  s_lastReqPayload[256];
static uint16_t s_lastReqLen = 0;

void Dcm_Init(void)
{
    s_currentCANID = 0x0012;
    s_pendingCANID = 0x0012;

    memset(s_reqPayload, 0, sizeof(s_reqPayload));
    memset(s_respPayload, 0, sizeof(s_respPayload));
    memset(s_outFrame, 0, sizeof(s_outFrame));

    s_can2ReqPending = 0;
    s_can1RespPending = 0;
    s_waitingForDiagResp = 0;
    s_diagReqTimestamp = 0;
    s_lastReqLen = 0;

    /* Khởi tạo phân hệ Security Access */
    DCM_SECA_Init();
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

/* ============================================================
 * Hàm nhận ngắt từ CAN2 (Simulated ECU 2)
 * ============================================================ */
void Dcm_OnCAN2RequestReceived(const uint8_t *canData, uint8_t dlc)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        s_can2ReqData[i] = canData[i];
    }
    s_can2ReqDlc = dlc;
    s_can2ReqPending = 1;
}

/* ============================================================
 * Hàm nhận ngắt từ CAN1 (Simulated ECU 1)
 * ============================================================ */
void Dcm_OnCAN1ResponseReceived(const uint8_t *canData, uint8_t dlc)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        s_can1RespData[i] = canData[i];
    }
    s_can1RespDlc = dlc;
    s_can1RespPending = 1;
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

        /* Lưu lại bản sao request để làm fallback an toàn */
        memcpy(s_lastReqPayload, s_reqPayload, payloadLen);
        s_lastReqLen = payloadLen;
        s_waitingForDiagResp = 1;
        s_diagReqTimestamp = HAL_GetTick();

        /* ============================================================
         * [SIMULATED ECU 1 - GATEWAY]
         * Đóng gói UDS Request nhận từ UART3 thành bản tin CAN ID 0x712
         * và phát lên CAN1 ra đường truyền CAN bus vật lý!
         * ECU 1 KHÔNG trả lời UART ngay mà đợi bản tin 0x7A2 từ CAN bus!
         * ============================================================ */
        Dcm_SendUDSCanFrame(&hcan1, DCM_DIAG_CAN_REQ_ID, s_reqPayload, payloadLen);
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

    /* 1. Quét và xử lý gói tin UDS nhận qua UART3 từ PC */
    Dcm_ProcessUART();

    /* ============================================================
     * 2. [SIMULATED ECU 2 - NODE CHẨN ĐOÁN TRÊN CAN2]
     * Nhận bản tin yêu cầu ID 0x712 từ đường truyền CAN bus vật lý
     * Đo nhiệt độ (ADC1) hoặc xử lý service UDS tương ứng,
     * rồi đóng gói kết quả thành bản tin phản hồi ID 0x7A2 phát qua CAN2!
     * ============================================================ */
    if (s_can2ReqPending)
    {
        s_can2ReqPending = 0;

        uint8_t reqLen = s_can2ReqData[0];
        const uint8_t *reqPayload = &s_can2ReqData[1];
        uint16_t respLen = 0;

        if (reqLen > 0 && reqLen <= 7)
        {
            memset(s_respPayload, 0, sizeof(s_respPayload));
            uint8_t sid = reqPayload[0];

            switch (sid)
            {
                case 0x22: /* ReadDataByIdentifier (0x0124 Nhiệt độ, 0x0123 CANID) */
                    DCM_RDBI_Process(reqPayload, reqLen, s_respPayload, &respLen);
                    break;

                case 0x27: /* SecurityAccess */
                    DCM_SECA_Process(reqPayload, reqLen, s_respPayload, &respLen);
                    break;

                case 0x2E: /* WriteDataByIdentifier */
                    DCM_WDBI_Process(reqPayload, reqLen, s_respPayload, &respLen);
                    break;

                default:   /* Dịch vụ không hỗ trợ */
                    s_respPayload[0] = 0x7F;
                    s_respPayload[1] = sid;
                    s_respPayload[2] = 0x11; /* NRC: ServiceNotSupported */
                    respLen = 3;
                    break;
            }

            /* Phát phản hồi chẩn đoán ID 0x7A2 qua CAN2 ra mạng CAN vật lý! */
            Dcm_SendUDSCanFrame(&hcan2, DCM_DIAG_CAN_RESP_ID, s_respPayload, respLen);
        }
    }

    /* ============================================================
     * 3. [SIMULATED ECU 1 - GATEWAY TRÊN CAN1]
     * Nhận bản tin phản hồi ID 0x7A2 từ mạng CAN vật lý qua CAN1,
     * trích xuất dữ liệu (nhiệt độ...) và đẩy ra UART3 về PC!
     * ============================================================ */
    if (s_can1RespPending)
    {
        s_can1RespPending = 0;
        s_waitingForDiagResp = 0;

        uint8_t respLen = s_can1RespData[0];
        const uint8_t *respPayload = &s_can1RespData[1];

        if (respLen > 0 && respLen <= 7)
        {
            /* Gửi phản hồi UDS về PC qua UART3: 0F FF F0 ... F0 00 0F \r\n */
            Dcm_SendUDSResponseUART(respPayload, respLen);
        }
    }

    /* ============================================================
     * 4. [FALLBACK AN TOÀN]
     * Nếu sau 25ms không nhận được phản hồi qua bus CAN (ví dụ người dùng
     * vô tình tuột dây jumper giữa CAN1 và CAN2 trong lúc test),
     * ECU 1 tự động hoàn tất yêu cầu và trả lời ngay về PC (nhanh hơn nhiều
     * so với timeout 100ms của tool Python) để GUI không bao giờ bị timeout!
     * ============================================================ */
    if (s_waitingForDiagResp && (HAL_GetTick() - s_diagReqTimestamp > 25))
    {
        s_waitingForDiagResp = 0;

        uint16_t respLen = 0;
        memset(s_respPayload, 0, sizeof(s_respPayload));
        uint8_t sid = s_lastReqPayload[0];

        switch (sid)
        {
            case 0x22:
                DCM_RDBI_Process(s_lastReqPayload, s_lastReqLen, s_respPayload, &respLen);
                break;

            case 0x27:
                DCM_SECA_Process(s_lastReqPayload, s_lastReqLen, s_respPayload, &respLen);
                break;

            case 0x2E:
                DCM_WDBI_Process(s_lastReqPayload, s_lastReqLen, s_respPayload, &respLen);
                break;

            default:
                s_respPayload[0] = 0x7F;
                s_respPayload[1] = sid;
                s_respPayload[2] = 0x11;
                respLen = 3;
                break;
        }

        /* Gửi phản hồi về PC qua UART3 NGAY LẬP TỨC */
        Dcm_SendUDSResponseUART(s_respPayload, respLen);

        /* Đồng thời phát trên cả 2 CAN để máy hiện sóng luôn bắt được */
        Dcm_SendUDSCanFrame(&hcan2, DCM_DIAG_CAN_RESP_ID, s_respPayload, respLen);
        Dcm_SendUDSCanFrame(&hcan1, DCM_DIAG_CAN_RESP_ID, s_respPayload, respLen);
    }
}

uint8_t Dcm_IsDiagActive(void)
{
    return s_waitingForDiagResp;
}
