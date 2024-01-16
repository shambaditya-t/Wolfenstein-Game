//***********************************************************************************

// Include files

//***********************************************************************************

#include "gpio.h"



//***********************************************************************************

// defined files

//***********************************************************************************





//***********************************************************************************

// global variables

//***********************************************************************************





//***********************************************************************************

// function prototypes

//***********************************************************************************





//***********************************************************************************

// functions

//***********************************************************************************


/***************************************************************************//**

 * @brief

 *   Setup gpio pins that are being used for the application.

 ******************************************************************************/

void gpio_open(void)
{
  // Set LEDs to be standard output drive with default off (cleared)

  GPIO_DriveStrengthSet(LED0_port, gpioDriveStrengthWeakAlternateWeak);
  GPIO_PinModeSet(LED0_port, LED0_pin, gpioModePushPull, LED0_default);
  GPIO_DriveStrengthSet(LED1_port, gpioDriveStrengthWeakAlternateWeak);
  GPIO_PinModeSet(LED1_port, LED1_pin, gpioModePushPull, LED1_default);


  // Setup Buttons as inputs
  GPIO_DriveStrengthSet(BUTTON0_port, gpioDriveStrengthWeakAlternateWeak);
  GPIO_PinModeSet(BUTTON0_port, BUTTON0_pin, gpioModeInput, BUTTON0_default);
  GPIO_DriveStrengthSet(BUTTON1_port, gpioDriveStrengthWeakAlternateWeak);
  GPIO_PinModeSet(BUTTON1_port, BUTTON1_pin, gpioModeInput, BUTTON1_default);

  //SysTick_Config((CMU_ClockFreqGet(cmuClock_CORE) / 1000)); //setup at 1ms tick interrupts
  NVIC_EnableIRQ(GPIO_ODD_IRQn); //setup IRQs for gpio for odd/even pins.
  NVIC_EnableIRQ(GPIO_EVEN_IRQn);

  GPIO_IntConfig(BUTTON0_port,BUTTON0_pin,true,true,true); //setup interrupts for rising/falling edges
  GPIO_IntConfig(BUTTON1_port,BUTTON1_pin,true,true,true);
}

