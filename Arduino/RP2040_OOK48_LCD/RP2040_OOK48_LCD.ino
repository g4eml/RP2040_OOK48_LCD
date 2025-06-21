
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
    loadSettings();
    pinMode(PPSINPUT,INPUT);
    pinMode(KEYPIN,OUTPUT);
    digitalWrite(KEYPIN,0);
    pinMode(TXPIN,OUTPUT);
    digitalWrite(TXPIN,0);
    mode = RX;  
    RxInit();
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
  while(settings.gpsBaud == 0)                   //wait for core zero to initialise the baud rate for GPS. 
   {
    delay(1);
   }
  Serial2.begin(settings.gpsBaud);                        
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
 if(RMCValid())                                               //is this a valid RMC sentence?
  {
    int p=strcspn(gpsBuffer , ",") +1;                        // find and skip the first comma
    p= p + strcspn(gpsBuffer+p , ",") + 1;                    // find and skip the second comma 
    if(gpsBuffer[p] == 'A')                                   // is the data valid?
      {
       p=strcspn(gpsBuffer , ",") +1;                         // find and skip the first comma again
       gpsTime = strtof(gpsBuffer+p , NULL);                  //copy the time to a floating point number
       gpsSec = int(gpsTime) % 100;
       gpsTime = gpsTime / 100;
       gpsMin = int(gpsTime) % 100; 
       gpsTime = gpsTime / 100;
       gpsHr = int(gpsTime) % 100;  

       p= p + strcspn(gpsBuffer+p , ",") + 1;                  // find and skip the second comma 
       p= p + strcspn(gpsBuffer+p , ",") + 1 ;                 // find and skip the third comma
       latitude = strtof(gpsBuffer+p , NULL);                  // copy the latitude value
       latitude = convertToDecimalDegrees(latitude);           // convert to ddd.ddd
       p = p + strcspn(gpsBuffer+p , ",") + 1;                 // find and skip the fourth comma  
       if(gpsBuffer[p] == 'S')  latitude = 0-latitude;         // adjust southerly Lats to be negative values                
       p = p + strcspn(gpsBuffer+p , ",") + 1;                 // find and skip the fifth comma      
       longitude = strtof(gpsBuffer+p , NULL);                 // copy the lpngitude value 
       longitude = convertToDecimalDegrees(longitude);         // convert to ddd.ddd   
       p = p + strcspn(gpsBuffer+p , ",") + 1;                 // find and skip the sixth comma  
       if(gpsBuffer[p] == 'W')  longitude = 0 - longitude;     // adjust easterly Longs to be negative values 
       convertToMaid();     
      }
    else
     {
       gpsSec = -1;                                            //GPS time not valid
       gpsMin = -1;
       gpsHr = -1;
       latitude = 0;
       longitude = 0;
       strcpy(qthLocator,"----------");
       qthLocator[settings.locatorLength] = '\0'; // Shorten Locator string
     }
  }


}

bool RMCValid(void)
{
  if((gpsBuffer[3] == 'R') && (gpsBuffer[4] == 'M') && (gpsBuffer[5] == 'C'))
   {
    return checksum(gpsBuffer);
   }
   else 
   {
    return false;
   }
}
// Converts dddmm.mmm format to decimal degrees (ddd.ddd)
double convertToDecimalDegrees(double dddmm_mmm) 
{
    int degrees = (int)(dddmm_mmm / 100);                 // Extract the degrees part
    double minutes = dddmm_mmm - (degrees * 100);         // Extract the minutes part
    double decimalDegrees = degrees + (minutes / 60.0);   // Convert minutes to degrees
    return decimalDegrees;
}

void convertToMaid(void)
{
      // convert longitude to Maidenhead

    float d = 180.0 + longitude;
    d = 0.5 * d;
    int ii = (int)(0.1 * d);
    qthLocator[0] = char(ii + 65);
    float rj = d - 10.0 * (float)ii;
    int j = (int)rj;
    qthLocator[2] = char(j + 48);
    float fpd = rj - (float)j;
    float rk = 24.0 * fpd;
    int k = (int)rk;
    qthLocator[4] = char(k + 65);
    fpd = rk - (float)(k);
    float rl = 10.0 * fpd;
    int l = (int)(rl);
    qthLocator[6] = char(l + 48);
    fpd = rl - (float)(l);
    float rm = 24.0 * fpd;
    int mm = (int)(rm);
    qthLocator[8] = char(mm + 65);
    //  convert latitude to Maidenhead
    d = 90.0 + latitude;
    ii = (int)(0.1 * d);
    qthLocator[1] = char(ii + 65);
    rj = d - 10. * (float)ii;
    j = (int)rj;
    qthLocator[3] = char(j + 48);
    fpd = rj - (float)j;
    rk = 24.0 * fpd;
    k = (int)rk;
    qthLocator[5] = char(k + 65);
    fpd = rk - (float)(k);
    rl = 10.0 * fpd;
    l = int(rl);
    qthLocator[7] = char(l + 48);
    fpd = rl - (float)(l);
    rm = 24.0 * fpd;
    mm = (int)(rm);
    qthLocator[9] = char(mm + 65);
    qthLocator[settings.locatorLength] = '\0'; // Shorten Locator string
}

void loadSettings(void)
{
  EEPROM.get(0,settings);             //read the settings structure

  if(settings.baudMagic != 42)
   {
     if(autoBaud(9600))
      {
        settings.gpsBaud = 9600;
        settings.baudMagic = 42;
        saveSettings();
      }
    else 
      {
        settings.gpsBaud = 38400;
        settings.baudMagic = 42;
        saveSettings();
      }
   }

   if(settings.messageMagic != 173)
   {
    for(int i=0;i<10;i++)
     {
      strcpy(settings.TxMessage[i] , "Empty\r"); 
     } 
    settings.messageMagic = 173;
    saveSettings(); 
   }

  if((settings.locatorLength <6) || (settings.locatorLength > 10))
   {
    settings.locatorLength = 8;
    saveSettings();
   }
}

void saveSettings(void)
{
  EEPROM.put(0,settings);
  EEPROM.commit();
}

void clearEEPROM(void)
{
  for(int i=0;i<1024;i++)
   {
    EEPROM.write(i,0);
    EEPROM.commit();
   }
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

//replaces a token with an expanded string. returns the result in new. 
void replaceToken(char * news, char * orig, char search, char * rep)
{
  int outp=0;
  for(int i=0 ; ;i++ )
    {
      if(orig[i] == search)
       {
         for(int q=0 ; ; q++)
          {
            if(rep[q] == 0)
             {
              break;
             }
            news[outp++] = rep[q];
          }
       }
      else 
       {
         news[outp++] = orig[i];
       }
       if(orig[i] == 0)
        {
          break;
        }
    }
}

bool checksum(const char *sentence) 
{
    if (sentence == NULL || sentence[0] != '$') 
    {
        return false;
    }

    const char *checksum_str = strchr(sentence, '*');
    if (checksum_str == NULL || strlen(checksum_str) < 3) 
    {
        return false;
    }

    unsigned char calculated_checksum = 0;
    for (const char *p = sentence + 1; p < checksum_str; ++p) 
    {
        calculated_checksum ^= (unsigned char)(*p);
    }

    unsigned int provided_checksum = 0;
    if (sscanf(checksum_str + 1, "%2x", &provided_checksum) != 1) 
    {
        return false;
    }

    return calculated_checksum == (unsigned char)provided_checksum;
}