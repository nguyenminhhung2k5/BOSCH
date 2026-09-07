/*********************************************************/
/*********BOSCH BEA PROGRAM SKELETON DEMO CODE************/
/*********************************************************/

#ifndef _DCM_WDBI_H
#define _DCM_WDBI_H

#include "main.h"

/**
 * @brief Process WriteDataByIdentifier (SID 0x2E)
 * @param reqData: Pointer to request payload (starts with 0x2E)
 * @param reqLen:  Length of request payload
 * @param respData: Pointer to output buffer for response payload
 * @param respLen:  Pointer to variable storing length of response payload
 * @return 1 on positive response, 0 on negative response
 */
uint8_t DCM_WDBI_Process(const uint8_t *reqData, uint16_t reqLen, uint8_t *respData, uint16_t *respLen);

#endif /* _DCM_WDBI_H */
