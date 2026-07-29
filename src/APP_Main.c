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
#include "UART_Print.h"
#include "LED.h"


/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/

//#define RUN_SECURE_BOOT_PHASE1_DEMO
//#define RUN_MAC_ECC_EXAMPLE
/* RUN_APP_SMR_PROVISIONING is toggled in HSE_AppSmrProvision.h instead of here - that file's
   entire body (not just this call site) must disappear when it's off, so the macro needs to be
   visible to that translation unit too, not just this one. See that header's file-level
   @details for the required two-build sequence before flipping it on. */
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
	     UART_Print_Init();
	     UART_Print_String("\r\n=== GSLU_APP booting ===\r\n");
	     if(FALSE == HSE_checkHseFwFeatureFlagEnabled())
	     {
	    	 UART_Print_String("HSE FW usage flag not enabled - unlocking UTEST sector\r\n");
	    	 HSE_UnlockUtestSector();
	     }
	     else
	     {
	    	 	 HSE_Init();
	    	 	 UART_Print_Status("HSE_Status", (uint32_t)HSE_Status, (uint32_t)HSE_VER_OK);
//	    	 	 HSE_Example_StoreEncryptedDataDemo();
#ifdef RUN_SECURE_BOOT_PHASE1_DEMO
	    	 	 HSE_SecureBoot_Phase1_Demo();
	    	 	 UART_Print_HseResponse("SecureBoot ImportSmrSignKeyRam", HSE_ImportSmrSignKeyRamResponse);
	    	 	 UART_Print_HseResponse("SecureBoot ComputeSmrInstallTag", HSE_ComputeSmrInstallTagResponse);
	    	 	 UART_Print_HseResponse("SecureBoot ImportSmrVerifyKeyNvm", HSE_ImportSmrVerifyKeyNvmResponse);
	    	 	 UART_Print_HseResponse("SecureBoot InstallSmrEntry", HSE_InstallSmrEntryResponse);
	    	 	 UART_Print_HseResponse("SecureBoot EraseSmrSignKeyRam", HSE_EraseSmrSignKeyRamResponse);
	    	 	 UART_Print_HseResponse("SecureBoot VerifySmrEntry (expect OK)", HSE_VerifySmrEntryResponse);
	    	 	 UART_Print_HseResponse("SecureBoot VerifyAfterCorruption (expect FAIL)", HSE_VerifySmrEntryAfterCorruptionResponse);
	    	 	 UART_Print_Bool("SecureBoot NegativeControlPassed", HSE_SecureBootNegativeControlPassed);
#endif // RUN_SECURE_BOOT_PHASE1_DEMO
#ifdef RUN_MAC_ECC_EXAMPLE
	    	 	 /* ECC key-pair generate/sign/verify needs the ECC_PAIR/ECC_PUB RAM catalog
	    	 	    groups added in HSE_Main.c - only present after a build with
	    	 	    RUN_FORMAT_KEY_CATALOGS_IN_INIT defined has run at least once. */
	    	 	 HSE_Mac_Ecc_Example_Demo();
	    	 	 UART_Print_Bool("Mac FlashRoundTripVerified", HSE_Mac_FlashRoundTripVerified);
	    	 	 UART_Print_Bool("Ecc FlashRoundTripVerified", HSE_Ecc_FlashRoundTripVerified);
#endif // RUN_MAC_ECC_EXAMPLE
#ifdef RUN_APP_SMR_PROVISIONING
	    	 	 /* One-time provisioning build (BOOTLOADER_SECURE_BOOT_PLAN.md Stage 2) - see
	    	 	    HSE_AppSmrProvision.h's file-level @details. Flash this once, confirm
	    	 	    HSE_AppSmr_ImportVerifyKeyResponse and HSE_AppSmr_InstallEntryResponse ==
	    	 	    HSE_SRV_RSP_OK via debugger, then flip the flag back OFF in
	    	 	    HSE_AppSmrProvision.h and reflash the original all-zero build (the one you
	    	 	    actually ran tools/sign_tool.py against) permanently - that's what Bootlaoder
	    	 	    will check going forward. Also needs the NVM ECC_PUB catalog group added in
	    	 	    HSE_Main.c, same reformat caveat as the RAM ECC groups. */
	    	 	 HSE_AppSmr_Provision_Demo();
	    	 	 UART_Print_HseResponse("AppSmr GetVerifyKeyInfo", HSE_AppSmr_GetVerifyKeyInfoResponse);
	    	 	 UART_Print_HseResponse("AppSmr ImportVerifyKey", HSE_AppSmr_ImportVerifyKeyResponse);
	    	 	 UART_Print_HseResponse("AppSmr InstallEntry", HSE_AppSmr_InstallEntryResponse);
#endif // RUN_APP_SMR_PROVISIONING
	    	 	 UART_Print_String("=== Provisioning/demo sequence complete ===\r\n");

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
	/* RTD/src/Hse_Ip.c's Hse_Ip_RxIrqHandler() is the real MU0 RX interrupt handler - it clears
	   the RX interrupt flags (Hse_Ip_ClearInterruptFlags()) and, for a spurious/unclaimed
	   interrupt, still clears the status flag itself specifically to prevent the interrupt from
	   re-triggering immediately and looping forever (see that function's own comment). Leaving
	   this handler empty left that flag permanently set, so every HSE RX event re-entered this
	   NVIC-priority-0 ISR back-to-back for as long as the flag stayed set - starving everything
	   else on the core, including the UART print that runs right after HSE_Init()'s long chain
	   of HSE service calls (which is exactly the one call observed to come out corrupted). */
	Hse_Ip_RxIrqHandler(MU0_INSTANCE_U8);
	}

#ifdef __cplusplus
}
#endif

/** @} */
