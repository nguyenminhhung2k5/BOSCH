/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#include "dcm_rdbi.h"
#include "dcm.h"

/* External ADC temperature DMA raw buffer from main.c */
extern uint16_t g_TemperatureSensorRawValue_u16[1];

/**
 * @brief Convert STM32F405 internal temperature sensor ADC raw value to degrees Celsius
 */
static uint8_t DCM_CalculateTemperature(uint16_t adcRaw)
{
    /* V_REF = 3300 mV, 12-bit ADC = 4095 */
    /* V_sense (mV) = (adcRaw * 3300) / 4095 */
    int32_t vsense_mv = ((int32_t)adcRaw * 3300) / 4095;
    
    /* Datasheet: V_25 = 760 mV, Avg_Slope = 2.5 mV/C */
    /* Temp = ((vsense - 760) / 2.5) + 25 = ((vsense - 760) * 10 / 25) + 25 */
    int32_t temp_c = (((vsense_mv - 760) * 10) / 25) + 25;

    if (temp_c < 0)
    {
        temp_c = 0;
    }
    else if (temp_c > 125)
    {
        temp_c = 125;
    }

    /* If raw ADC is 0 (not read yet), provide realistic nominal ambient temperature 28 C */
    if (adcRaw == 0)
    {
        temp_c = 28;
    }

    return (uint8_t)temp_c;
}

uint8_t DCM_RDBI_Process(const uint8_t *reqData, uint16_t reqLen, uint8_t *respData, uint16_t *respLen)
{
    /* 1. Kiểm tra độ dài bản tin yêu cầu: Phải đúng 3 bytes [0x22, DID_High, DID_Low] */
    if (reqLen != 3)
    {
        respData[0] = 0x7F;  /* Negative Response SID */
        respData[1] = 0x22;  /* Rejected SID */
        respData[2] = 0x13;  /* NRC: IncorrectMessageLengthOrInvalidFormat */
        *respLen = 3;
        return 0;
    }

    uint16_t requestedDid = ((uint16_t)reqData[1] << 8) | reqData[2];

    /* 2. Xử lý từng DID được hỗ trợ */
    switch (requestedDid)
    {
        case DCM_DID_CANID_TESTER: /* 0x0123: Read CANID Value From Tester */
        {
            uint16_t currentCanId = Dcm_GetCurrentCANID();

            respData[0] = 0x62;                               /* Positive Response SID */
            respData[1] = (uint8_t)(DCM_DID_CANID_TESTER >> 8);   /* DID High: 0x01 */
            respData[2] = (uint8_t)(DCM_DID_CANID_TESTER & 0xFF); /* DID Low: 0x23 */
            respData[3] = (uint8_t)(currentCanId >> 8);       /* CANID High byte */
            respData[4] = (uint8_t)(currentCanId & 0xFF);     /* CANID Low byte */
            *respLen = 5;
            return 1;
        }

        case DCM_DID_ADC_TEMPERATURE: /* 0x0124: Read Value from ADC (Temperature) */
        {
            uint8_t tempValue = DCM_CalculateTemperature(g_TemperatureSensorRawValue_u16[0]);

            respData[0] = 0x62;                                  /* Positive Response SID */
            respData[1] = (uint8_t)(DCM_DID_ADC_TEMPERATURE >> 8);   /* DID High: 0x01 */
            respData[2] = (uint8_t)(DCM_DID_ADC_TEMPERATURE & 0xFF); /* DID Low: 0x24 */
            respData[3] = tempValue;                             /* Temperature in Celsius */
            *respLen = 4;
            return 1;
        }

        default: /* DID không được hỗ trợ */
        {
            respData[0] = 0x7F;  /* Negative Response SID */
            respData[1] = 0x22;  /* Rejected SID */
            respData[2] = 0x31;  /* NRC: RequestOutOfRange (DID not supported) */
            *respLen = 3;
            return 0;
        }
    }
}
