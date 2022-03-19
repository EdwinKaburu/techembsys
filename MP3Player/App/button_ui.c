#include "bsp.h"
#include "button_ui.h"
#include "tasks.h"


/*******************************************************************************
Button Handlers
*******************************************************************************/

void PlayBtn_Handler(void)
{
  
  INT8U err;
  
  OS_CPU_SR  cpu_sr;
  
  OS_ENTER_CRITICAL();
  

  
  
  
  // Non-Blocking Wait
  OSFlagAccept(btnFlags ,0x2,OS_FLAG_WAIT_SET_ANY,&err );
  
  pauseBottom.press(0);   stopSong = OS_FALSE;
  
  // Active Play Button
  OSFlagPost(btnFlags ,0x1,OS_FLAG_CLR ,&err);
  
  OS_EXIT_CRITICAL();
  
  OSIntExit();
  
}

void PauseBtn_Handler(void)
{
  INT8U err;
  
  OS_CPU_SR  cpu_sr;
  
  OS_ENTER_CRITICAL();
  
  // Redirected from Interrupt
  // PauseButton must Active at the end.
  // 1 - Means Active (Set)
  // 0 - Means Deactive (Clear)
    
  
  // Pause
  // Bit 1 or the Bit second ( 2 ) - Play Button Must be deactivated before continuing on.
  // Will be cleared on Pause-Button Click.
  OSFlagAccept(btnFlags ,0x1,OS_FLAG_WAIT_SET_ANY,&err );
  
  // If that is the case we can release the playBottom before
  // Activating the Pause Button
  
  playBottom.press(0);stopSong = OS_TRUE;
  
  // Active Pause Button, by setting the Bit 0
  OSFlagPost(btnFlags ,0x2,OS_FLAG_CLR,&err);
  
  OS_EXIT_CRITICAL();
  
  OSIntExit();
  
  
}


