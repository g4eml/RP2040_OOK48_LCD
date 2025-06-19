
// OOK48 Encoder and Decoder LCD version
// Colin Durbridge G4EML 2025


#include <hardware/dma.h>
#include <hardware/adc.h>
#include "hardware/irq.h"
#include "arduinoFFT.h"
#include <EEPROM.h>
#include <TFT_eSPI.h>                 // Hardware-specific library. Must be pre-configured for this display and touchscreen
#include "DEFINES.h"                  //include the defines for this project
#include "globals.h"                  //global variables
#include "float.h"

TFT_eSPI tft = TFT_eSPI();            // Invoke custom library
 
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, NUMBEROFBINS, SAMPLERATE);         //Declare FFT function

struct repeating_timer TxIntervalTimer;                   //repeating timer for Tx bit interval

//Run once on power up. Core 0 does the time critical work. Core 1 handles the GUI.  
void setup() 
{
    Serial.begin();                   //enable the debug serial port
    EEPROM.begin(1024);
    pinMode(PPSINPUT,INPUT);
    pinMode(KEYPIN,OUTPUT);
    digitalWrite(KEYPIN,0);
    pinMode(TXPIN,OUTPUT);
    digitalWrite(TXPIN,0);
    mode = RX;  
    RxInit();
    loadMsg();
    loadBaud();
    TxMessNo = 0;
    TxInit();
    attachInterrupt(PPSINPUT,ppsISR,RISING);
}

//Interrupt called every symbol time to update the Key output. 
bool TxIntervalInterrupt(struct repeating_timer *t)
{
  TxSymbol();
  return true;
}


//interrupt routine for 1 Pulse per second input
void ppsISR(void)
{
  PPSActive = 3;              //reset 3 second timeout for PPS signal
  if(mode == RX)
    {
      dma_stop();
      dma_handler();        //call dma handler to reset the DMA timing and restart the transfers
      dmaReady = 0;
      if((halfRate == false ) || (halfRate & (gpsSec & 0x01) ))
      {
        cachePoint = 0;        //Reset ready for the first symbol
      }
      else 
      {
        cachePoint = 8;        //Reset ready for the first symbol of the second character
      } 
    } 
  else 
    {
      cancel_repeating_timer(&TxIntervalTimer);                           //Stop the symbol timer if it is running. 
      add_repeating_timer_us(-TXINTERVAL,TxIntervalInterrupt,NULL,&TxIntervalTimer);    // re-start the Symbol timer
      TxSymbol();                       //send the first symbol
    }
 
}

//core 1 handles the GUI
void setup1()
{
  Serial2.setRX(GPSRXPin);              //Configure the GPIO pins for the GPS module
  Serial2.setTX(GPSTXPin);
  while(gpsBaud == 0)                   //wait for core zero to initialise the baud rate for GPS. 
   {
    delay(1);
   }
  Serial2.begin(gpsBaud);                        
  gpsPointer = 0;
  waterRow = 0;
  initGUI();                        //initialise the GUI screen
}


//Main Loop Core 0. Runs forever. Does most of the work.
void loop() 
{
  if(mode == RX)
   {
    RxTick();
   }
   else 
    {
     TxTick();
    }
}


//Core 1 handles the GUI. Including synchronising to GPS if available
void loop1()
{
  uint32_t command;
  char m[64];
  unsigned long inc;
 
    if((gpsSec != lastSec) | (millis() > lastTimeUpdate + 2000))
    {         
      showTime();                                   //display the time
      if(PPSActive >0) PPSActive--;                 //decrement the PPS active timeout. (rest by the next PPS pulse)
      lastSec = gpsSec;
      lastTimeUpdate = millis();
    }


  if(rp2040.fifo.pop_nb(&command))          //have we got something to process from core 0?
    {
      switch(command)
      {
        case GENPLOT:
        generatePlotData();
        break;
        case DRAWSPECTRUM:
        drawSpectrum();
        break;
        case DRAWWATERFALL:
        drawWaterfall();
        break;
        case REDLINE:
        markWaterfall(TFT_RED);
        break;
        case CYANLINE:
        markWaterfall(TFT_CYAN);
        break;
        case SHOWTONE0:
        showTone(0);
        break;
        case SHOWTONE1:
        showTone(1);
        break;
        case MESSAGE:
        textPrintChar(decoded,TFT_BLUE);                                 
        break;
        case TMESSAGE:
        textPrintChar(TxCharSent,TFT_RED);                               
        break;
        case ERROR:
        textPrintChar(decoded,TFT_ORANGE);                                           
        break;
      }
    }


  if((screenTouched()) && (noTouch))
    {
      processTouch();
    } 

  if(Serial2.available() > 0)           //data received from GPS module
      {
        while(Serial2.available() >0)
          {
            gpsCh=Serial2.read();
            if(gpsCh > 31) gpsBuffer[gpsPointer++] = gpsCh;
            if((gpsCh == 13) || (gpsPointer > 255))
              {
                gpsBuffer[gpsPointer] = 0;
                processNMEA();
                gpsPointer = 0;
              }
          }

      }
}


void processNMEA(void)
{
  float gpsTime;

 gpsActive = true;
 if(strstr(gpsBuffer , "RMC") != NULL)                         //is this the RMC sentence?
  {
    if(strstr(gpsBuffer , "A") != NULL)                       // is the data valid?
      {
       int p=strcspn(gpsBuffer , ",");                         // find the first comma
       gpsTime = strtof(gpsBuffer+p+1 , NULL);                 //copy the time to a floating point number
       gpsSec = int(gpsTime) % 100;
       gpsTime = gpsTime / 100;
       gpsMin = int(gpsTime) % 100; 
       gpsTime = gpsTime / 100;
       gpsHr = int(gpsTime) % 100;         
      }
    else
     {
       gpsSec = -1;                                            //GPS time not valid
       gpsMin = -1;
       gpsHr = -1;
     }
  }


}

void clearEEPROM(void)
{
  for(int i=0;i<1024;i++)
   {
    EEPROM.write(i,0);
    EEPROM.commit();
   }
}

void loadBaud(void)
{
  if(EEPROM.read(EEBAUDVALID) == 42)
   {
     EEPROM.get(EEBAUD,gpsBaud);
   }
  else 
   {
     if(autoBaud(9600))
      {
        gpsBaud = 9600;
        saveBaud();
      }
    else 
      {
        gpsBaud = 38400;
        saveBaud();
      }
   }
}

void clearBaud(void)
{
  EEPROM.put(EEBAUD,0);
  EEPROM.write(EEBAUDVALID,0);
  EEPROM.commit();
}

void saveBaud(void)
{
  EEPROM.put(EEBAUD,gpsBaud);
  EEPROM.write(EEBAUDVALID,42);
  EEPROM.commit();
}


void loadMsg(void)
{
  if(EEPROM.read(EEMSGVALID) == 173)
   {
     EEPROM.get(EEMSG,TxMessage);
   }
  else 
   {
     clearMsg();
   }
}

void clearMsg(void)
  {
    for(int i=0;i<10;i++)
     {
      strcpy(TxMessage[i] , "Empty\r"); 
     }  
  }

void saveMsg(void)
{
  EEPROM.put(EEMSG,TxMessage);
  EEPROM.write(EEMSGVALID,173);
  EEPROM.commit();
}

bool autoBaud(int rate)
{
  long baudTimer;
  char test[3];
  bool gotit;

  Serial2.begin(rate);                  //start GPS port comms
  baudTimer = millis();                    //make a note of the time
  gotit = false;
  while((millis() < baudTimer+2000) & (gotit == false))       //try 38400 for two seconds
    {
      if(Serial2.available())
       {
         test[0] = test[1];             //shift the previous chars up one
         test[1] = test[2];
         test[2]=Serial2.read();        //get the next char
         if((test[0] == 'R') & (test[1] == 'M') & (test[2] == 'C'))    //have we found the string 'RMC'?
          {
            gotit = true;
          }
       }
    }     
   Serial2.end();     
   return gotit;
}
