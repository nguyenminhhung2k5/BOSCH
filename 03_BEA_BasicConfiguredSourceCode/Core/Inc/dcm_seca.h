/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#ifndef _DCM_SECA_H
#define _DCM_SECA_H

#include "main.h"

/* Security Access Sub-functions */
#define DCM_SECA_SUBFUNC_REQ_SEED       0x01
#define DCM_SECA_SUBFUNC_SEND_KEY       0x02

/* Security Timeout Settings */
#define DCM_SECA_UNLOCK_TIMEOUT_MS      5000   /* Mở khoá trong 5 giây */
#define DCM_SECA_PENALTY_DELAY_MS       10000  /* Khóa phạt 10 giây nếu nhập sai key */

/**
 * @brief Initialize Security Access module
 */
void DCM_SECA_Init(void);

/**
 * @brief Main function of Security Access (handles 5s unlock timeout and 10s penalty delay)
 * Should be called periodically in while(1)
 */
void DCM_SECA_MainFunction(void);

/**
 * @brief Check if ECU is currently unlocked at Security Level 1
 * @return 1 if unlocked, 0 if locked
 */
uint8_t DCM_SECA_IsUnlocked(void);

/**
 * @brief Process SecurityAccess (SID 0x27)
 * @param reqData: Pointer to request payload (starts with 0x27)
 * @param reqLen:  Length of request payload
 * @param respData: Pointer to output buffer for response payload
 * @param respLen:  Pointer to variable storing length of response payload
 * @return 1 on positive response, 0 on negative response
 */
uint8_t DCM_SECA_Process(const uint8_t *reqData, uint16_t reqLen, uint8_t *respData, uint16_t *respLen);

#endif /* _DCM_SECA_H */
