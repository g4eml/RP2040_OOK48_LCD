// Keyboard start position, key sizes and spacing
#define BOARDX 0
#define BOARDWIDTH 480
#define BOARDHEIGHT 320
#define KEYB_X BOARDX + 25 // Centre of top left 
#define KEYB_Y 120
#define KEYB_W 33 // Width and height
#define KEYB_H 33
#define KEYB_SPACING_X 10 // X and Y gap
#define KEYB_SPACING_Y 10
#define KEYB_TEXTSIZE 1   // Font size multiplier

#define KB_FONT &FreeSansBold12pt7b    // Key label font 2

// Text display box size and location
#define TEXT_X BOARDX + 6
#define TEXT_Y 5
#define TEXT_W BOARDWIDTH - 12
#define TEXT_H 50

//Prompt Line
#define BPROMPT_X BOARDX + 10
#define BPROMPT_Y 65

// string length, buffer for storing it and character index
char textBuffer[33];
uint8_t textIndex = 0;


// Create  keys for the keyBOARD
char keybLabel[40][2] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
                        "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P",
                        "A", "S", "D", "F", "G", "H", "J", "K", "L", "/",
                        "Z", "X", "C", "V", "B", "N", "M", "+", "-", "."};

char keybSpecial[5][6] = {"SPACE", "CLR", "DEL", "<-", "LOC"};

// Invoke the TFT_eSPI button class and create all the button objects
TFT_eSPI_Button keyb[45];


//------------------------------------------------------------------------------------------
int getText(const char* prompt, char* st, int len) 
{

  textIndex = 0;
  textBuffer[0] = 0;

  if(len > 30) len = 30;

  strcpy(textBuffer,st);
  textIndex = strlen(textBuffer) -1;                      //ignore the CR character

  // Draw keyBOARD background
  tft.fillRect(BOARDX, 0, BOARDWIDTH, BOARDHEIGHT, TFT_DARKGREY);

  // Draw text display area and frame
  tft.fillRect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H, TFT_BLACK);
  tft.drawRect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H, TFT_WHITE);

  //Write Existing text value
  displaytext(TFT_CYAN);

  //write Prompt

  tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
  tft.setFreeFont(&FreeSans12pt7b);  // Choose a nice font that fits box
  // Draw the string, the value returned is the width in pixels
  tft.setTextColor(TFT_CYAN);
  tft.drawString(prompt, BPROMPT_X, BPROMPT_Y);

  drawKeyBoard();

  bool done = false;
  bool locEntered = false;

  while(!done)
  {
      // Pressed will be set true is there is a valid touch on the screen
      bool pressed = tft.getTouch(&t_x, &t_y);

      // / Check if any key coordinate boxes contain the touch coordinates
      for (uint8_t b = 0; b < 45; b++) 
      {
        if (pressed && keyb[b].contains(t_x, t_y)) 
        {
          keyb[b].press(true);  // tell the button it is pressed
        }
        else 
        {
          keyb[b].press(false);  // tell the button it is NOT pressed
        }
      }

      // Check if any key has changed state
      for (uint8_t b = 0; b < 45; b++) 
      {

       tft.setFreeFont(KB_FONT);

        if (keyb[b].justReleased()) keyb[b].drawButton();     // draw normal

        if (keyb[b].justPressed()) 
        {
          keyb[b].drawButton(true);  // draw invert

          // if a text button, append the relevant char to the textBuffer
          if(b < 40) 
          {
            if (textIndex < len) 
            {
              textBuffer[textIndex] = keybLabel[b][0];
              textIndex++;
              textBuffer[textIndex] = 0; // zero terminate
            }
          }

          if(b == 40)
          {
            if (textIndex < len) 
            {
              textBuffer[textIndex] = ' ';
              textIndex++;
              textBuffer[textIndex] = 0; // zero terminate
            }

          }

          // Del button, so delete last char
          if (b == 42) 
          {
            textBuffer[textIndex] = 0;
            if (textIndex > 0) 
            {
              textIndex--;
              textBuffer[textIndex] = 0;//' ';
            }
          }

        //Ent button
          if (b == 43) 
          {
            int t = textIndex;
            while(t < len)
              {
               textBuffer[t++] = 0;
              }
            done = true;
          }

          if ((b == 44) && (!locEntered))          //Locator (Only one allowed per message)
          {
            if (textIndex < len)
            {
            locEntered = true;
            textBuffer[textIndex++] = LOCTOKEN; // Place locator token in buffer
            textBuffer[textIndex] = 0 ; // Place null in buffer
            }
          }  

          if (b == 41)          //Clear
          {
            textIndex = 0; // Reset index to 0
            textBuffer[textIndex] = 0; // Place null in buffer
          }

          // Update the number display field
          displaytext(TFT_CYAN);

          delay(10); // UI debouncing
        }
      }
  } 

  int p = strlen(textBuffer);
  textBuffer[p]=13;
  textBuffer[p+1] = 0;
  strcpy(st , textBuffer);
  return textIndex;
}

void displaytext(int colour)
{
  char newstr[50];                             //allow space for Locator text expansion

    // check for Token and replace if found
    replaceToken(newstr,textBuffer, LOCTOKEN, "{LOC}");

    tft.setTextDatum(TL_DATUM);        // Use top left corner as text coord datum
    tft.setFreeFont(&FreeSans9pt7b);  // Choose a nice font that fits box
    // Draw the string, the value returned is the width in pixels
    tft.setTextColor(colour);
    int xwidth = tft.drawString(newstr, TEXT_X + 4, TEXT_Y + 12);

    // Now cover up the rest of the line up by drawing a black rectangle.  No flicker this way
    // but it will not work with italic or oblique fonts due to character overlap.
    tft.fillRect(TEXT_X + 4 + xwidth, TEXT_Y + 1, TEXT_W - xwidth - 5, TEXT_H - 2, TFT_BLACK);     
}



//------------------------------------------------------------------------------------------

void drawKeyBoard()
{
  // Draw the keys
  for (uint8_t row = 0; row < 4; row++) 
  {
    for (uint8_t col = 0; col < 10; col++) 
    {
      uint8_t b = col + row * 10;


    tft.setFreeFont(KB_FONT);

      keyb[b].initButton(&tft, KEYB_X + col * (KEYB_W + KEYB_SPACING_X),
                        KEYB_Y + row * (KEYB_H + KEYB_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEYB_W, KEYB_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keybLabel[b], KEYB_TEXTSIZE);
      keyb[b].drawButton();
    }
  }
// Draw the space key
      keyb[40].initButton(&tft, BOARDWIDTH/2,
                        KEYB_Y + 4 * (KEYB_H + KEYB_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEYB_W * 5, KEYB_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keybSpecial[0], KEYB_TEXTSIZE);
      keyb[40].drawButton();

// Draw the CLR key
      keyb[41].initButton(&tft, 80,
                        KEYB_Y + 4 * (KEYB_H + KEYB_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEYB_W * 2, KEYB_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keybSpecial[1], KEYB_TEXTSIZE);
      keyb[41].drawButton();

// Draw the DEL key
      keyb[42].initButton(&tft, 365,
                        KEYB_Y + 4 * (KEYB_H + KEYB_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEYB_W * 2, KEYB_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keybSpecial[2], KEYB_TEXTSIZE);
      keyb[42].drawButton();

//Draw the Ent Key
      keyb[43].initButton(&tft, 455,
                        KEYB_Y + 1 * (KEYB_H + KEYB_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEYB_W, KEYB_H *3, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keybSpecial[3], KEYB_TEXTSIZE);
      keyb[43].drawButton();

// Draw the LOC key
      keyb[44].initButton(&tft, 440,
                        KEYB_Y + 4 * (KEYB_H + KEYB_SPACING_Y), // x, y, w, h, outline, fill, text
                        KEYB_W * 2, KEYB_H, TFT_WHITE, TFT_BLUE, TFT_WHITE,
                        keybSpecial[4], KEYB_TEXTSIZE);
      keyb[44].drawButton();

}