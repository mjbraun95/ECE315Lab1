
/*
 *  ECE- 315 WINTER 2021 - COMPUTER INTERFACING COURSE
 *
 *  Created on	: 	14 July, 2021
 *  Author		: 	Shyama M. Gandhi, Mazen Elbaz
 *
 * -----------------------------------------------------------------------------------------------------------
 * KEYPAD AND SEVEN-SEGEMENT DECODER IMPLEMENTATION FOR PART-1 LAB-1
 * Run the given code template. You will see that the SDK terminal will display any key presses from the user. Initially, the SSD is OFF and you won't see any key press being displayed on SSD.
 * In order to see the key presses on SSD, you need to set the direction of SSD GPIO in the int main() as indicated by the comment. You also have to finish the remaining cases in the SSD_decode() function.
 * Run the code again and you will now see the key presses being displayed even on the right SSD.
 *
 * Next Goal: Current key being pressed, shows on the right SSD. When next key is pressed, the previous digit is shown on the left SSD and the new key press shows on the right SSD.
 * Example: Key pressed: A, right ssd: A
 * 			Key pressed: 8, left ssd: A, right ssd: 8
 * 			Key pressed: 7, left ssd: 8, right ssd: 7
 *
 * 			and so on...
 *
 * Write few lines of the code in the given commented section to implement this functionality and verify this on the board.
 *
 * Try to increase or decrease this delay period and see how you see the segments are being displayed. Default template has 100ms delay.
 * Increase the frequency to a point where digits are stable. Can you find this approximate value of frequency/time period?
 * -----------------------------------------------------------------------------------------------------------
 *
 */

//Include FreeRTOS Library
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "xparameters.h"
#include "xgpio.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"

#include "pmodkypd.h"	//PMOD keypad driver provided by digilent
#include "sleep.h"
#include "xil_cache.h"

// Parameter definitions
#define SSD_DEVICE_ID		XPAR_AXI_GPIO_PMOD_SSD_DEVICE_ID
#define KYPD_DEVICE_ID		XPAR_AXI_GPIO_PMOD_KEYPAD_DEVICE_ID

//Button Variable
XGpio SSDInst, KYPDInst;

/* The Tx described at the top of this file. */
static void prvTxTask( void *pvParameters );

void DemoInitialize();

u32 SSD_decode(u8 key_value,u8 cathode);

PmodKYPD myDevice;

/*-----------------------------------------------------------*/

static TaskHandle_t xTxTask;

// keytable is determined as follows (indices shown in Keypad position below)
// 12 13 14 15
// 8  9  10 11
// 4  5  6  7
// 0  1  2  3

#define DEFAULT_KEYTABLE "0FED789C456B123A"

void DemoInitialize() {
   KYPD_begin(&myDevice, XPAR_AXI_GPIO_PMOD_KEYPAD_BASEADDR);
   KYPD_loadKeyTable(&myDevice, (u8*) DEFAULT_KEYTABLE);
}

/*
 * This function is hard coded to display the seven segment bits for two cases when cathode=0 and when cathode=1.
 * See the lines 126 to 140 in zybo.xdc file in the Vivado design Suite. In the Sources tab -> Under Constraints -> zybo.xdc
 * Consult the .xdc file and data sheet for SSD to exactly understand the interfacing of these modules with the ZYBO board PMOD header.
 * You have also been provided with a explanation manual for PMOD SSD. Consult this manual to code the remaining cases from 6-F.
 * Complete this function to include the encoded values for '6' to 'F'.
 * Include the default case too to show that segment do not lit up for any other letters than 0 to F or during the start of the application.
 */
u32 SSD_decode(u8 key_value, u8 cathode){

	switch(key_value){
	case 48: if(cathode==0) return 0b00111111; else return 0b10111111;//0
	case 49: if(cathode==0) return 0b00000110; else return 0b10000110;//1
	case 50: if(cathode==0) return 0b01011011; else return 0b11011011;//2
	case 51: if(cathode==0) return 0b01001111; else return 0b11001111;//3
	case 52: if(cathode==0) return 0b01100110; else return 0b11100110;//4
	case 53: if(cathode==0) return 0b01101101; else return 0b11101101;//5
	case 54: if(cathode==0) return 0b01111101; else return 0b11111101;//6
	case 55: if(cathode==0) return 0b00000111; else return 0b10000111;//7
	case 56: if(cathode==0) return 0b01111111; else return 0b11111111;//8
	case 57: if(cathode==0) return 0b01101111; else return 0b11101111;//9
	case 65: if(cathode==0) return 0b01110111; else return 0b11110111;//A
	case 66: if(cathode==0) return 0b01111100; else return 0b11111100;//b
	case 67: if(cathode==0) return 0b00111001; else return 0b10111001;//C
	case 68: if(cathode==0) return 0b01011110; else return 0b11011110;//d
	case 69: if(cathode==0) return 0b01111001; else return 0b11111001;//E
	case 70: if(cathode==0) return 0b01110001; else return 0b11110001;//F
	default: if(cathode==0) return 0b00000000; else return 0b10000000;
	/**********************************/
	//  Include the remaining cases of 6-F and default case here.....
	/**********************************/

	}
}

// MAIN FUNCTION
int main (void)
{
  int status;

  // Initialize SSD
  status = XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
  if(status != XST_SUCCESS){
    xil_printf("GPIO Initialization for SSD unsuccessful.\r\n");
    return XST_FAILURE;
  }

  /**********************************/
  // Set SSD GPIO direction to output here... (YOU MAY GET AND IDEA ON HOW TO DO THIS FROM LAB_0)
  XGpio_SetDataDirection(&SSDInst, 1, 0x00);


  /**********************************/

  xil_printf("Initialization Complete, System Ready!\n");


  xTaskCreate( prvTxTask,					/* The function that implements the task. */
    			( const char * ) "Tx", 		/* Text name for the task, provided to assist debugging only. */
    			configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
    			NULL, 						/* The task parameter is not used, so set to NULL. */
    			tskIDLE_PRIORITY,			/* The task runs at the idle priority. */
    			&xTxTask );

  DemoInitialize();

  vTaskStartScheduler();

  while(1);

  return 0;
}

/*-----------------------------------------------------------*/
static void prvTxTask( void *pvParameters )
{
	for( ;; ) {
	   u16 keystate;
	   XStatus status, last_status = KYPD_NO_KEY;
	   u8 new_key, current_key = 'x', previous_key = 'x';
	   u32 ssd_value=0;


	   // Initial value of last_key cannot be contained in loaded KEYTABLE string
	   Xil_Out32(myDevice.GPIO_addr, 0xF);

	   xil_printf("Pmod KYPD demo started. Press any key on the Keypad.\r\n");
	   while (1) {
	      // Capture state of each key
	      keystate = KYPD_getKeyStates(&myDevice);

	      // Determine which single key is pressed, if any
	      // if a key is pressed, stored the value of the new key in new_key
	      status = KYPD_getKeyPressed(&myDevice, keystate, &new_key);

	      // Print key detect if a new key is pressed or if status has changed
	      if (status == KYPD_SINGLE_KEY
	            && (status != last_status || new_key != current_key)) {
	         xil_printf("Key Pressed: %c\r\n", (char) new_key);
	         // Before the new key becomes the current key, we will first pass on the value of current_key to previous_key
	         previous_key = current_key;
	         // The current key digit showing on the right SSD will now store the new key pressed by the user
	         current_key = new_key;

	      }
	      else if (status == KYPD_MULTI_KEY && status != last_status)
	         xil_printf("Error: Multiple keys pressed\r\n");

	      last_status = status;

	      //------------------------------------------------
	      XGpio_DiscreteWrite(&SSDInst, 1, 0b10000000);
	      // The most recent key pressed by the user will show on the RIGHT SSD
	      ssd_value = SSD_decode(current_key, 0);		//this function will decode the ASCII value obtained from keypad into the corresponding HEX value for display on SSD
	      XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);	// The GPIO function to write the value obtained from the above function on RIGHT SSD

	  	  vTaskDelay(pdMS_TO_TICKS(10));

	  	  /********************************************************************************/
	  	  // write the 3 lines of code required to display the previous key on LEFT SSD.
	  	  // Hint: Use the similar logic of current key in order to display the previous key on LEFT SSD.
	  	  /********************************************************************************/
	  	  XGpio_DiscreteWrite(&SSDInst, 1, 0b10000000);
	  	  ssd_value = SSD_decode(previous_key, 1);
	  	  XGpio_DiscreteWrite(&SSDInst, 1, ssd_value);
	      //AFTER WRITING THE ABOVE GPIO functions for both SSD segments, increase or decrease the time/frequency and see the frequency where both segments appear to lit up simultaneously
	  	  // in other words, the digit don't blink anymore now!!!
	  	  vTaskDelay(pdMS_TO_TICKS(10));
	   }
	}
}
/*-----------------------------------------------------------*/

