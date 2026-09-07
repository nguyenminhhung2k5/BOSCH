/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#ifndef _DCM_RDBI_H
#define _DCM_RDBI_H

#include "main.h"

/* Supported Data Identifiers (DIDs) */
#define DCM_DID_CANID_TESTER        0x0123
#define DCM_DID_ADC_TEMPERATURE     0x0124

/**
 * @brief Process ReadDataByIdentifier (SID 0x22)
 * @param reqData: Pointer to request payload (starts with 0x22)
 * @param reqLen:  Length of request payload
 * @param respData: Pointer to output buffer for response payload
 * @param respLen:  Pointer to variable storing length of response payload
 * @return 1 on positive response, 0 on negative response
 */
uint8_t DCM_RDBI_Process(const uint8_t *reqData, uint16_t reqLen, uint8_t *respData, uint16_t *respLen);

#endif /* _DCM_RDBI_H */
