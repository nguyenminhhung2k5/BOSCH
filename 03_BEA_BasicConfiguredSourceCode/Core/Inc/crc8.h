/**
  ******************************************************************************
  * @file           : crc8.h
  * @brief          : CRC-8 SAE J1850 Calculation
  ******************************************************************************
  */

#ifndef __CRC8_H__
#define __CRC8_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Tính toán Checksum theo chuẩn CRC-8 SAE J1850
 *         - Polynomial: 0x1D (x^8 + x^4 + x^3 + x^2 + 1)
 *         - Initial Value: 0xFF
 *         - Final XOR: 0xFF
 * @param  data: Con trỏ tới mảng dữ liệu cần tính
 * @param  length: Số lượng byte (thường là 6 byte đầu của bản tin CAN)
 * @return Giá trị Checksum CRC-8
 */
static inline uint8_t calc_SAE_J1850(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFF;
    uint8_t i, bit;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (bit = 0; bit < 8; bit++)
        {
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ 0x1D;
            }
            else
            {
                crc = (crc << 1);
            }
        }
    }

    return crc ^ 0xFF;
}

#ifdef __cplusplus
}
#endif

#endif /* __CRC8_H__ */
