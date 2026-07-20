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
#include "LED.h"


/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/


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
	    	 	 HSE_Example_StoreEncryptedDataDemo();
#ifdef RUN_SECURE_BOOT_PHASE1_DEMO
	    	 	 HSE_SecureBoot_Phase1_Demo();
#endif // RUN_SECURE_BOOT_PHASE1_DEMO

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
