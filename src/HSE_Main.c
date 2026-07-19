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
	    /* NvmKeyGroup_MASTER_ECU_KEY__BOOT_MAC_KEY__Key1_To_Key10 */
	    {HSE_ALL_MU_MASK, HSE_KEY_OWNER_CUST, HSE_KEY_TYPE_AES, 4U, HSE_KEY128_BITS, {0U, 0U}},
	    /* Marker to end the key catalog */
	    {0U, 0U, 0U, 0U, 0U, {0U, 0U}}
	};

	/* Table containing RAM key catalog entries */
	static hseKeyGroupCfgEntry_t Hse_aRamKeyCatalog[] =
	{
	    /* RamKeyGroup_RamKey */
	    {HSE_ALL_MU_MASK, HSE_KEY_OWNER_CUST, HSE_KEY_TYPE_AES, 4U, HSE_KEY128_BITS, {0U, 0U}},
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
#define AES_RAM_KEY_HANDLE GET_KEY_HANDLE(HSE_KEY_CATALOG_ID_RAM,1,0)

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

	/* Holds the raw response of the last HSE_ImportAESKey() call, for inspection */
	hseSrvResponse_t HSE_ImportKeyResponse = HSE_SRV_RSP_GENERAL_ERROR;

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

		    /* =============================================================================================================================== */
		    /*      Format HSE Nvm and Ram key catalogs                                                                                             */
		    /* =============================================================================================================================== */
			status = HSE_FormatHseKeyCatalogs();
		    /* =============================================================================================================================== */
		    /*    Import a key in the  RAM key slot                                                                                         */
		    /* =============================================================================================================================== */
			HSE_ImportKeyResponse = HSE_ImportAESKey();

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
