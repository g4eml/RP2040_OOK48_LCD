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
    touch_calibrate(1);
    clearMsg();
    clearBaud();
    Serial2.end();
    loadBaud();
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

void showTime(void)
{
  char t[20];
  char v[10];
  if((PPSActive > 0) & (gpsSec != -1))
   {
     sprintf(t,"   %02d:%02d:%02d      ",gpsHr,gpsMin,gpsSec);
   }
  else 
  {
     sprintf(t,"Waiting for GPS");
  }

  tft.fillRect(0,0,230,20,TFT_CYAN);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(t,0,0);
}


void textLine(void)
{
  if(textrow > 3)
  {
    tft.drawFastHLine(TEXTLEFT,textrow-3,TEXTWIDTH,TFT_BLUE);
  }
}

void drawButtons(void)
{

  tft.setTextColor(TFT_BLUE);
  tft.setTextDatum(TL_DATUM);
  tft.setFreeFont(&FreeSans9pt7b);

  tft.fillRect(BUT1LEFT, BUT1TOP, BUT1WIDTH, BUT1HEIGHT, TFT_CYAN);
  tft.drawRect(BUT1LEFT, BUT1TOP, BUT1WIDTH, BUT1HEIGHT, TFT_WHITE);
  tft.drawString("Clear",BUT1LEFT +8,BUT1TOP +3);
  tft.drawString("Screen",BUT1LEFT +5,BUT1TOP +23);

  tft.fillRect(BUT2LEFT, BUT2TOP, BUT2WIDTH, BUT2HEIGHT, TFT_CYAN);
  tft.drawRect(BUT2LEFT, BUT2TOP, BUT2WIDTH, BUT2HEIGHT, TFT_WHITE);
  tft.drawString("",BUT2LEFT +10,BUT2TOP +15);

  tft.fillRect(BUT3LEFT, BUT3TOP, BUT3WIDTH, BUT3HEIGHT, TFT_CYAN);
  tft.drawRect(BUT3LEFT, BUT3TOP, BUT3WIDTH, BUT3HEIGHT, TFT_WHITE);
  tft.drawString("",BUT3LEFT +10,BUT3TOP +15);

  drawBut4();

  tft.fillRect(BUT5LEFT, BUT5TOP, BUT5WIDTH, BUT5HEIGHT, TFT_CYAN);
  tft.drawRect(BUT5LEFT, BUT5TOP, BUT5WIDTH, BUT5HEIGHT, TFT_WHITE);
  tft.drawString("Set Tx",BUT5LEFT +8,BUT5TOP +3);
  tft.drawString("Text",BUT5LEFT +15,BUT5TOP +23);


  tft.fillRect(BUT6LEFT, BUT6TOP, BUT6WIDTH, BUT6HEIGHT, TFT_CYAN); 
  tft.drawRect(BUT6LEFT, BUT6TOP, BUT6WIDTH, BUT6HEIGHT, TFT_WHITE);
  tft.setFreeFont(&FreeSans18pt7b);
  tft.drawString("Tx",BUT6LEFT +10,BUT6TOP +5);
}

void drawBut4(void)
{
  tft.fillRect(BUT4LEFT, BUT4TOP, BUT4WIDTH, BUT4HEIGHT, TFT_BLACK);
  tft.drawRect(BUT4LEFT, BUT4TOP, BUT4WIDTH, BUT4HEIGHT, TFT_WHITE);
  if(halfRate)
   {
     tft.setTextColor(TFT_WHITE);
     tft.drawString("1S",BUT4LEFT +27,BUT4TOP +3);
     tft.setTextColor(TFT_GREEN);   
     tft.drawString("-> 2S",BUT4LEFT +5,BUT4TOP +23);
   }
  else 
   {
     tft.setTextColor(TFT_GREEN);
     tft.drawString("-> 1S",BUT4LEFT +5,BUT4TOP +3);
     tft.setTextColor(TFT_WHITE);   
     tft.drawString("2S",BUT4LEFT +27,BUT4TOP +23); 
   }

  tft.setTextColor(TFT_BLUE);
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
    bool pressed =tft.getTouch(&t_x, &t_y);
    return pressed;
  }
  else
  {
    noTouch = true;
    return false;
  }   
 }

 void processTouch(void)
 {
   if(touchZone(BUT1LEFT, BUT1TOP, BUT1WIDTH, BUT1HEIGHT) && noTouch)
    {
      noTouch = false;
      textClear();
      return;
    }


   if(touchZone(BUT4LEFT, BUT4TOP, BUT4WIDTH, BUT4HEIGHT) && noTouch)
    {
      noTouch = false;
      halfRate = !halfRate;
      if(halfRate) 
       {
        cacheSize = 16;
       }
      else 
       {
        cacheSize = 8;  
       }
      drawBut4();
      return;
    }

   if(touchZone(BUT5LEFT, BUT5TOP, BUT5WIDTH, BUT5HEIGHT) && noTouch && mode == RX)
    {
      TxMessNo = doMemPad();
      getText("Enter TX Message", settings.TxMessage[TxMessNo], 30);
      saveSettings();
      homeScreen();
      return;
    }

    if(touchZone(BUT6LEFT, BUT6TOP, BUT6WIDTH, BUT6HEIGHT) && noTouch)
    {
      noTouch = false;
      if(mode == RX)
       {
         mode = TX;
         TxInit();
         digitalWrite(TXPIN, 1);
         tft.setTextColor(TFT_BLUE);
         tft.fillRect(BUT6LEFT, BUT6TOP, BUT6WIDTH, BUT6HEIGHT, TFT_CYAN); 
         tft.drawRect(BUT6LEFT, BUT6TOP, BUT6WIDTH, BUT6HEIGHT, TFT_WHITE);
         tft.setFreeFont(&FreeSans18pt7b);
         tft.drawString("Rx",BUT6LEFT +10,BUT6TOP +5);
         tft.fillRect(BUT5LEFT, BUT5TOP, BUT5WIDTH, BUT5HEIGHT, TFT_CYAN);
         tft.drawRect(BUT5LEFT, BUT5TOP, BUT5WIDTH, BUT5HEIGHT, TFT_WHITE);
         displayTx();
         textPrintChar(13,TFT_RED);
         TxPointer = 0;
         TxBitPointer = 0;
       }
      else 
       {
         mode = RX;
         digitalWrite(KEYPIN, 0);
         digitalWrite(TXPIN, 0);
         cancel_repeating_timer(&TxIntervalTimer);
         tft.setTextColor(TFT_BLUE);
         tft.fillRect(BUT6LEFT, BUT6TOP, BUT6WIDTH, BUT6HEIGHT, TFT_CYAN); 
         tft.drawRect(BUT6LEFT, BUT6TOP, BUT6WIDTH, BUT6HEIGHT, TFT_WHITE);
        tft.setFreeFont(&FreeSans18pt7b);
         tft.drawString("Tx",BUT6LEFT +10,BUT6TOP +5);
         tft.fillRect(BUT5LEFT, BUT5TOP, BUT5WIDTH, BUT5HEIGHT, TFT_CYAN);
         tft.drawRect(BUT5LEFT, BUT5TOP, BUT5WIDTH, BUT5HEIGHT, TFT_WHITE);
         tft.setFreeFont(&FreeSans9pt7b);
         tft.drawString("Set Tx",BUT5LEFT +8,BUT5TOP +3);
         tft.drawString("Text",BUT5LEFT +15,BUT5TOP +23);
         clearSpectrum();
         drawLegend();
         waterRow = 0;
         textPrintChar(13,TFT_BLUE);
       }
      return;
    }


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

 bool touchZone(int x, int y, int w, int h) 
{
  return ((t_x > x) && (t_x < x + w) && (t_y > y) && (t_y < y + h));
}

void drawLegend(void)
{
  tft.fillRect(LEGLEFT,LEGTOP,LEGWIDTH,LEGHEIGHT, TFT_WHITE);
  tft.fillRect(toneLegend[0], LEGTOP, 1 + toneLegend[1] , LEGHEIGHT , TFT_ORANGE);
}

void showTone(uint8_t tone)
{
  static uint8_t lastTone = 0;

  if((tone != lastTone)&(mode == RX))
    {
      if(tone == 1)
      {
      tft.fillRect(toneLegend[0], LEGTOP,toneLegend[1], LEGHEIGHT , TFT_GREEN);
      }
      else 
      {
      tft.fillRect(toneLegend[0], LEGTOP,toneLegend[1], LEGHEIGHT , TFT_ORANGE);
      }
      lastTone = tone;     
    }
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
