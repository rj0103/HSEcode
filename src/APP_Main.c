/**
 ******************************************************************************
 * @file     App_Main.c
 * @author   rjadhav
 * @version  V1.0.0
 * @date     Jul 7, 2026
 * @brief    Add file details
 * @location /test/src/App_Main.c
 ******************************************************************************
 *
 * <h2><center>&copy; COPYRIGHT 2026-2027 Curtiss-Wright </center></h2>
 ******************************************************************************
 */

#ifdef __cplusplus
extern "C"{
#endif
/*==================================================================================================
*                                          INCLUDE FILES
* 1) system and project includes
* 2) needed interfaces from external units
* 3) internal and external interfaces from this unit
==================================================================================================*/
#include "APP_Main.h"
#include "HSE_Main.h"
#include "HSE_FlashStorage_Example.h"
#include "HSE_SecureBoot.h"
#include "HSE_Mac_Ecc_Example.h"
#include "HSE_AppSmrProvision.h"
#include "LED.h"


/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/

//#define RUN_SECURE_BOOT_PHASE1_DEMO
//#define RUN_MAC_ECC_EXAMPLE
//#define RUN_APP_SMR_PROVISIONING
/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/

/*==================================================================================================
*                           LOCAL TYPEDEFS (STRUCTURES, UNIONS, ENUMS)
==================================================================================================*/


/*==================================================================================================
*                                          LOCAL MACROS
==================================================================================================*/


/*==================================================================================================
*                                         LOCAL CONSTANTS
==================================================================================================*/


/*==================================================================================================
*                                         LOCAL VARIABLES
==================================================================================================*/


/*==================================================================================================
*                                    LOCAL FUNCTION PROTOTYPES
==================================================================================================*/

/*!
 * @brief       Function Brief 
 * @details     Details of the function
 *
 * @param[in]   NA
 *
 * @return      NA
 */
 
 void APP_Main(void)
 {
	 for(uint32_t count=0; count < 1000000U; count++)
	 {
		 __asm volatile ("nop");
	 }
	 	Clock_Ip_Init(Clock_Ip_aClockConfig);
   	 for(uint32_t count=0; count < 1000000U; count++)
   	 {
   		 __asm volatile ("nop");
   	 }
	 	C40_Ip_Init(NULL_PTR);
	 	OsIf_Init(NULL_PTR);

	 	IntCtrl_Ip_Init(&IntCtrlConfig_0);
	     /* Enable ORed RX interrupt for MU-0 */
	     IntCtrl_Ip_EnableIrq(HSE_MU0_RX_IRQn);
	     /* Check if HSE FW usage flag is already enabled. Otherwise program the flag */
	     LED_Init();
	     if(FALSE == HSE_checkHseFwFeatureFlagEnabled())
	     {
	    	 HSE_UnlockUtestSector();
	     }
	     else
	     {
	    	 	 HSE_Init();
//	    	 	 HSE_Example_StoreEncryptedDataDemo();
#ifdef RUN_SECURE_BOOT_PHASE1_DEMO
	    	 	 HSE_SecureBoot_Phase1_Demo();
#endif // RUN_SECURE_BOOT_PHASE1_DEMO
#ifdef RUN_MAC_ECC_EXAMPLE
	    	 	 /* ECC key-pair generate/sign/verify needs the ECC_PAIR/ECC_PUB RAM catalog
	    	 	    groups added in HSE_Main.c - only present after a build with
	    	 	    RUN_FORMAT_KEY_CATALOGS_IN_INIT defined has run at least once. */
	    	 	 HSE_Mac_Ecc_Example_Demo();
#endif // RUN_MAC_ECC_EXAMPLE
#ifdef RUN_APP_SMR_PROVISIONING
	    	 	 /* One-time provisioning build (BOOTLOADER_SECURE_BOOT_PLAN.md Stage 2) - flash
	    	 	    once with tools/sign_tool.py's real output in app_smr_provision_data.h (NOT
	    	 	    the checked-in all-zero placeholder), confirm HSE_AppSmr_VerifyEntryResponse
	    	 	    == HSE_SRV_RSP_OK via debugger, then leave this flag undefined again - the key
	    	 	    and SMR entry both persist in HSE across reset. Also needs the NVM ECC_PUB
	    	 	    catalog group added in HSE_Main.c, same reformat caveat as the RAM ECC groups. */
	    	 	 HSE_AppSmr_Provision_Demo();
#endif // RUN_APP_SMR_PROVISIONING

	     }

	     while(1)
	     {
	    	 //TODO Add required timing tasks
	    	 for(uint32_t count=0; count < 1000000U; count++)
	    	 {
	    		 __asm volatile ("nop");
	    	 }
	    	 if(HSE_OK == HSE_Status)
	    	 {
	    		 LED_ToggleLED(LED_GREEN_PIN);
	    	 }
	    	 else if(NO_HSE == HSE_Status)
	    	 {
	    		 LED_ToggleLED(LED_RED_PIN);
	    	 }
	    	 else if (HSE_VER_OK == HSE_Status)
	    	 {
	    		 LED_ToggleLED(LED_BLUE_PIN);
	    	 }
	    	 else if (HSE_VER_NOK == HSE_Status)
	    	 {
	    		 LED_ToggleLED(LED_YELLOW_PIN);
	    	 }
	    	 else if (HSE_ERASEOK == HSE_Status)
	    	 {
	    		 LED_ToggleLED(LED_CYAN_PIN);
	    	 }


	     }
 }
void HSE_RX_IRQ()
{
	}

#ifdef __cplusplus
}
#endif

/** @} */
