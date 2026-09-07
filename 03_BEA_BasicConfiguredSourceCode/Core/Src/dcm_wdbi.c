/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#include "dcm_wdbi.h"
#include "dcm.h"
#include "dcm_seca.h"
#include "dcm_rdbi.h"

uint8_t DCM_WDBI_Process(const uint8_t *reqData, uint16_t reqLen, uint8_t *respData, uint16_t *respLen)
{
    /* 1. Kiểm tra quyền bảo mật: BẮT BUỘC Security Level 1 phải đang được mở khóa */
    if (!DCM_SECA_IsUnlocked())
    {
        respData[0] = 0x7F;  /* Negative Response SID */
        respData[1] = 0x2E;  /* Rejected SID */
        respData[2] = 0x33;  /* NRC: SecurityAccessDenied */
        *respLen = 3;
        return 0;
    }

    /* 2. Kiểm tra độ dài tối thiểu của yêu cầu: ít nhất 5 bytes [0x2E, DID_H, DID_L, Data_H, Data_L] */
    if (reqLen < 5)
    {
        respData[0] = 0x7F;
        respData[1] = 0x2E;
        respData[2] = 0x13;  /* NRC: IncorrectMessageLengthOrInvalidFormat */
        *respLen = 3;
        return 0;
    }

    uint16_t did = ((uint16_t)reqData[1] << 8) | reqData[2];

    /* 3. Kiểm tra DID có được hỗ trợ ghi không */
    if (did == DCM_DID_CANID_TESTER) /* 0x0123: Write CANID Value From Tester */
    {
        uint16_t newCanId = ((uint16_t)reqData[3] << 8) | reqData[4];

        /* Lưu lại CAN ID mới chờ Ignition cycle (IG OFF -> IG ON bằng nút PA0) */
        Dcm_SetNewCANIDPending(newCanId);

        /* Phản hồi tích cực: [0x6E, 0x01, 0x23] (theo chuẩn UDS ISO 14229) */
        respData[0] = 0x6E;
        respData[1] = (uint8_t)(DCM_DID_CANID_TESTER >> 8);
        respData[2] = (uint8_t)(DCM_DID_CANID_TESTER & 0xFF);
        *respLen = 3;
        return 1;
    }
    else
    {
        respData[0] = 0x7F;
        respData[1] = 0x2E;
        respData[2] = 0x31;  /* NRC: RequestOutOfRange (DID not supported) */
        *respLen = 3;
        return 0;
    }
}
