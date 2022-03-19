/********************************************************
* AUTHOR : Edwin Kaburu
* FILE: tasks.c, tasks.h
* DATE: 2021/3 Edwin Kaburu adapted it for  
* MCU: STM-32L475VG, Arm Cortex M4
* PURPOSE/ Process: Implementation of UCOS II along with the defined tasks used 
* in the MP3 Music Player. StartUp Task Destroyed after creation. However it will
* create the application tasks (with priority ),other Inter-Process communication needed
*
* INPUT: Button Inputs from LCD, based on User Assertion
*
*
* OUTPUT: Prints on LCD (Adafruit 2.8" TFT Touch Shield v2). 
* Debugging(UART) on Tera Term (speed : 115200)
*
* ******************************************************/



/*******************************************************************************
* Libraries , Directives, Constants
* ****************************************************************************/

// ----------------------- Libraries -----------------------
#include <stdarg.h>
#include "bsp.h"
#include "print.h"
#include "mp3Util.h"
#include "SD.h"
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include "train_crossing.h" // Non-SD Card

// ----------------------- Directives -----------------------
#define PENRADIUS 3
#define BUFSIZE 256

// ----------------------- Constants -----------------------
// System is very Deterministic.
// Enter in INT8U CAPACITY = (TOTAL NUMBER OF SONGS IN YOUR SD CARD)

const INT8U CAPACITY = 5; // Should be modified to Numbers of Music Files you Have.
const INT8U QUEUE_CAPACITY = 2;

/*******************************************************************************

Allocate the stacks for each task.
The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h

*******************************************************************************/

static OS_STK   LcdTouchTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK   Mp3SDTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK   ControlTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK   DisplayTaskStk[APP_CFG_TASK_START_STK_SIZE];

/* TODO NO-SD CARD
static OS_STK   Mp3DemoTaskStk[APP_CFG_TASK_START_STK_SIZE];
*/

static HANDLE hMp3; // Static Handler

/*******************************************************************************
* Functions / Task Prototypes
* ****************************************************************************/

// Function : LcdTouchTask()
// Purpose : Assertion of User Touch Points / Buttons Clicks
// Return : Void
void LcdTouchTask(void* pdata); 

// Function : ControlTask()
// Purpose : Implementation of User Touch Points / Buttons Clicks. Determines Actions To Perfom
// Return : Void
void ControlTask(void* pdata);

// Function : DisplayTask()
// Purpose : Action is To Display Information Handed to the Task
// Return : Void
void DisplayTask(void* pdata); // UI Display

// Function : Mp3SDTask()
// Purpose : Action is To Play Music, based on Information handed To The Task from 
// Return : Void
void Mp3SDTask(void* pdata);

// Function : MapTouchToScreen()
// Purpose : Mapping of User Touch Points/Location. Used in LcdTouchTask() 
// Return : mapped location
long MapTouchToScreen(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Function : PrintToLcdWithBuf()
// Purpose : Print Information Given . Debugging 
// Return : void
void PrintToLcdWithBuf(char *buf, int size, char *format, ...);

void *qMusicStatus[QUEUE_CAPACITY]; 

/* TODO NO-SD CARD
void Mp3DemoTask(void* pdata);
*/

/*******************************************************************************
* Others
* ****************************************************************************/

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller

Adafruit_FT6206 touchCtrl = Adafruit_FT6206(); // The touch controller

// ----------------------- Buttons -----------------------

Adafruit_GFX_Button playButton;
Adafruit_GFX_Button pauseButton;
Adafruit_GFX_Button nextButton;
Adafruit_GFX_Button previousButton;
Adafruit_GFX_Button haltButton;
Adafruit_GFX_Button VolIncButton;
Adafruit_GFX_Button VolDesButton;

// ---------------------- OS Events  ----------------------

// ---------------------- Mail Box ----------------------
OS_EVENT * buttonMBox; // Get Button Clicked / Posted 
OS_EVENT * mstatus_Change; // Display Current State of Music Status during interrupted state
OS_EVENT * volume_Change; // Display Volume Change
OS_EVENT *queueMusic; // Display Current Music and Status

// Available Previous Files
File prevFiles[CAPACITY];

typedef enum
{
  VOLUP_COMMAND,
  VOLDW_COMMAND,
  PAUSE_COMMAND,
  PLAY_COMMAND,
  NEXT_COMMAND,
  PREVIOUS_COMMAND,
  HALT_COMMAND
}ButtonControlsEnum;

// ---------------------- Mp3 Status Pointers  ----------------------

INT8U DefVolume =  0x00;

BOOLEAN nextSong = OS_FALSE;
BOOLEAN stopSong = OS_TRUE;
BOOLEAN prevSong = OS_FALSE;
BOOLEAN haltPlayer = OS_TRUE; 
BOOLEAN isr_M = OS_FALSE; // True, when current music is interrupt by next or previous click
BOOLEAN Read_Update = OS_FALSE; 
BOOLEAN Read_MailUpdate = OS_FALSE;
BOOLEAN After_Start = OS_FALSE; // Need to Know, if we started streaming music
BOOLEAN SystemVolUp = OS_FALSE; // Need to Know, if there is a volume Change

/******************************************************************************/


/*******************************************************************************
This task is the initial task running, started by main(). It starts
the system tick timer and creates all the other tasks. Then it deletes itself.
*******************************************************************************/
void StartupTask(void* pdata)
{
  char buf[BUFSIZE];
  
  PjdfErrCode pjdfErr;
  INT32U length;
  static HANDLE hSD = 0;
  static HANDLE hSPI = 0;
  
  PrintWithBuf(buf, BUFSIZE, "StartupTask: Begin\n");
  
  // Start the system tick
  SetSysTick(OS_TICKS_PER_SEC);
  
  // ------------------ Create Queue and Mutex ------------------
  
  // Button Mail box, need to send to Control, the button asserted.
  buttonMBox = OSMboxCreate(NULL);
  
  // Music Status Change, need to send to Display,something during Interrupted State
  // Interrupted State - Pause , Halted assertion.
  // Print To LCD
  mstatus_Change = OSMboxCreate(NULL);
  
  // Volume Change, need send to Display, A volume Change to Print to LCD
  volume_Change = OSMboxCreate(NULL);
  
  // Current Music Status, Need to send To Display. A Queue, with Music name and status
  queueMusic = OSQCreate(qMusicStatus, QUEUE_CAPACITY);
  
  // Initialize SD card
  //PrintWithBuf(buf, PRINTBUFMAX, "Opening handle to SD driver: %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
  hSD = Open(PJDF_DEVICE_ID_SD_ADAFRUIT, 0);
  if (!PJDF_IS_VALID_HANDLE(hSD)) while(1);
  
  
  PrintWithBuf(buf, PRINTBUFMAX, "Opening SD SPI driver: %s\n", SD_SPI_DEVICE_ID);
  // We talk to the SD controller over a SPI interface therefore
  // open an instance of that SPI driver and pass the handle to 
  // the SD driver.
  hSPI = Open(SD_SPI_DEVICE_ID, 0);
  if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);
  
  length = sizeof(HANDLE);
  pjdfErr = Ioctl(hSD, PJDF_CTRL_SD_SET_SPI_HANDLE, &hSPI, &length);
  if(PJDF_IS_ERROR(pjdfErr)) while(1);
  
  if (!SD.begin(hSD)) {
    PrintWithBuf(buf, PRINTBUFMAX, "Attempt to initialize SD card failed.\n");
  }
  
  // Create the test tasks
  PrintWithBuf(buf, BUFSIZE, "StartupTask: Creating the application tasks\n");
  
  // The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h
  uint16_t task_prio =APP_TASK_TEST2_PRIO;
  
  
  
  // ------------------------------ Task Creation ------------------------------
  OSTaskCreate(LcdTouchTask, (void*)0, &LcdTouchTaskStk[APP_CFG_TASK_START_STK_SIZE-1], task_prio++);
  
  /* TODO NO-SD CARD
  OSTaskCreate(Mp3DemoTask, (void*)0, &Mp3DemoTaskStk[APP_CFG_TASK_START_STK_SIZE-1], task_prio++);
  */
  
  OSTaskCreate(Mp3SDTask, (void*)0, &Mp3SDTaskStk[APP_CFG_TASK_START_STK_SIZE-1], task_prio++);
  
  OSTaskCreate(ControlTask, (void*)0, &ControlTaskStk[APP_CFG_TASK_START_STK_SIZE-1], task_prio++);
  
  OSTaskCreate(DisplayTask, (void*)0, &DisplayTaskStk[APP_CFG_TASK_START_STK_SIZE-1], task_prio++);
  
  // Delete Task 
  OSTaskDel(OS_PRIO_SELF);
}

static void DrawLcdContents()
{
  char buf[BUFSIZE];
  OS_CPU_SR cpu_sr;
  
  // allow slow lower pri drawing operation to finish without preemption
  OS_ENTER_CRITICAL(); 
  
  lcdCtrl.fillScreen(ILI9341_BLACK);
  
  // Print a message on the LCD
  lcdCtrl.setCursor(0, 0);
  lcdCtrl.setTextColor(ILI9341_WHITE);  
  lcdCtrl.setTextSize(2);
  PrintToLcdWithBuf(buf, BUFSIZE, "Music Player");
  
  OS_EXIT_CRITICAL();
  
}

void DisplaySong(char* string, INT16S x, INT16S y, INT16S w, INT16S h)
{
  char buf[BUFSIZE];
  OS_CPU_SR cpu_sr;
  
  // allow slow lower pri drawing operation to finish without preemption
  OS_ENTER_CRITICAL(); 
  
  lcdCtrl.fillRect(x,y,w, h,ILI9341_NAVY);
  
  // Print a message on the LCD
  lcdCtrl.setCursor(x, y); 
  lcdCtrl.setTextColor(ILI9341_WHITE);  
  lcdCtrl.setTextSize(2);
  PrintToLcdWithBuf(buf, BUFSIZE, string);
  
  OS_EXIT_CRITICAL();
  
}


/*******************************************************************************
RUN SD TASK CODE
********************************************************************************/

void Mp3SDTask(void* pdata)
{
  INT8U err;
  
  PjdfErrCode pjdfErr;
  INT32U length;
  
  char buf[BUFSIZE];
  PrintWithBuf(buf, BUFSIZE, "Mp3DemoTask: starting\n");
  
  // Open handle to the MP3 decoder driver
  hMp3 = Open(PJDF_DEVICE_ID_MP3_VS1053, 0);
  if (!PJDF_IS_VALID_HANDLE(hMp3)) while(1);
  
  PrintWithBuf(buf, BUFSIZE, "Opening MP3 SPI driver: %s\n", MP3_SPI_DEVICE_ID);
  // We talk to the MP3 decoder over a SPI interface therefore
  // open an instance of that SPI driver and pass the handle to 
  // the MP3 driver.
  HANDLE hSPI = Open(MP3_SPI_DEVICE_ID, 0);
  if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);
  
  length = sizeof(HANDLE);
  pjdfErr = Ioctl(hMp3, PJDF_CTRL_MP3_SET_SPI_HANDLE, &hSPI, &length);
  if(PJDF_IS_ERROR(pjdfErr)) while(1);
  
  // Send initialization data to the MP3 decoder and run a test
  PrintWithBuf(buf, BUFSIZE, "Starting MP3 device test\n");
  Mp3Init(hMp3);
  
  int count = 0;
  
  // We are Halted By Default
  char *music_status = "Halting";
  
  // Notify DisplayTask of Messsage Queue, need to be extracted
  Read_Update = OS_TRUE;
  
  // "Halting" inserted for Name and Status To be Displayed  
  err = OSQPost(queueMusic, (void*)music_status); // Display Name
  err = OSQPost(queueMusic, (void*)music_status); // Display Music Status
  
  
  File dir = SD.open("/");
  
  while (1)
  {
    // Reset Values, before restarting 
    INT8U counter = 0u;
    
    // ISR-M - Interrupted Music Files (Next, Previous Action)
    
    // Need To Reset The Pointers 
    INT8U at_counter = 0u; // After Counter
    INT8U lp_counter = 0u; // Looping Counter
    
    // HaltPlayer must be False To Continue, to play Music Files, after
    // Task Creation. 
    if(haltPlayer)
    {
      OSTimeDly(300);
    }
    else
    {
      // haltPlayer Set To False, after Reset. Variable used in Mp3Util
      After_Start = OS_TRUE;
      
      // Loop To Play Song
      while (1)
      {
        File entry;
        
        // Default Music Status
        music_status = "Playing";
        
        // Check if we are in isr_M, and run those files, else read from SD Card.
        if(isr_M)
        { 
          
          if(prevSong)
          {
            // Previous Button is True and asserted , Decrement LP_Counter;
            if(lp_counter != 0)
            {
              lp_counter--;
            }
            
            // Get File using the  Lp_Counter 
            // Lp_Counter - Least Previous Counter
            entry = prevFiles[lp_counter];
            
            // Play Song 
            PrintWithBuf(buf, BUFSIZE, "\nP-Begin streaming isr file: %d \n", lp_counter);       
            
            // Reset Previous Button
            prevSong = OS_FALSE;
            
          }
          else
          {
            if(nextSong || nextSong == OS_FALSE)
            {
              if(nextSong)
              {
                // Reset Next Button, if Asserted
                nextSong = OS_FALSE;
              }
              
              // We need the lp_counter, and highest at_counter to be equal
              // in order to exit isr_M and return to Reading from SD Card.
              
              if(lp_counter < at_counter)
              {
                lp_counter++;
                
                // Only Play when Lpcounter != at_counter
                // Get File using Lp_Counter 
                entry = prevFiles[lp_counter];
                
                // Play Song 
                PrintWithBuf(buf, BUFSIZE, "\nNP-Begin streaming isr file: %d\n", lp_counter);   
                
              }
              else 
              {
                // Both Pointers are Equal Need to Exit ISR_M
                if(lp_counter == at_counter)
                {
                  isr_M = OS_FALSE; // This will exit ISR_M
                }
              }
            }
          }
          // Notify DisplayTask of Messsage Queue, need to be extracted
          Read_Update = OS_TRUE;
          
          // Display Name
          err = OSQPost(queueMusic, (void*)entry.name()); 
          // Display Music Status
          err = OSQPost(queueMusic, (void*)music_status); 
          
          // Stream a given File, based on Above Condition
          Mp3StreamSDFile(hMp3, entry.name());          
          
          PrintWithBuf(buf, BUFSIZE, "\nDone streaming isr file at LP: %d", lp_counter);
          
          continue;
        }
        else
        {
          // Get The Next File
          entry = dir.openNextFile();
          
          if (!entry)
          {
            break;
          }
          if (entry.isDirectory())  // skip directories
          {
            entry.close();
            continue;
          }
          
          // Play Song from SD Card
          PrintWithBuf(buf, BUFSIZE, "\nBegin streaming sd file  count=%d\n", ++count);    
          
          // Notify DisplayTask of Messsage Queue, need to be extracted
          Read_Update = OS_TRUE;
          
          // Display Name
          err = OSQPost(queueMusic, (void*)entry.name()); 
          // Display Music Status
          err = OSQPost(queueMusic, (void*)music_status); 
          
          // Stream a given File
          Mp3StreamSDFile(hMp3, entry.name()); 
          
          PrintWithBuf(buf, BUFSIZE, "\nDone streaming sd file  count=%d\n", ++count);    
          
          // Will Reset is Counter is Above Capacitor (CAPACITY)
          // Capacity = 3 music files, counter = 0
          // 0 mod 3 = 0, 1 mod 3 = 1, 
          // 2 mod 3 = 2, 3 mod 3 = 0
          
          // We are determining where (index) to save the File, so that we can go back(previous)
          // and view them
          counter = counter % CAPACITY; 
          
          // Store this played / Interrupted File at prevFiles[]
          prevFiles[counter] = entry;
          
          // Increment Counter, There is something In Previous List . prevFiles[]
          counter++;
          
          // This was Next Button CLick / Assertion
          if(nextSong) 
          {
            // Reset Next_Song to False
            // We already Saved The Files in to the prevFiles[]
            nextSong = OS_FALSE;
          }
          
          // If we are playing prevFiles, we are now in the isr_M.
          // We only enter into it, during a Previous Button Click / Assertion.
          
          // Insert Ourself Into isr_M
          // This is our entry point of intersection 
          if(isr_M == OS_FALSE && counter > 0 && prevSong)
          { 
            // Set isr_M Pointer
            isr_M = OS_TRUE;
            
            // Set Up At_counter  And LP_counter
            // counter = 2, at_counter = 1, lp_counter  = 1.
            // lp_counter will be decremented later on to 0.
            
            at_counter = counter - 1 ;
            lp_counter = at_counter;
            
            // Debugging Display 
            PrintWithBuf(buf, BUFSIZE, "\nInserted At_C: %d LP_C: %d\n", at_counter, lp_counter);  
            
          }
          
          PrintWithBuf(buf, BUFSIZE, "FileSV =%d\n", counter);
        }
        
        entry.close();
        
      }
      
    }
    
    dir.seek(0); // reset directory file to read again;
    
  }
}

/*******************************************************************************

Runs Control Task code

*******************************************************************************/

void ControlTask(void* pdata)
{
  INT8U err;
  char buf[BUFSIZE];
  ButtonControlsEnum *BtnControl_type;
  
  PrintWithBuf(buf, BUFSIZE,"Control Task building\n");
  
  while(1)
  {
    // Get Which Button Was Clicked
    BtnControl_type = (ButtonControlsEnum*)OSMboxPend(buttonMBox,0,&err);
    
    ButtonControlsEnum* value = BtnControl_type;
    
    // Display its Value: Debugging Display
    PrintWithBuf(buf, BUFSIZE,"Value: %d \t %d \n", value, (*value));
    
    char *music_status;
    
    char volumeDigit[7];
    
    INT8U displayVolume;
    
    // Get The CPU Status
    OS_CPU_SR  cpu_sr;
    
    switch((int)BtnControl_type)
    {
    case VOLUP_COMMAND:
      
      // De-Assert VolumeBtn : Make The Button Available
      VolIncButton.press(0);
      
      // Set SystemVolUp Pointer To True.
      SystemVolUp = OS_TRUE;
      
      // Highest Volume in our System is 0x00 (0), 100%
      // Lowest Volume  is 0x64 (100), 0 %     
      if(DefVolume != 0x00)
      {
        // Increase Volume by decrementing it to 0x00 or 0
        DefVolume =  DefVolume - 0xA; // subtract 10
        
        // We need to set the volume, uninterrupted, otherwise it won't work
        OS_ENTER_CRITICAL();
        
        // See mp3Util.c for its implementation
        Mp3VolumeUpDown(hMp3);
        
        OS_EXIT_CRITICAL();
      }
      
      // Need to Display Updated Volume
      Read_Update = OS_TRUE;
      Read_MailUpdate = OS_TRUE;
      
      // Subtract DefVolume and from maximum a Hundred 
      displayVolume = 0x64 - DefVolume;
      
      // Convert Digit To a String
      snprintf(volumeDigit,7,"%d %s", displayVolume, "%%");
      
      // Post into the volume_Change Mailbox
      OSMboxPost(volume_Change, (void*) volumeDigit);
      
      // Delay
      OSTimeDly(100);
      
      break;
    case VOLDW_COMMAND:
      
      // De-Assert VolumeBtn : Make The Button Available
      VolDesButton.press(0);
      
      // Set SystemVolUp Pointer To True.
      SystemVolUp = OS_TRUE;
      
      // Highest Volume in our System is 0x00 (0), 100%
      // Lowest Volume  is 0x64 (100), 0 %     
      
      if(DefVolume < 0x64) 
      {
        // Decrease Volume, by increasing it to 0x64
        DefVolume =  DefVolume + 0xA; // Add 10
        
        // We need to set the volume, uninterrupted, otherwise it won't work
        OS_ENTER_CRITICAL();
        
        Mp3VolumeUpDown(hMp3);
        
        // See mp3Util.c for its implementation
        OS_EXIT_CRITICAL();
      }
      
      // Need to Display Updated Volume
      Read_Update = OS_TRUE;
      Read_MailUpdate = OS_TRUE;
      
      // Subtract DefVolume and from maximum a Hundred 
      displayVolume = 0x64 - DefVolume;
      
      // Convert Digit To a String
      snprintf(volumeDigit,7,"%d %s", displayVolume, "%%");
      
      // Post into the volume_Change Mailbox
      OSMboxPost(volume_Change, (void*) volumeDigit);
      
      // Delay
      OSTimeDly(100);
      
      break;
    case PAUSE_COMMAND:
      // De-assert Play Button and setStop To True
      playButton.press(0); stopSong = OS_TRUE;
      
      // Need to Display Updated Music Status
      Read_Update = OS_TRUE;
      Read_MailUpdate = OS_TRUE;
      
      music_status = "Paused";
      // Add To Mail Box
      OSMboxPost(mstatus_Change,(void*)music_status);
      
      break;
    case PLAY_COMMAND:
      pauseButton.press(0);  
      haltButton.press(0);
      
      haltPlayer = OS_FALSE;
      
      stopSong = OS_FALSE;
      
      if(After_Start)
      {
        Read_Update = OS_TRUE;
        
        Read_MailUpdate = OS_TRUE;
        
        music_status = "Playing";
        // Add To Mail Box
        OSMboxPost(mstatus_Change,(void*)music_status);
      }
      
      break;
    case NEXT_COMMAND:
      prevSong = OS_FALSE;  nextSong = OS_TRUE;
      
      // De-assert Button Pressed. 
      nextButton.press(0); 
      
      pauseButton.press(0);
      
      // Un-Pause Song if paused.
      // Condition will be taken care off by the Button Click Capture.
      // We are not un-Halting the entire music player, just the song.
      stopSong = OS_FALSE;
      
      break;
    case PREVIOUS_COMMAND:
      
      prevSong = OS_TRUE; nextSong = OS_FALSE;
      
      previousButton.press(0); 
      pauseButton.press(0);
      
      // Un-Pause Song if paused.
      // Condition will be taken care off by the Button Click Capture.
      // We are not un-Halting the entire music player, just the song.
      stopSong = OS_FALSE;
      
      break;      
    default:
      // Set Halt Player  To True
      haltPlayer = OS_TRUE;
      // Set Stop Song To True
      stopSong = OS_TRUE;
      
      // Release Play Button if Locked
      playButton.press(0);
      
      Read_Update = OS_TRUE;
      
      Read_MailUpdate = OS_TRUE;
      
      music_status = "Halting";
      // Add To Mail Box
      OSMboxPost(mstatus_Change,(void*)music_status);
      
      break;
    }
  }
  
}

/*******************************************************************************

Runs Display Task Code

*******************************************************************************/
void DisplayTask(void* pdata)
{
  
  INT8U err;
  
  INT16U rdOnce = 0; // Read Once
  
  char *display_name;
  char *display_status;
  char *display_volume = "100 %%";
  
  char buf[BUFSIZE];
  PrintWithBuf(buf, BUFSIZE,"Display Task building\n");
  
  while(1)
  {
    if(Read_Update)
    {
      if(Read_MailUpdate)
      {
        // Message Display,could be Halt Command or Pause Command 
        // or Stop Command.
        
        // Check for Volume Update
        if(SystemVolUp )
        {
          // Capture Volume Data in MailBox
          display_volume = (char*)OSMboxPend(volume_Change,100,&err);      
        }
        else
        {
          // Read from Mail Box
          display_status = (char*)OSMboxPend(mstatus_Change,100,&err);
        }
        
      }
      else
      {
        // Read From Queue 
        display_name = (char*)OSQPend(queueMusic,0,&err);
        
        display_status = (char*)OSQPend(queueMusic,0,&err);
        
        // rdOnce - Read Once To 0
        rdOnce = 0;
        
      }
      
      // Display Values
      if(rdOnce == 0)
      {
        // Only Display Once
        DisplaySong(display_name,0,40,150,20);
      }
      
      // Display Updated Music Status
      DisplaySong(display_status,0,90,150,20);
      
      // Display Volume
      DisplaySong(display_volume, 0, 140, 100,20);
      
      // Reset Booleans
      rdOnce++;
      Read_MailUpdate = OS_FALSE;
      Read_Update = OS_FALSE;
      
      SystemVolUp = OS_FALSE;
      
    }
    
    OSTimeDly(100);
    
  }
  
}

/*******************************************************************************

Runs LCD/Touch  code

*******************************************************************************/
void LcdTouchTask(void* pdata)
{
  PjdfErrCode pjdfErr;
  // INT8U err;
  INT32U length;
  
  char buf[BUFSIZE];
  PrintWithBuf(buf, BUFSIZE, "LcdTouchDemoTask: starting\n");
  
  PrintWithBuf(buf, BUFSIZE, "Opening LCD driver: %s\n", PJDF_DEVICE_ID_LCD_ILI9341);
  // Open handle to the LCD driver
  HANDLE hLcd = Open(PJDF_DEVICE_ID_LCD_ILI9341, 0);
  if (!PJDF_IS_VALID_HANDLE(hLcd)) while(1);
  
  PrintWithBuf(buf, BUFSIZE, "Opening LCD SPI driver: %s\n", LCD_SPI_DEVICE_ID);
  // We talk to the LCD controller over a SPI interface therefore
  // open an instance of that SPI driver and pass the handle to 
  // the LCD driver.
  HANDLE hSPI = Open(LCD_SPI_DEVICE_ID, 0);
  if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);
  
  length = sizeof(HANDLE);
  pjdfErr = Ioctl(hLcd, PJDF_CTRL_LCD_SET_SPI_HANDLE, &hSPI, &length);
  if(PJDF_IS_ERROR(pjdfErr)) while(1);
  
  PrintWithBuf(buf, BUFSIZE, "Initializing LCD controller\n");
  lcdCtrl.setPjdfHandle(hLcd);
  lcdCtrl.begin();
  
  DrawLcdContents();
  
  PrintWithBuf(buf, BUFSIZE, "Initializing FT6206 touchscreen controller\n");
  
  // DRIVER TODO
  // Open a HANDLE for accessing device PJDF_DEVICE_ID_I2C1
  // <your code here>
  HANDLE hSPI1 = Open(PJDF_DEVICE_ID_I2C,0);
  
  if(!PJDF_IS_VALID_HANDLE(hSPI1))
  {
    while(1);
  }
  
  // Call Ioctl on that handle to set the I2C device address to FT6206_ADDR
  // <your code here>
  //(void *)FT6206_ADDR
  uint8_t address = FT6206_ADDR;
  pjdfErr =  Ioctl(hSPI1, PJDF_CTRL_I2C_SET_DEVICE_ADDRESS, &address , &length);
  if(PJDF_IS_ERROR(pjdfErr)) while(1);
  
  // Call setPjdfHandle() on the touch contoller to pass in the I2C handle
  // <your code here>
  touchCtrl.setPjdfHandle(hSPI1);
  touchCtrl.begin();
  
  
  if (! touchCtrl.begin(40)) {  // pass in 'sensitivity' coefficient
    //PrintWithBuf(buf, BUFSIZE, "Couldn't start FT6206 touchscreen controller\n");
    while (1);
  }
  
  int currentcolor = ILI9341_GREEN;
  
  //  Adafruit_GFX_Button pauseButton
  pauseButton = Adafruit_GFX_Button(); 
  
  // Adafruit_GFX_Button playButton
  playButton  = Adafruit_GFX_Button();
  
  // Adafruit_GFX_Button nextButton
  nextButton = Adafruit_GFX_Button();
  
  // Adafruit_GFX_Button haltButton
  haltButton = Adafruit_GFX_Button();
  
  // Adafruit_GFX_Button previousButton
  previousButton = Adafruit_GFX_Button();
  
  // Adafruit_GFX_Bitton VolIncButton
  VolIncButton = Adafruit_GFX_Button();
  
  // Adafruit_GFX_Bitton VolDesButton
  VolDesButton = Adafruit_GFX_Button();
  
  pauseButton.initButton(&lcdCtrl, ILI9341_TFTWIDTH-30, ILI9341_TFTHEIGHT-30, // x, y center of button
                         60, 50, // width, height
                         ILI9341_YELLOW, // outline
                         ILI9341_BLACK, // fill
                         ILI9341_YELLOW, // text color
                         "Pause", // label
                         1); // text size
  pauseButton.drawButton(0);
  
  
  
  playButton.initButton(&lcdCtrl, ILI9341_TFTWIDTH-100, ILI9341_TFTHEIGHT-30, // x, y center of button
                        60, 50, // width, height
                        ILI9341_YELLOW, // outline
                        ILI9341_BLACK, // fill
                        ILI9341_YELLOW, // text color
                        "Play", // label
                        1); // text size
  playButton.drawButton(0);
  
  nextButton.initButton(&lcdCtrl, ILI9341_TFTWIDTH-170, ILI9341_TFTHEIGHT-30, // x, y center of button
                        60, 50, // width, height
                        ILI9341_YELLOW, // outline
                        ILI9341_BLACK, // fill
                        ILI9341_YELLOW, // text color
                        "Next", // label
                        1); // text size
  nextButton.drawButton(0);
  
  previousButton.initButton(&lcdCtrl, ILI9341_TFTWIDTH-170, ILI9341_TFTHEIGHT-90, // x, y center of button
                            60, 50, // width, height
                            ILI9341_YELLOW, // outline
                            ILI9341_BLACK, // fill
                            ILI9341_YELLOW, // text color
                            "Previous", // label
                            1); // text size
  previousButton.drawButton(0);
  
  
  haltButton.initButton(&lcdCtrl, ILI9341_TFTWIDTH-30, ILI9341_TFTHEIGHT-90, // x, y center of button
                        60, 50, // width, height
                        ILI9341_YELLOW, // outline
                        ILI9341_BLACK, // fill
                        ILI9341_YELLOW, // text color
                        "Stop", // label
                        1); // text size
  haltButton.drawButton(0);
  
  VolIncButton.initButton(&lcdCtrl, ILI9341_TFTWIDTH-30, ILI9341_TFTHEIGHT-150, // x, y center of button
                          60, 50, // width, height
                          ILI9341_YELLOW, // outline
                          ILI9341_BLACK, // fill
                          ILI9341_YELLOW, // text color
                          "Vol Up", // label
                          1); // text size
  VolIncButton.drawButton(0);
  
  
  VolDesButton.initButton(&lcdCtrl, ILI9341_TFTWIDTH-30, ILI9341_TFTHEIGHT-210, // x, y center of button
                          60, 50, // width, height
                          ILI9341_YELLOW, // outline
                          ILI9341_BLACK, // fill
                          ILI9341_YELLOW, // text color
                          "Vol Down", // label
                          1); // text size
  VolDesButton.drawButton(0);
  
  // By Default this will be 0 - False.
  // Meaning that is was Released
  
  pauseButton.press(0);
  
  playButton.press(0);
  
  nextButton.press(0);
  
  previousButton.press(0);
  
  VolIncButton.press(0);
  
  while (1) { 
    boolean touched;
    
    touched = touchCtrl.touched();
    
    if (! touched) {
      OSTimeDly(5);
      continue;
    }
    
    TS_Point point;
    
    point = touchCtrl.getPoint();
    if (point.x == 0 && point.y == 0)
    {
      continue; // usually spurious, so ignore
    }
    
    // transform touch orientation to screen orientation.
    TS_Point p = TS_Point();
    p.x = MapTouchToScreen(point.x, 0, ILI9341_TFTWIDTH, ILI9341_TFTWIDTH, 0);
    p.y = MapTouchToScreen(point.y, 0, ILI9341_TFTHEIGHT, ILI9341_TFTHEIGHT, 0);
    
    lcdCtrl.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
    
    // Don't Accept Buttons when HaltPlayer is set To True, Apart from Play Button
    if(pauseButton.contains(p.x, p.y) && pauseButton.isPressed() == 0 && !haltPlayer)
    {
      
      if(pauseButton.justReleased() == 1 || pauseButton.justPressed() == 0)
      {
        
        // Assert Pause Button
        pauseButton.press(1); 
        
        if(pauseButton.isPressed() == 1)
        {
          // Send Control Message to Mailbox
          
          PrintString("\nPause Asserted\n");
          
          OSMboxPost(buttonMBox,(void*)PAUSE_COMMAND);
          
        }
        
      } //OSTimeDly(100);
      
    }
    else if(playButton.contains(p.x, p.y) && playButton.isPressed() == 0)
    { 
      
      if(playButton.justReleased() == 1 || playButton.justPressed() == 0)
      {
        // Assert Play Button
        playButton.press(1); 
        
        // Call Interrupt
        if(playButton.isPressed() == 1)
        {
          // Send Control Message to Mailbox
          
          PrintString("\nPlay Asserted\n");
          
          OSMboxPost(buttonMBox,(void*)PLAY_COMMAND);
          
        }
      }
      
    }
    else if (nextButton.contains(p.x, p.y) && nextButton.isPressed() == 0 && !haltPlayer)
    {
      if(nextButton.justReleased() == 1 || nextButton.justPressed() == 0)
      {
        // Assert Next Button
        nextButton.press(1); 
        
        // Call Interrupt
        if(nextButton.isPressed() == 1)
        {
          // Send Control Message to Mailbox
          
          PrintString("\nNext Asserted\n");
          
          OSMboxPost(buttonMBox,(void*)NEXT_COMMAND);
          
        }
      }
    }
    else if (previousButton.contains(p.x, p.y) && previousButton.isPressed() == 0 && !haltPlayer)
    {
      if(previousButton.justReleased() == 1 || previousButton.justPressed() == 0)
      {
        // Assert Previous Button
        previousButton.press(1); 
        
        // Call Interrupt
        if(previousButton.isPressed() == 1)
        {
          // Send Control Message to Mailbox
          
          PrintString("\nPrevious Asserted\n");
          
          // NEXT_COMMAND, PREVIOUS_COMMAND, previousBottom
          
          OSMboxPost(buttonMBox,(void*)PREVIOUS_COMMAND);
          
        }
      }
    }//haltButton
    else if(haltButton.contains(p.x, p.y) && haltButton.isPressed() == 0 && !haltPlayer)
    {
      if(haltButton.justReleased() == 1 || haltButton.justPressed() == 0)
      {
        // Assert Halt Button
        haltButton.press(1);
        
        // Send Button Clicked to Control Task
        if(haltButton.isPressed() == 1)
        {
          PrintString("\nHalt Asserted\n");
          
          OSMboxPost(buttonMBox,(void*)HALT_COMMAND);
        }
      }
    }
    else if(VolIncButton.contains(p.x, p.y) && VolIncButton.isPressed() == 0 && !haltPlayer)
    {
      if(VolIncButton.justReleased() == 1 || VolIncButton.justPressed() == 0)
      {
        // Assert Vol Up Button
        VolIncButton.press(1);
        
        // Send Button Clicked to Control Task
        if(VolIncButton.isPressed() == 1)
        {
          PrintString("\nIncrease Volume \n");
          
          OSMboxPost(buttonMBox,(void*)VOLUP_COMMAND);
        }
      }
    }
    else if(VolDesButton.contains(p.x, p.y) && VolDesButton.isPressed() == 0 && !haltPlayer)
    {
      if(VolDesButton.justReleased() == 1 || VolDesButton.justPressed() == 0)
      {
        // Assert Vol Down Button
        VolDesButton.press(1);
        
        // Send Button Clicked to Control Task
        if(VolDesButton.isPressed() == 1)
        {
          PrintString("\nDecrease Volume \n");
          
          OSMboxPost(buttonMBox,(void*)VOLDW_COMMAND);
        }
      }
    }
    
    OSTimeDly(100);
    
  } 
  
}

/************************************************************************************

Runs MP3 demo code

************************************************************************************/

/* TODO NO-SD CARD
void Mp3DemoTask(void* pdata)
{
  PjdfErrCode pjdfErr;
  INT32U length;
  INT8U err;
  
  char buf[BUFSIZE];
  PrintWithBuf(buf, BUFSIZE, "Mp3DemoTask: starting\n");
  
  PrintWithBuf(buf, BUFSIZE, "Opening MP3 driver: %s\n", PJDF_DEVICE_ID_MP3_VS1053);
  // Open handle to the MP3 decoder driver
  HANDLE hMp3 = Open(PJDF_DEVICE_ID_MP3_VS1053, 0);
  if (!PJDF_IS_VALID_HANDLE(hMp3)) while(1);
  
  PrintWithBuf(buf, BUFSIZE, "Opening MP3 SPI driver: %s\n", MP3_SPI_DEVICE_ID);
  // We talk to the MP3 decoder over a SPI interface therefore
  // open an instance of that SPI driver and pass the handle to 
  // the MP3 driver.
  HANDLE hSPI = Open(MP3_SPI_DEVICE_ID, 0);
  if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);
  
  length = sizeof(HANDLE);
  pjdfErr = Ioctl(hMp3, PJDF_CTRL_MP3_SET_SPI_HANDLE, &hSPI, &length);
  if(PJDF_IS_ERROR(pjdfErr)) while(1);
  
  // Send initialization data to the MP3 decoder and run a test
  PrintWithBuf(buf, BUFSIZE, "Starting MP3 device test\n");
  Mp3Init(hMp3);
  int count = 0;
  
  // We are Halted By Default
  char *music_status = "Halting";
  
  // Notify DisplayTask of Messsage Queue, need to be extracted
  Read_Update = OS_TRUE;
  
  // "Halting" inserted for Name and Status To be Displayed  
  err = OSQPost(queueMusic, (void*)music_status); // Display Name
  err = OSQPost(queueMusic, (void*)music_status); // Display Music Status
  
  while (1)
  {
    
    if(haltPlayer)
    {
      OSTimeDly(300);
    }
    else
    {
      PrintWithBuf(buf, BUFSIZE, "Begin streaming sound file  count=%d\n", ++count);
      music_status = "Playing";
      
      Read_Update = OS_TRUE;
      err = OSQPost(queueMusic, (void*)"Train.Mp3"); // Display Name
      err = OSQPost(queueMusic, (void*) music_status ); // Display Music Status
      
     
      Mp3Stream(hMp3, (INT8U*)Train_Crossing, sizeof(Train_Crossing)); 
      PrintWithBuf(buf, BUFSIZE, "Done streaming sound file  count=%d\n", count);
    }
    
    OSTimeDly(100);
  }
}

*/

// Renders a character at the current cursor position on the LCD
static void PrintCharToLcd(char c)
{
  lcdCtrl.write(c);
}

/************************************************************************************

Print a formated string with the given buffer to LCD.
Each task should use its own buffer to prevent data corruption.

************************************************************************************/
void PrintToLcdWithBuf(char *buf, int size, char *format, ...)
{
  va_list args;
  va_start(args, format);
  PrintToDeviceWithBuf(PrintCharToLcd, buf, size, format, args);
  va_end(args);
}




