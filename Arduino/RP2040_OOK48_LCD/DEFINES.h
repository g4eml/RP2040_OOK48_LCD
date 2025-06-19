#define GPSTXPin 4                      //Serial data to GPS module 
#define GPSRXPin 5                      //Serial data from GPS module
#define TXPIN 6                         //Transmit output pin
#define KEYPIN 7                        //Key Output Pin
#define PPSINPUT 3                      //1 PPS signal from GPS 
#define ADC_CHAN 2                      //ADC2 is on GPIO Pin 28. Analogue input from Receiver. DC biased to half Supply rail.
#define ADC_VOLTS 3                     //ADC 3 is battery voltage/2

#define REPEAT_CAL false              // Set REPEAT_CAL to true instead of false to run calibration again, otherwise it will only be done once.
#define PLOTPOINTS 234                //Number of FFT points to display
#define SPECLOW 300.00                //spectrum display low frequency
#define SPECHIGH 2200.00              //spectrum display High Frequency

#define SPECLEFT 0                    //Spectrum Display Left Edge in Pixels
#define SPECTOP 0                     //Spectrum Display Top Edge in Pixels
#define SPECWIDTH PLOTPOINTS          //Spectrum Width in Pixels 
#define SPECHEIGHT 100                //Spectrum Height in Pixels

#define LEGLEFT 0                     //Legend for spectrum display
#define LEGTOP 100
#define LEGWIDTH PLOTPOINTS
#define LEGHEIGHT 10

#define WATERLEFT 0                   //Waterfall Display Left Edge in Pixels
#define WATERTOP 110                  //Waterfall Display Top Edge in Pixels
#define WATERWIDTH PLOTPOINTS         //Waterfall Disply Width in Pixels
#define WATERHEIGHT 160               //Waterfall Diaply Height in Pixels

#define TEXTLEFT PLOTPOINTS + 5      //left edge of text output area
#define TEXTTOP 0                    //top edge of text output area
#define TEXTWIDTH 241                //width of text output area
#define TEXTHEIGHT 270               //height of text output area

#define BUT1LEFT 5                  //Button locations
#define BUT1TOP 280
#define BUT1WIDTH 70
#define BUT1HEIGHT 40

#define BUT2LEFT 85                  //Button locations
#define BUT2TOP 280
#define BUT2WIDTH 70
#define BUT2HEIGHT 40

#define BUT3LEFT 165                  //Button locations
#define BUT3TOP 280
#define BUT3WIDTH 70
#define BUT3HEIGHT 40

#define BUT4LEFT 245                  //Button locations
#define BUT4TOP 280
#define BUT4WIDTH 70
#define BUT4HEIGHT 40

#define BUT5LEFT 325                  //Button locations
#define BUT5TOP 280
#define BUT5WIDTH 70
#define BUT5HEIGHT 40

#define BUT6LEFT 405                  //Button locations
#define BUT6TOP 280
#define BUT6WIDTH 70
#define BUT6HEIGHT 40



#define OVERSAMPLE 32                                           //multiple samples are averaged to reduce noise floor. 

#define NUMBEROFBINS 1024                                       // 1024 samples gives a scan rate of the bitrate
#define NUMBEROFSAMPLES NUMBEROFBINS * OVERSAMPLE              // ADC samples. will be averaged to number of Bins to reduce sampling noise.


//Detection Values

#define SAMPLERATE 9216 * OVERSAMPLE  //9216 samples per second with 1024 bins gives 9Hz sample rate and 9Hz bins. 
#define TONE800 89                     //tone 89 * 9 = 801 Hz
#define TONE1250 138                   //138*9 = 1242 (Centre of spectrumm screen)
#define TONETOLERANCE 11            //Tone tolerance 11 * 9 = +- 99Hz 
#define CACHESIZE 8                // 8 bits 
#define HZPERBIN 9                   //Hertz per bin. Used to generate displayed spectrum. 
#define SNBINS 277.00                //number of bins for 2.5Khz noise bandwidth

#define MAXTONETOLERANCE 105
//Tx constants

#define TXINTERVAL 111111           //9 symbols per second in microseconds


