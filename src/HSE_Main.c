/**
 ******************************************************************************
 * @file     HSE_Main.c
 * @author   rjadhav
 * @version  V1.0.0
 * @date     Jul 7, 2026
 * @brief    Add file details
 * @location /test/src/HSE_Main.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */

#ifdef __cplusplus
extern "C"
{
#endif
/*==================================================================================================
*                                          INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/
#include "HSE_Main.h"
#include "C40_Ip.h"
#include "string.h"
//#include "hse_srv_attr.h"

	/*==================================================================================================
	*                                        GLOBAL CONSTANTS
	==================================================================================================*/

/* Force-reformat every boot is only safe/wanted for the Provisioning app (Flash_Load_KEY) - that's
   the one place a fresh reformat + re-import of the current Hse_aNvmKeyCatalog contents (e.g. a
   newly generated key pair) is actually desired. Main app (Release_FLASH/Debug_FLASH) must NOT
   force this: it also calls HSE_Init(), and an unconditional reformat there would wipe the
   AppSmrVerify NVM key (group 1) on every single Main app boot, breaking Bootlaoder's verify on
   the very next reset since Main app has no ImportVerifyKey capability to re-provision it. */
#ifdef PROVISION_APP_BUILD
#define RUN_FORMAT_KEY_CATALOGS_IN_INIT
#define FORCE_KEY_CATALOG_REFORMAT
#endif

	/*==================================================================================================
	*                                        GLOBAL VARIABLES
	==================================================================================================*/

	/* Variable to store HSE FW version details */
	// hseAttrFwVersion_t gHseFwVersion = {0U};

	// volatile fwteststatus_t gInstallHSEFwTest = FW_NOT_INSTALLED;

	/* Array of variables used to store the application requests to Hse Ip layer. This example uses both synchronous and asynchronous
	calls to Hse Ip, made only on MU0 instance. For this reason, we will need to have a request variable for each of the channels of the MU. */
	static Hse_Ip_ReqType HseIp_aRequest[HSE_IP_NUM_OF_CHANNELS_PER_MU];

	static hseAttrFwVersion_t version_data = {0U};

	HSE_STAT_t HSE_Status = NO_HSE;

	static uint8_t RandomNum[16] = {0};

	static hseAttrCapabilities_t Hse_AttrCapabilities;
	static char inbuff_SHA[] = "Hello World";
	static uint8_t SHA_Result[32] = {0};
	static uint8_t p_length= 0;


	/* Table containing NVM key catalog entries */
	static hseKeyGroupCfgEntry_t Hse_aNvmKeyCatalog[] =
	{
	    /* NvmKeyGroup_MASTER_ECU_KEY__BOOT_MAC_KEY__Key1_To_Key10 - group index 0 */
	    {HSE_ALL_MU_MASK, HSE_KEY_OWNER_CUST, HSE_KEY_TYPE_AES, 4U, HSE_KEY128_BITS, {0U, 0U}},
	    /* NvmKeyGroup_AppSmrVerify - group index 1: persistent secp256r1 public key used to verify
	       Main app's real SMR entry (index 2). Imported by the Provisioning app (a separate build
	       config of this same project, "Flash_Load_KEY", flashed at 0x004C2000 - factory/EOL only,
	       never shipped), never by this build itself. Must be NVM, not RAM - RAM keys are wiped
	       every reset, but this key has to survive across reset for Bootlaoder's on-demand SMR
	       check to ever succeed. Same "needs a reformat once" caveat as the RAM ECC groups below -
	       see their comment. */
	    {HSE_ALL_MU_MASK, HSE_KEY_OWNER_CUST, HSE_KEY_TYPE_ECC_PUB, 1U, HSE_KEY256_BITS, {0U, 0U}},
	    /* Marker to end the key catalog */
	    {0U, 0U, 0U, 0U, 0U, {0U, 0U}}
	};

	/* Table containing RAM key catalog entries */
	static hseKeyGroupCfgEntry_t Hse_aRamKeyCatalog[] =
	{
	    /* RamKeyGroup_RamKey. RAM key group owner must always be HSE_KEY_OWNER_ANY.
	       Group index 0 - slot 0: demo AES key (this file), slot 1: generated AES key (this file),
	       slot 2: transient SMR sign key (HSE_SecureBoot.c), slot 3: MAC key (HSE_Mac_Ecc_Example.c). */
	    {HSE_ALL_MU_MASK, HSE_KEY_OWNER_ANY, HSE_KEY_TYPE_AES, 4U, HSE_KEY128_BITS, {0U, 0U}},
	    /* RamKeyGroup_EccPair - group index 1: secp256r1 key pair (HSE_Mac_Ecc_Example.c).
	       NOTE: adding this group is a catalog *structure* change - unlike reusing a spare slot in
	       an existing group, it only takes effect after HSE_FormatHseKeyCatalogs() re-runs (i.e.
	       building once with RUN_FORMAT_KEY_CATALOGS_IN_INIT defined), which wipes all previously
	       provisioned keys before HSE_Init()'s existing logic re-provisions them. */
	    {HSE_ALL_MU_MASK, HSE_KEY_OWNER_ANY, HSE_KEY_TYPE_ECC_PAIR, 1U, HSE_KEY256_BITS, {0U, 0U}},
	    /* RamKeyGroup_EccPub - group index 2: standalone public-only key (HSE_Mac_Ecc_Example.c),
	       used to re-verify a signature using only the public key read back from Data Flash - a
	       different key TYPE than the pair above, so it needs its own group. */
	    {HSE_ALL_MU_MASK, HSE_KEY_OWNER_ANY, HSE_KEY_TYPE_ECC_PUB, 1U, HSE_KEY256_BITS, {0U, 0U}},
	    /* Marker to end the key catalog */
	    {0U, 0U, 0U, 0U, 0U, {0U, 0U}}
	};
/*==================================================================================================
*                           LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/

/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/
#define UTEST_START_DATA 0xFFFFFFFFFFFFFFFFUL
#define AES_RAM_KEY_HANDLE GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM,0,0)
#define AES_NVM_KEY_HANDLE GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_NVM,0,0)
#define AES_GENERATED_RAM_KEY_HANDLE GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM,0,1)

#define HSE_INVALID_KEY_HANDLE ((hseKeyHandle_t)0xFFFFFFFFUL)

	/*==================================================================================================
	*                                         LOCAL CONSTANTS
	==================================================================================================*/

	/*==================================================================================================
	*                                         LOCAL VARIABLES
	==================================================================================================*/
	/* HSE FW feature usage flag to be loaded in UTEST */
	static uint8_t hseFwFeatureFlagEnabledValue[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xDD, 0xCC, 0xBB, 0xAA};

	/* Variable that will store the internal state machine of the Hse Ip layer. As this application uses only MU0 to communicate
	   with HSE, there is no need for an array of such variables.
	   The address of the variable storing the internal state machine should be provided as parameter when the Hse Ip layer
	is initialized on one MU instance. */
	static Hse_Ip_MuStateType HseIp_MuState;

	/* Variable where HSE will write (return) the supported capabilities */
	static hseAttrCapabilities_t Hse_AttrCapabilities;

	/* Array of variables used to store the descriptors containing the service request for HSE firmware. This example uses both synchronous and
	asynchronous calls to Hse Ip, made only on MU0 instance. For this reason, we need to have a descriptor variable available for each of the channels in the MU0.
	   The descriptors should be placed in the shared memory, in a non-cacheable area */
	static hseSrvDescriptor_t Hse_aSrvDescriptor[HSE_IP_NUM_OF_CHANNELS_PER_MU];/* Variable where application will store the request of configuring (enabling/disabling) the MU instances */
	static hseAttrMUConfig_t     Hse_MuConfig;

	static const uint8_t aes128_key0[16] = {0xAA, 0xBB, 0xCC, 0xDD, 0xDD, 0xCC, 0xBB, 0xAA,
			0xAA, 0xBB, 0xCC, 0xDD, 0xDD, 0xCC, 0xBB, 0xAA};

	/* Holds the raw response of the last HSE_FormatHseKeyCatalogs() call, for inspection */
	hseSrvResponse_t HSE_FormatKeyCatalogsResponse = HSE_SRV_RSP_GENERAL_ERROR;
	/* Distinguishes "format actually ran" from "skipped, HSE_STATUS_INSTALL_OK was already set" -
	   HSE_FormatKeyCatalogsResponse alone reads HSE_SRV_RSP_OK in both cases. */
	bool HSE_CatalogsWereAlreadyInstalled = false;

	/* Holds the raw response of the last HSE_ImportAESKey() call, for inspection */
	hseSrvResponse_t HSE_ImportKeyResponse = HSE_SRV_RSP_GENERAL_ERROR;

	/* Holds the stored key properties (flags/type/bit length) read back by HSE_GetAesKeyInfo(), for inspection.
	   Never contains the raw key value - HSE does not return that for symmetric keys. */
	hseKeyInfo_t     HSE_AesKeyInfo;
	hseSrvResponse_t HSE_GetAesKeyInfoResponse = HSE_SRV_RSP_GENERAL_ERROR;

	/* AES ECB round-trip self-test buffers/results, for inspection */
	static const uint8_t AesTestPlainText[APP_AES128_ECB_PLAIN_TEXT_SIZE] = "GSLU_APP_AESTST";
	static uint8_t        AesTestEncryptedData[APP_AES128_ECB_CIPHER_TEXT_SIZE]  = {0};
	static uint8_t        AesTestDecryptedText[APP_AES128_ECB_PLAIN_TEXT_SIZE] = {0};
	hseSrvResponse_t HSE_AesEncryptResponse = HSE_SRV_RSP_GENERAL_ERROR;
	hseSrvResponse_t HSE_AesDecryptResponse = HSE_SRV_RSP_GENERAL_ERROR;
	bool             HSE_AesRoundTripMatch  = false;

	/* Holds the raw response of the last HSE_ImportNvmAESKey() call, for inspection */
	hseSrvResponse_t HSE_ImportNvmKeyResponse = HSE_SRV_RSP_GENERAL_ERROR;

	/* Holds the stored key properties read back for AES_NVM_KEY_HANDLE, for inspection.
	   Also used by HSE_ImportNvmAESKey() to detect a key already provisioned by a previous boot,
	   since (unlike the RAM key) the NVM key persists across reset. */
	hseKeyInfo_t     HSE_NvmAesKeyInfo;
	hseSrvResponse_t HSE_GetNvmAesKeyInfoResponse = HSE_SRV_RSP_GENERAL_ERROR;

	/* AES ECB round-trip self-test buffers/results for the NVM key, for inspection */
	static uint8_t   AesNvmTestEncryptedData[APP_AES128_ECB_CIPHER_TEXT_SIZE]  = {0};
	static uint8_t   AesNvmTestDecryptedText[APP_AES128_ECB_PLAIN_TEXT_SIZE] = {0};
	hseSrvResponse_t HSE_AesEncryptNvmResponse = HSE_SRV_RSP_GENERAL_ERROR;
	hseSrvResponse_t HSE_AesDecryptNvmResponse = HSE_SRV_RSP_GENERAL_ERROR;
	bool             HSE_AesNvmRoundTripMatch  = false;

	/* Holds the raw response of the last HSE_GenerateAesKey() call, for inspection */
	hseSrvResponse_t HSE_GenerateAesKeyResponse = HSE_SRV_RSP_GENERAL_ERROR;

	/* Holds the stored properties read back for AES_GENERATED_RAM_KEY_HANDLE, for inspection.
	   As with any AES key, the raw generated value is never readable from the host - it never
	   leaves HSE, which is the whole point of generating it there instead of importing one. */
	hseKeyInfo_t     HSE_GeneratedAesKeyInfo;
	hseSrvResponse_t HSE_GetGeneratedAesKeyInfoResponse = HSE_SRV_RSP_GENERAL_ERROR;

	/* AES ECB round-trip self-test buffers/results for the generated key, for inspection */
	static uint8_t   AesGenTestEncryptedData[APP_AES128_ECB_CIPHER_TEXT_SIZE] = {0};
	static uint8_t   AesGenTestDecryptedText[APP_AES128_ECB_PLAIN_TEXT_SIZE] = {0};
	hseSrvResponse_t HSE_AesEncryptGeneratedResponse = HSE_SRV_RSP_GENERAL_ERROR;
	hseSrvResponse_t HSE_AesDecryptGeneratedResponse = HSE_SRV_RSP_GENERAL_ERROR;
	bool             HSE_AesGeneratedRoundTripMatch  = false;

	/*==================================================================================================
	*                                    LOCAL FUNCTION PROTOTYPES
	*
	*
	==================================================================================================*/

	/*!
	 * @brief       Initialization Function of HSE module
	 * @details     This function will be used for Initialization HSE module
	 *
	 * @param[in]   NA		Small brief of the variable
	 *
	 *
	 * @return      return type/name
	 */

	void HSE_Init(void)
	{
		static volatile hseStatus_t HseStatus;
		static Hse_Ip_StatusType HseIpStatus = HSE_IP_STATUS_ERROR; // Init with not expected value
		static volatile uint32 u32Timeout = TIMEOUT_TICKS_U32;

		while (u32Timeout != 0U)
		{
			HseStatus = Hse_Ip_GetHseStatus(MU0_INSTANCE_U8);
			if (0U != (HseStatus & HSE_STATUS_INIT_OK))
			{
				break;
			}
			u32Timeout--;
		}
		// TODO: add a false safe for timeout
		if (0U != u32Timeout)
		{
#ifdef ERASE_HSE
			bool status = 0;
			status = HSE_EraseHSE();
			if (status == true)
			{
				HSE_Status = HSE_ERASEOK;
			}
			else
			{
				HSE_Status = HSE_OK;
			}
#else

		HseIpStatus = Hse_Ip_Init(MU0_INSTANCE_U8, &HseIp_MuState);

		if (HSE_IP_STATUS_SUCCESS != HseIpStatus)
		{
			HSE_Status = NO_HSE;
		}
		else
		{
			volatile bool status = 0;

			status = HSE_ActivateAllMuInstances();

			status = HSE_getHSECapabilites();
			HSE_Status = HSE_OK;
			/*Check HSE version*/
//			bool status = 0;
			status = HSE_checkHseVersion();
			if (1u == status)
			{
				HSE_Status = HSE_VER_OK;
				// HSE version match Working
			}
			else
			{
				HSE_Status = HSE_VER_NOK;
			}
		    /* =============================================================================================================================== */
		    /*    Generate 32byte random number using HSE service                                                                                      */
		    /* =============================================================================================================================== */

			status = HSE_GetTRNG(RandomNum,sizeof(RandomNum) );
		    /* =============================================================================================================================== */
		    /*    Generate SHA hashing for a Hello World buffer using defaults settings                                                                                       */
		    /* =============================================================================================================================== */

			status = HSE_genreateSHA((uint8_t*)inbuff_SHA,strlen(inbuff_SHA),SHA_Result,sizeof(SHA_Result));

#ifdef RUN_FORMAT_KEY_CATALOGS_IN_INIT
		    /* =============================================================================================================================== */
		    /*      Format HSE Nvm and Ram key catalogs. This is a one-time provisioning step: once HSE_STATUS_INSTALL_OK is set, the         */
		    /*      catalogs already exist, so skip re-formatting (it would wipe existing keys, and repeated NVM reformats can also fail      */
		    /*      with HSE_SRV_RSP_NOT_ENOUGH_SPACE). Only run it on a genuinely blank/never-provisioned HSE.                               */
		    /*      FORCE_KEY_CATALOG_REFORMAT bypasses that skip - needed when the catalog *structure* itself changed (a group was added/    */
		    /*      resized in Hse_aNvmKeyCatalog/Hse_aRamKeyCatalog), since HSE_STATUS_INSTALL_OK being set from an earlier, different       */
		    /*      catalog layout does not mean the CURRENT layout has ever actually been formatted. Deliberately a separate macro from      */
		    /*      RUN_FORMAT_KEY_CATALOGS_IN_INIT so the common case (already formatted, nothing changed) stays a safe no-op by default.    */
		    /* =============================================================================================================================== */
#ifdef PROVISION_APP_BUILD
			HSE_CatalogsWereAlreadyInstalled = (0U != (HseStatus & HSE_STATUS_INSTALL_OK));
#ifdef FORCE_KEY_CATALOG_REFORMAT
			HSE_FormatKeyCatalogsResponse = HSE_FormatHseKeyCatalogs();
#else
			if (HSE_CatalogsWereAlreadyInstalled)
			{
				HSE_FormatKeyCatalogsResponse = HSE_SRV_RSP_OK; /* already installed - nothing to do */
			}
			else
			{
				HSE_FormatKeyCatalogsResponse = HSE_FormatHseKeyCatalogs();
			}
#endif // FORCE_KEY_CATALOG_REFORMAT
#else
			/* Main app (Release_FLASH/Debug_FLASH) never formats/reformats the NVM key catalog -
			   that's exclusively the Provisioning app's job (see PROVISION_APP_BUILD above). Main
			   app can only ever run after Bootlaoder has already verified its SMR entry, which
			   itself can only succeed after the Provisioning app already formatted+populated the
			   catalogs - so by the time Main app reaches this point the catalogs are guaranteed
			   installed. Do NOT gate on HSE_STATUS_INSTALL_OK here: confirmed empirically that it
			   reads back false on a Main app boot immediately following a real, successful
			   Provisioning app run and Bootlaoder verify (not reliable across a Bootlaoder-
			   mediated jump) - trusting it caused Main app to call HSE_FormatHseKeyCatalogs()
			   again on already-populated catalogs, which failed with HSE_SRV_RSP_GENERAL_ERROR
			   (0x33D6D4F1). */
			HSE_CatalogsWereAlreadyInstalled = true;
			HSE_FormatKeyCatalogsResponse = HSE_SRV_RSP_OK;
#endif // PROVISION_APP_BUILD
			status = (HSE_SRV_RSP_OK == HSE_FormatKeyCatalogsResponse);
#endif // RUN_FORMAT_KEY_CATALOGS_IN_INIT
//		    /* =============================================================================================================================== */
//		    /*    Import a key in the  RAM key slot                                                                                         */
//		    /* =============================================================================================================================== */
//			HSE_ImportKeyResponse = HSE_ImportAESKey();
//		    /* =============================================================================================================================== */
//		    /*    Read back the stored key's properties to confirm it was actually imported, before using it                                   */
//		    /* =============================================================================================================================== */
//			HSE_GetAesKeyInfoResponse = HSE_GetAesKeyInfo();
//		    /* =============================================================================================================================== */
//		    /*    Encrypt/decrypt round-trip test using the imported RAM AES key                                                                */
//		    /* =============================================================================================================================== */
//			HSE_AesEncryptResponse = HSE_AesEncrypt(AesTestPlainText, AesTestEncryptedData, sizeof(AesTestPlainText));
//			HSE_AesDecryptResponse = HSE_AesDecrypt(AesTestEncryptedData, AesTestDecryptedText, sizeof(AesTestEncryptedData));
//			HSE_AesRoundTripMatch  = (0 == memcmp(AesTestPlainText, AesTestDecryptedText, sizeof(AesTestPlainText)));
//#ifdef ERASE_NVM_KEYS
//		    /* =============================================================================================================================== */
//		    /*    DEV ONLY: wipe the NVM test key so it gets freshly re-provisioned below. Undefine ERASE_NVM_KEYS for normal persistent use.  */
//		    /* =============================================================================================================================== */
//			(void)HSE_EraseNvmAesKey();
//#endif // ERASE_NVM_KEYS
//		    /* =============================================================================================================================== */
//		    /*    Import a persistent AES key into the NVM key catalog (only actually writes on the first boot)                               */
//		    /* =============================================================================================================================== */
//			HSE_ImportNvmKeyResponse = HSE_ImportNvmAESKey();
//		    /* =============================================================================================================================== */
//		    /*    Encrypt/decrypt round-trip test using the imported NVM AES key                                                                */
//		    /* =============================================================================================================================== */
//			HSE_AesEncryptNvmResponse = HSE_AesEncryptNvm(AesTestPlainText, AesNvmTestEncryptedData, sizeof(AesTestPlainText));
//			HSE_AesDecryptNvmResponse = HSE_AesDecryptNvm(AesNvmTestEncryptedData, AesNvmTestDecryptedText, sizeof(AesNvmTestEncryptedData));
//			HSE_AesNvmRoundTripMatch  = (0 == memcmp(AesTestPlainText, AesNvmTestDecryptedText, sizeof(AesTestPlainText)));
//		    /* =============================================================================================================================== */
//		    /*    Generate a random AES-128 key inside HSE (raw value never leaves HSE) and use it to encrypt/decrypt                          */
//		    /* =============================================================================================================================== */
//			HSE_GenerateAesKeyResponse = HSE_GenerateAesKey();
//			HSE_AesEncryptGeneratedResponse = HSE_AesEncryptGenerated(AesTestPlainText, AesGenTestEncryptedData, sizeof(AesTestPlainText));
//			HSE_AesDecryptGeneratedResponse = HSE_AesDecryptGenerated(AesGenTestEncryptedData, AesGenTestDecryptedText, sizeof(AesGenTestEncryptedData));
//			HSE_AesGeneratedRoundTripMatch  = (0 == memcmp(AesTestPlainText, AesGenTestDecryptedText, sizeof(AesTestPlainText)));

		}
		// TODO: Protect against failed INIT
#endif // ERASE_HSE_FLAG
		}
		else
		{
			// TODO: Need to add fallback for timeout errors
		}
	}

	/*!
	 * @brief       Initialization Function of HSE module
	 * @details     This function will be used for Initialization HSE module
	 *
	 * @param[in]   NA		Small brief of the variable
	 *
	 *
	 * @return      return type/name
	 */

	void HSE_UnlockUtestSector(void)
	{
		// TODO: Need to use C40 Drivers or else someother ways to unlock
		/* Clear UTEST lock bit */
		IP_PFLASH->PFCBLKU_SPELOCK[0] = 0;

		/* Program sequence described in "21.5.1.2 Program" in S32K3 RM: */
		/* Write address to be programmed aligned to 1024 bits */
		IP_PFLASH->PFCPGM_PEADR_L = 0x1B000000;
		/* Write data to be programmed to data registers */

		IP_FLASH->DATA[0] = 0xAAAA5555;
		IP_FLASH->DATA[1] = 0x5555AAAA;
		/* Change the value in MCR[PGM] from a 0 to a 1. */
		IP_FLASH->MCR |= FLASH_MCR_PGM_MASK;
		/* Write a 1 to MCR[EHV] to start the internal program sequence */
		IP_FLASH->MCR |= FLASH_MCR_EHV_MASK;
		/* Wait until MCRS[DONE] becomes 1. */
		while (0U == (IP_FLASH->MCRS & FLASH_MCRS_DONE_MASK))
			;
		/* Write 0 to MCR[EHV]. */
		IP_FLASH->MCR &= ~FLASH_MCR_EHV_MASK;
		/* Write 0 to MCR[PGM] */
		IP_FLASH->MCR &= ~FLASH_MCR_PGM_MASK;
	}

	/*!
	 * @brief       Get required data from HSE module
	 * @details     This function will be used for get required data from HSE module
	 *
	 * @param[in]   u8MuInstance		Message Unit Instance ID
	 * @param[in]   u8MuInstance		Message Unit Instance ID
	 * @param[in]   u8MuInstance		Message Unit Instance ID
	 * @param[in]   u8MuInstance		Message Unit Instance ID
	 * @param[in]   u8MuInstance		Message Unit Instance ID
	 *
	 * @return      hseSrvResponse_t
	 */

	static hseSrvResponse_t HSE_GetAttributeData(
		const uint8 u8MuInstance, void *attributebuffer,
		uint32_t size_attr, hseSrvId_t srvID, hseAttrId_t attrID)
	{
		hseSrvDescriptor_t *pHseSrvDescriptor;

		/* Fill the descriptor for the HSE request. Because the request to get an attribute is an administrative one,
		it should be sent over channel 0 */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[MU_ADMIN_CHANNEL_U8];

		pHseSrvDescriptor->srvId = srvID;
		pHseSrvDescriptor->hseSrv.getAttrReq.attrId = attrID;
		pHseSrvDescriptor->hseSrv.getAttrReq.attrLen = size_attr;
		pHseSrvDescriptor->hseSrv.getAttrReq.pAttr = HSE_PTR_TO_HOST_ADDR(attributebuffer);

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		return Hse_Ip_ServiceRequest(u8MuInstance, MU_ADMIN_CHANNEL_U8, &HseIp_aRequest[MU_ADMIN_CHANNEL_U8], pHseSrvDescriptor);
	}


	/*!
	 * @brief       Read UTEST location for unlock Status
	 * @details     This function will be used for Initialization HSE module
	 *
	 * @param[in]   NA		Small brief of the variable
	 *
	 * @return      return type/name
	 */
	boolean HSE_checkHseFwFeatureFlagEnabled(void)
	{
		boolean fw_enabled = FALSE;
		uint64_t hsefwfeatureflag = 0;
		C40_Ip_StatusType read_status = C40_IP_STATUS_ERROR;
		read_status = C40_Ip_Read(UTEST_BASE_ADDRESS, sizeof(hsefwfeatureflag), (uint8 *)&hsefwfeatureflag);

		if (read_status != C40_IP_STATUS_SUCCESS)
		{
			// TODO add reaction to failed read action
		}
		else
		{
			if (UTEST_START_DATA != hsefwfeatureflag)
			{
				fw_enabled = TRUE;
			}
			else // requried by MISRA
			{
				fw_enabled = FALSE;
			}
		}
		return fw_enabled;
	}

	/*!
	 * @brief       Read UTEST location for unlock Status
	 * @details     This function will be used for Initialization HSE module
	 *
	 * @param[in]   NA		Small brief of the variable
	 *
	 * @return      return type/name
	 */
	boolean HSE_checkHseVersion(void)
	{
		static volatile hseSrvResponse_t HseResponse;
		hseAttrFwVersion_t CurrVersion = HSE_FW_VERSION;

		hseSrvDescriptor_t *pHseSrvDescriptor;

		/* Fill the descriptor for the HSE request. Because the request to get an attribute is an administrative one,
		it should be sent over channel 0 */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[MU_ADMIN_CHANNEL_U8];

		pHseSrvDescriptor->srvId = HSE_SRV_ID_GET_ATTR;
		pHseSrvDescriptor->hseSrv.getAttrReq.attrId = HSE_FW_VERSION_ATTR_ID;
		pHseSrvDescriptor->hseSrv.getAttrReq.attrLen = sizeof(hseAttrFwVersion_t);
		pHseSrvDescriptor->hseSrv.getAttrReq.pAttr = HSE_PTR_TO_HOST_ADDR(&version_data);

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		HseResponse= Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, MU_ADMIN_CHANNEL_U8, &HseIp_aRequest[MU_ADMIN_CHANNEL_U8], pHseSrvDescriptor);

		if (HSE_SRV_RSP_OK != HseResponse)
		{
			// TODO: Response error
			return 0; // error
		}
		if ((CurrVersion.fwTypeId != version_data.fwTypeId) ||
			(CurrVersion.majorVersion != version_data.majorVersion) ||
			(CurrVersion.minorVersion != version_data.minorVersion) ||
			(CurrVersion.patchVersion != version_data.patchVersion) ||
			(CurrVersion.reserved != version_data.reserved))
		{
			// TODO: Response error
			return 0; // error
		}
		// Response is OK version matched
		return 1;
	}



	bool HSE_GetTRNG(uint8_t *rngNumBuf, uint32_t numofRNG)
	{


		hseSrvResponse_t HseResponse = HSE_SRV_RSP_GENERAL_ERROR;
		hseSrvDescriptor_t *pHseSrvDescriptor;
		pHseSrvDescriptor = &Hse_aSrvDescriptor[MU_ADMIN_CHANNEL_U8];
		hseGetRandomNumSrv_t *pGetRndSrv;

		uint8_t srvChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pGetRndSrv = &(pHseSrvDescriptor->hseSrv.getRandomNumReq);
		pGetRndSrv->rngClass = HSE_RNG_CLASS_DRG3;
		pGetRndSrv->pRandomNum = (HOST_ADDR)rngNumBuf;
		pGetRndSrv->randomNumLength = numofRNG;
		/* Fill the descriptor for the HSE request. Because the request to get an attribute is an administrative one,
		it should be sent over channel 0 */
		pHseSrvDescriptor->srvId = HSE_SRV_ID_GET_RANDOM_NUM;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		HseResponse =  Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, srvChannel, &HseIp_aRequest[MU_ADMIN_CHANNEL_U8], pHseSrvDescriptor);

		if (HSE_SRV_RSP_OK != HseResponse)
		{
			// TODO: Response error
			return 0; // error
		}
		return 1;
	}


	bool HSE_getHSECapabilites(void)
	{
		hseSrvResponse_t HseResponse;

				hseSrvDescriptor_t *pHseSrvDescriptor;

				/* Fill the descriptor for the HSE request. Because the request to get an attribute is an administrative one,
				it should be sent over channel 0 */
				pHseSrvDescriptor = &Hse_aSrvDescriptor[MU_ADMIN_CHANNEL_U8];

				pHseSrvDescriptor->srvId = HSE_SRV_ID_GET_ATTR;
				pHseSrvDescriptor->hseSrv.getAttrReq.attrId = HSE_CAPABILITIES_ATTR_ID;
				pHseSrvDescriptor->hseSrv.getAttrReq.attrLen = sizeof(hseAttrCapabilities_t);
				pHseSrvDescriptor->hseSrv.getAttrReq.pAttr = HSE_PTR_TO_HOST_ADDR(&Hse_AttrCapabilities);

				/* Build the request to be sent to Hse Ip layer */
				HseIp_aRequest[MU_ADMIN_CHANNEL_U8].eReqType = HSE_IP_REQTYPE_SYNC;
				HseIp_aRequest[MU_ADMIN_CHANNEL_U8].u32Timeout = TIMEOUT_TICKS_U32;

				/* Send the request to Hse Ip layer */
				HseResponse= Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, MU_ADMIN_CHANNEL_U8, &HseIp_aRequest[MU_ADMIN_CHANNEL_U8], pHseSrvDescriptor);
				if (HSE_SRV_RSP_OK != HseResponse)
								{
									// TODO: Response error
									return 0; // error
								}
								return 1;

	}



	hseSrvResponse_t HSE_ActivateAllMuInstances(void)
	{
		hseSrvResponse_t HseResponse;
	    uint8               u8Index;
	    hseSrvDescriptor_t* pHseSrvDescriptor;

	    /* MU 0 is by default activated and should stay like this */
	    Hse_MuConfig.muInstances[0U].muConfig = HSE_MU_ACTIVATED;

	    /* Activate the other MU instances */
	    for(u8Index = 1U; u8Index < HSE_IP_NUM_OF_MU_INSTANCES; u8Index++)
	    {
	        Hse_MuConfig.muInstances[u8Index].muConfig = HSE_MU_ACTIVATED;
	    }

	    /* Fill the descriptor for the HSE request. Because the request to set an attribute is an administrative one,
	    it should be sent over channel 0. */
	    pHseSrvDescriptor = &Hse_aSrvDescriptor[MU_ADMIN_CHANNEL_U8];

	    pHseSrvDescriptor->srvId                     = HSE_SRV_ID_SET_ATTR;
	    pHseSrvDescriptor->hseSrv.setAttrReq.attrId  = HSE_MU_CONFIG_ATTR_ID;
	    pHseSrvDescriptor->hseSrv.setAttrReq.attrLen = sizeof(hseAttrMUConfig_t);
	    pHseSrvDescriptor->hseSrv.setAttrReq.pAttr   = HSE_PTR_TO_HOST_ADDR(&Hse_MuConfig);

	    /* Build the request to be sent to Hse Ip layer */
	    HseIp_aRequest[MU_ADMIN_CHANNEL_U8].eReqType   = HSE_IP_REQTYPE_SYNC;
	    HseIp_aRequest[MU_ADMIN_CHANNEL_U8].u32Timeout = TIMEOUT_TICKS_U32;

	    /* Send the request to Hse Ip layer. As the MU0 is the only one used for now by HSE to listen for requests,
	    the request will be sent on MU0 */
	    HseResponse=  Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, MU_ADMIN_CHANNEL_U8, &HseIp_aRequest[MU_ADMIN_CHANNEL_U8], pHseSrvDescriptor);
	    if (HSE_SRV_RSP_OK != HseResponse)
	    {
	    // TODO: Response error
	    	return 0; // error
	    }
	    return 1;

	}

	bool HSE_genreateSHA(uint8_t* in_databuf, uint32_t in_databufLen, uint8_t* resultbuf, uint8_t resultbuf_length )
	{
		p_length = resultbuf_length;	//HSE needs the variable in dSpace so need to give it global state
		hseSrvResponse_t HseResponse;
		hseSrvDescriptor_t *pHseSrvDescriptor;
		pHseSrvDescriptor = &Hse_aSrvDescriptor[MU_ADMIN_CHANNEL_U8];
		hseHashSrv_t *pHSE_hashBuff;
		uint8_t srvChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pHseSrvDescriptor->srvId = HSE_SRV_ID_HASH;
		pHSE_hashBuff = &(pHseSrvDescriptor->hseSrv.hashReq);
		pHSE_hashBuff->accessMode = HSE_ACCESS_MODE_ONE_PASS;
		pHSE_hashBuff->inputLength = in_databufLen;
		pHSE_hashBuff->hashAlgo=HSE_HASH_ALGO_SHA2_256;
		pHSE_hashBuff->pHash= (HOST_ADDR)resultbuf;
		pHSE_hashBuff->pHashLength = (HOST_ADDR)&p_length;
		pHSE_hashBuff->pInput =(HOST_ADDR) in_databuf;
		pHSE_hashBuff->sgtOption=HSE_SGT_OPTION_NONE;
		pHSE_hashBuff->streamId= 0;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].eReqType = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[MU_ADMIN_CHANNEL_U8].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		HseResponse= Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, srvChannel, &HseIp_aRequest[MU_ADMIN_CHANNEL_U8], pHseSrvDescriptor);
		if (HSE_SRV_RSP_OK != HseResponse)
		{
			// TODO: Response error
			return 0; // error
		}
		return 1;
	}


	hseSrvResponse_t HSE_FormatHseKeyCatalogs(void)
	{
	    hseSrvDescriptor_t* pHseSrvDescriptor;
	    hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
	    uint8               u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);


	        /* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
	        pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
	        memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
	        hseFormatKeyCatalogsSrv_t*  pFormatKeyCatalogsReq = &(pHseSrvDescriptor->hseSrv.formatKeyCatalogsReq);
	        /* Create the service request for HSE by setting the descriptor's members */
	        pHseSrvDescriptor->srvId = HSE_SRV_ID_FORMAT_KEY_CATALOGS;
	        pFormatKeyCatalogsReq->pNvmKeyCatalogCfg = (HOST_ADDR)&Hse_aNvmKeyCatalog;
	        pFormatKeyCatalogsReq->pRamKeyCatalogCfg = (HOST_ADDR)&Hse_aRamKeyCatalog;

	        /* Build the request to be sent to Hse Ip layer */
	        HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
	        HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

	        /* Send the request to Hse Ip layer */
	        RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

	    return RetVal;
	}





	hseSrvResponse_t HSE_ImportAESKey(void)
	{
		hseSrvDescriptor_t*      pHseSrvDescriptor;
		hseImportKeySrv_t*       pImportKeyReq;
		hseSrvResponse_t         RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8                    u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);
		static hseKeyInfo_t      KeyInfo;

		memset(&KeyInfo, 0, sizeof(KeyInfo));
		KeyInfo.keyFlags  = (HSE_KF_USAGE_ENCRYPT | HSE_KF_USAGE_DECRYPT);
		KeyInfo.keyBitLen = 128U;
		KeyInfo.keyType   = HSE_KEY_TYPE_AES;

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pImportKeyReq = &(pHseSrvDescriptor->hseSrv.importKeyReq);

		/* Create the service request for HSE by setting the descriptor's members.
		   Imports aes128_key0 as a plain, unauthenticated AES128 key into the RAM key catalog slot
		   identified by AES_RAM_KEY_HANDLE (group 1, slot 0). */
		pHseSrvDescriptor->srvId                  = HSE_SRV_ID_IMPORT_KEY;
		pImportKeyReq->targetKeyHandle            = AES_RAM_KEY_HANDLE;
		pImportKeyReq->pKeyInfo                   = (HOST_ADDR)&KeyInfo;
		pImportKeyReq->pKey[0]                    = 0U;
		pImportKeyReq->pKey[1]                    = 0U;
		pImportKeyReq->pKey[2]                    = (HOST_ADDR)aes128_key0;
		pImportKeyReq->keyLen[0]                  = 0U;
		pImportKeyReq->keyLen[1]                  = 0U;
		pImportKeyReq->keyLen[2]                  = sizeof(aes128_key0);
		pImportKeyReq->cipher.cipherKeyHandle      = HSE_INVALID_KEY_HANDLE;
		pImportKeyReq->keyContainer.authKeyHandle  = HSE_INVALID_KEY_HANDLE;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}


	/*!
	 * @brief       Reads back the stored properties of AES_RAM_KEY_HANDLE (flags, key type, bit length) without
	 *              exposing the raw key value. Confirms the key was actually stored by HSE_ImportAESKey().
	 *              Result is left in HSE_AesKeyInfo for inspection.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_GetAesKeyInfo(void)
	{
		hseSrvDescriptor_t*   pHseSrvDescriptor;
		hseGetKeyInfoSrv_t*   pGetKeyInfoReq;
		hseSrvResponse_t      RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8                 u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		memset(&HSE_AesKeyInfo, 0, sizeof(HSE_AesKeyInfo));

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pGetKeyInfoReq = &(pHseSrvDescriptor->hseSrv.getKeyInfoReq);

		/* Create the service request for HSE by setting the descriptor's members */
		pHseSrvDescriptor->srvId    = HSE_SRV_ID_GET_KEY_INFO;
		pGetKeyInfoReq->keyHandle   = AES_RAM_KEY_HANDLE;
		pGetKeyInfoReq->pKeyInfo    = (HOST_ADDR)&HSE_AesKeyInfo;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}


	/*!
	 * @brief       Reads back the stored properties of AES_NVM_KEY_HANDLE (flags, key type, bit length) without
	 *              exposing the raw key value. HSE_SRV_RSP_KEY_EMPTY means the slot has never been provisioned yet.
	 *              Result is left in HSE_NvmAesKeyInfo for inspection.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_GetNvmAesKeyInfo(void)
	{
		hseSrvDescriptor_t*   pHseSrvDescriptor;
		hseGetKeyInfoSrv_t*   pGetKeyInfoReq;
		hseSrvResponse_t      RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8                 u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		memset(&HSE_NvmAesKeyInfo, 0, sizeof(HSE_NvmAesKeyInfo));

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pGetKeyInfoReq = &(pHseSrvDescriptor->hseSrv.getKeyInfoReq);

		/* Create the service request for HSE by setting the descriptor's members */
		pHseSrvDescriptor->srvId    = HSE_SRV_ID_GET_KEY_INFO;
		pGetKeyInfoReq->keyHandle   = AES_NVM_KEY_HANDLE;
		pGetKeyInfoReq->pKeyInfo    = (HOST_ADDR)&HSE_NvmAesKeyInfo;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}


	/*!
	 * @brief       Imports aes128_key0 as a plain, unauthenticated AES128 key into the NVM key catalog slot
	 *              identified by AES_NVM_KEY_HANDLE (group 0, slot 0). Unlike the RAM key, this key persists
	 *              across reset once written to internal flash.
	 * @details     A plain, unauthenticated import is only allowed into an EMPTY NVM slot; overwriting an
	 *              existing NVM key requires an authenticated import. So on every boot this first calls
	 *              HSE_GetNvmAesKeyInfo() to check whether the slot is already provisioned from a previous
	 *              boot, and skips re-importing if it is (avoids failing on the 2nd+ boot, and avoids
	 *              unnecessary flash wear from re-writing the same key every boot).
	 *
	 * @return      hseSrvResponse_t. HSE_SRV_RSP_OK also covers the "already provisioned, skipped" case.
	 */
	hseSrvResponse_t HSE_ImportNvmAESKey(void)
	{
		hseSrvDescriptor_t*      pHseSrvDescriptor;
		hseImportKeySrv_t*       pImportKeyReq;
		hseSrvResponse_t         RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8                    u8MuChannel;
		static hseKeyInfo_t      KeyInfo;

		HSE_GetNvmAesKeyInfoResponse = HSE_GetNvmAesKeyInfo();
		if (HSE_SRV_RSP_OK == HSE_GetNvmAesKeyInfoResponse)
		{
			/* Already provisioned by an earlier boot - nothing to do. */
			return HSE_SRV_RSP_OK;
		}

		u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		memset(&KeyInfo, 0, sizeof(KeyInfo));
		KeyInfo.keyFlags   = (HSE_KF_USAGE_ENCRYPT | HSE_KF_USAGE_DECRYPT);
		KeyInfo.keyBitLen  = 128U;
		KeyInfo.keyCounter = 0U;
		KeyInfo.keyType    = HSE_KEY_TYPE_AES;

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pImportKeyReq = &(pHseSrvDescriptor->hseSrv.importKeyReq);

		pHseSrvDescriptor->srvId                  = HSE_SRV_ID_IMPORT_KEY;
		pImportKeyReq->targetKeyHandle            = AES_NVM_KEY_HANDLE;
		pImportKeyReq->pKeyInfo                   = (HOST_ADDR)&KeyInfo;
		pImportKeyReq->pKey[0]                    = 0U;
		pImportKeyReq->pKey[1]                    = 0U;
		pImportKeyReq->pKey[2]                    = (HOST_ADDR)aes128_key0;
		pImportKeyReq->keyLen[0]                  = 0U;
		pImportKeyReq->keyLen[1]                  = 0U;
		pImportKeyReq->keyLen[2]                  = sizeof(aes128_key0);
		pImportKeyReq->cipher.cipherKeyHandle      = HSE_INVALID_KEY_HANDLE;
		pImportKeyReq->keyContainer.authKeyHandle  = HSE_INVALID_KEY_HANDLE;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		/* Refresh the cached key info now that the key has (hopefully) been written */
		HSE_GetNvmAesKeyInfoResponse = HSE_GetNvmAesKeyInfo();

		return RetVal;
	}


	/*!
	 * @brief       Erases just the NVM key group holding AES_NVM_KEY_HANDLE, leaving the RAM catalog and
	 *              any other NVM key groups untouched.
	 * @details     Not called automatically anywhere - RAM keys already clear themselves on every reset,
	 *              but the NVM key persists across reset/reflash, so call this manually (e.g. temporarily
	 *              from HSE_Init(), or from a debugger) whenever you want to wipe the NVM test key and let
	 *              HSE_ImportNvmAESKey() re-provision it from scratch on the next boot. Requires CUST
	 *              SuperUser rights for this group's owner (already granted by default in CUST_DEL life
	 *              cycle, which is what HSE_STATUS_CUST_SUPER_USER being set confirms).
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_EraseNvmAesKey(void)
	{
		hseSrvDescriptor_t*  pHseSrvDescriptor;
		hseEraseKeySrv_t*    pEraseKeyReq;
		hseSrvResponse_t     RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8                u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pEraseKeyReq = &(pHseSrvDescriptor->hseSrv.eraseKeyReq);

		/* Create the service request for HSE by setting the descriptor's members */
		pHseSrvDescriptor->srvId       = HSE_SRV_ID_ERASE_KEY;
		pEraseKeyReq->keyHandle        = AES_NVM_KEY_HANDLE;
		pEraseKeyReq->eraseKeyOptions  = HSE_ERASE_KEYGROUP_ON_MU_IF;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		/* Refresh the cached key info so HSE_NvmAesKeyInfo/HSE_GetNvmAesKeyInfoResponse reflect the erase */
		HSE_GetNvmAesKeyInfoResponse = HSE_GetNvmAesKeyInfo();

		return RetVal;
	}


	/*!
	 * @brief       Reads back the stored properties of AES_GENERATED_RAM_KEY_HANDLE (flags, key type, bit
	 *              length) without exposing the raw key value. Result is left in HSE_GeneratedAesKeyInfo.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_GetGeneratedAesKeyInfo(void)
	{
		hseSrvDescriptor_t*   pHseSrvDescriptor;
		hseGetKeyInfoSrv_t*   pGetKeyInfoReq;
		hseSrvResponse_t      RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8                 u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		memset(&HSE_GeneratedAesKeyInfo, 0, sizeof(HSE_GeneratedAesKeyInfo));

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pGetKeyInfoReq = &(pHseSrvDescriptor->hseSrv.getKeyInfoReq);

		/* Create the service request for HSE by setting the descriptor's members */
		pHseSrvDescriptor->srvId    = HSE_SRV_ID_GET_KEY_INFO;
		pGetKeyInfoReq->keyHandle   = AES_GENERATED_RAM_KEY_HANDLE;
		pGetKeyInfoReq->pKeyInfo    = (HOST_ADDR)&HSE_GeneratedAesKeyInfo;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}


	/*!
	 * @brief       Generates a random AES-128 key inside HSE and stores it directly in the RAM key catalog
	 *              slot identified by AES_GENERATED_RAM_KEY_HANDLE (group 0, slot 1). Unlike
	 *              HSE_ImportAESKey()/HSE_ImportNvmAESKey(), the raw key bytes are never chosen by or
	 *              exposed to the host - HSE's RNG generates them internally, so this is the stronger
	 *              option whenever the application doesn't need a specific, pre-known key value.
	 * @details     RAM keys can always be (re)generated, so unlike HSE_ImportNvmAESKey() this has no
	 *              "already provisioned" guard - every call generates a brand new key and overwrites
	 *              whatever was in that RAM slot before. It only makes sense to call once per boot,
	 *              since RAM (like the imported RAM key) does not persist across reset anyway.
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_GenerateAesKey(void)
	{
		hseSrvDescriptor_t*   pHseSrvDescriptor;
		hseKeyGenerateSrv_t*  pKeyGenReq;
		hseSrvResponse_t      RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8                 u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pKeyGenReq = &(pHseSrvDescriptor->hseSrv.keyGenReq);

		/* Create the service request for HSE by setting the descriptor's members */
		pHseSrvDescriptor->srvId          = HSE_SRV_ID_KEY_GENERATE;
		pKeyGenReq->targetKeyHandle       = AES_GENERATED_RAM_KEY_HANDLE;
		pKeyGenReq->keyInfo.keyFlags      = (HSE_KF_USAGE_ENCRYPT | HSE_KF_USAGE_DECRYPT);
		pKeyGenReq->keyInfo.keyBitLen     = 128U;
		pKeyGenReq->keyInfo.keyType       = HSE_KEY_TYPE_AES;
		pKeyGenReq->keyGenScheme          = HSE_KEY_GEN_SYM_RANDOM_KEY;
		pKeyGenReq->sch.symKey            = 0U; /* no scheme parameters for a random symmetric key */

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		/* Refresh the cached key info now that the key has (hopefully) been generated */
		HSE_GetGeneratedAesKeyInfoResponse = HSE_GetGeneratedAesKeyInfo();

		return RetVal;
	}


	/*!
	 * @brief       Runs a one-pass AES-128 ECB cipher operation using the given AES key handle
	 *              (AES_RAM_KEY_HANDLE or AES_NVM_KEY_HANDLE).
	 * @details     Shared by HSE_AesEncrypt()/HSE_AesDecrypt() (RAM) and HSE_AesEncryptNvm()/HSE_AesDecryptNvm()
	 *              (NVM); not exposed outside this file.
	 *
	 * @param[in]   direction   HSE_CIPHER_DIR_ENCRYPT or HSE_CIPHER_DIR_DECRYPT
	 * @param[in]   keyHandle   AES_RAM_KEY_HANDLE or AES_NVM_KEY_HANDLE
	 * @param[in]   pInput      Source buffer (plaintext for encrypt, ciphertext for decrypt)
	 * @param[out]  pOutput     Destination buffer (ciphertext for encrypt, plaintext for decrypt)
	 * @param[in]   length      Length in bytes of pInput/pOutput; must be a multiple of 16 (AES block size)
	 *
	 * @return      hseSrvResponse_t
	 */
	static hseSrvResponse_t HSE_Aes128EncryptDecrypt(hseCipherDir_t direction, hseKeyHandle_t keyHandle, const uint8_t* pInput, uint8_t* pOutput, uint32_t length)
	{
		hseSrvDescriptor_t* pHseSrvDescriptor;
		hseSymCipherSrv_t*  pCipherReq;
		hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8               u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pCipherReq = &(pHseSrvDescriptor->hseSrv.symCipherReq);

		/* Create the service request for HSE by setting the descriptor's members */
		pHseSrvDescriptor->srvId    = HSE_SRV_ID_SYM_CIPHER;
		pCipherReq->accessMode      = HSE_ACCESS_MODE_ONE_PASS;
		pCipherReq->cipherAlgo      = HSE_CIPHER_ALGO_AES;
		pCipherReq->cipherBlockMode = HSE_CIPHER_BLOCK_MODE_ECB;
		pCipherReq->cipherDir       = direction;
		pCipherReq->sgtOption       = HSE_SGT_OPTION_NONE;
		pCipherReq->keyHandle       = keyHandle;
		pCipherReq->pIV             = 0U; /* ignored for ECB */
		pCipherReq->inputLength     = length;
		pCipherReq->pInput          = (HOST_ADDR)pInput;
		pCipherReq->pOutput         = (HOST_ADDR)pOutput;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       Encrypts pPlainText with the AES-128 RAM key (AES_RAM_KEY_HANDLE) using ECB mode.
	 *
	 * @param[in]   pPlainText   Plaintext buffer, length must be a multiple of 16 bytes
	 * @param[out]  pCipherText  Output buffer for the ciphertext, same length as pPlainText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesEncrypt(const uint8_t* pPlainText, uint8_t* pCipherText, uint32_t length)
	{
		return HSE_Aes128EncryptDecrypt(HSE_CIPHER_DIR_ENCRYPT, AES_RAM_KEY_HANDLE, pPlainText, pCipherText, length);
	}

	/*!
	 * @brief       Decrypts pCipherText with the AES-128 RAM key (AES_RAM_KEY_HANDLE) using ECB mode.
	 *
	 * @param[in]   pCipherText  Ciphertext buffer, length must be a multiple of 16 bytes
	 * @param[out]  pPlainText   Output buffer for the recovered plaintext, same length as pCipherText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesDecrypt(const uint8_t* pCipherText, uint8_t* pPlainText, uint32_t length)
	{
		return HSE_Aes128EncryptDecrypt(HSE_CIPHER_DIR_DECRYPT, AES_RAM_KEY_HANDLE, pCipherText, pPlainText, length);
	}

	/*!
	 * @brief       Runs a one-pass AES-128 CTR cipher operation using the given AES key handle.
	 * @details     Unlike ECB/CBC, CTR needs no block-alignment or padding - the ciphertext is
	 *              always exactly the same length as the plaintext, for any length. In exchange,
	 *              pIV (16 bytes) must be a nonce/counter value that is NEVER reused with the same
	 *              key: reusing an IV lets an attacker XOR the two ciphertexts together and recover
	 *              the XOR of the two plaintexts, which breaks the encryption. Generate a fresh
	 *              random IV per message (e.g. via HSE_GetTRNG()) and store it alongside the
	 *              ciphertext - it does not need to be kept secret, only unique.
	 *
	 * @param[in]   direction   HSE_CIPHER_DIR_ENCRYPT or HSE_CIPHER_DIR_DECRYPT
	 * @param[in]   keyHandle   The AES key to use
	 * @param[in]   pIV         16-byte nonce/counter, must match between encrypt and decrypt
	 * @param[in]   pInput      Source buffer (plaintext for encrypt, ciphertext for decrypt)
	 * @param[out]  pOutput     Destination buffer, same length as pInput
	 * @param[in]   length      Length in bytes of pInput/pOutput; any value, no alignment needed
	 *
	 * @return      hseSrvResponse_t
	 */
	static hseSrvResponse_t HSE_Aes128CtrEncryptDecrypt(hseCipherDir_t direction, hseKeyHandle_t keyHandle, const uint8_t* pIV, const uint8_t* pInput, uint8_t* pOutput, uint32_t length)
	{
		hseSrvDescriptor_t* pHseSrvDescriptor;
		hseSymCipherSrv_t*  pCipherReq;
		hseSrvResponse_t    RetVal      = HSE_SRV_RSP_GENERAL_ERROR;
		uint8               u8MuChannel = Hse_Ip_GetFreeChannel(MU0_INSTANCE_U8);

		/* Optimize a bit the code by storing the address of the channel's descriptor in a pointer */
		pHseSrvDescriptor = &Hse_aSrvDescriptor[u8MuChannel];
		memset(pHseSrvDescriptor, 0, sizeof(hseSrvDescriptor_t));
		pCipherReq = &(pHseSrvDescriptor->hseSrv.symCipherReq);

		/* Create the service request for HSE by setting the descriptor's members */
		pHseSrvDescriptor->srvId    = HSE_SRV_ID_SYM_CIPHER;
		pCipherReq->accessMode      = HSE_ACCESS_MODE_ONE_PASS;
		pCipherReq->cipherAlgo      = HSE_CIPHER_ALGO_AES;
		pCipherReq->cipherBlockMode = HSE_CIPHER_BLOCK_MODE_CTR;
		pCipherReq->cipherDir       = direction;
		pCipherReq->sgtOption       = HSE_SGT_OPTION_NONE;
		pCipherReq->keyHandle       = keyHandle;
		pCipherReq->pIV             = (HOST_ADDR)pIV;
		pCipherReq->inputLength     = length;
		pCipherReq->pInput          = (HOST_ADDR)pInput;
		pCipherReq->pOutput         = (HOST_ADDR)pOutput;

		/* Build the request to be sent to Hse Ip layer */
		HseIp_aRequest[u8MuChannel].eReqType   = HSE_IP_REQTYPE_SYNC;
		HseIp_aRequest[u8MuChannel].u32Timeout = TIMEOUT_TICKS_U32;

		/* Send the request to Hse Ip layer */
		RetVal = Hse_Ip_ServiceRequest(MU0_INSTANCE_U8, u8MuChannel, &HseIp_aRequest[u8MuChannel], pHseSrvDescriptor);

		return RetVal;
	}

	/*!
	 * @brief       Encrypts pPlainText with the AES-128 NVM key (AES_NVM_KEY_HANDLE) using CTR mode.
	 *              Any length is allowed, no padding needed - see HSE_Aes128CtrEncryptDecrypt().
	 *
	 * @param[in]   pIV          16-byte nonce, must be fresh/unique per message (e.g. from HSE_GetTRNG())
	 * @param[in]   pPlainText   Plaintext buffer, any length
	 * @param[out]  pCipherText  Output buffer for the ciphertext, same length as pPlainText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesCtrEncryptNvm(const uint8_t* pIV, const uint8_t* pPlainText, uint8_t* pCipherText, uint32_t length)
	{
		return HSE_Aes128CtrEncryptDecrypt(HSE_CIPHER_DIR_ENCRYPT, AES_NVM_KEY_HANDLE, pIV, pPlainText, pCipherText, length);
	}

	/*!
	 * @brief       Decrypts pCipherText with the AES-128 NVM key (AES_NVM_KEY_HANDLE) using CTR mode.
	 *
	 * @param[in]   pIV          The same 16-byte nonce that was used to encrypt this ciphertext
	 * @param[in]   pCipherText  Ciphertext buffer, any length
	 * @param[out]  pPlainText   Output buffer for the recovered plaintext, same length as pCipherText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesCtrDecryptNvm(const uint8_t* pIV, const uint8_t* pCipherText, uint8_t* pPlainText, uint32_t length)
	{
		return HSE_Aes128CtrEncryptDecrypt(HSE_CIPHER_DIR_DECRYPT, AES_NVM_KEY_HANDLE, pIV, pCipherText, pPlainText, length);
	}

	/*!
	 * @brief       Encrypts pPlainText with the AES-128 NVM key (AES_NVM_KEY_HANDLE) using ECB mode.
	 *
	 * @param[in]   pPlainText   Plaintext buffer, length must be a multiple of 16 bytes
	 * @param[out]  pCipherText  Output buffer for the ciphertext, same length as pPlainText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesEncryptNvm(const uint8_t* pPlainText, uint8_t* pCipherText, uint32_t length)
	{
		return HSE_Aes128EncryptDecrypt(HSE_CIPHER_DIR_ENCRYPT, AES_NVM_KEY_HANDLE, pPlainText, pCipherText, length);
	}

	/*!
	 * @brief       Decrypts pCipherText with the AES-128 NVM key (AES_NVM_KEY_HANDLE) using ECB mode.
	 *
	 * @param[in]   pCipherText  Ciphertext buffer, length must be a multiple of 16 bytes
	 * @param[out]  pPlainText   Output buffer for the recovered plaintext, same length as pCipherText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesDecryptNvm(const uint8_t* pCipherText, uint8_t* pPlainText, uint32_t length)
	{
		return HSE_Aes128EncryptDecrypt(HSE_CIPHER_DIR_DECRYPT, AES_NVM_KEY_HANDLE, pCipherText, pPlainText, length);
	}

	/*!
	 * @brief       Encrypts pPlainText with the AES-128 key generated by HSE_GenerateAesKey()
	 *              (AES_GENERATED_RAM_KEY_HANDLE) using ECB mode.
	 *
	 * @param[in]   pPlainText   Plaintext buffer, length must be a multiple of 16 bytes
	 * @param[out]  pCipherText  Output buffer for the ciphertext, same length as pPlainText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesEncryptGenerated(const uint8_t* pPlainText, uint8_t* pCipherText, uint32_t length)
	{
		return HSE_Aes128EncryptDecrypt(HSE_CIPHER_DIR_ENCRYPT, AES_GENERATED_RAM_KEY_HANDLE, pPlainText, pCipherText, length);
	}

	/*!
	 * @brief       Decrypts pCipherText with the AES-128 key generated by HSE_GenerateAesKey()
	 *              (AES_GENERATED_RAM_KEY_HANDLE) using ECB mode.
	 *
	 * @param[in]   pCipherText  Ciphertext buffer, length must be a multiple of 16 bytes
	 * @param[out]  pPlainText   Output buffer for the recovered plaintext, same length as pCipherText
	 * @param[in]   length       Length in bytes of both buffers
	 *
	 * @return      hseSrvResponse_t
	 */
	hseSrvResponse_t HSE_AesDecryptGenerated(const uint8_t* pCipherText, uint8_t* pPlainText, uint32_t length)
	{
		return HSE_Aes128EncryptDecrypt(HSE_CIPHER_DIR_DECRYPT, AES_GENERATED_RAM_KEY_HANDLE, pCipherText, pPlainText, length);
	}


#ifdef ERASE_HSE
	/*!
	 * @brief       Erase HSE and update the file as required
	 * @details     This function will be used to erase HSE and update the HSE FW.
	 *
	 * @param[in]   NA
	 *
	 * @return      return type/name
	 */
	bool HSE_EraseHSE(void)
	{

		static volatile hseSrvResponse_t HseResponse;

		HseResponse = HSE_GetAttributeData(MU0_INSTANCE_U8, &Hse_AttrCapabilities,
										   sizeof(Hse_AttrCapabilities), HSE_SRV_ID_ERASE_FW, HSE_CAPABILITIES_ATTR_ID);
		/* Send the request to Hse Ip layer */
		if (HSE_SRV_RSP_OK != HseResponse)
		{
			// TODO: Response error
			return 0; // error
		}
		// Response is ok we now check the version
		return 1;
	}

#endif // ERASE_HSE
#ifdef __cplusplus
}
#endif

/** @} */
