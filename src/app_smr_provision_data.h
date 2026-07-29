/**
 * PLACEHOLDER - regenerate before this is used for anything real.
 *
 * Real usage: run, from the tools/ directory -
 *   python sign_tool.py genkey  --priv app_signing_key.pem --pub app_public_key_raw.bin
 *   python sign_tool.py sign    --priv app_signing_key.pem --in <built GSLU_APP image> --out-prefix app_image
 *   python sign_tool.py header  --pub app_public_key_raw.bin --sig-prefix app_image --out ../src/app_smr_provision_data.h
 *
 * This checked-in placeholder is intentionally all-zero - a zero-filled EC point is not a valid
 * public key, so HSE_AppSmr_ImportVerifyKey_Nvm() will fail loudly (not silently "pass") if
 * someone builds RUN_APP_SMR_PROVISIONING without regenerating this file first.
 */
#ifndef APP_SMR_PROVISION_DATA_H
#define APP_SMR_PROVISION_DATA_H

#include <stdint.h>

static const uint8_t AppSmr_PublicKeyXY[64] = {0};
static const uint8_t AppSmr_SignatureR[32]  = {0};
static const uint8_t AppSmr_SignatureS[32]  = {0};

#endif /* APP_SMR_PROVISION_DATA_H */
