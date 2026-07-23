/**
 ******************************************************************************
 * @file     HSE_Mac_Ecc_Example.h
 * @brief    Example: CMAC (AES-128) tag generate/verify, and ECC (secp256r1)
 *           key-pair generate/sign/verify, each followed by a Data Flash
 *           storage-integrity check (write, read back, re-verify against the
 *           flash-read data).
 * @location /test/src/HSE_Mac_Ecc_Example.h
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */
#ifndef __HSE_MAC_ECC_EXAMPLE_H__
#define __HSE_MAC_ECC_EXAMPLE_H__

#ifdef __cplusplus
extern "C"{
#endif

/*==================================================================================================
*                                          INCLUDE FILES
==================================================================================================*/
#include "HSE_Main.h"
#include "C40_Ip.h"

/*==================================================================================================
                                 GLOBAL VARIABLE DECLARATIONS
==================================================================================================*/
/* ---- MAC (CMAC/AES-128) example ---- */
extern hseSrvResponse_t  HSE_Mac_ImportKeyResponse;
extern hseSrvResponse_t  HSE_Mac_GenerateTagResponse;
extern uint8_t           HSE_Mac_Tag[16];
extern uint32_t          HSE_Mac_TagLength;
extern hseSrvResponse_t  HSE_Mac_VerifyTagResponse;          /* fresh, in-RAM sanity check */

extern C40_Ip_StatusType HSE_Mac_FlashWriteStatus;
extern C40_Ip_StatusType HSE_Mac_FlashReadStatus;
extern uint8_t           HSE_Mac_TagFromFlash[16];
extern hseSrvResponse_t  HSE_Mac_VerifyFromFlashResponse;    /* verify against flash-read data   */
extern bool              HSE_Mac_FlashRoundTripVerified;

/* ---- ECC (secp256r1 / ECDSA-SHA256) example ---- */
extern hseSrvResponse_t  HSE_Ecc_GenerateKeyPairResponse;
extern uint8_t           HSE_EccPublicKey[64];               /* X || Y, big-endian, 32B each */
extern hseSrvResponse_t  HSE_Ecc_SignResponse;
extern uint8_t           HSE_EccSignatureR[32];
extern uint8_t           HSE_EccSignatureS[32];
extern uint32_t          HSE_EccSignatureRLength;
extern uint32_t          HSE_EccSignatureSLength;
extern hseSrvResponse_t  HSE_Ecc_VerifyResponse;             /* fresh, in-RAM sanity check */

extern C40_Ip_StatusType HSE_Ecc_FlashWriteStatus;
extern C40_Ip_StatusType HSE_Ecc_FlashReadStatus;
extern hseSrvResponse_t  HSE_Ecc_ImportPubKeyFromFlashResponse;
extern hseSrvResponse_t  HSE_Ecc_VerifyFromFlashResponse;    /* verify against flash-read data,
                                                                 using only the flash-read public key */
extern bool              HSE_Ecc_FlashRoundTripVerified;

/*==================================================================================================
                                     FUNCTION PROTOTYPES
==================================================================================================*/
/*!
 * @brief   Imports a dedicated AES-128 key (SIGN | VERIFY) into a spare RAM catalog slot, used
 *          only by this MAC example - separate from the AES demo keys in HSE_Main.c and the
 *          transient SMR sign key in HSE_SecureBoot.c.
 */
hseSrvResponse_t HSE_Mac_ImportKey_Ram(void);

/*!
 * @brief   Generates a CMAC (AES-128) tag over [pData, pData + dataLen) using the MAC example
 *          key, storing the result in HSE_Mac_Tag / HSE_Mac_TagLength.
 */
hseSrvResponse_t HSE_Mac_GenerateTag(const uint8_t *pData, uint32_t dataLen);

/*!
 * @brief   Verifies a CMAC (AES-128) tag over [pData, pData + dataLen) against pTag, using the
 *          MAC example key. Returns HSE_SRV_RSP_OK on match, HSE_SRV_RSP_VERIFY_FAILED otherwise.
 */
hseSrvResponse_t HSE_Mac_VerifyTag(const uint8_t *pData, uint32_t dataLen, const uint8_t *pTag, uint32_t tagLen);

/*!
 * @brief   Full MAC example: import key, generate a tag over a sample message, verify it fresh
 *          (in RAM), then write [message][tag] to a dedicated Data Flash sector, read it back,
 *          and re-verify the tag against the flash-read message - proving the data survived
 *          storage intact and the tag is still cryptographically valid.
 */
void HSE_Mac_StorageCheckDemo(void);

/*!
 * @brief   Generates a secp256r1 ECC key pair into a dedicated RAM catalog slot (SIGN | VERIFY),
 *          capturing the public key (X || Y, 64 bytes) into HSE_EccPublicKey.
 * @note    Requires the ECC_PAIR/ECC_PUB RAM catalog groups added in HSE_Main.c's
 *          Hse_aRamKeyCatalog to actually be provisioned - i.e. a build with
 *          RUN_FORMAT_KEY_CATALOGS_IN_INIT defined must have run at least once.
 */
hseSrvResponse_t HSE_Ecc_GenerateKeyPair(void);

/*!
 * @brief   ECDSA-SHA256 signs [pMessage, pMessage + msgLen) using the ECC key pair's private
 *          component, storing the (r,s) signature in HSE_EccSignatureR/S (+ lengths).
 */
hseSrvResponse_t HSE_Ecc_SignMessage(const uint8_t *pMessage, uint32_t msgLen);

/*!
 * @brief   ECDSA-SHA256 verifies [pMessage, pMessage + msgLen) against the (r,s) signature at
 *          [pR, rLen) / [pS, sLen), using verifyKeyHandle's public component. Works with either
 *          the original ECC_PAIR handle or a standalone ECC_PUB key imported via
 *          HSE_Ecc_ImportPublicKey_Ram().
 */
hseSrvResponse_t HSE_Ecc_VerifyMessage(hseKeyHandle_t verifyKeyHandle, const uint8_t *pMessage, uint32_t msgLen,
                                       const uint8_t *pR, uint32_t rLen, const uint8_t *pS, uint32_t sLen);

/*!
 * @brief   Imports a 64-byte raw (X || Y) public key into the dedicated ECC_PUB RAM catalog slot,
 *          verify-only. Used to reconstruct a usable public key purely from bytes read back out
 *          of Data Flash, independent of the original HSE_Ecc_GenerateKeyPair() key pair slot.
 */
hseSrvResponse_t HSE_Ecc_ImportPublicKey_Ram(const uint8_t *pPubKeyXY);

/*!
 * @brief   Full ECC example: generate a key pair, sign a sample message, verify it fresh (in
 *          RAM), then write [message][public key][r][s] to a dedicated Data Flash sector, read
 *          it back, re-import the flash-read public key into a separate verify-only slot, and
 *          verify the flash-read signature against the flash-read message using that
 *          flash-recovered key - proving the whole bundle survived storage intact and is still
 *          cryptographically valid, using only what came back out of flash.
 */
void HSE_Ecc_StorageCheckDemo(void);

/*!
 * @brief   Runs both storage-check demos (MAC, then ECC) in sequence.
 */
void HSE_Mac_Ecc_Example_Demo(void);

#ifdef __cplusplus
}
#endif

#endif /* __HSE_MAC_ECC_EXAMPLE_H__ */

/** @} */
