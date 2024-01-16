/*
 * btnqueue.h
 *
 *  Created on: Mar 13, 2023
 *      
 */

#ifndef BTNQUEUE_H_
#define BTNQUEUE_H_

#include <stdio.h>


#define BTN_QUEUE_SIZE                        10
typedef struct{
  uint8_t elementID;
  uint8_t *array;
}BtnQueue;


void button0_struct_init(BtnQueue* queue);
void button1_struct_init(BtnQueue* queue);
int8_t push(BtnQueue* queue, uint8_t btnState);
uint8_t pop(BtnQueue* queue);


#endif /* BTNQUEUE_H_ */
