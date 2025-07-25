//This file contains the functions for the Graphical User Interface

void initGUI(void)
{
  tft.init();                       //initialise the TFT display hardware
  tft.setRotation(1);               //Set the TFT to Landscape
  tft.fillScreen(TFT_BLACK);        //Clear the TFT
  if(screenTouched())
   {
    while(screenTouched());
    delay(500);
    Serial2.end();
    clearEEPROM();
    touch_calibrate(1);
    loadSettings();
    Serial2.begin(settings.gpsBaud);
   }
   else
   {
    touch_calibrate(0);
   }
  homeScreen();
}

void homeScreen(void)
{
  tft.fillScreen(TFT_BLACK);        //Clear the TFT
  drawButtons();
  clearSpectrum();
  drawLegend();                     //draw spectrum legend
  textClear();
}

//clear the spectrum and waterfall areas of the screen
void clearSpectrum(void)
{
  tft.fillRect(SPECLEFT,SPECHEIGHT,SPECWIDTH, WATERHEIGHT + LEGHEIGHT,TFT_BLACK);   //Create Black background for the Waterfall
  tft.fillRect(SPECLEFT,SPECTOP,SPECWIDTH,SPECHEIGHT,TFT_CYAN);   //Create background for the Spectrum Display
}

//replace the Spectrum and Waterfall with a large red TX indication
void displayTx(void)
{
  tft.fillRect(SPECLEFT,SPECTOP,SPECWIDTH, SPECHEIGHT + WATERHEIGHT + LEGHEIGHT,TFT_RED);   //Create Black background for the Waterfall
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.drawString("TX",SPECLEFT + (SPECWIDTH)/2 -40,SPECTOP + SPECHEIGHT);
}

//Add a line to the Waterfall Display
void drawWaterfall(void)
{
  if(mode == RX)
  {
    if(waterRow < WATERHEIGHT-1) tft.drawFastHLine(WATERLEFT,WATERTOP + waterRow + 1,WATERWIDTH,TFT_WHITE);       //Draw White line acrost the Waterfall to highlight the current position.
    for(int p=0 ; p < PLOTPOINTS ; p++)                                              //for each of the data points in the current row
    {
      tft.drawPixel(WATERLEFT + p,WATERTOP + waterRow, waterColours[(plotData[p] + 10) *2]);             //draw a pixel of the required colour
    } 
    waterRow++;                                                                      //Increment the row for next time
    if(waterRow >= WATERHEIGHT) waterRow = 0;                                        //Cycle back to the start at the end of the display. (would be nice to scroll the display but this is too slow)
    }
}

void markWaterfall(unsigned int col)
{
  if(mode == RX)
  {
  tft.drawFastHLine(WATERLEFT,WATERTOP + waterRow -1,WATERWIDTH,col); 
  }
}

//Draw the Spectrum Display
void drawSpectrum(void)
{
  if(mode == RX)
  {
    for(int p=1 ; p < PLOTPOINTS ; p++)                                             //for each of the data points in the current row
      {
        tft.drawLine(SPECLEFT + p - 1, SPECTOP + SPECHEIGHT - lastplotData[p-1], SPECLEFT + p, SPECTOP + SPECHEIGHT - lastplotData[p], TFT_CYAN);   //erase previous plot
        tft.drawLine(SPECLEFT + p - 1, SPECTOP + SPECHEIGHT - plotData[p-1], SPECLEFT + p, SPECTOP + SPECHEIGHT - plotData[p], TFT_RED);            //draw new plot
      }
    memcpy(lastplotData , plotData, PLOTPOINTS);       //need to save this plot so that we can erase it next time (faster than clearing the screen)
  }  
}

void textClear(void)
{
  tft.fillRect(TEXTLEFT, TEXTTOP, TEXTWIDTH, TEXTHEIGHT, TFT_WHITE);
  textrow = tft.fontHeight();
  textcol = 0;
}

void textPrintLine(const char* message)
{
 if(textrow > (TEXTTOP + TEXTHEIGHT - tft.fontHeight()))
    {
      textClear();
    }
  tft.setTextColor(TFT_BLUE);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(message,TEXTLEFT,TEXTTOP+textrow);
  textrow=textrow + tft.fontHeight();
}

void textPrintChar(char m, uint16_t col)
{
  if((sdpresent) & (sdfile))
   {
    sdfile.write(&m,1);
   }

 if(textrow > (TEXTTOP + TEXTHEIGHT - tft.fontHeight()))
    {
      textClear();
    }
  tft.setTextColor(col);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextDatum(TL_DATUM);
  if((m == 13)|(m == 10))
   {
     textrow=textrow + tft.fontHeight();
     textcol = 0;
   }
   else 
   {
     int16_t w = tft.drawChar(m,TEXTLEFT + textcol,TEXTTOP+textrow);
     textcol=textcol+w;
     if(textcol > (TEXTWIDTH - w))
       {
         textrow=textrow + tft.fontHeight();
         textcol = 0; 
       }
   }
  
  

}

void fileprintChar(char m)
{

}

void showTime(void)
{
  char t[20];
  char q[12];
  tft.setTextSize(1);
  if((PPSActive > 0) & (gpsSec != -1))
   {
     sprintf(t," %02d:%02d:%02d      ",gpsHr,gpsMin,gpsSec);
   }
  else 
  {
     sprintf(t,"Waiting for GPS");
  }

  sprintf(q,"%10s",qthLocator);


  tft.fillRect(0,0,230,20,TFT_CYAN);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(t,0,0);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(q,228,0);
  tft.setTextDatum(TL_DATUM);  
}


void textLine(void)
{
  if(textrow > 3)
  {
    tft.drawFastHLine(TEXTLEFT,textrow-3,TEXTWIDTH,TFT_BLUE);
  }
}

// Create 6 Buttons
char BUTLabel[6][10] = {"Clear","Config","","","Set Tx","Tx"};

// Invoke the TFT_eSPI button class and create all the  objects
TFT_eSPI_Button BUTkey[6];

void drawButtons(void)
{
  tft.fillRect(BUTSLEFT,BUTSTOP,BUTSWIDTH,BUTSHEIGHT,TFT_BLACK);

// Draw the keys
  int tsz;
  for (uint8_t i= 0; i< 6; i++) 
  {
      char blank[2] = " ";

      tft.setFreeFont(BUTLABEL_FONT);

      if(i == 5)
       {
        tsz=2;
       }
      else 
       {
         tsz=1;
       }

      BUTkey[i].initButton(&tft, BUTLEFT + i * (BUTWIDTH + BUTGAP),BUTTOP, 
                        BUTWIDTH, BUTHEIGHT, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        blank, tsz);
      BUTkey[i].drawButton(0,BUTLabel[i]);
  }

 if(sdpresent)
  {
    if(sdfile) 
     {
       stopButton();
     }
    else
     {
       recButton();
     }
  }

}

void touch_calibrate(bool force)
{

  if (settings.calMagic == 0x0A && !REPEAT_CAL && !force)
  {
    // calibration data valid
    tft.setTouch(settings.calData);
  } 
  else 
  {
    // data not valid so recalibrate
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(20, 0);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);

    tft.println("Touch corners as indicated");

    tft.setTextFont(1);
    tft.println();

    if (REPEAT_CAL) 
    {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("Set REPEAT_CAL to false to stop this running again!");
    }

    tft.calibrateTouch(settings.calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.println("Calibration complete!");
    settings.calMagic = 0x0A;
    saveSettings();
  }

}

bool screenTouched(void)
 {
  uint16_t raw = tft.getTouchRawZ();
  if(raw > 1000)
  {
    if(noTouch == false) return false;
    bool pressed =tft.getTouch(&t_x, &t_y);
    return pressed;
  }
  else
  {
    for(int i=0;i < 6;i++)
        {
          BUTkey[i].press(false);  // tell the buttons they are NOT pressed
        }
    noTouch = true;
    return false;
  }   
 }


void processTouch(void)
{
  int butPressed = -1;
  char fname[16];
// Check if any key coordinate boxes contain the touch coordinates
      for (uint8_t b = 0; b < 6; b++) 
      {
        if (BUTkey[b].contains(t_x, t_y)) 
        {
          BUTkey[b].press(true);  // tell the button it is pressed
        }
      }

     // Check if any key has changed state
      for (uint8_t b = 0; b < 6; b++) 
      {
        if (BUTkey[b].justPressed()) 
        {
          butPressed = b;
        }
      }
      
 if(butPressed >=0)
 {
    switch(butPressed)
    {
      case 0:
      noTouch = false;
      textClear();
      break;
      
      case 1:
      noTouch = false;
      mode = RX;
      digitalWrite(KEYPIN, 0);
      digitalWrite(TXPIN, 0);
      configPage();
      saveSettings();
      homeScreen();
      break;

      case 2:
      if(sdpresent)
       {
        if(sdfile)                 //is the SD file Open?
         {
           sdfile.close();        //close it
           recButton();
         }
       else
         {
          if(gpsHr !=-1)
          {
           sprintf(fname,"%02d_%02d_%02d.txt",gpsHr,gpsMin,gpsSec);
           sdfile.open(fname,FILE_WRITE);
           if(sdfile) stopButton(); 
          }
 
         }
       }
      noTouch = false;
      break;

      case 3:
      noTouch = false;
      break;

      case 4:
      noTouch = false;
      messageChanging = true;
      TxMessNo = doMemPad();
      getText("Enter TX Message", settings.TxMessage[TxMessNo], 30);
      saveSettings();
      homeScreen();
      if(mode == TX)
       {
         mode = RX;
         digitalWrite(KEYPIN, 0);
         cancel_repeating_timer(&TxIntervalTimer);
         mode = TX;
         TxInit();
         BUTkey[5].drawButton(0,"Rx");
         displayTx();        
       }
       messageChanging = false;
      break;

      case 5:
      noTouch = false;
      if(mode == RX)
       {
         mode = TX;
         TxInit();
         digitalWrite(TXPIN, 1);
         BUTkey[5].drawButton(0,"Rx");
         displayTx();

         TxPointer = 0;
         TxBitPointer = 0;
       }
       else 
       {
         mode = RX;
         digitalWrite(KEYPIN, 0);
         digitalWrite(TXPIN, 0);
         cancel_repeating_timer(&TxIntervalTimer);
         BUTkey[5].drawButton(0,"Tx");
         BUTkey[4].drawButton(0,"Set Tx");
         clearSpectrum();
         drawLegend();
         waterRow = 0;
         textPrintChar(13,TFT_BLUE);
       }
      break;
    }
 }
 else
 {
  if(touchZone(SPECLEFT, SPECTOP, SPECWIDTH/2, SPECHEIGHT)&& noTouch)
    {
      noTouch = false;
      autolevel = false;
      return;
    }

   if(touchZone(SPECLEFT + SPECWIDTH/2, SPECTOP, SPECWIDTH/2, SPECHEIGHT)&& noTouch)
    {
      noTouch = false;
      autolevel = true;
      return;
    }

   if(touchZone(WATERLEFT, WATERTOP, WATERWIDTH, WATERHEIGHT)&& noTouch)
    {
      noTouch = false;
      switch(toneTolerance)
      {
        case 5:
        rxTone = TONE800;
        toneTolerance = 11;
        break;       
        case 11:
        rxTone = TONE800;
        toneTolerance = 28;
        break;
        case 28:
        rxTone = TONE800;
        toneTolerance = 55;
        break;
        case 55:
        rxTone = TONE1250;
        toneTolerance = MAXTONETOLERANCE;
        break;
        case 105:
        rxTone = TONE800;
        toneTolerance = 5;
        break;       
      }
      calcLegend();
      drawLegend();
      return;
    }
 }


}


 bool touchZone(int x, int y, int w, int h) 
{
  return ((t_x > x) && (t_x < x + w) && (t_y > y) && (t_y < y + h));
}

void drawLegend(void)
{
  tft.fillRect(LEGLEFT,LEGTOP,LEGWIDTH,LEGHEIGHT, TFT_WHITE);
  tft.fillRect(toneLegend[0], LEGTOP, 1 + toneLegend[1] , LEGHEIGHT , TFT_ORANGE);
}

void calcLegend(void)
{
    int point;
    int width;
    float freq;
    float plotIncrement;
    plotIncrement = (SPECHIGH - SPECLOW) / (float) PLOTPOINTS;

    freq = hzPerBin * ((float) rxTone - (float)toneTolerance);
    freq = freq - SPECLOW;
    point = freq / plotIncrement;
    toneLegend[0] = point;
    freq = hzPerBin * ((float) rxTone  + (float)toneTolerance);
    freq = freq - SPECLOW;
    point = (freq / plotIncrement) - point;
    toneLegend[1] = point;
}

void stopButton(void)
{
    tft.fillRect(BUTLEFT +2 * (BUTWIDTH + BUTGAP)-10,BUTTOP-10, 20, 20, TFT_WHITE);
}


void recButton(void)
{
  tft.fillRect(BUTLEFT +2 * (BUTWIDTH + BUTGAP)-10,BUTTOP-10, 20, 20, TFT_BLUE);
  tft.fillCircle(BUTLEFT +2 * (BUTWIDTH + BUTGAP), BUTTOP , 10 , TFT_RED);
}
