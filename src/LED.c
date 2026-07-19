/**
 ******************************************************************************
 * @file     LED.c
 * @author   rjadhav
 * @version  V1.0.0
 * @date     Jul 10, 2026
 * @brief    Add file details
 * @location /test/src/LED.c
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

#include "LED.h"
/*==================================================================================================
*                                        GLOBAL CONSTANTS
==================================================================================================*/


/*==================================================================================================
*                                        GLOBAL VARIABLES
==================================================================================================*/

bool val=0;
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
 * @param[in]   para1		Small brief of the variable    
 * @param[in]   para2       Small brief of the variable 
 *
 * @return      return type/name
 */
 
 void LED_Init(void)
 {
	 Siul2_Port_Ip_Init(NUM_OF_CONFIGURED_PINS_PortContainer_0_BOARD_InitPeripherals,
			 g_pin_mux_InitConfigArr_PortContainer_0_BOARD_InitPeripherals);
 }

 void LED_ToggleLED(uint8_t led)
 {

	 switch (led)
	 {

	 case LED_RED_PIN:
		 val = !val;
		 Siul2_Dio_Ip_WritePin(LED_RED_PORT, LED_RED_PIN, val);
		 break;
	 case LED_BLUE_PIN: val =! val;
	 Siul2_Dio_Ip_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, val);
		 break;
	 case LED_GREEN_PIN: val =! val;
	 Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, val);
		 break;
	 case LED_YELLOW_PIN:
		 val =! val;
		 Siul2_Dio_Ip_WritePin(LED_RED_PORT, LED_RED_PIN, val);
		 Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, val);
		 break;
	 case LED_CYAN_PIN:
		 val =! val;
		 Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, val);
		 Siul2_Dio_Ip_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, val);
		 break;

	 }


 }


#ifdef __cplusplus
}
#endif

/** @} */
