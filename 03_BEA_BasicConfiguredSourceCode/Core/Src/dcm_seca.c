/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#include "dcm_seca.h"

/* External ADC temperature buffer for random seed generation */
extern uint16_t g_TemperatureSensorRawValue_u16[1];

/* State variables */
static uint8_t  s_isUnlocked = 0;
static uint32_t s_unlockExpireTime = 0;
static uint32_t s_penaltyDelayExpireTime = 0;
static uint8_t  s_currentSeed[4] = {0x12, 0x34, 0x56, 0x78};
static uint8_t  s_seedGenerated = 0;

void DCM_SECA_Init(void)
{
    s_isUnlocked = 0;
    s_unlockExpireTime = 0;
    s_penaltyDelayExpireTime = 0;
    s_seedGenerated = 0;

    /* Tắt LED-0 (PB0) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

void DCM_SECA_MainFunction(void)
{
    uint32_t now = HAL_GetTick();

    /* 1. Kiểm tra hết hạn mở khóa sau 5 giây */
    if (s_isUnlocked)
    {
        if (now >= s_unlockExpireTime)
        {
            s_isUnlocked = 0;
            s_seedGenerated = 0;
            /* Tắt LED-0 (PB0) khi hết hạn 5 giây */
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        }
        else
        {
            /* Giữ LED-0 (PB0) sáng trong suốt thời gian mở khóa */
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
        }
    }

    /* 2. Kiểm tra hết thời gian phạt 10 giây nếu nhập sai key */
    if (s_penaltyDelayExpireTime > 0)
    {
        if (now >= s_penaltyDelayExpireTime)
        {
            s_penaltyDelayExpireTime = 0;
        }
    }
}

uint8_t DCM_SECA_IsUnlocked(void)
{
    return s_isUnlocked;
}

/**
 * @brief Sinh 4 byte SEED ngẫu nhiên (khác 00000000h và FFFFFFFFh)
 */
static void DCM_GenerateRandomSeed(uint8_t *seedOut)
{
    uint32_t tick = HAL_GetTick();
    uint16_t adc = g_TemperatureSensorRawValue_u16[0];

    seedOut[0] = (uint8_t)(tick & 0xFF) ^ (uint8_t)(adc & 0xFF);
    seedOut[1] = (uint8_t)((tick >> 8) & 0xFF) + 0x3A;
    seedOut[2] = (uint8_t)((tick >> 16) & 0xFF) ^ (uint8_t)((adc >> 4) & 0xFF);
    seedOut[3] = (uint8_t)((tick >> 24) & 0xFF) + 0x7B;

    /* Đảm bảo không bị trùng 00 00 00 00 hoặc FF FF FF FF */
    if (seedOut[0] == 0x00 && seedOut[1] == 0x00 && seedOut[2] == 0x00 && seedOut[3] == 0x00)
    {
        seedOut[0] = 0x24;
        seedOut[3] = 0x68;
    }
    else if (seedOut[0] == 0xFF && seedOut[1] == 0xFF && seedOut[2] == 0xFF && seedOut[3] == 0xFF)
    {
        seedOut[0] = 0xAA;
        seedOut[3] = 0x55;
    }
}

uint8_t DCM_SECA_Process(const uint8_t *reqData, uint16_t reqLen, uint8_t *respData, uint16_t *respLen)
{
    /* Kiểm tra độ dài tối thiểu của yêu cầu: ít nhất 2 bytes [0x27, SubFunction] */
    if (reqLen < 2)
    {
        respData[0] = 0x7F;  /* Negative Response SID */
        respData[1] = 0x27;  /* Rejected SID */
        respData[2] = 0x13;  /* NRC: IncorrectMessageLengthOrInvalidFormat */
        *respLen = 3;
        return 0;
    }

    uint8_t subFunc = reqData[1];

    /* ============================================================
     * SUB-FUNCTION 0x01: Request Seed (Level 1)
     * ============================================================ */
    if (subFunc == DCM_SECA_SUBFUNC_REQ_SEED)
    {
        /* Yêu cầu Request Seed phải đúng 2 bytes [0x27, 0x01] */
        if (reqLen != 2)
        {
            respData[0] = 0x7F;
            respData[1] = 0x27;
            respData[2] = 0x13;  /* NRC: IncorrectMessageLengthOrInvalidFormat */
            *respLen = 3;
            return 0;
        }

        /* Kiểm tra nếu đang trong thời gian bị phạt 10 giây do nhập sai key trước đó */
        if (s_penaltyDelayExpireTime > 0 && HAL_GetTick() < s_penaltyDelayExpireTime)
        {
            respData[0] = 0x7F;
            respData[1] = 0x27;
            respData[2] = 0x37;  /* NRC: RequiredTimeDelayNotExpired */
            *respLen = 3;
            return 0;
        }

        /* Sinh 4 byte SEED ngẫu nhiên */
        DCM_GenerateRandomSeed(s_currentSeed);
        s_seedGenerated = 1;

        /* Phản hồi tích cực: [0x67, 0x01, SEED0, SEED1, SEED2, SEED3] */
        respData[0] = 0x67;
        respData[1] = DCM_SECA_SUBFUNC_REQ_SEED;
        respData[2] = s_currentSeed[0];
        respData[3] = s_currentSeed[1];
        respData[4] = s_currentSeed[2];
        respData[5] = s_currentSeed[3];
        *respLen = 6;
        return 1;
    }

    /* ============================================================
     * SUB-FUNCTION 0x02: Send Key (Level 1)
     * ============================================================ */
    else if (subFunc == DCM_SECA_SUBFUNC_SEND_KEY)
    {
        /* Yêu cầu Send Key phải đúng 6 bytes [0x27, 0x02, KEY0, KEY1, KEY2, KEY3] */
        if (reqLen != 6)
        {
            respData[0] = 0x7F;
            respData[1] = 0x27;
            respData[2] = 0x13;  /* NRC: IncorrectMessageLengthOrInvalidFormat */
            *respLen = 3;
            return 0;
        }

        /* Phải có yêu cầu Request Seed trước đó */
        if (!s_seedGenerated)
        {
            respData[0] = 0x7F;
            respData[1] = 0x27;
            respData[2] = 0x35;  /* NRC: InvalidKeys (hoặc RequestSequenceError) */
            *respLen = 3;
            return 0;
        }

        /* Tính toán KEY chuẩn theo đề bài:
         * KEY-0 = SEED-0 XOR SEED-1
         * KEY-1 = SEED-1  +  SEED-2
         * KEY-2 = SEED-2 XOR SEED-3
         * KEY-3 = SEED-3  +  SEED-0
         */
        uint8_t expectedKey[4];
        expectedKey[0] = s_currentSeed[0] ^ s_currentSeed[1];
        expectedKey[1] = (uint8_t)(s_currentSeed[1] + s_currentSeed[2]);
        expectedKey[2] = s_currentSeed[2] ^ s_currentSeed[3];
        expectedKey[3] = (uint8_t)(s_currentSeed[3] + s_currentSeed[0]);

        /* So sánh key tester gửi tới với key mong đợi */
        if ((reqData[2] == expectedKey[0]) &&
            (reqData[3] == expectedKey[1]) &&
            (reqData[4] == expectedKey[2]) &&
            (reqData[5] == expectedKey[3]))
        {
            /* KEY HỢP LỆ -> Mở khóa ECU thành công! */
            s_isUnlocked = 1;
            s_unlockExpireTime = HAL_GetTick() + DCM_SECA_UNLOCK_TIMEOUT_MS; /* Đếm ngược 5 giây */
            s_seedGenerated = 0;

            /* Bật LED-0 (PB0) để chỉ thị mở khoá */
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

            /* Phản hồi tích cực: [0x67, 0x02] */
            respData[0] = 0x67;
            respData[1] = DCM_SECA_SUBFUNC_SEND_KEY;
            *respLen = 2;
            return 1;
        }
        else
        {
            /* KEY KHÔNG HỢP LỆ -> Khóa và phạt 10 giây delay */
            s_isUnlocked = 0;
            s_seedGenerated = 0;
            s_penaltyDelayExpireTime = HAL_GetTick() + DCM_SECA_PENALTY_DELAY_MS; /* Khóa phạt 10s */

            /* Tắt LED-0 (PB0) */
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

            /* Phản hồi tiêu cực: [0x7F, 0x27, 0x35] */
            respData[0] = 0x7F;
            respData[1] = 0x27;
            respData[2] = 0x35;  /* NRC: InvalidKeys */
            *respLen = 3;
            return 0;
        }
    }

    /* Sub-function không hỗ trợ */
    else
    {
        respData[0] = 0x7F;
        respData[1] = 0x27;
        respData[2] = 0x12;  /* NRC: SubFunctionNotSupported */
        *respLen = 3;
        return 0;
    }
}
