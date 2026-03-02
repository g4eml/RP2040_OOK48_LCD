
#define AppLABEL_FONT &FreeSans18pt7b    // Button label font


// Create buttons
char AppLabel[4][30] = {"OOK48","Morse","JT4G Decoder","PI4 Decoder"};

// Invoke the TFT_eSPI button class and create all the button objects
TFT_eSPI_Button Appkey[4];

//------------------------------------------------------------------------------------------
int getApp(void) 
{

 int ap;
 
  tft.fillScreen(TFT_BLACK);        //Clear the TFT

  drawApp();

  bool done = false;
  while(!done)
  {
      // Pressed will be set true is there is a valid touch on the screen
      bool pressed = tft.getTouch(&t_x, &t_y);

      // / Check if any key coordinate boxes contain the touch coordinates
      for (uint8_t b = 0; b < 4; b++) 
      {
        if (pressed && Appkey[b].contains(t_x, t_y)) 
        {
          Appkey[b].press(true);  // tell the button it is pressed
        }
        else 
        {
          Appkey[b].press(false);  // tell the button it is NOT pressed
        }
      }

      // Check if any key has changed state
      for (uint8_t b = 0; b < 4; b++) 
      {

        tft.setFreeFont(AppLABEL_FONT);

        if (Appkey[b].justPressed()) 
        {
          ap=b;
          done = true;
          delay(10); // UI debouncing
        }
      }
  } 
  
  return ap;  
}


//------------------------------------------------------------------------------------------

void drawApp()
{
  char blank[2] = " ";
  uint16_t backcolour[4];
  // Draw the three buttons
     for(int i = 0;i<4;i++)
     {
      if(settings.app == i)
       {
        backcolour[i] = TFT_DARKGREEN;
       }
      else
       {
        backcolour[i] = TFT_BLUE;
       }
     }


      tft.setFreeFont(AppLABEL_FONT);

      Appkey[0].initButton(&tft, 240, 40,300,60, // x, y, w, h, outline, fill, text
                        TFT_WHITE, backcolour[0], TFT_WHITE,
                        blank, 1);
      Appkey[0].drawButton(0,AppLabel[0]);

      Appkey[1].initButton(&tft, 240, 110,300,60, // x, y, w, h, outline, fill, text
                        TFT_WHITE, backcolour[1], TFT_WHITE,
                        blank, 1);
      Appkey[1].drawButton(0,AppLabel[1]);

      Appkey[2].initButton(&tft, 240, 180,300,60, // x, y, w, h, outline, fill, text
                        TFT_WHITE, backcolour[2], TFT_WHITE,
                        blank, 1);
      Appkey[2].drawButton(0,AppLabel[2]);

      Appkey[3].initButton(&tft, 240, 250,300,60, // x, y, w, h, outline, fill, text
                        TFT_WHITE, backcolour[3], TFT_WHITE,
                        blank, 1);
      Appkey[3].drawButton(0,AppLabel[3]);
}

//------------------------------------------------------------------------------------------

