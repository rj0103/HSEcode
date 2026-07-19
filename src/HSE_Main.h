/**
 ******************************************************************************
 * @file     HSE_Main.h
 * @author   rjadhav
 * @version  V1.0.0
 * @date     Jul 7, 2026
 * @brief    user to Add file details
 * @location /test/src/HSE_Main.h
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __HSE_MAIN_H__
#define __HSE_MAIN_H__



#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/

#include "S32K312.h"
#include "std_typedefs.h"
#include "Hse_Ip.h"
#include "Hse_Ip_Cfg.h"
#include "Siul2_Port_Ip.h"
#include "Siul2_Dio_Ip.h"



/*==================================================================================================
*                                          CONSTANTS
==================================================================================================*/


/*==================================================================================================
*                                      DEFINES AND MACROS
==================================================================================================*/

#define UTEST_BASE_ADDRESS		        0x1B000000UL
/* Function-like macros to set and get the status of the example run */
#define App_SetSuccessStatus(value)         (u32NumFailedOperations += ((value) ? 0U : 1U))
#define App_GetSuccessStatus()              (0U == u32NumFailedOperations)

/* Identifier for the only MU instance used in this example (MU0) */
#define MU0_INSTANCE_U8                     ((uint8)0U)

/* Identifier of the MU channel used by HSE to listen for administrative requests (eg. read/write attributes) */
#define MU_ADMIN_CHANNEL_U8                 ((uint8)0U)
#define MU_ADMIN_CHANNEL_U8_1                ((uint8)1U)

/* Timeout for the Hse_Ip layer while waiting for a response from HSE for a synchronous request.
   As the Hse component is configured in this example to have the type of Timeout Counter set to 'OSIF_COUNTER_DUMMY',
the timeout in the variable below will be expressed in ticks */
#define TIMEOUT_TICKS_U32                   ((uint32)10000000U)

/* Size in bytes of the AES128 Encrypt/Decrypt key */
#define APP_SHE_KEY_SIZE                    (16U)

/* Size in bytes of the plain text that will be used to test encryption */
#define APP_AES128_ECB_PLAIN_TEXT_SIZE      (16U)

/* Size in bytes of the cipher text that will result after encryption */
#define APP_AES128_ECB_CIPHER_TEXT_SIZE     (16U)

/* Identifier of the HSE key in the key catalog: RAM catalog, group 0, slot 0 */
#define SHE_RAM_KEY_HSE_KEY_HANDLE          (0x00020000U)


/*==================================================================================================
*                                             ENUMS
==================================================================================================*/
typedef enum
{
    FW_NOT_INSTALLED = 0,
    FW_INSTALLED
}fwteststatus_t;


typedef enum
{
	HSE_OK=1,
	NO_HSE,
	HSE_VER_OK,
	HSE_VER_NOK,
	HSE_ERASEOK
}HSE_STAT_t;
/*==================================================================================================
								STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/


/*==================================================================================================
								GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/
extern HSE_STAT_t HSE_Status;
extern hseSrvResponse_t HSE_FormatKeyCatalogsResponse;
extern hseSrvResponse_t HSE_ImportKeyResponse;
extern hseKeyInfo_t     HSE_AesKeyInfo;
extern hseSrvResponse_t HSE_GetAesKeyInfoResponse;
extern hseSrvResponse_t HSE_AesEncryptResponse;
extern hseSrvResponse_t HSE_AesDecryptResponse;
extern bool             HSE_AesRoundTripMatch;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
void HSE_Init(void);
void HSE_UnlockUtestSector(void);
void HSE_SetData(void);
boolean HSE_checkHseFwFeatureFlagEnabled(void);
boolean HSE_checkHseVersion(void);
bool HSE_GetTRNG(uint8_t *rngNumBuf, uint32_t numofRNG);
bool HSE_getHSECapabilites(void);
hseSrvResponse_t HSE_ActivateAllMuInstances(void);
bool HSE_genreateSHA(uint8_t* in_databuf, uint32_t in_databufLen, uint8_t* resultbuf, uint8_t resultbuf_length );
hseSrvResponse_t HSE_FormatHseKeyCatalogs(void);
hseSrvResponse_t HSE_ImportAESKey(void);
hseSrvResponse_t HSE_GetAesKeyInfo(void);
hseSrvResponse_t HSE_AesEncrypt(const uint8_t* pPlainText, uint8_t* pCipherText, uint32_t length);
hseSrvResponse_t HSE_AesDecrypt(const uint8_t* pCipherText, uint8_t* pPlainText, uint32_t length);
#ifdef ERASE_HSE
bool HSE_EraseHSE(void);
#endif //ERASE_HSE
#ifdef __cplusplus
}
#endif

#endif /* __HSE_MAIN_H__ */

/** @} */
