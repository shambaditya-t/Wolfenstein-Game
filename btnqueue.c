//***********************************************************************************

// Include files

//***********************************************************************************

#include "btnqueue.h"


uint8_t button0arr[BTN_QUEUE_SIZE];
uint8_t button1arr[BTN_QUEUE_SIZE];
//***********************************************************************************

// functions

//***********************************************************************************
/***************************************************************************//**

 * @brief

 *  Initialize button0 struct

 ******************************************************************************/

void button0_struct_init(BtnQueue* queue) {
  queue->elementID = 0;
//  queue->queue_size = BTN_QUEUE_SIZE;
  queue->array = button0arr;
  for(int i = 0; i < BTN_QUEUE_SIZE; i++) {
      queue->array[i] = 0;
  }
  return;
}
/***************************************************************************//**

 * @brief

 *  Initialize button1 struct

 ******************************************************************************/

void button1_struct_init(BtnQueue* queue) {
  queue->elementID = 0;
//  queue->queue_size = BTN_QUEUE_SIZE;
  queue->array = button1arr;
  for(int i = 0; i < BTN_QUEUE_SIZE; i++) {
      queue->array[i] = 0;
  }
  return;
}
/***************************************************************************//**

 * @brief

 *   Push to the queue, passed by reference, with the current button state.

 ******************************************************************************/
int8_t push(BtnQueue* queue, uint8_t btnState) {
  if(queue->elementID == BTN_QUEUE_SIZE) {
      return -1;
  }
  queue->array[queue->elementID] = btnState;
  queue->elementID++;
  return 1;
}
/***************************************************************************//**

 * @brief

 *   Pop from the queue, passed by reference. If successful, returns the popped element - aka the button state.

 ******************************************************************************/

uint8_t pop(BtnQueue* queue) {
  if(queue->elementID == 0) {
      return 0;
  }
  uint8_t tempbtn[BTN_QUEUE_SIZE];
  uint8_t popped_element = queue->array[0];
  queue->elementID--;

  for(int i = 0; i < BTN_QUEUE_SIZE - 1; i++) {
      tempbtn[i] = queue->array[i+1];
  }
  tempbtn[BTN_QUEUE_SIZE-1] = 0;

  for(int i =0; i < BTN_QUEUE_SIZE; i++) {
      queue->array[i] = tempbtn[i];
  }
  return popped_element;
}
