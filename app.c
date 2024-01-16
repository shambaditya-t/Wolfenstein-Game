/***************************************************************************//**
 * @file
 * @brief Top level application functions
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of Silicon Labs Master Software License
 * Agreement (MSLA) available at
 * www.silabs.com/about-us/legal/master-software-license-agreement. This
 * software is distributed to you in Source Code format and is governed by the
 * sections of the MSLA applicable to Source Code.
 *
 ******************************************************************************/

/***************************************************************************//**
 * Initialize application.
 ******************************************************************************/
//***********************************************************************************
// Include files
//***********************************************************************************
#include "app.h"
//***********************************************************************************
// global variables
//***********************************************************************************
#define  APP_DEFAULT_TASK_STK_SIZE       4096u  /*   Stack size in CPU_STK.         */
#define  APP_DEFAULT_TASK_PRIORITY       22u
#define  APP_IDLE_TASK_PRIORITY         24u
#define  APP_MENU_TASK_PRIORITY       20u
#define  APP_PHYS_TASK_PRIORITY       19u
#define  tauSlider                      1u
#define  tauDisplay                     1u
#define Max_Safe_Speed                   25
#define discharge_cost                  50
#define max_shield_and_start            300
#define railgun_charge_rate             5
#define railgun_max_charge             50
#define game_destruction_max            118
#define game_destruction_evac           5
#define hits_to_destroy                 1

//Global structs only accessed upon pending mutex for related struct, then posting.
PlayerStatistics PlayerStats;
PlatformDirection PlatformDirectionInst;

GLIB_Rectangle_t Platform;
GLIB_Rectangle_t RightCanyon;
GLIB_Rectangle_t RailgunProjectile;
GLIB_Rectangle_t ShieldCharge;
GLIB_Rectangle_t RailgunCharge;
GLIB_Rectangle_t SatchelCharge;

GlibCliff WallObjects;
GlibCastle CastleObjects;
GameState Game;

//Global button queue, cap array states, and timer for updating time spent holding a direction
BtnQueue button0;
BtnQueue button1;
volatile bool cap_array[4] = {false,false,false,false};
volatile uint32_t currTimeTicks = 0;

//***********************************************************************************
// OS Stack variables
//***********************************************************************************
OS_TCB   App_PlayerActionTaskTCB;                            /*   Task Control Block.   */
CPU_STK  App_PlayerActionTaskStk[APP_DEFAULT_TASK_STK_SIZE]; /*   Stack.                */

OS_TCB App_GameTaskTCB;
CPU_STK  App_GameTaskStk[APP_DEFAULT_TASK_STK_SIZE]; /*   Stack.                */

OS_TCB   App_PlatformCtrlTaskTCB;                            /*   Task Control Block.   */
CPU_STK  App_PlatformCtrlTaskStk[APP_DEFAULT_TASK_STK_SIZE]; /*   Stack.                */

OS_TCB   App_PhysicsTaskTCB;                            /*   Task Control Block.   */
CPU_STK  App_PhysicsTaskStk[APP_DEFAULT_TASK_STK_SIZE]; /*   Stack.                */

OS_TCB   App_LEDoutputTaskTCB;                            /*   Task Control Block.   */
CPU_STK  App_LEDoutputTaskStk[APP_DEFAULT_TASK_STK_SIZE]; /*   Stack.                */

OS_TCB   App_LCDdisplayTaskTCB;                            /*   Task Control Block.   */
CPU_STK  App_LCDdisplayTaskStk[APP_DEFAULT_TASK_STK_SIZE]; /*   Stack.                */

/* Example Task Data:      */
OS_TCB   App_IdleTaskTCB;                            /*   Task Control Block.   */
CPU_STK  App_IdleTaskStk[APP_DEFAULT_TASK_STK_SIZE]; /*   Stack.                */

//***********************************************************************************
// Intertask communication variables - semaphores, event flags, mutex, timers, LCD Glib Context
//***********************************************************************************
static OS_SEM App_Game_Semaphore;
static OS_SEM App_PlayerAction_Semaphore;
static OS_SEM App_Platform_Semaphore;

static OS_FLAG_GRP App_Physics_Event_Flag_Group;
static OS_FLAG_GRP App_LEDoutput_Event_Flag_Group;

static OS_MUTEX App_PlatformAction_Mutex;
static OS_MUTEX App_PlayerAction_Mutex;

static OS_TMR App_Platform_Timer;

static GLIB_Context_t glibContext;
//***********************************************************************************
// Task synchronization creation functions (mutex,semaphore, event flag groups)
// IRQhandler, TMR callback creation as well.
//***********************************************************************************
/***************************************************************************//**
 * @brief
 *   Interrupt handler to service pressing of buttons - specifically button0
 ******************************************************************************/
void GPIO_EVEN_IRQHandler(void)
{
  GPIO_IntClear(1 << BUTTON0_pin); //clear interrupt for even pin
  push(&button0, update_button0());
  RTOS_ERR  err;
  OSSemPost(&App_PlayerAction_Semaphore,
            OS_OPT_POST_ALL,  /* No special option.                     */
            &err);
}
/***************************************************************************//**
 * @brief
 *   Interrupt handler to service pressing of buttons - specifically button1
 ******************************************************************************/
void GPIO_ODD_IRQHandler(void)
{
  GPIO_IntClear(1 << BUTTON1_pin); //clear interrupt for odd pin

  push(&button1, update_button1());
  RTOS_ERR  err;
  OSSemPost(&App_PlayerAction_Semaphore,
            OS_OPT_POST_ALL,  /* No special option.                     */
            &err);
}
/***************************************************************************//**
*   Updates button 0 state upon even IRQ
*******************************************************************************/
uint8_t update_button0(void){
  if(!GPIO_PinInGet(BUTTON0_port, BUTTON0_pin) && GPIO_PinInGet(BUTTON1_port, BUTTON1_pin)) {
      return (button0high);
  }
  else if(!GPIO_PinInGet(BUTTON0_port, BUTTON0_pin) && !GPIO_PinInGet(BUTTON1_port, BUTTON1_pin)) {
      return (button0low);
  }
  else {
      return (button0low);
  }
}
/***************************************************************************//**
*   Updates button 1 state upon odd IRQ
*******************************************************************************/
uint8_t update_button1(void){
  if(!GPIO_PinInGet(BUTTON1_port, BUTTON1_pin) && GPIO_PinInGet(BUTTON0_port, BUTTON0_pin)) {
      return (button1high);
  }
  else if(!GPIO_PinInGet(BUTTON1_port, BUTTON1_pin) && !GPIO_PinInGet(BUTTON0_port, BUTTON0_pin)) {
      return (button1low);
  }
  else {
      return (button1low);
  }
}
/***************************************************************************//**
*   PlayerAction data mutex creation
*******************************************************************************/
void App_PlayerAction_MutexCreation(void){
  RTOS_ERR     err;

  OSMutexCreate (&App_PlayerAction_Mutex,
                 "App_PlayerAction_Mutex ",
                 &err);
  if (err.Code != RTOS_ERR_NONE) {
      /* Handle error on task creation. */
      printf("Error handling PlayerAction mutex creation");
  }
}
/***************************************************************************//**
*   PlatformAction data mutex creation
*******************************************************************************/
void App_PlatformAction_MutexCreation(void){
  RTOS_ERR     err;

  OSMutexCreate (&App_PlatformAction_Mutex,
                 "App_PlatformAction_Mutex ",
                 &err);
  if (err.Code != RTOS_ERR_NONE) {
      /* Handle error on task creation. */
      printf("Error handling App_PlatformAction_Mutex creation");
  }
}
/***************************************************************************//**
*   Timer callback to communicate with Platform action task using a semaphore,
*    to indicate when state of capsense should be updated
*******************************************************************************/
void App_TimerCallback (void *p_tmr, void *p_arg)
{
  RTOS_ERR    err;
  (void)&p_arg;
  (void)&p_tmr;
  OSSemPost(&App_Platform_Semaphore,
            OS_OPT_POST_ALL,  /* No special option.                     */
            &err);
  //Logic here
  currTimeTicks = currTimeTicks + 1; //One fifth of a second has passed

  if (err.Code != RTOS_ERR_NONE) {
      /* Handle error on task semaphore post. */
      printf("Error handling timer callback with posting semaphore");
  }

}
/***************************************************************************//**
*   Timer Creation for timer to periodically update capsense states.
*******************************************************************************/
void  App_OS_TimerCreation (void)
{
    RTOS_ERR     err;
                                       /* Create a periodic timer.               */
    OSTmrCreate(&App_Platform_Timer,            /*   Pointer to user-allocated timer.     */
                "App Platform Timer",           /*   Name used for debugging.             */
                   0,                  /*     10 initial delay.                   */
                 tauSlider,                  /*   10 Timer Ticks period.              */
                 OS_OPT_TMR_PERIODIC,  /*   Timer is periodic.                   */
                &App_TimerCallback,    /*   Called when timer expires.           */
                 DEF_NULL,             /*   No arguments to callback.            */
                &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on timer create. */
      printf("Error while Handling OS Timer Creation");
    }
}
/***************************************************************************//**
*   Semaphore Creation for Game State Task
*******************************************************************************/
void  App_OS_GameState_SemaphoreCreation (void)
{
    RTOS_ERR     err;       /* Create the semaphore. */
    OSSemCreate(&App_Game_Semaphore,    /*   Pointer to user-allocated semaphore.          */
                "App_Game Semaphore",   /*   Name used for debugging.                      */
                 0,                /*   Initial count: available in this case.        */
                &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on semaphore create. */
      printf("Error while Handling Game state Semaphore creation");
    }
}
/***************************************************************************//**
*   Semaphore Creation for Platform ctrl Task
*******************************************************************************/
void  App_OS_PlatformCtrl_SemaphoreCreation (void)
{
    RTOS_ERR     err;       /* Create the semaphore. */
    OSSemCreate(&App_Platform_Semaphore,    /*   Pointer to user-allocated semaphore.          */
                "App_Platform Semaphore",   /*   Name used for debugging.                      */
                 0,                /*   Initial count: available in this case.        */
                &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on semaphore create. */
      printf("Error while Handling OS Semaphore creation");
    }
}
/***************************************************************************//**
*   Semaphore Creation for PlayerAction Task
*******************************************************************************/
void  App_OS_PlayerAction_SemaphoreCreation (void)
{
    RTOS_ERR     err;       /* Create the semaphore. */
    OSSemCreate(&App_PlayerAction_Semaphore,    /*   Pointer to user-allocated semaphore.          */
                "App_PlayerAction Semaphore",   /*   Name used for debugging.                      */
                 0,                /*   Initial count: available in this case.        */
                &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on semaphore create. */
      printf("Error while Handling OS Semaphore creation");
    }
}
/***************************************************************************//**
*   Event Flags Creation for Physics task.
*******************************************************************************/
void  App_OS_Physics_EventFlagGroupCreation (void)
{
    RTOS_ERR     err;                               /* Create the event flag. */
    OSFlagCreate(&App_Physics_Event_Flag_Group,              /*   Pointer to user-allocated event flag.         */
                 "App Physics Event Flag Group",             /*   Name used for debugging.                      */
                  0,                      /*   Initial flags, all cleared.                   */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on event flag create. */
        printf("Error while Handling OS Physics Event Flag Groups creation");
    }
}
/***************************************************************************//**
*   Event Flags Creation for LED output task, to indicate current vioations
*******************************************************************************/
void  App_OS_LEDoutput_EventFlagGroupCreation (void)
{
    RTOS_ERR     err;                               /* Create the event flag. */
    OSFlagCreate(&App_LEDoutput_Event_Flag_Group,              /*   Pointer to user-allocated event flag.         */
                 "App LEDoutput Event Flag Group",             /*   Name used for debugging.                      */
                  0,                      /*   Initial flags, all cleared.                   */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on event flag create. */
        printf("Error while Handling OS LEDoutput Event Flag Groups creation");
    }
}
//***********************************************************************************
// task creation functions
//***********************************************************************************
/***************************************************************************//**
 * @brief
 *  PlayerAction task creation
 *
 ******************************************************************************/
void  App_PlayerAction_Creation (void)
{
    RTOS_ERR     err;

    OSTaskCreate(&App_PlayerActionTaskTCB,                /* Pointer to the task's TCB.  */
                 "PlayerAction Task.",                    /* Name to help debugging.     */
                 &App_PlayerAction_Task,                   /* Pointer to the task's code. */
                  DEF_NULL,                          /* Pointer to task's argument. */
                  APP_DEFAULT_TASK_PRIORITY,             /* Task's priority.            */
                 &App_PlayerActionTaskStk[0],             /* Pointer to base of stack.   */
                 (APP_DEFAULT_TASK_STK_SIZE / 10u),  /* Stack limit, from base.     */
                  APP_DEFAULT_TASK_STK_SIZE,         /* Stack size, in CPU_STK.     */
                  10u,                               /* Messages in task queue.     */
                  0u,                                /* Round-Robin time quanta.    */
                  DEF_NULL,                          /* External TCB data.          */
                  OS_OPT_TASK_STK_CHK,               /* Task options.               */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on task creation. */
        printf("Error while Handling Speed Setpoint Task Creation");
    }
}
/***************************************************************************//**
 * @brief
 *  Platform ctrl creation
 *
 ******************************************************************************/
void  App_PlatformCtrl_creation (void)
{
    RTOS_ERR     err;

    OSTaskCreate(&App_PlatformCtrlTaskTCB,                /* Pointer to the task's TCB.  */
                 "PlatformCtrl Task.",                    /* Name to help debugging.     */
                 &App_PlatformCtrl_Task,                   /* Pointer to the task's code. */
                  DEF_NULL,                          /* Pointer to task's argument. */
                  APP_DEFAULT_TASK_PRIORITY,             /* Task's priority.            */
                 &App_PlatformCtrlTaskStk[0],             /* Pointer to base of stack.   */
                 (APP_DEFAULT_TASK_STK_SIZE / 10u),  /* Stack limit, from base.     */
                  APP_DEFAULT_TASK_STK_SIZE,         /* Stack size, in CPU_STK.     */
                  10u,                               /* Messages in task queue.     */
                  0u,                                /* Round-Robin time quanta.    */
                  DEF_NULL,                          /* External TCB data.          */
                  OS_OPT_TASK_STK_CHK,               /* Task options.               */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on task creation. */
        printf("Error while Handling PlatformCtrl Task Creation");
    }
}
/***************************************************************************//**
 * @brief
 *  Physics task creation
 *
 ******************************************************************************/
void  App_Physics_Creation (void)
{
    RTOS_ERR     err;

    OSTaskCreate(&App_PhysicsTaskTCB,                /* Pointer to the task's TCB.  */
                 "Physics Task.",                    /* Name to help debugging.     */
                 &App_Physics_Task,                   /* Pointer to the task's code. */
                  DEF_NULL,                          /* Pointer to task's argument. */
                  APP_PHYS_TASK_PRIORITY,             /* Task's priority.            */
                 &App_PhysicsTaskStk[0],             /* Pointer to base of stack.   */
                 (APP_DEFAULT_TASK_STK_SIZE / 10u),  /* Stack limit, from base.     */
                  APP_DEFAULT_TASK_STK_SIZE,         /* Stack size, in CPU_STK.     */
                  10u,                               /* Messages in task queue.     */
                  0u,                                /* Round-Robin time quanta.    */
                  DEF_NULL,                          /* External TCB data.          */
                  OS_OPT_TASK_STK_CHK,               /* Task options.               */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on task creation. */
        printf("Error while Handling Phys Task Creation");
    }
}
/***************************************************************************//**
 * @brief
 *  LED Output task creation
 *
 ******************************************************************************/
void  App_Game_Creation (void)
{
    RTOS_ERR     err;

    OSTaskCreate(&App_GameTaskTCB,                /* Pointer to the task's TCB.  */
                 "Game Task.",                    /* Name to help debugging.     */
                 &App_GameTask,                   /* Pointer to the task's code. */
                  DEF_NULL,                          /* Pointer to task's argument. */
                  APP_MENU_TASK_PRIORITY,             /* Task's priority.            */
                 &App_GameTaskStk[0],             /* Pointer to base of stack.   */
                 (APP_DEFAULT_TASK_STK_SIZE / 10u),  /* Stack limit, from base.     */
                  APP_DEFAULT_TASK_STK_SIZE,         /* Stack size, in CPU_STK.     */
                  10u,                               /* Messages in task queue.     */
                  0u,                                /* Round-Robin time quanta.    */
                  DEF_NULL,                          /* External TCB data.          */
                  OS_OPT_TASK_STK_CHK,               /* Task options.               */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on task creation. */
        printf("Error while Handling Game Task Creation");
    }
}

/***************************************************************************//**
 * @brief
 *  LED Output task creation
 *
 ******************************************************************************/
void  App_LEDoutput_Creation (void)
{
    RTOS_ERR     err;

    OSTaskCreate(&App_LEDoutputTaskTCB,                /* Pointer to the task's TCB.  */
                 "LEDoutput Task.",                    /* Name to help debugging.     */
                 &App_LEDoutput_Task,                   /* Pointer to the task's code. */
                  DEF_NULL,                          /* Pointer to task's argument. */
                  APP_DEFAULT_TASK_PRIORITY,             /* Task's priority.            */
                 &App_LEDoutputTaskStk[0],             /* Pointer to base of stack.   */
                 (APP_DEFAULT_TASK_STK_SIZE / 10u),  /* Stack limit, from base.     */
                  APP_DEFAULT_TASK_STK_SIZE,         /* Stack size, in CPU_STK.     */
                  10u,                               /* Messages in task queue.     */
                  0u,                                /* Round-Robin time quanta.    */
                  DEF_NULL,                          /* External TCB data.          */
                  OS_OPT_TASK_STK_CHK,               /* Task options.               */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on task creation. */
        printf("Error while Handling LEDoutput Task Creation");
    }
}
/***************************************************************************//**
 * @brief
 * LCD display task creation
 *
 ******************************************************************************/
void  App_LCDdisplay_Creation (void)
{
    RTOS_ERR     err;

    OSTaskCreate(&App_LCDdisplayTaskTCB,                /* Pointer to the task's TCB.  */
                 "LCDdisplay Task.",                    /* Name to help debugging.     */
                 &App_LCDdisplay_Task,                   /* Pointer to the task's code. */
                  DEF_NULL,                          /* Pointer to task's argument. */
                  APP_DEFAULT_TASK_PRIORITY,             /* Task's priority.            */
                 &App_LCDdisplayTaskStk[0],             /* Pointer to base of stack.   */
                 (APP_DEFAULT_TASK_STK_SIZE / 10u),  /* Stack limit, from base.     */
                  APP_DEFAULT_TASK_STK_SIZE,         /* Stack size, in CPU_STK.     */
                  10u,                               /* Messages in task queue.     */
                  0u,                                /* Round-Robin time quanta.    */
                  DEF_NULL,                          /* External TCB data.          */
                  OS_OPT_TASK_STK_CHK,               /* Task options.               */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on task creation. */
        printf("Error while Handling LCD display Task Creation");
    }
}
//***********************************************************************************
// task creation functions
//***********************************************************************************
/***************************************************************************//**
*   Requests update from the global button queue with exclusive access
*   Pends mutex to gain access to PlayerStats struct and update speed information
*   and total increments, decrements of speed. Posts mutex upon finishing.
*******************************************************************************/
void  App_PlayerAction_Task(void  *p_arg){
 (void)&p_arg;
 RTOS_ERR  err;
 PlayerStats.currSpeed = 0;
 PlayerStats.totalDecrement = 0;
 PlayerStats.totalIncrement = 0;
 volatile uint8_t prevRailgun = button0high;

 while (DEF_TRUE) {
     OSSemPend(&App_PlayerAction_Semaphore,
                0,
                OS_OPT_PEND_BLOCKING,
                NULL,
                &err);
     //Ensure exclusive access during push and pop operation
     CORE_DECLARE_IRQ_STATE;
     CORE_ENTER_ATOMIC();
     uint8_t rail_gun = pop(&button0);
     uint8_t shield = pop(&button1);
     CORE_EXIT_ATOMIC();

           /* Acquire resource protected by mutex.       */
      OSMutexPend(&App_PlayerAction_Mutex,             /*   Pointer to user-allocated mutex.         */
      0,                  /*   Wait for a maximum of 0 OS Ticks.     */
      OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
      DEF_NULL,              /*   Timestamp is not used.                   */
      &err);
//     if(rail_gun == button0high && prevRailgun == button0low) {
      if(rail_gun == button0high) {
         PlayerStats.railgun_charge = 0;
         PlayerStats.railgun_charging = true;
         PlayerStats.railgun_fire = false;
     }
//     else if(rail_gun == button0high && prevRailgun == button0high) {
//         PlayerStats.railgun_charging = true;
//         PlayerStats.railgun_fire = false;
//     }
     else if(rail_gun == button0low && prevRailgun == button0high) {
         PlayerStats.railgun_fire = true;
         PlayerStats.railgun_charging = false;
     }
     else {
         PlayerStats.railgun_fire = false;
         PlayerStats.railgun_charging = false;
     }
      prevRailgun = rail_gun;


     if(shield == button1high) {
         PlayerStats.shield_active = true;
     }
     else {
         PlayerStats.shield_active = false;
     }

     OSFlagPost(&App_Physics_Event_Flag_Group,
                 button_action,
                 OS_OPT_POST_FLAG_SET,
                 &err);

           /* Release resource protected by mutex.       */
      OSMutexPost(&App_PlayerAction_Mutex,         /*   Pointer to user-allocated mutex.         */
      OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
      &err);

     if (err.Code != RTOS_ERR_NONE) {
         printf("Error while Handling PlayerAction task");
     }
   }
 }
/***************************************************************************//**
*   Requests update to capacitive touch sensor periodically, and determines platform control movement.
*  Pends mutex to gain access to update PlatformDirectionInst then posts mutex upon finishing.
*******************************************************************************/
void  App_PlatformCtrl_Task(void  *p_arg){
 (void)&p_arg;
 RTOS_ERR  err;
 OSTmrStart (&App_Platform_Timer,
             &err);

 while (DEF_TRUE) {
     OSSemPend(&App_Platform_Semaphore,
                0,
                OS_OPT_PEND_BLOCKING,
                NULL,
                &err);

          /* Acquire resource protected by mutex.       */
     OSMutexPend(&App_PlatformAction_Mutex,             /*   Pointer to user-allocated mutex.         */
     0,                  /*   Wait for a maximum of 0 OS Ticks.     */
     OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
     DEF_NULL,              /*   Timestamp is not used.                   */
     &err);

     //Logic to Update capsense
    update_capsense();

      //Logic to update Direction of platform during mutex
     if(cap_array[0]) {
         PlatformDirectionInst.currDirection = hardLeft;
     }
     else if(cap_array[1]) {
         PlatformDirectionInst.currDirection = gradualLeft;
     }
     else if(cap_array[2]) {
         PlatformDirectionInst.currDirection = gradualRight;
     }
     else if(cap_array[3]) {
         PlatformDirectionInst.currDirection = hardRight;
     }
     else {
         PlatformDirectionInst.currDirection = none;
     }

     OSFlagPost(&App_Physics_Event_Flag_Group,
                platform_action,
                 OS_OPT_POST_FLAG_SET,
                 &err);

           /* Release resource protected by mutex.       */
      OSMutexPost(&App_PlatformAction_Mutex,         /*   Pointer to user-allocated mutex.         */
      OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
      &err);
     if (err.Code != RTOS_ERR_NONE) {
         printf("Error while Handling App_PlatformCtrl_Task task");
     }
   }
 }
/***************************************************************************//**
 * @brief
 *  Reads Capacitive sensor input and updates global bool array for each section.
 *  Note: pressing two sections on the same side will register as pressing the inner section (Gradual)
 ******************************************************************************/
void update_capsense(void){
    CAPSENSE_Sense();
    for(uint8_t i = 0; i < 4; i++) {
        cap_array[i] = CAPSENSE_getPressed(i);
    }
    //being touched on opposite sides
      if((cap_array[0] && cap_array[2]) || (cap_array[0] && cap_array[3])) {
          for(uint8_t i = 0; i < 4; i++) {//being touched on opposite sides
              cap_array[i] = false;
          }
      }
      //being touched on opposite sides
      else if((cap_array[1] && cap_array[2]) || (cap_array[1] && cap_array[3])) {
          for(uint8_t i = 0; i < 4; i++) {
              cap_array[i] = false;
          }
      }
      else if(cap_array[0] && cap_array[1]) {
          for(uint8_t i = 0; i < 4; i++) {
              cap_array[i] = false;
          }
          cap_array[1] = true;
      }
      else if(cap_array[2] && cap_array[3]) {
          for(uint8_t i = 0; i < 4; i++) {
              cap_array[i] = false;
          }
          cap_array[2] = true;
      }
    return;
}


/***************************************************************************//**
* Pends upon Physics event flag group when either a capsense update or button update occurs
* Handles mutexes for each struct to update data, and then checks for game conditions and LED conditions.
* Sends flag to LED output event group to update visual indicators, such as for evac.
*******************************************************************************/
void  App_Physics_Task(void  *p_arg){
 (void)&p_arg;
 RTOS_ERR  err;
 OS_FLAGS flag;

 int8_t currentAccel = 0;
 int8_t num;
 uint8_t temp_railgun_charge = 0;
 uint8_t t_hz = 0;
 uint8_t t_hz_charge = 0;

 bool speedSet = false;
 bool satchelSet = false;

// bool pwm_on = false;
// bool pwm0_on = false;

 while (DEF_TRUE) {
     OSFlagPend(&App_Physics_Event_Flag_Group,                /*   Pointer to user-allocated event flag. */
               (button_action | platform_action),             /*   Flag bitmask to be matched.                */
               0,                      /*   Wait for 0 OS Ticks maximum.        */
               OS_OPT_PEND_FLAG_SET_ANY |/*   Wait until all flags are set and      */
               OS_OPT_PEND_BLOCKING     |/*    task will block and                  */
               OS_OPT_PEND_FLAG_CONSUME, /*    function will clear the flags.       */
               DEF_NULL,                 /*   Timestamp is not used.                */
              &err);
     flag = OSFlagPendGetFlagsRdy(&err);
           /* Acquire resource protected by mutex.       */
     OSMutexPend(&App_PlayerAction_Mutex,             /*   Pointer to user-allocated mutex.         */
     0,                  /*   Wait for a maximum of 1000 OS Ticks.     */
     OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
     DEF_NULL,              /*   Timestamp is not used.                   */
     &err);
          /* Acquire resource protected by mutex.       */
     OSMutexPend(&App_PlatformAction_Mutex,             /*   Pointer to user-allocated mutex.         */
     0,                  /*   Wait for a maximum of 1000 OS Ticks.     */
     OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
     DEF_NULL,              /*   Timestamp is not used.                   */
     &err);

     if(flag == button_action || platform_action) {
         PlatformDirectionInst.currTime = (currTimeTicks/5); //1 tick = 1/5th of a secondS
        //Logic
         if(Game.destructionAmount >= game_destruction_evac) {//(game_destruction_max)/2) {
             Game.game_status = evacuation;
             if((t_hz < 5)) { //1 second pwm
                OSFlagPost(&App_LEDoutput_Event_Flag_Group,
                           evac_led_on,
                            OS_OPT_POST_FLAG_SET,
                            &err);
//                pwm_on = true;
                t_hz++;
             }
             else {
                 OSFlagPost(&App_LEDoutput_Event_Flag_Group,
                            evac_led_off,
                             OS_OPT_POST_FLAG_SET,
                             &err);
//                 pwm_on = false;
                 t_hz++;
                 if(t_hz >= 10) {
                     t_hz = 0;
                 }
             }
         }
         else {
            OSFlagPost(&App_LEDoutput_Event_Flag_Group,
                       evac_led_off,
                        OS_OPT_POST_FLAG_SET,
                        &err);
         }


         if(PlayerStats.railgun_charge > 0) {
             if(t_hz_charge < (railgun_max_charge/PlayerStats.railgun_charge)) {
                OSFlagPost(&App_LEDoutput_Event_Flag_Group,
                           railgun_led_on,
                            OS_OPT_POST_FLAG_SET,
                            &err);
//                pwm0_on = true;
                t_hz_charge++;
             }
             else {
                 OSFlagPost(&App_LEDoutput_Event_Flag_Group,
                            railgun_led_off,
                             OS_OPT_POST_FLAG_SET,
                             &err);
//                 pwm0_on = false;
                 t_hz_charge++;
                 if(t_hz_charge >= (railgun_max_charge/PlayerStats.railgun_charge)) {
                     t_hz_charge = 0;
                 }
             }
         }
         else {
            OSFlagPost(&App_LEDoutput_Event_Flag_Group,
                       railgun_led_off,
                        OS_OPT_POST_FLAG_SET,
                        &err);
         }

     //Update platform accel, current velocity.
     if(PlatformDirectionInst.currDirection == hardLeft) {
         currentAccel = -2;
     }
     else if(PlatformDirectionInst.currDirection == gradualLeft) {
         currentAccel = -1;
     }
     else if(PlatformDirectionInst.currDirection == hardRight) {
         currentAccel = 2;
     }
     else if(PlatformDirectionInst.currDirection == gradualRight) {
         currentAccel = 1;
     }
     else {
         currentAccel = 0;
     }
     PlatformDirectionInst.velocity += currentAccel;
     Platform.xMin += PlatformDirectionInst.velocity;
     Platform.xMax += PlatformDirectionInst.velocity;


//     //Right wall bounce
//     if(Platform.xMax >= RightCanyon.xMin) {
//         if(PlatformDirectionInst.velocity > Max_Safe_Speed) {
//             //Destroy platform
////             Game.game_status = platform_crash;
//             Game.game_status = platform_crash;
////             break;
//         }
//         else {
//             PlatformDirectionInst.velocity = -PlatformDirectionInst.velocity;
////             break;
//             //Bounce harmlessly off right wall.
//         }
//     }
//
//     //Left wall bounce (only need to check one thickness, but check the four above it.
//     for(int i = 13; i < 16; i++) {
//         if((Platform.xMin <= WallObjects.P_Rectangle_T[0][i].xMax) && (PlayerStats.hit_wall[0][i] == false)) { //Hit the wall and it still exists
//             if(-PlatformDirectionInst.velocity > Max_Safe_Speed) { //Flip sign since you are travelling left
//                 //Destroy platform
//                 Game.game_status = platform_crash;
//                 break;
//             }
//             else {
//                 PlatformDirectionInst.velocity = -PlatformDirectionInst.velocity;
//                 break;
//                 //Bounce harmlessly off left wall.
//             }
//         }
//     }


     //Update railgun charge, fire status, etc
     if(PlayerStats.railgun_charging == true && PlayerStats.railgun_charge < railgun_max_charge) {
         PlayerStats.railgun_charge += railgun_charge_rate;
     }
     if(PlayerStats.railgun_fire == true) {
         PlayerStats.railgun_fire = false;
         PlayerStats.proj_active = true;
         temp_railgun_charge = PlayerStats.railgun_charge;
         PlayerStats.railgun_charge = 0;
         speedSet = false;

     }
     //Update projectile status and position
     if(PlayerStats.proj_active == true) {
         if(speedSet == false) {
           PlayerStats.proj_velocity_x = (temp_railgun_charge/4);
           PlayerStats.proj_velocity_y = (temp_railgun_charge/2);
           RailgunProjectile.xMin = (Platform.xMin + 18) - 4;
           RailgunProjectile.xMax = (Platform.xMax) - 4;
           RailgunProjectile.yMin = (Platform.yMin + 2) - 11;
           RailgunProjectile.yMax = (Platform.yMax) - 11;
           speedSet = true;
         }
         else {
             PlayerStats.proj_velocity_y -= 1; //Gravity
             RailgunProjectile.xMin -= PlayerStats.proj_velocity_x;
             RailgunProjectile.xMax -= PlayerStats.proj_velocity_x;
             RailgunProjectile.yMin -= PlayerStats.proj_velocity_y;
             RailgunProjectile.yMax -= PlayerStats.proj_velocity_y;
         }
     }
     else {
         RailgunProjectile.xMin = 150;
         RailgunProjectile.xMax = 150;
         RailgunProjectile.yMin = 150;
         RailgunProjectile.yMax = 150;
         speedSet = false;
     }
      RailgunCharge.yMin = 130 -(PlayerStats.railgun_charge);
      RailgunCharge.yMax = 130;

     //Check Projectile collision between Railgun and Walls
      if(PlayerStats.proj_active == true) {
        for(int i = 8; i > 0; i--) {
            //Third cliff set
            if( (RailgunProjectile.xMin) <= (WallObjects.P_Rectangle_T[2][i].xMax) && PlayerStats.hit_wall[2][i] < hits_to_destroy ) {
                    if((RailgunProjectile.yMax >= WallObjects.P_Rectangle_T[2][i].yMin) && (RailgunProjectile.yMin <= WallObjects.P_Rectangle_T[2][i].yMin) ) {
                        PlayerStats.hit_wall[2][i] += 1;
                        PlayerStats.proj_active = false;
                        if(PlayerStats.hit_wall[2][i] >= hits_to_destroy) {
                            Game.destructionAmount += 1;
                        }
                        break;
                    }
                    else if((RailgunProjectile.yMin <= WallObjects.P_Rectangle_T[2][i].yMax) && (RailgunProjectile.yMax >= WallObjects.P_Rectangle_T[2][i].yMax) ) {
                        PlayerStats.hit_wall[2][i] += 1;
                        PlayerStats.proj_active = false;
                        if(PlayerStats.hit_wall[2][i] >= hits_to_destroy) {
                            Game.destructionAmount += 1;
                        }
                        break;
                    }
                    else {
                        //PlayerStats.hit_wall[2][i] = false;
                        }
            }
        }
      }
      if(PlayerStats.proj_active == true) {
        for(int i = 12; i > 0; i--) {
            //Third cliff set
            if( (RailgunProjectile.xMin) <= (WallObjects.P_Rectangle_T[1][i].xMax) && PlayerStats.hit_wall[1][i] < hits_to_destroy ) {
                    if((RailgunProjectile.yMax >= WallObjects.P_Rectangle_T[1][i].yMin) && (RailgunProjectile.yMin <= WallObjects.P_Rectangle_T[1][i].yMin) ) {
                        PlayerStats.hit_wall[1][i] += 1;
                        PlayerStats.proj_active = false;
                        if(PlayerStats.hit_wall[1][i] >= hits_to_destroy) {
                            Game.destructionAmount += 1;
                        }
                        break;
                    }
                    else if((RailgunProjectile.yMin <= WallObjects.P_Rectangle_T[1][i].yMax) && (RailgunProjectile.yMax >= WallObjects.P_Rectangle_T[1][i].yMax) ) {
                        PlayerStats.hit_wall[1][i] += 1;
                        PlayerStats.proj_active = false;
                        if(PlayerStats.hit_wall[1][i] >= hits_to_destroy) {
                            Game.destructionAmount += 1;
                        }
                        break;
                    }
                    else {
                        //PlayerStats.hit_wall[2][i] = false;
                        }
            }
        }
      }
      if(PlayerStats.proj_active == true) {
        for(int i = 16; i > 0; i--) {
            //Third cliff set
            if( (RailgunProjectile.xMin) <= (WallObjects.P_Rectangle_T[0][i].xMax) && PlayerStats.hit_wall[0][i] < hits_to_destroy ) {
                    if((RailgunProjectile.yMax >= WallObjects.P_Rectangle_T[0][i].yMin) && (RailgunProjectile.yMin <= WallObjects.P_Rectangle_T[0][i].yMin) ) {
                        PlayerStats.hit_wall[0][i] += 1;
                        PlayerStats.proj_active = false;
                        if(PlayerStats.hit_wall[0][i] >= hits_to_destroy) {
                            Game.destructionAmount += 1;
                        }
                        break;
                    }
                    else if((RailgunProjectile.yMin <= WallObjects.P_Rectangle_T[0][i].yMax) && (RailgunProjectile.yMax >= WallObjects.P_Rectangle_T[0][i].yMax) ) {
                        PlayerStats.hit_wall[0][i] += 1;
                        PlayerStats.proj_active = false;
                        if(PlayerStats.hit_wall[0][i] >= hits_to_destroy) {
                            Game.destructionAmount += 1;
                        }
                        break;
                    }
                    else {
                        //PlayerStats.hit_wall[2][i] = false;
                        }
            }
        }
      }
     if(PlayerStats.proj_active == true) {
       //Inner walls of castle
       for(int i = 0; i < 4; i++) {
           for(int j = 0; j < 7; j++) {
               if((j == 1 && i != 3) || (j == 3 && i != 3) || (j == 5 && i != 3)) {
                   continue; //Make windows not rectangles!
               }
               if( (RailgunProjectile.xMin) <= (CastleObjects.P_Rectangle_T[i][j].xMax) && PlayerStats.hit_castle[i][j] < hits_to_destroy) {
                       if((RailgunProjectile.yMax >= CastleObjects.P_Rectangle_T[i][j].yMin) && (RailgunProjectile.yMin <= CastleObjects.P_Rectangle_T[i][j].yMin)) {
                           PlayerStats.hit_castle[i][j] += 1;
                           PlayerStats.proj_active = false;
                           if(PlayerStats.hit_castle[i][j] >= hits_to_destroy) {
                               Game.destructionAmount += 2;
                           }
                           break;
                       }
                       else if((RailgunProjectile.yMin <= CastleObjects.P_Rectangle_T[i][j].yMax) && (RailgunProjectile.yMax >= CastleObjects.P_Rectangle_T[i][j].yMax)) {
                           PlayerStats.hit_castle[i][j] += 1;
                           PlayerStats.proj_active = false;
                           if(PlayerStats.hit_castle[i][j] >= hits_to_destroy) {
                               Game.destructionAmount += 2;
                           }
                           break;
                       }
                       else {
                           //PlayerStats.hit_castle[i][j] = false;
                       }
               }
           }
       }
     }

     //Update shield charge, discharge values, and indicate whether protection is active.
     if(PlayerStats.shield_active == true && (PlayerStats.shield_remaining >= discharge_cost/5)) {
         PlayerStats.shield_remaining -= discharge_cost/5;
         PlayerStats.shield_protection = true;
     }
     else {
         if(PlayerStats.shield_active == true) {
             PlayerStats.shield_protection = false;
         }
         else {
             if(PlayerStats.shield_remaining <= (max_shield_and_start -  discharge_cost/20) )
             PlayerStats.shield_remaining += discharge_cost/20;
             PlayerStats.shield_protection = false;
         }
     }
     ShieldCharge.yMin = 130 -(PlayerStats.shield_remaining/10);
     ShieldCharge.yMax = 130;

     //Update projectile status and position
     PlayerStats.satchel_active = true;
     if(PlayerStats.satchel_active == true) {
         if(satchelSet == true) {
           PlayerStats.satchel_velocity_y -= 1; //Gravity
           SatchelCharge.xMin -= PlayerStats.satchel_velocity_x;
           SatchelCharge.xMax -= PlayerStats.satchel_velocity_x;
           SatchelCharge.yMin -= PlayerStats.satchel_velocity_y;
           SatchelCharge.yMax -= PlayerStats.satchel_velocity_y;
         }
         else {
             for (int i = 0; i < 1; i++) {
                 num = (rand() % (8 - (-8) + 1)) + (-8);
             }
             PlayerStats.satchel_velocity_x = -num;
             PlayerStats.satchel_velocity_y = 0;
             SatchelCharge.xMin = CastleObjects.P_Rectangle_T[0][6].xMin;
             SatchelCharge.xMax = CastleObjects.P_Rectangle_T[0][6].xMax;
             SatchelCharge.yMin = CastleObjects.P_Rectangle_T[0][6].yMin + 26;
             SatchelCharge.yMax = CastleObjects.P_Rectangle_T[0][6].yMax + 26;
             satchelSet = true;
         }
     }
     else {
         satchelSet = false;
     }

     //Right wall bounce
     if(Platform.xMax >= RightCanyon.xMin) {
         if(PlatformDirectionInst.velocity > Max_Safe_Speed) {
             //Destroy platform
             Game.game_status = platform_crash;
         }
         else {
             PlatformDirectionInst.velocity = -PlatformDirectionInst.velocity;
             //Bounce harmlessly off right wall.
         }
     }

     //Left wall bounce (only need to check one thickness, but check the four above it.
     for(int i = 13; i < 16; i++) {
         if((Platform.xMin <= WallObjects.P_Rectangle_T[0][i].xMax) && (PlayerStats.hit_wall[0][i] < hits_to_destroy)) { //Hit the wall and it still exists
             if(-PlatformDirectionInst.velocity > Max_Safe_Speed) { //Flip sign since you are travelling left
                 //Destroy platform
                 Game.game_status = platform_crash;
                 break;
             }
             else {
                 PlatformDirectionInst.velocity = -PlatformDirectionInst.velocity;
                 break;
                 //Bounce harmlessly off left wall.
             }
         }
     }

     //Check for satchel collision and going too far below.
     if(SatchelCharge.yMax > 130) {
         satchelSet = false;
     }
     else if(SatchelCharge.xMin > 130) {
         satchelSet = false;
     }
     //Right wall bounce
     if(SatchelCharge.xMax >= RightCanyon.xMin) {
         PlayerStats.satchel_velocity_x = -PlayerStats.satchel_velocity_x;
     }

     //Left wall bounce (only need to check one thickness, but check the four above it.
     for(int i = 14; i > 0; i--) {
         for(int j = 2; j > 0; j--)
           if((SatchelCharge.xMin <= WallObjects.P_Rectangle_T[j][i].xMax) && (PlayerStats.hit_wall[j][i] < hits_to_destroy)) { //Hit the wall and it still exists
                   PlayerStats.satchel_velocity_x = -PlayerStats.satchel_velocity_x;
                   break;
                   //Bounce harmlessly off left wall.
               }
         }

     //Check Satchel and Platform Collision
     if(PlayerStats.shield_protection == true) {
         if((SatchelCharge.xMin <= Platform.xMin+15) && (SatchelCharge.xMax >= Platform.xMin+15) && (SatchelCharge.yMax >= (Platform.yMin - 35))) {
             satchelSet = false;
         }
         else if((SatchelCharge.xMin >= Platform.xMin+15) && (SatchelCharge.xMin <= Platform.xMax+15) && (SatchelCharge.yMax >= (Platform.yMin - 35))) {
             satchelSet = false;
         }
     }
     else {
         if((SatchelCharge.xMin <= Platform.xMin) && (SatchelCharge.xMax >= Platform.xMin) && (SatchelCharge.yMax >= Platform.yMin)) {
             Game.game_status = satchel_explosion;
         }
         else if((SatchelCharge.xMin >= Platform.xMin) && (SatchelCharge.xMin <= Platform.xMax) && (SatchelCharge.yMax >= Platform.yMin)) {
             Game.game_status = satchel_explosion;
         }
     }

     } //end if(flags)

         /* Release resource protected by mutex.       */
     OSMutexPost(&App_PlayerAction_Mutex,         /*   Pointer to user-allocated mutex.         */
     OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
     &err);
          /* Release resource protected by mutex.       */
     OSMutexPost(&App_PlatformAction_Mutex,         /*   Pointer to user-allocated mutex.         */
     OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
     &err);


     if (err.Code != RTOS_ERR_NONE) {
         printf("Error while Handling App_Physics_Task task");
     }
   }
 }
/***************************************************************************//**
*  Outputs to LED0 and LED1 based on conditions of evacuation or railgun charging force
*******************************************************************************/
void  App_LEDoutput_Task(void  *p_arg){
 (void)&p_arg;
 RTOS_ERR  err;
 OS_FLAGS flag;

 while (DEF_TRUE) {
     OSFlagPend(&App_LEDoutput_Event_Flag_Group,                /*   Pointer to user-allocated event flag. */
               (evac_led_on | evac_led_off | railgun_led_on | railgun_led_off),             /*   Flag bitmask to be matched.                */
               0,                      /*   Wait for 0 OS Ticks maximum.        */
               OS_OPT_PEND_FLAG_SET_ANY |/*   Wait until all flags are set and      */
               OS_OPT_PEND_BLOCKING     |/*    task will block and                  */
               OS_OPT_PEND_FLAG_CONSUME, /*    function will clear the flags.       */
               DEF_NULL,                 /*   Timestamp is not used.                */
              &err);
     flag = OSFlagPendGetFlagsRdy(&err);
     if(flag == evac_led_on) {
         GPIO_PinOutSet(LED0_port, LED0_pin);
         }
     else if(flag == evac_led_off ) {
         GPIO_PinOutClear(LED0_port, LED0_pin);
     }
     if(flag == railgun_led_on) {
         GPIO_PinOutSet(LED1_port, LED1_pin);
     }
     else if(flag == railgun_led_off) {
         GPIO_PinOutClear(LED1_port, LED1_pin);
     }

     if (err.Code != RTOS_ERR_NONE) {
         printf("Error while Handling App_LEDoutput_Task task");
     }
   }
 }
/***************************************************************************//**
*  Updates LCD display with Wolfenstein graphics
*******************************************************************************/
void  App_LCDdisplay_Task(void  *p_arg){
 (void)&p_arg;
 RTOS_ERR  err;


 uint8_t dispTime;
// char *dispDir = "none"; //none initially
 char str[100];
 ShieldCharge.xMax = 95;
 ShieldCharge.xMin = 100;
 RailgunCharge.xMax = 105;
 RailgunCharge.xMin = 110;
 uint32_t status;
 uint8_t evac_time = 0;
 uint32_t MAX_STR_LEN = 100;
 bool set_time = false;
 Game.game_status = active_game;

 while (DEF_TRUE) {
     /* Acquire resource protected by mutex.       */
      OSMutexPend(&App_PlayerAction_Mutex,             /*   Pointer to user-allocated mutex.         */
      0,                  /*   Wait for a maximum of 1000 OS Ticks.     */
      OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
      DEF_NULL,              /*   Timestamp is not used.                   */
      &err);
           /* Acquire resource protected by mutex.       */
      OSMutexPend(&App_PlatformAction_Mutex,             /*   Pointer to user-allocated mutex.         */
      0,                  /*   Wait for a maximum of 1000 OS Ticks.     */
      OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
      DEF_NULL,              /*   Timestamp is not used.                   */
      &err);

     /* Initialize the glib context */
     status = GLIB_contextInit(&glibContext);
     EFM_ASSERT(status == GLIB_OK);
     glibContext.backgroundColor = White;
     glibContext.foregroundColor = Black;
     /* Fill lcd with background color */
     GLIB_clear(&glibContext);

     // --------------------------- START DISPLAY ---------------------------
     //Draw static canyon
     GLIB_drawRectFilled(&glibContext, &RightCanyon);


     //dispDir = PlatformDirectionInst.currDirection;
     dispTime = PlatformDirectionInst.currTime;


     //Update Castle cliff wall
     for(int i = 0; i < 16; i++) {
         for(int j = 0; j < 3; j++) {
             if(PlayerStats.hit_wall[j][i] >= hits_to_destroy) {
             }
             else {
                 GLIB_drawRectFilled(&glibContext, &WallObjects.P_Rectangle_T[j][i]);
             }
         }
     }
     //Update castle
     for(int i = 0; i < 4; i++) {
         for(int j = 0; j < 7; j++) {
             if(PlayerStats.hit_castle[i][j] >= hits_to_destroy) {
             }
             else {
                 GLIB_drawRectFilled(&glibContext, &CastleObjects.P_Rectangle_T[i][j]);
             }
         }
     }
     //Draw updated platform
     GLIB_drawRectFilled(&glibContext, &Platform);

     //Draw updated gun, gun projectile
     snprintf(str, MAX_STR_LEN, "\\");
     GLIB_drawStringOnLine(&glibContext, str, 11, GLIB_ALIGN_LEFT,(Platform.xMax + Platform.xMin)/2,7,true);
     GLIB_drawRectFilled(&glibContext, &RailgunProjectile);

     //Draw shield bar
     GLIB_drawRect(&glibContext, &ShieldCharge);


     //Draw current railgun charging and indicate if railgun has been fired by filling rect in.
     if(PlayerStats.railgun_fire == true) {
         GLIB_drawRectFilled(&glibContext, &RailgunCharge);
     }
     else {
         if(PlayerStats.railgun_charge == railgun_max_charge) {
             GLIB_drawRectFilled(&glibContext, &RailgunCharge);
         }
         else {
             GLIB_drawRect(&glibContext, &RailgunCharge);
         }
     }

     //Draw Shield (if active)
     if(PlayerStats.shield_protection == true) {
         snprintf(str, MAX_STR_LEN, "(    )\n");
         GLIB_drawStringOnLine(&glibContext, str, 10, GLIB_ALIGN_LEFT,(Platform.xMax + Platform.xMin)/2 - 20,12,true);
     }
     else {
         snprintf(str, MAX_STR_LEN, "\n");
         GLIB_drawStringOnLine(&glibContext, str, 10, GLIB_ALIGN_LEFT,(Platform.xMax + Platform.xMin)/2 - 20,12,true);
     }

     //Draw SatchelCharge
     GLIB_drawRectFilled(&glibContext, &SatchelCharge);

//     if(PlatformDirectionInst.currDirection == hardLeft) {
//        dispDir = "HardLeft";;
//    }
//    else if(PlatformDirectionInst.currDirection == gradualLeft) {
//        dispDir = "GradualLeft";
//    }
//    else if(PlatformDirectionInst.currDirection == gradualRight) {
//        dispDir = "GradualRight";
//    }
//    else if(PlatformDirectionInst.currDirection == hardRight) {
//        dispDir = "HardRight";
//    }
//    else if(PlatformDirectionInst.currDirection == none) {
//        dispDir = "none";
//    }

     if(Game.game_status == platform_crash) {
//         snprintf(str, MAX_STR_LEN, "YOU CRASHED");
//         GLIB_drawStringOnLine(&glibContext, str, 4, GLIB_ALIGN_LEFT,25,5,true);
         OSSemPost(&App_Game_Semaphore,
                   OS_OPT_POST_ALL,  /* No special option.                     */
                   &err);

     }
     else if(Game.game_status == satchel_explosion) {
//         snprintf(str, MAX_STR_LEN, "YOU GOT HIT");
//         GLIB_drawStringOnLine(&glibContext, str, 4, GLIB_ALIGN_LEFT,25,5,true);
         OSSemPost(&App_Game_Semaphore,
                   OS_OPT_POST_ALL,  /* No special option.                     */
                   &err);
     }
     if(Game.game_status == evacuation) {//(game_destruction_max)/2) {
         if(set_time == false) {
           evac_time = dispTime + 10;
           set_time = true;
         }
         snprintf(str, MAX_STR_LEN, "EVACUATION \nSTARTED:%d",(evac_time - dispTime));
         GLIB_drawStringOnLine(&glibContext, str, 4, GLIB_ALIGN_LEFT,25,5,true);
         if((evac_time - dispTime) == 0) {
             OSSemPost(&App_Game_Semaphore,
                       OS_OPT_POST_ALL,  /* No special option.                     */
                       &err);
             set_time = false;
         }
     }

     //snprintf(str, MAX_STR_LEN, "LFT:%d",dispLeft);
     //GLIB_drawStringOnLine(&glibContext, str, 1, GLIB_ALIGN_LEFT,0,5,true);

//     snprintf(str, MAX_STR_LEN, "SH:%d",PlayerStats.shield_remaining);
//     GLIB_drawStringOnLine(&glibContext, str, 8, GLIB_ALIGN_LEFT,0,5,true);
//
//     snprintf(str, MAX_STR_LEN, "DIR:%s\n",dispDir);
//     GLIB_drawStringOnLine(&glibContext, str, 9, GLIB_ALIGN_LEFT,0,5,true);
//
//     snprintf(str, MAX_STR_LEN, "TIME:%d",dispTime);
//     GLIB_drawStringOnLine(&glibContext, str, 4, GLIB_ALIGN_LEFT,0,5,true);
//
//     snprintf(str, MAX_STR_LEN, "V:%d",PlayerStats.proj_velocity_x);
//     GLIB_drawStringOnLine(&glibContext, str, 3, GLIB_ALIGN_LEFT,0,5,true);

//     snprintf(str, MAX_STR_LEN, "H:%d",PlayerStats.hit);
//     GLIB_drawStringOnLine(&glibContext, str, 2, GLIB_ALIGN_LEFT,0,5,true);


         /* Release resource protected by mutex.       */
     OSMutexPost(&App_PlayerAction_Mutex,         /*   Pointer to user-allocated mutex.         */
     OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
     &err);
          /* Release resource protected by mutex.       */
     OSMutexPost(&App_PlatformAction_Mutex,         /*   Pointer to user-allocated mutex.         */
     OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
     &err);

     /* Post updates to display */
     DMD_updateDisplay();
     OSTimeDly(tauDisplay,              /*   Delay the task for 100 OS Ticks.         */
                OS_OPT_TIME_DLY,  /*   Delay is relative to current time.       */
               &err);
     if (err.Code != RTOS_ERR_NONE) {
         printf("Error while Handling App_LCDdisplay_Task task");
     }
   }
 }



static void LCD_init()
{
  uint32_t status;
  /* Enable the memory lcd */
  status = sl_board_enable_display();
  EFM_ASSERT(status == SL_STATUS_OK);

  /* Initialize the DMD support for memory lcd display */
  status = DMD_init(0);
  EFM_ASSERT(status == DMD_OK);

  /* Initialize the glib context */
  status = GLIB_contextInit(&glibContext);
  EFM_ASSERT(status == GLIB_OK);

  glibContext.backgroundColor = White;
  glibContext.foregroundColor = Black;

  /* Fill lcd with background color */
  GLIB_clear(&glibContext);

  /* Use Normal font */
  GLIB_setFont(&glibContext, (GLIB_Font_t *) &GLIB_FontNormal8x8);

  /* Draw text on the memory lcd display*/
  GLIB_drawStringOnLine(&glibContext,
                        "Welcome to...\n**Lab 7**!",
                        0,
                        GLIB_ALIGN_LEFT,
                        5,
                        5,
                        true);

  /* Draw text on the memory lcd display*/
  GLIB_drawStringOnLine(&glibContext,
                        "Review the lab\ninstructions!",
                        2,
                        GLIB_ALIGN_LEFT,
                        5,
                        5,
                        true);
  /* Post updates to display */
  DMD_updateDisplay();
}
/***************************************************************************//**
 * @brief
 *  Idle Task for PART III of lab
 *
 ******************************************************************************/
void  App_IdleTaskCreation (void)
{
    RTOS_ERR     err;

    OSTaskCreate(&App_IdleTaskTCB,                /* Pointer to the task's TCB.  */
                 "Idle Task.",                    /* Name to help debugging.     */
                 &App_IdleTask,                   /* Pointer to the task's code. */
                  DEF_NULL,                          /* Pointer to task's argument. */
                  APP_IDLE_TASK_PRIORITY,             /* Task's priority.            */
                 &App_IdleTaskStk[0],             /* Pointer to base of stack.   */
                 (APP_DEFAULT_TASK_STK_SIZE / 10u),  /* Stack limit, from base.     */
                  APP_DEFAULT_TASK_STK_SIZE,         /* Stack size, in CPU_STK.     */
                  10u,                               /* Messages in task queue.     */
                  0u,                                /* Round-Robin time quanta.    */
                  DEF_NULL,                          /* External TCB data.          */
                  OS_OPT_TASK_STK_CHK,               /* Task options.               */
                 &err);
    if (err.Code != RTOS_ERR_NONE) {
        /* Handle error on task creation. */
        printf("Error while Idle Task Creation");
    }
}
/***************************************************************************//**
*  Idle Low Energy Mode task
*******************************************************************************/
void  App_IdleTask (void  *p_arg)
{
    /* Use argument. */
   (void)&p_arg;
    while (DEF_TRUE) {
        EMU_EnterEM1();
    }
}
/***************************************************************************//**
*  Game Menu State test after game end conditions have occurred
*******************************************************************************/
void  App_GameTask (void  *p_arg)
{
    /* Use argument. */
   (void)&p_arg;

   RTOS_ERR     err;

   uint32_t status;

   char str[100];
   uint32_t MAX_STR_LEN = 100;

    while (DEF_TRUE) {
        OSSemPend(&App_Game_Semaphore,
                   0,
                   OS_OPT_PEND_BLOCKING,
                   NULL,
                   &err);

        /* Initialize the glib context */
             status = GLIB_contextInit(&glibContext);
             EFM_ASSERT(status == GLIB_OK);
             glibContext.backgroundColor = White;
             glibContext.foregroundColor = Black;
             /* Fill lcd with background color */
             GLIB_clear(&glibContext);

             OSMutexPend(&App_PlayerAction_Mutex,             /*   Pointer to user-allocated mutex.         */
             0,                  /*   Wait for a maximum of 1000 OS Ticks.     */
             OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
             DEF_NULL,              /*   Timestamp is not used.                   */
             &err);
                  /* Acquire resource protected by mutex.       */
             OSMutexPend(&App_PlatformAction_Mutex,             /*   Pointer to user-allocated mutex.         */
             0,                  /*   Wait for a maximum of 1000 OS Ticks.     */
             OS_OPT_PEND_BLOCKING,  /*   Task will block.                         */
             DEF_NULL,              /*   Timestamp is not used.                   */
             &err);
             if(Game.game_status == platform_crash) {
                 snprintf(str, MAX_STR_LEN, "YOU CRASHED");
                 GLIB_drawStringOnLine(&glibContext, str, 4, GLIB_ALIGN_LEFT,25,5,true);

             }
             else if(Game.game_status == satchel_explosion) {
                 snprintf(str, MAX_STR_LEN, "YOU GOT HIT");
                 GLIB_drawStringOnLine(&glibContext, str, 4, GLIB_ALIGN_LEFT,25,5,true);
             }
             else if(Game.game_status == evacuation) {//(game_destruction_max)/2) {
                 snprintf(str, MAX_STR_LEN, "EVACUATION \nSUCCESS");
                 GLIB_drawStringOnLine(&glibContext, str, 4, GLIB_ALIGN_LEFT,25,5,true);
             }
             Game.game_status = end;

             snprintf(str, MAX_STR_LEN, "GAME MENU");
             GLIB_drawStringOnLine(&glibContext, str, 1, GLIB_ALIGN_LEFT,0,5,true);

             snprintf(str, MAX_STR_LEN, "START GAME:(B0)");
             GLIB_drawStringOnLine(&glibContext, str, 2, GLIB_ALIGN_LEFT,0,5,true);

             snprintf(str, MAX_STR_LEN, "EDIT GAME:(B1)");
             GLIB_drawStringOnLine(&glibContext, str, 3, GLIB_ALIGN_LEFT,0,5,true);


             /* Post updates to display */
             DMD_updateDisplay();

             while(Game.game_status == end) {
             uint8_t START = pop(&button0);
             uint8_t EDIT = pop(&button1);

             if(START == button0high) {
                   /* Release resource protected by mutex.       */
               OSMutexPost(&App_PlayerAction_Mutex,         /*   Pointer to user-allocated mutex.         */
               OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
               &err);
                    /* Release resource protected by mutex.       */
               OSMutexPost(&App_PlatformAction_Mutex,         /*   Pointer to user-allocated mutex.         */
               OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
               &err);
               OSTimeDly(tauDisplay,              /*   Delay the task for 100 OS Ticks.         */
                          OS_OPT_TIME_DLY,  /*   Delay is relative to current time.       */
                         &err);
               castle_open();
               player_setup(&PlayerStats);
             }
             else if(EDIT == button1high) {
//                 OSMutexPost(&App_PlayerAction_Mutex,         /*   Pointer to user-allocated mutex.         */
//                 OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
//                 &err);
//                      /* Release resource protected by mutex.       */
//                 OSMutexPost(&App_PlatformAction_Mutex,         /*   Pointer to user-allocated mutex.         */
//                 OS_OPT_POST_1,     /*   Only wake up highest-priority task.      */
//                 &err);
//                 OSTimeDly(tauDisplay,              /*   Delay the task for 100 OS Ticks.         */
//                            OS_OPT_TIME_DLY,  /*   Delay is relative to current time.       */
//                           &err);
             }
             else {
             }
             }
    }
}

/***************************************************************************//**

 * @brief

 *   Setup castle drawing for display

 ******************************************************************************/

void castle_open(void)
{

//  //Platform initial position
//  Platform.xMin =   45;
//  Platform.yMin =   125;
//  Platform.xMax = 65;
//  Platform.yMax = 130;
  //Right canyon wall - unbreakable
  RightCanyon.xMax = 125;
  RightCanyon.yMax = 150;
  RightCanyon.xMin = 120;
  RightCanyon.yMin = 0;
  //Map Setup
  for(int i = 0; i < 16; i++) {
      //First cliff set
      WallObjects.P_Rectangle_T[0][i].xMin = 5;
      WallObjects.P_Rectangle_T[0][i].xMax = 10;
      WallObjects.P_Rectangle_T[0][i].yMin = 28 + (7*i);
      WallObjects.P_Rectangle_T[0][i].yMax = 33 + (7*i);
  }
  for(int i = 0; i < 12; i++) {
      //Second cliff set
      WallObjects.P_Rectangle_T[1][i].xMin = 12;
      WallObjects.P_Rectangle_T[1][i].xMax = 17;
      WallObjects.P_Rectangle_T[1][i].yMin = 28 + (7*i);
      WallObjects.P_Rectangle_T[1][i].yMax = 33 + (7*i);
  }
  for(int i = 0; i < 8; i++) {
      //Third cliff set
      WallObjects.P_Rectangle_T[2][i].xMin = 19;
      WallObjects.P_Rectangle_T[2][i].xMax = 24;
      WallObjects.P_Rectangle_T[2][i].yMin = 28 + (7*i);
      WallObjects.P_Rectangle_T[2][i].yMax = 33 + (7*i);
  }
  //Inner walls of castle
  for(int i = 0; i < 4; i++) {
      for(int j = 0; j < 7; j++) {
          if((j == 1 && i != 3) || (j == 3 && i != 3) || (j == 5 && i != 3)) {
              continue; //Make windows not rectangles!
          }
          CastleObjects.P_Rectangle_T[i][j].xMin = 5 + (7*j);
          CastleObjects.P_Rectangle_T[i][j].xMax = 10 + (7*j);
          CastleObjects.P_Rectangle_T[i][j].yMin = 0 + (7*i);
          CastleObjects.P_Rectangle_T[i][j].yMax = 5 + (7*i);
      }
  }
}
/***************************************************************************//**

 * @brief

 *   Setup initial player stat values

 ******************************************************************************/
void player_setup(volatile PlayerStatistics *Stats) {
  //Platform initial position
  Platform.xMin =   45;
  Platform.yMin =   125;
  Platform.xMax = 65;
  Platform.yMax = 130;
  Game.game_status = active_game;
  Game.destructionAmount = 0;
  PlatformDirectionInst.velocity = 0;
  Stats->shield_remaining = max_shield_and_start;
  //Reset player statistics
  Stats->railgun_charge = 0;
  Stats->proj_velocity_x = 0;
  Stats->proj_velocity_y = 0;
  for(int i = 0; i < 3; i++) {
      for(int j = 0; j < 16; j++) {
          Stats->hit_wall[i][j] = 0;
      }
  }
  for(int i = 0; i < 5; i++) {
      for(int j = 0; j < 7; j++) {
          Stats->hit_castle[i][j] = 0;
      }
  }
  Stats->shield_active = false;
  Stats->shield_protection = false;
  Stats->railgun_fire = false;
  Stats->railgun_charging = false;
  Stats-> proj_active = false;

}

void app_init(void)
{
  // Initialize GPIO
  gpio_open();

  // Initialize our capactive touch sensor driver!
  CAPSENSE_Init();

  // Initialize our LCD system
  LCD_init();
  player_setup(&PlayerStats);
  castle_open();

  button0_struct_init(&button0);
  button1_struct_init(&button1);
  App_PlayerAction_Creation();
  App_OS_GameState_SemaphoreCreation();
  App_OS_PlayerAction_SemaphoreCreation();
  App_OS_PlatformCtrl_SemaphoreCreation();
  App_PlayerAction_MutexCreation();
  App_PlatformAction_MutexCreation();
  App_OS_LEDoutput_EventFlagGroupCreation();
  App_OS_Physics_EventFlagGroupCreation();
  App_OS_TimerCreation ();
  App_Game_Creation();
  App_PlatformCtrl_creation();
  App_Physics_Creation();
  App_LEDoutput_Creation();
  App_LCDdisplay_Creation();
//  App_IdleTaskCreation();
}
