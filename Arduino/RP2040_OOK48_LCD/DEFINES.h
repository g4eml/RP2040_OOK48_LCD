#define VERSION "Version 0.11"

#define GPSTXPin 4                      //Serial data to GPS module 
#define GPSRXPin 5                      //Serial data from GPS module
#define TXPIN 6                         //Transmit output pin
#define KEYPIN 7                        //Key Output Pin
#define PPSINPUT 3                      //1 PPS signal from GPS 
#define ADC_CHAN 2                      //ADC2 is on GPIO Pin 28. Analogue input from Receiver. DC biased to half Supply rail.
#define ADC_VOLTS 3                     //ADC 3 is battery voltage/2
#define SDCLK 10                        //SD card Clock
#define SDI 11                          //SD card data in
#define SDO 12                          //SD card data out
#define SDCS 22                         //SD card select

#define REPEAT_CAL false              // Set REPEAT_CAL to true instead of false to run calibration again, otherwise it will only be done once.

#define PIXELSPERBIN 3                //number of horizontal pixels per bin for spectrun and waterfall
#define SPECLEFT 0                    //Spectrum Display Left Edge in Pixels
#define SPECTOP 0                     //Spectrum Display Top Edge in Pixels
#define SPECWIDTH NUMBEROFBINS * PIXELSPERBIN          //Spectrum Width in Pixels 
#define SPECHEIGHT 100                //Spectrum Height in Pixels

#define LEGLEFT 0                     //Legend for spectrum display
#define LEGTOP 100
#define LEGWIDTH NUMBEROFBINS * PIXELSPERBIN
#define LEGHEIGHT 10

#define WATERLEFT 0                   //Waterfall Display Left Edge in Pixels
#define WATERTOP 110                  //Waterfall Display Top Edge in Pixels
#define WATERWIDTH NUMBEROFBINS * PIXELSPERBIN         //Waterfall Disply Width in Pixels
#define WATERHEIGHT 165               //Waterfall Diaply Height in Pixels

#define TEXTLEFT NUMBEROFBINS * PIXELSPERBIN +5                 //left edge of text output area
#define TEXTTOP 0                    //top edge of text output area
#define TEXTWIDTH 480-TEXTLEFT                //width of text output area
#define TEXTHEIGHT 275               //height of text output area

#define BUTSLEFT 0
#define BUTSTOP 280
#define BUTSWIDTH 480
#define BUTSHEIGHT 80

 
#define BUTWIDTH 70
#define BUTLEFT 5 + BUTWIDTH/2
#define BUTTOP 300
#define BUTHEIGHT 40
#define BUTGAP 10

#define BUTKEY_TEXTSIZE 1   // Font size multiplier

#define BUTLABEL_FONT &FreeSans9pt7b    // Button label font

// Setup/config selection start position, key sizes and spacing
#define CFG_X 0
#define CFG_WIDTH 480
#define CFG_HEIGHT 320
#define CFG_LINESPACING 20
#define CFG_TEXTLEFT 10
#define CFG_BUTTONSLEFT CFG_WIDTH/2
#define CFG_W 72 // Width and height
#define CFG_H 33
#define CFG_SPACING_X 10 // X and Y gap
#define CFG_SPACING_Y 20
#define CFG_TEXTSIZE 1   // Font size multiplier
#define CFG_NUMBEROFBUTTONS 13     //number of config buttons.

//Detection Values
#define OVERSAMPLE 8                                           //multiple samples are averaged to reduce noise floor. 
#define NUMBEROFSAMPLES 1024                                       // 1024 samples gives a scan rate of the bitrate
#define NUMBEROFOVERSAMPLES NUMBEROFSAMPLES * OVERSAMPLE              // ADC samples. will be averaged to number of Bins to reduce sampling noise.
#define SAMPLERATE 9216                                         //9216 samples per second with 1024 bins gives 9Hz sample rate and 9Hz bins. 
#define OVERSAMPLERATE SAMPLERATE * OVERSAMPLE         

#define BATCAL 587.0                                            //default battery calibration value. (can be reset in config menu)

#define STARTFREQ 495                                          //first frequency of interest (to nearest 9 Hz)
#define STARTBIN 55                                            // equivalent bin number from 512 FFT bins 
#define ENDFREQ 1098                                           //last frequency of interest (to nearest 9 Hz)

#define TONE800 34                                             // 800 Hz is the 34th bin between STARTFREQ and ENDFREQ

#define TONETOLERANCE 11                                        // 11 * 9  = 99Hz Tolerance

#define NUMBEROFBINS 68                                         //68 bins between STARTFREQ and ENDFREQ

#define CACHESIZE 8                                           // 8 bits per character

//Tx constants

#define TXINTERVAL 111111           //9 symbols per second in microseconds

#define LOCTOKEN 0x86


