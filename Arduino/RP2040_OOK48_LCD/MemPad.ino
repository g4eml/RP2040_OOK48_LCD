// memory selection start position, key sizes and spacing
#define MEM_X 0
#define MEM_WIDTH 480
#define MEM_HEIGHT 320
#define MEMKEY_X MEM_X + 115 // Centre of top left 
#define MEMKEY_Y 30
#define MEMKEY_W 230 // Width and height
#define MEMKEY_H 50
#define MEMKEY_SPACING_X 20 // X and Y gap
#define MEMKEY_SPACING_Y 12

#define MEMKEY_TEXTSIZE 1
#define MEMLABEL_FONT &FreeSans9pt7b    // Button label font


// Create 10 keys for the keypad
char MEMkeyLabel[10][30];

// Invoke the TFT_eSPI button class and create all the button objects
TFT_eSPI_Button MEMkey[10];

//------------------------------------------------------------------------------------------
int doMemPad(void) 
{
  int ch=TxMessNo;
  char s[50];

  // Draw pad background
  tft.fillRect(MEM_X, 0, MEM_WIDTH, MEM_HEIGHT, TFT_DARKGREY);

  for(int i = 0;i<10;i++)
  {
//    strncpy(s,settings.TxMessage[i],30);

    replaceToken(s,settings.TxMessage[i], LOCTOKEN, "{LOC}");                     // check for Token and replace if found

    if(strlen(s)>20)
      {
        s[19]=' ';
        s[20]='.';
        s[21]='.';
        s[22]='.';
        s[23]=0;
      }
    sprintf(MEMkeyLabel[i],"%s",s);
  }
  drawMempad();

  bool done = false;
  while(!done)
  {
      // Pressed will be set true is there is a valid touch on the screen
      bool pressed = tft.getTouch(&t_x, &t_y);

      // / Check if any key coordinate boxes contain the touch coordinates
      for (uint8_t b = 0; b < 10; b++) 
      {
        if (pressed && MEMkey[b].contains(t_x, t_y)) 
        {
          MEMkey[b].press(true);  // tell the button it is pressed
        }
        else 
        {
          MEMkey[b].press(false);  // tell the button it is NOT pressed
        }
      }

      // Check if any key has changed state
      for (uint8_t b = 0; b < 10; b++) 
      {

        tft.setFreeFont(MEMLABEL_FONT);

        if (MEMkey[b].justPressed()) 
        {
          ch=b;
          done = true;
          delay(10); // UI debouncing
        }
      }
  } 
  
  return ch;  
}


//------------------------------------------------------------------------------------------

void drawMempad()
{
  // Draw the keys
  for (uint8_t col = 0; col < 2; col++) 
  {
    for (uint8_t row = 0; row < 5; row++) 
    {
      char blank[2] = " ";
      uint8_t b = col *5 + row;

      tft.setFreeFont(MEMLABEL_FONT);

      MEMkey[b].initButton(&tft, MEMKEY_X + col * (MEMKEY_W + MEMKEY_SPACING_X),
                        MEMKEY_Y + row * (MEMKEY_H + MEMKEY_SPACING_Y), // x, y, w, h, outline, fill, text
                        MEMKEY_W, MEMKEY_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        blank, MEMKEY_TEXTSIZE);
      MEMkey[b].drawButton(0,MEMkeyLabel[b]);
    }
  }
}

//------------------------------------------------------------------------------------------

