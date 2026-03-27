#define VERSION "Version 0.31"

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

#define SPECLEFT 0                    //Spectrum Display Left Edge in Pixels
#define SPECTOP 0                     //Spectrum Display Top Edge in Pixels
#define SPECWIDTH 204                 //Spectrum Width in Pixels 
#define SPECHEIGHT 100                //Spectrum Height in Pixels

#define LEGLEFT 0                     //Legend for spectrum display
#define LEGTOP 100
#define LEGWIDTH 204 
#define LEGHEIGHT 10

#define WATERLEFT 0                   //Waterfall Display Left Edge in Pixels
#define WATERTOP 110                  //Waterfall Display Top Edge in Pixels
#define WATERWIDTH 204                //Waterfall Disply Width in Pixels
#define WATERHEIGHT 165               //Waterfall Diaply Height in Pixels

#define TEXTLEFT 209                 //left edge of text output area
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

// OOK48 Spectrum display from 495Hz to 1098 Hz     Tone freq = 800 Hz
//Detection Values
#define OVERSAMPLE 8                                           //multiple samples are averaged to reduce noise floor. 
#define NUMBEROFSAMPLES 1024                                       // 1024 samples gives a scan rate of the bitrate
#define NUMBEROFOVERSAMPLES NUMBEROFSAMPLES * OVERSAMPLE              // ADC samples. will be averaged to number of Bins to reduce sampling noise.
#define SAMPLERATE 9216                                         //9216 samples per second with 1024 bins gives 9Hz sample rate and 9Hz bins. 
#define OVERSAMPLERATE SAMPLERATE * OVERSAMPLE         

#define BATCAL 587.0                                            //default battery calibration value. (can be reset in config menu)

#define STARTFREQ 495                                          //first frequency of interest (to nearest 9 Hz)
#define OOKSTARTBIN 55                                            // equivalent bin number from 512 FFT bins 
#define ENDFREQ 1098                                           //last frequency of interest (to nearest 9 Hz)

#define TONE800 34                                             // 800 Hz is the 34th bin between STARTFREQ and ENDFREQ

#define TONETOLERANCE 11                                        // 11 * 9  = 99Hz Tolerance

#define OOKNUMBEROFBINS 68                                         //68 bins between STARTFREQ and ENDFREQ

#define CACHESIZE 8                                           // 8 bits per character

//OOK48 Tx constants

#define TXINTERVAL 111111           //9 symbols per second in microseconds

#define LOCTOKEN 0x86


// Beacon Decoder Defines
//JT4G Detection Values
// Spectrum display from 498Hz to 1999 Hz  Tones 800 1115 1430 1745

#define JT4SAMPLERATE 4480              //4480 samples per second * oversample. FFT Bandwidth of 0-2240 Hz at 4.375 Hz
#define JT4OVERSAMPLERATE JT4SAMPLERATE * OVERSAMPLE

 
#define JT4CACHESIZE 240                 // 240 tone decode samples is approx 55 Seconds
#define JT4SYMBOLCOUNT 207              //number of symbols in JT4 message
#define JT4BITCOUNT 206                 //number of bits in JT4 Message
#define JT4HZPERBIN 4.375              //Hertz per bin. Used to generate displayed spectrum. 
#define JT4SNBINS 571.00                //number of bins for 2.5Khz noise bandwidth

#define JT4STARTFREQ 498                                          //first frequency of interest (to nearest 4.375 Hz)
#define JT4STARTBIN 114                                            // equivalent bin number from 512 FFT bins 
#define JT4ENDFREQ 1999                                           //last frequency of interest (to nearest 9 Hz)
#define JT4TONE0 69                                              // 800 Hz is the 69th bin between STARTFREQ and ENDFREQ
#define JT4TONESPACING 72                                         //tone spacing in number of bins. 315 / 4.375 = 72
#define JT4TONETOLERANCE 22                                        //  22 * 4.375 = +- 96Hz
#define JT4NUMBEROFBINS 343                                        //343 bins between STARTFREQ and ENDFREQ


//PI4 Detection Values
// Spectrum display from 498Hz to 1500 Hz  Tones 683 917 1151 1385

#define PI4SAMPLERATE 6144                             //6144 samples per second * oversample FFT Bandwidth of 0-3072 Hz at 6 Hz
#define PI4OVERSAMPLERATE PI4SAMPLERATE * OVERSAMPLE
 
#define PI4CACHESIZE 180                // 180 tone decode samples is approx 30 Seconds
#define PI4SYMBOLCOUNT 146               //number of symbols in PI4 message
#define PI4BITCOUNT 146                  //number of bits in PI4 Message
#define PI4HZPERBIN 6                   //Hertz per bin. Used to generate displayed spectrum. 
#define PI4SNBINS 416.00                //number of bins for 2.5Khz noise bandwidth

#define PI4STARTFREQ 498                                          //first frequency of interest (to nearest 6 Hz)
#define PI4STARTBIN 83                                            // equivalent bin number from 512 FFT bins 
#define PI4ENDFREQ 1500                                           //last frequency of interest (to nearest 6 Hz)
#define PI4TONE0 31                                               // 683Hz is the 31st bin between STARTFREQ and ENDFREQ
#define PI4TONESPACING 39                                         //tone spacing in number of bins. 234 / 6 = 39 
#define PI4TONETOLERANCE 12                                        //Tone tolerance 12 * 6 = +- 72Hz 
#define PI4NUMBEROFBINS 167                                        //167 bins between STARTFREQ and ENDFREQ

// Morse RX mode FFT / DMA parameters
// 256-pt FFT at 9216 Hz effective → 36 Hz/bin, 36 fps
//spectrum plot is 288Hz to 3024 Hz in 36Hz Bins
#define MORSE_FFT_SIZE        256
#define MORSE_FRAME_SAMPLES   (MORSE_FFT_SIZE * OVERSAMPLE)   // 2048 ADC samples/frame
#define MORSE_FRAME_RATE      36
#define MORSESTARTFREQ        288                             //Lowest frequency we are interested in to nearest 36 Hz
#define MORSEENDFREQ          1512                             //Highest frequency we are interested in     
#define MORSESTARTBIN         8                             //corresponding bin number
#define MORSE_TONE_BIN        14                              // 800 Hz / 36 Hz per bin ≈ bin 22 - start bin =14
#define MORSE_TONETOLERANCE    1
#define MORSE_FFT_BINS        34                             // End bin - start bin 
#define MORSE_WF_FRAMES       4     // accumulate N frames before waterfall send (~9/sec)
#define MORSE_DEFAULT_WPM   12
#define MORSE_MIN_WPM       6
#define MORSE_MAX_WPM       30

#define MORSETEXTHEIGHT 230               //height of text output area when in morse mode.