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

/* Independent per-channel software toggle state - NOT read back from hardware. The R/G/B LED
   pins are configured with inputBuffer = PORT_INPUT_BUFFER_DISABLED (output-only), so
   Siul2_Dio_Ip_ReadPin() cannot reliably report their actual output level on this board; a
   read-then-invert toggle silently stuck every LED on its first call. Tracking state in software
   per-channel avoids depending on read-back entirely, while still fixing the original bug (one
   shared toggle flag for every color, which let an unrelated color's leftover state bleed into
   whichever colors the caller is actively toggling). */
static bool LedRedState   = false;
static bool LedGreenState = false;
static bool LedBlueState  = false;

static void LED_ToggleRed(void)
{
	LedRedState = !LedRedState;
	Siul2_Dio_Ip_WritePin(LED_RED_PORT, LED_RED_PIN, LedRedState);
}

static void LED_ToggleGreen(void)
{
	LedGreenState = !LedGreenState;
	Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, LedGreenState);
}

static void LED_ToggleBlue(void)
{
	LedBlueState = !LedBlueState;
	Siul2_Dio_Ip_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, LedBlueState);
}

/*!
 * @brief       Forces every color channel (red/green/blue) off, and resets their software toggle
 *              state to match. Call once when entering a new steady LED pattern (e.g. a halt
 *              loop) so a leftover channel state from whatever pattern was running before can't
 *              bleed into the new one.
 */
void LED_AllOff(void)
{
	LedRedState = false;
	LedGreenState = false;
	LedBlueState = false;
	Siul2_Dio_Ip_WritePin(LED_RED_PORT, LED_RED_PIN, 0U);
	Siul2_Dio_Ip_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, 0U);
	Siul2_Dio_Ip_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, 0U);
}

 void LED_ToggleLED(uint8_t led)
 {

	 switch (led)
	 {

	 case LED_RED_PIN:
		 LED_ToggleRed();
		 break;
	 case LED_BLUE_PIN:
		 LED_ToggleBlue();
		 break;
	 case LED_GREEN_PIN:
		 LED_ToggleGreen();
		 break;
	 case LED_YELLOW_PIN:
		 LED_ToggleRed();
		 LED_ToggleGreen();
		 break;
	 case LED_CYAN_PIN:
		 LED_ToggleGreen();
		 LED_ToggleBlue();
		 break;

	 }


 }


#ifdef __cplusplus
}
#endif

/** @} */
