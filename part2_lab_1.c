
/*
 *  ECE- 315 WINTER 2021 - COMPUTER INTERFACING COURSE
 *  Created on	: 	14 July, 2021
 *      Author	: 	Shyama M. Gandhi, Mazen Elbaz
 *
 * --------------------------------------------------------------------------------
 * IMPLEMENTATION OF A SINGLE DIGIT LOGICAL AND MODULO CALCULATOR.
 *
 * Inputs Operands from the keypad (such as using two operands op1=9, op2=3)
 * Output of the operation is displayed on the Seven Segment (if one digit output, display it on right segment with left segment as '0'.
 * Two digit output will be displayed on right and left side as well. Say, 9x9=81. '8' on left SSD and '1' on right SSD)
 * Operations available : ^, |, & and %, selected using the push buttons on the board, 0001 for ^, 0010 for |, 0100 for &, 1000 for %
 * Remember that x mod 0 is error so we display -1 on the SSD! (- on left SSD and 1 on the right SSD)
 * Methodology for executing the application on board:
 * You enter op1=9 from keypad, then press E, hold on the btn 0 for addition and keep it pressed. Now, enter the op2=8, press 'E'.
 * btn=1 right now, output will be displayed on the ssd = 01 in this case.
 *
 * --------------------------------------------------------------------------------
 * Please be aware that the initial template will not produce any output. Also, if you see an "error" on the line 195 where this is "if" statement, that is ok! Once you write the required code 
 * on this line, the syntax error will go away! You have to code the sections provided inside the comments. Carefully examine the comments during the
 * course of the code so you don't miss anything.
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

#include "pmodkypd.h"
#include "sleep.h"
#include "xil_cache.h"
#include "math.h"

// Parameter definitions
#define BTNS_DEVICE_ID		XPAR_AXI_GPIO_BUTTONS_DEVICE_ID
#define SSD_DEVICE_ID		XPAR_AXI_GPIO_PMOD_SSD_DEVICE_ID

//Button Variable
XGpio BTNInst, SSDInst;

/* The Tx and Rx tasks as described at the top of this file. */
static void prvTxTask( void *pvParameters );
static void prvRxTask( void *pvParameters );

void DemoInitialize();
u32 SSD_decode(u8 key_value, u8 cathode);

PmodKYPD myDevice;

/*-----------------------------------------------------------*/

static TaskHandle_t xTxTask;
static TaskHandle_t xRxTask;
static QueueHandle_t xQueue = NULL;

#define DEFAULT_KEYTABLE "0FED789C456B123A"

void DemoInitialize() {
   KYPD_begin(&myDevice, XPAR_AXI_GPIO_PMOD_KEYPAD_BASEADDR);
   KYPD_loadKeyTable(&myDevice, (u8*) DEFAULT_KEYTABLE);
}

//valid key press from 0-9 has already been encoded for you.
u32 SSD_decode(u8 key_value, u8 cathode){

	switch(key_value){
	case 0: if(cathode==0) return 0b00111111; else return 0b10111111;
	case 1: if(cathode==0) return 0b00000110; else return 0b10000110;
	case 2: if(cathode==0) return 0b01011011; else return 0b11011011;
	case 3: if(cathode==0) return 0b01001111; else return 0b11001111;
	case 4: if(cathode==0) return 0b01100110; else return 0b11100110;
	case 5: if(cathode==0) return 0b01101101; else return 0b11101101;
	case 6: if(cathode==0) return 0b01111101; else return 0b11111101;
	case 7: if(cathode==0) return 0b00000111; else return 0b10000111;
	case 8: if(cathode==0) return 0b01111111; else return 0b11111111;
	case 9: if(cathode==0) return 0b01101111; else return 0b11101111;
	default:if(cathode==0) return 0b00000000; else return 0b00000000;
	}
}

// MAIN FUNCTION
int main (void)
{
  int status;

  // Initialize Push Buttons
  status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
  if(status != XST_SUCCESS){
	  xil_printf("GPIO Initialization for BUTTONS unsuccessful.\r\n");
	  return XST_FAILURE;
  }

  // Initialize SSD
  status = XGpio_Initialize(&SSDInst, SSD_DEVICE_ID);
  if(status != XST_SUCCESS){
    xil_printf("GPIO Initialization for SSD unsuccessful.\r\n");
    return XST_FAILURE;
  }

  /*************************************/
  //Set the directions of the buttons and SSD GPIO peripherals here
  XGpio_SetDataDirection(&SSDInst, 1, 0x0);
  XGpio_SetDataDirection(&BTNInst, 1, 0xF);
  /*************************************/

  xil_printf("Initialization Complete, System Ready!\n");

  /* Create the two tasks.  The Tx task is given a higher priority than the
  Rx task. Dynamically changing the priority of Rx Task later on so the Rx task will leave the Blocked state and pre-empt the Tx
  task as soon as the Tx task fills the queue. */
  xTaskCreate( prvTxTask,					/* The function that implements the task. */
    			( const char * ) "Tx", 		/* Text name for the task, provided to assist debugging only. */
    			configMINIMAL_STACK_SIZE, 	/* The stack allocated to the task. */
    			NULL, 						/* The task parameter is not used, so set to NULL. */
    			tskIDLE_PRIORITY+2,			/* The task runs at the idle priority. */
    			&xTxTask );

  xTaskCreate( prvRxTask,
    			( const char * ) "Rx",
				configMINIMAL_STACK_SIZE,
				NULL,
    			tskIDLE_PRIORITY + 1,
    			&xRxTask );

  xQueue = xQueueCreate( 2,				/* There are only two spaces in the queue. */
		  	  sizeof( unsigned int ) );	/* Each space in the queue is large enough to hold a uint32_t. */

  /* Check the queue was created. */
  configASSERT(xQueue);

  DemoInitialize();

  vTaskStartScheduler();

  while(1);

  return 0;
}

/*-----------------------------------------------------------*/
static void prvTxTask( void *pvParameters )
{
	UBaseType_t uxPriority;

	for( ;; ) {
	   u16 keystate;
	   XStatus status, last_status = KYPD_NO_KEY;
	   u8 key, last_key = 'x', store_key = 'x';
	   u32 key_stroke_on_SSD=0;

	   // Initial value of last_key cannot be contained in loaded KEYTABLE string
	   Xil_Out32(myDevice.GPIO_addr, 0xF);

	   xil_printf("PMOD KYPD demo started. Press any key on the Keypad.\r\n");

	   uxPriority = uxTaskPriorityGet( NULL );

	   while (1) {

		   /*******************************************/
		   //write one line of code to capture the state of each key here. Hint: use the keystate variable to store the output
		   //Examine the function KYPD_getKeyStates() to implement it.
		   /*******************************************/
		  keystate = KYPD_getKeyStates(&myDevice);
	      // Determine which single key is pressed, if any
	      status = KYPD_getKeyPressed(&myDevice, keystate, &key);

	      if(uxQueueMessagesWaiting( xQueue ) == 2){

	    	  /*********************************/
	    	  // enter the function to dynamically change the priority when queue is full. This way when the queue is full here, we change the priority of this task.
	    	  // and hence queue will be read in the receive task to perform the operation. If you change the priority here dynamically, make sure in the receive task to do the counter part!!!
	    	  /*********************************/
	    	  vTaskPrioritySet( xTxTask, ( uxPriority - 2 ) );
	      }

	      // Print key detect if a new key is pressed or if status has changed
	      if (status == KYPD_SINGLE_KEY
	            && (status != last_status || key != last_key)) {
				 xil_printf("Key Pressed: %c\r\n", (char) key);
				 last_key = key;

				 /***************************************/
				 //Keys 'A', 'B', 'C', 'D' and 'F' will be ignored for this part of the lab 1. So, if user hits these keys, consider them invalid.
				 //write the code to consider these key presses as invalid (use if condition) and prompt a message to user in this case
				 //write the required code inside the "if" black box given below!!!
				 /***************************************/
				 if((char)key == 'A'){
					 xil_printf("Key input is invalid.\n");
				 } else if((char)key == 'B'){
					 xil_printf("Key input is invalid.\n");
				 } else if((char)key == 'C'){
					 xil_printf("Key input is invalid.\n");
				 } else if((char)key == 'D'){
					 xil_printf("Key input is invalid.\n");
				 } else if((char)key == 'F'){
					 xil_printf("Key input is invalid.\n");
				 }
				 //case when we consider input key strokes from '0' to '9' (only these are the valid key inputs for arithmetic operation)
				 else if((char)key != 'E' ){
					 store_key = key;
				 }
				 //if user presses 'E' key, consider the last input key pressed as the operand
				 else if((char)key == 'E'){
				 	if(store_key != 'x'){
						 xil_printf("Storing the operand %c to Queue...\n",(char) store_key);
						 key_stroke_on_SSD = SSD_decode((int)store_key-48, 0);
						 XGpio_DiscreteWrite(&SSDInst, 1, key_stroke_on_SSD);	//display the digit on right SSD stored as an operand.
						 /****************************************/
						 //Length of queue=2, hence we only store the key pressed value in queue, when 'E' will be pressed.
						 //here you write the Queue function to store the value of the last key pressed before 'E'
						 //hint: a variable is being used in this task that keeps the track of this key value (key presses before 'E')
						 /****************************************/
						 u32 value = ((int)(store_key-48));
						 xQueueSendToBack(xQueue, &value, 0);
					}
				 }
	      }
	      else if (status == KYPD_MULTI_KEY && status != last_status)
	    	  	  xil_printf("Error: Multiple keys pressed\r\n"); //this is valid whenever two or more keys are pressed together

	      last_status = status;
	      usleep(1000);
	   }
	}
}
/*-----------------------------------------------------------*/
static void prvRxTask( void *pvParameters )
{
	UBaseType_t uxPriority;
	uxPriority = uxTaskPriorityGet( NULL );

	for( ;; )
	{
		u32 store_operands[2];
		int result=0; 
		u32 btn;
		unsigned int btn_value;

		/***************************************/
		//write the code to read the queue values which store two operands for the calculation here.
		//You may use store_operands[] for doing that or your wish of variable can be used too.
		/***************************************/
		xQueueReceive(xQueue, &store_operands[0], pdMS_TO_TICKS(1500));
		xQueueReceive(xQueue, &store_operands[1], pdMS_TO_TICKS(1500));

		/***************************************/
		//read the btn value here to check what user has pressed (^/|/&/%) and store it in say "btn_value" variable declared in this task
		//For btn(3:0) --> "1000" is for %, "0100" is for &, "0010" is for | and "0001" is for ^
		btn = XGpio_DiscreteRead(&BTNInst, 1);
		xil_printf("Push Buttons Status %x\r\n", btn);
		/***************************************/
		while(1)
		{

			btn = XGpio_DiscreteRead(&BTNInst, 1);
			if (btn != 0) {
				xil_printf("Push Buttons Status %x\r\n", btn);
				if (btn == 1) {
					btn_value = 1;
					xil_printf("1\r\n");
					break;
				} else if (btn == 2) {
					btn_value = 2;
					xil_printf("2\r\n");
					break;
				} else if (btn == 4) {
					btn_value = 3;
					xil_printf("3\r\n");
					break;
				} else if (btn == 8) {
					btn_value = 4;
					xil_printf("4\r\n");
					break;
				} else {
					xil_printf("not 1 2 4 or 8: %x\r\n", btn);
				}
			}
		}

		//keep the button pressed for your choice of the arithmetic/logical operation
		switch(btn_value){
			case 1: result=store_operands[0]^store_operands[1]; break; //XOR
			/*****************************************************************************************/
			//add the remaining cases here
			//you may also use the default case to display nothing or some prompt message saying that no operation selected by user.
			//in the case when no operation is selected, exit this task and go back to the TxTask if you want.
			//you may also wait here until the user selects any operation, later on perform the calculation and then go to the TxTask.
			/*****************************************************************************************/
			case 2: result=store_operands[0]|store_operands[1]; break; //OR
			case 3: result=store_operands[0]&store_operands[1]; break; //AND
			case 4:
				if (store_operands[1] == 0) {
					result=-1;
					break;
				} else {
					result=store_operands[0]%store_operands[1];
					break; //MOD
				}
		default: result=-1; break;
		}

		xil_printf("Operation result = %d\n\n",result);

		//Once the result is computed, we wish to display it on SSD
		//the following logic is to extract the digits from the result. For example, 9x9=81 so we will first display 1 on right SSD and then 8 on the left SSD!
		//please note that our operands are between 0 to 9 only. The result will never exceed a two-digit number in any case.


		if(result<0)
			xil_printf("Result is less than zero!!!\n");


		vTaskDelay(pdMS_TO_TICKS(1500)); //this delay is merely to introduce the visual difference between the input operands and the output result!


		//Compute MSB and LSB digits for 2-digit output
		u8 lsb = result % 10;
		u8 msb = result / 10;

		u32 l_s_b = SSD_decode(lsb, 0);
		u32 m_s_b = SSD_decode(msb, 1);

		/**********************************************************************************/
		//Use a for loop to display result for 100 Cycles (15ms + 15ms = 30 ms/loop iteration = output displayed for 3 seconds)
		//for two digits, first a right segment is seen and then left segment is lit up.
		//introduce a delay between left and the right side of display on SSD such that both segments seem to lit up simultaneously!
		//If you found out the appropriate frequency value in the previous part1, that might help!
		/**********************************************************************************/

		for (int i=0; i < 100; i++) {
			XGpio_DiscreteWrite(&SSDInst, 1, l_s_b);
			vTaskDelay(pdMS_TO_TICKS(15));
			XGpio_DiscreteWrite(&SSDInst, 1, m_s_b);
			vTaskDelay(pdMS_TO_TICKS(15));
		}

		//clear both the segments after the result is displayed.
		XGpio_DiscreteWrite(&SSDInst, 1, 0b00000000);
		XGpio_DiscreteWrite(&SSDInst, 1, 0b10000000);

		//we are now done doing the calculation so again go back to the task 1 (TxTask) to get the new inputs!
		vTaskPrioritySet( xTxTask, ( uxPriority + 1 ) );

	}
}

