

uint8_t ch;
TFT_eSPI_Button cfgKbd[CFG_NUMBEROFBUTTONS];

void configPage(void){
/*
Configuration items

1S/2S Timing
6/8/10 character QTH locator
Clear 'EEPROM' - allows new GPS and clears all messages

*/

char txt[10];
bool done = false;
bool cfgLoop = false;
  while(!cfgLoop)
  {
  drawCFGKbd();
  delay(50); // UI debouncing
    while(!done)
    {

        // Pressed will be set true is there is a valid touch on the screen
        bool pressed = tft.getTouch(&t_x, &t_y);
        // / Check if any key coordinate boxes contain the touch coordinates
        for (uint8_t b = 0; b < CFG_NUMBEROFBUTTONS; b++) 
        {
          if (pressed && cfgKbd[b].contains(t_x, t_y)) 
          {
            cfgKbd[b].press(true);  // tell the button it is pressed
          }
          else 
          {
            cfgKbd[b].press(false);  // tell the button it is NOT pressed
          }
        }

        // Check if any key has changed state
        for (uint8_t b = 0; b < CFG_NUMBEROFBUTTONS; b++) 
        {
          if (cfgKbd[b].justPressed()) 
          {
            ch=b;
            done = true;
          }
        }
    }

  switch(ch)
    {
    case 0:
      settings.locatorLength = 6;
      break;
    case 1:
      settings.locatorLength = 8;
      break;
    case 2:
      settings.locatorLength = 10;
      break;    
    case 3:
      halfRate = false;
      break;
    case 4:
      halfRate = true;
      break;
    case 5:
      settings.gpsBaud = 9600;
      settings.baudMagic = 42;
      Serial2.end();
      Serial2.begin(settings.gpsBaud);
      break;
    case 6:
      settings.gpsBaud = 38400;
      settings.baudMagic = 42;
      Serial2.end();
      Serial2.begin(settings.gpsBaud);
      break;

    case 7:
      settings.decodeMode = NORMALMODE;
      break;
    
    case 8:
      settings.decodeMode = ALTMODE;
      break;
    case 9:
      txt[0] = 32;
      txt[1] = 0;
      getText("Enter Tx Timing Advance in ms", txt,10);
      settings.txAdvance = atoi(txt);
      if(settings.txAdvance <0) settings.txAdvance = 0;
      if(settings.txAdvance >999) settings.txAdvance = 999;
      break;
    case 10:
      txt[0] = 32;
      txt[1] = 0;
      getText("Enter Rx Timing Retard in ms", txt,10);
      settings.rxRetard = atoi(txt);
      if(settings.rxRetard < 0) settings.rxRetard = 0;
      if(settings.rxRetard > 999) settings.rxRetard = 999;
      break;
    case 11:
      cfgLoop = true;
      break;
    }
    done = false;
  }
}


void drawCFGKbd(void){
char congfglabels[CFG_NUMBEROFBUTTONS][6]={"6", "8", "10", "1s", "2s", "9600", "38400", "Norm", "Alt", "","","EXIT"};
char txt[10];
int ypos;
uint16_t cfgTextcolour;

  // Draw pad background
  tft.fillRect(CFG_X, 0, CFG_WIDTH, CFG_HEIGHT, TFT_DARKGREY);


  // Line 1
  tft.setFreeFont(&FreeSans12pt7b);  // Font
  // Draw the string, the value returned is the width in pixels
  tft.setTextColor(TFT_CYAN);
  ypos=CFG_LINESPACING*0.5;
  tft.drawString("Set Locator length", CFG_TEXTLEFT, ypos);
  ypos=ypos + CFG_LINESPACING*2;
  tft.drawString("Character Period ", CFG_TEXTLEFT, ypos);
  ypos=ypos + CFG_LINESPACING*2;
  tft.drawString("GPS Baud Rate", CFG_TEXTLEFT, ypos);
  ypos=ypos + CFG_LINESPACING*2;
  tft.drawString("Decode Mode", CFG_TEXTLEFT, ypos);
  ypos=ypos + CFG_LINESPACING*2;
  tft.drawString("Tx Timing Advance                  ms", CFG_TEXTLEFT, ypos);
  ypos=ypos + CFG_LINESPACING*2;
  tft.drawString("Rx Timing Retard                     ms", CFG_TEXTLEFT, ypos);
  tft.setFreeFont(KB_FONT); 

  ypos=CFG_LINESPACING*0.5; 
   //Locator Buttons
      if (settings.locatorLength == 6) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[0].initButton(&tft, CFG_BUTTONSLEFT + CFG_W/2,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[0], CFG_TEXTSIZE);
      cfgKbd[0].drawButton(); 
      if (settings.locatorLength == 8) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[1].initButton(&tft, CFG_BUTTONSLEFT + CFG_W + CFG_W/2 + CFG_SPACING_X,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[1], CFG_TEXTSIZE);
      cfgKbd[1].drawButton(); 
      if (settings.locatorLength == 10) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[2].initButton(&tft, CFG_BUTTONSLEFT + CFG_W*2 + CFG_W/2 + 2*CFG_SPACING_X,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[2], CFG_TEXTSIZE);
      cfgKbd[2].drawButton(); 
  ypos=ypos + CFG_LINESPACING*2;
// Character Period Buttons
      if (!halfRate) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[3].initButton(&tft, CFG_BUTTONSLEFT + CFG_W/2,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[3], CFG_TEXTSIZE);
      cfgKbd[3].drawButton(); 
      if (halfRate) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[4].initButton(&tft, CFG_BUTTONSLEFT + CFG_W + CFG_W/2 + CFG_SPACING_X,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[4], CFG_TEXTSIZE);
      cfgKbd[4].drawButton(); 
  ypos=ypos + CFG_LINESPACING*2;
// GPS Baud Rate Buttons
      if (settings.gpsBaud == 9600) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[5].initButton(&tft, CFG_BUTTONSLEFT + CFG_W/2,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[5], CFG_TEXTSIZE);
      cfgKbd[5].drawButton(); 
      if (settings.gpsBaud == 38400) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[6].initButton(&tft, CFG_BUTTONSLEFT + CFG_W + CFG_W/2 + CFG_SPACING_X,
                          ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[6], CFG_TEXTSIZE);
      cfgKbd[6].drawButton(); 
  ypos=ypos + CFG_LINESPACING*2;
// Decode Mode Buttons
      if (settings.decodeMode == NORMALMODE) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[7].initButton(&tft, CFG_BUTTONSLEFT + CFG_W/2,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[7], CFG_TEXTSIZE);
      cfgKbd[7].drawButton(); 
      if (settings.decodeMode == ALTMODE) cfgTextcolour = TFT_GREEN; else cfgTextcolour = TFT_WHITE;
      cfgKbd[8].initButton(&tft, CFG_BUTTONSLEFT + CFG_W + CFG_W/2 + CFG_SPACING_X,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[8], CFG_TEXTSIZE);
      cfgKbd[8].drawButton();

// Tx Advance Button
      ypos=ypos + CFG_LINESPACING*2;
      cfgTextcolour = TFT_WHITE;
      cfgKbd[9].initButton(&tft, CFG_BUTTONSLEFT + CFG_W/2,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[9], CFG_TEXTSIZE);
      sprintf(txt,"%d",settings.txAdvance);
      cfgKbd[9].drawButton(false,txt); 

// Rx Retard Buttons
      ypos=ypos + CFG_LINESPACING*2;
      cfgTextcolour = TFT_WHITE;
      cfgKbd[10].initButton(&tft, CFG_BUTTONSLEFT + CFG_W/2,
                        ypos + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, cfgTextcolour,
                        congfglabels[10], CFG_TEXTSIZE);
      sprintf(txt,"%d",settings.rxRetard);      
      cfgKbd[10].drawButton(false,txt);

// Exit Button
      cfgKbd[11].initButton(&tft, CFG_WIDTH - (CFG_W),
                        CFG_LINESPACING*14 + CFG_LINESPACING/2, // x, y, w, h, outline, fill, text
                        CFG_W, CFG_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        congfglabels[11],  CFG_TEXTSIZE);
      cfgKbd[11].drawButton(); 
}
