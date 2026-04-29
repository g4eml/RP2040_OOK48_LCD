#include "morse_rx.h"
//Global variables

//EEPROM Structure. This structure is written to the EEPROM. All members are therefore non-volatile. 

struct eepromstruct
{
  uint8_t calMagic;               //calibration magic value to indicate validity
  uint16_t calData[5];            //screen calibration data
  uint8_t baudMagic;              //baud rate magic value to indicate validity
  uint16_t gpsBaud;               //GPS baud rate
  uint8_t messageMagic;          //message magic value to indicate validity
  char TxMessage[10][32];         //message storage
  uint8_t locatorLength;          //Default QTH Locator
  uint8_t decodeMode;             //allow multiple decode modes
  uint16_t txAdvance;             //Tx timing advance in ms
  uint16_t rxRetard;              //Rx Timing retard in ms
  float batcal;                   //battery voltage calibration factor
  uint8_t app;                    //currently selected application OOK48 JT4G or PI4
  uint8_t  morseWpm;              //Morse WPM

};

struct eepromstruct settings;

enum decodemodes {NORMALMODE,ALTMODE};

enum core1Message {GENPLOT,DRAWSPECTRUM,DRAWWATERFALL,REDLINE,CYANLINE,MESSAGE,TMESSAGE,JTMESSAGE,PIMESSAGE,MORSEMESSAGE,MORSELOCKED,MORSELOST};         //messages for control of Core 1 from Core 2


uint16_t dma_chan;                        //DMA Channel Number
bool dmaReady;                        //Flag to indicate a DMA buffer is ready to be processed.
uint8_t bufIndex = 0;                 //Index to the current DMA buffer. Alternates 0/1.

enum apps {OOK48,MORSE,BEACONJT4,BEACONPI4};

uint8_t mode;
enum modes {RX,TX};

uint8_t beaconMode;
enum bmodes {JT4,PI4};

uint32_t sampleRate;                  //samples per second.
uint16_t rxToneBin;                      //tone in bins. 
uint16_t toneTolerance;             //Tone tolerance in bins. 
uint16_t cacheSize;                 // tone decode samples.
float hzPerBin;
uint16_t activeBins;
int overlap = 1;
int numberOfTones =1;
int numberOfBins;
int startBin;

//Beacon mode variables
uint16_t toneSpacing;                //tone spacing in number of bins.
uint16_t tone0;                      //tone zero in bins.
uint16_t symbolCount;               //number of symbols message.
uint16_t bitCount;                  //number of bits in Message
uint8_t BeaconToneCache[JT4CACHESIZE];         // Array for tone cache (JT4 is larger than PI4 so we will use that for both.)
char JTmessage[14];                     //decoded JT4 Message
char PImessage[9];                      //decoded PI4 Message

float sigNoise;
float snBins;
float threshold;
float toneCache[JT4NUMBEROFBINS][CACHESIZE *2];          // Array large enough for the biggest tone magnitude cache
uint16_t cachePoint;                  // Pointer to next cache entry. 
bool halfRate = false;

char decoded;                         //decoded  Message character

char gpsBuffer[256];                     //GPS data buffer
int gpsPointer;                          //GPS buffer pointer. 
char gpsCh;
bool GPSDebug = false;
bool gpsActive = false; 
int lastSec = 0;
int gpsSec = -1;                       //GPS clock time  -1 for GPS Invalid
int gpsMin = -1;
int gpsHr = -1;
int gpsDay= -1;
int gpsMonth = -1;
int gpsYear = -1;
uint8_t PPSActive = 0;
long lastTimeUpdate = 0;
long lastmin;                         // Timing variable
float latitude = 0;
float longitude =0;
char qthLocator[12] = "----------";

uint16_t buffer[2][NUMBEROFOVERSAMPLES];     //2 DMA buffers to allow one to be processed while the next is being received.
float sample[NUMBEROFSAMPLES];              //array for the averaged samples 
float sampleI[NUMBEROFSAMPLES];             //imaginary part for FFT
float magnitude[JT4NUMBEROFBINS];            //Array for signal spectrum
uint8_t  audioLevel;    // RX audio level 0-100 (peak-smoothed, for volume guidance)

uint16_t t_x = 0, t_y = 0;            // To store the touch coordinates
uint16_t textrow;                    //current row for text output
uint16_t textcol;                    //current colume position for text output
uint8_t waterRow;                 //Counter for current Waterfall display row. 
bool autolevel = true;

uint16_t textHeight;

bool noTouch = true;

int TxPointer = 0;
uint8_t TxBitPointer = 0;
uint8_t TxBuffer[50];                       //needs to be large enough to allow for Locator Expansion
char visualTxMessage[50];
uint8_t TxMessNo;
uint8_t TxMessLen;
bool Key;
bool TxSent;
char TxCharSent;
bool messageChanging;

bool sdpresent;

// Morse RX
MorseRxDecoder morseDecoder;
float morseWpmEst;    // WPM at lock, for status display
float morseWpmCurrent;  //Current tracking morse WPM
uint32_t dmaTransferCount;  // set by RxInit(), read by dma_init()

uint8_t plotData[SPECWIDTH];        //Array of Plot points for spectrum display. Log scaled and offset to 0 - SPECHEIGHT and used to display new line.  
uint8_t lastplotData[SPECWIDTH];    //Array of Plot points for last Spectrum display. Used to erase previous line.

uint8_t toneLegend[4][2];                  // start and end pixels for tone indicator legend

//Waterfall Display colours. Based on Spectravue values. 
uint16_t waterColours[256] =
{0X0, 0X1, 0X2, 0X3, 0X4, 0X5, 0X6, 0X7, 0X7, 0X8, 0X9, 0XA, 0XB, 0XC, 0XD, 0XE,
 0XE, 0XF, 0X10, 0X11, 0X12, 0X13, 0X14, 0X15, 0X15, 0X16, 0X17, 0X18, 0X19, 0X1A,
 0X1B, 0X1C, 0X3C, 0X5C, 0X7C, 0X9C, 0XBC, 0XDC, 0X11C, 0X13C, 0X15C, 0X17C, 0X19D,
 0X1BD, 0X1FD, 0X21D, 0X23D, 0X25D, 0X27D, 0X29D, 0X2BD, 0X2FD, 0X31D, 0X33E, 0X35E,
 0X37E, 0X39E, 0X3DE, 0X3FE, 0X41E, 0X43E, 0X45E, 0X47E, 0X4BF, 0X4BE, 0X4DE, 0X4FE,
 0X51E, 0X53D, 0X53D, 0X55D, 0X57D, 0X59C, 0X5BC, 0X5BC, 0X5DC, 0X5FB, 0X61B, 0X63B,
 0X65B, 0X65A, 0X67A, 0X69A, 0X6BA, 0X6D9, 0X6D9, 0X6F9, 0X719, 0X738, 0X758, 0X758,
 0X778, 0X797, 0X7B7, 0X7D7, 0X7F7, 0X7F6, 0X7F5, 0X7F4, 0X7F4, 0X7F3, 0X7F2, 0XFF1, 
 0XFF1, 0XFF0, 0XFEF, 0XFEF, 0XFEE, 0X17ED, 0X17EC, 0X17EC, 0X17EB, 0X17EA, 0X17EA, 
 0X17E9, 0X1FE8, 0X1FE7, 0X1FE7, 0X1FE6, 0X1FE5, 0X1FE5, 0X27E4, 0X27E3, 0X27E2, 
 0X27E2, 0X27E1, 0X27E0, 0X2FE0, 0X2FE0, 0X37E0, 0X3FE0, 0X47E0, 0X4FE0, 0X4FE0, 
 0X57E0, 0X5FE0, 0X67E0, 0X6FE0, 0X6FE0, 0X77E0, 0X7FE0, 0X87E0, 0X8FE0, 0X97E0, 
 0X97E0, 0X9FE0, 0XA7E0, 0XAFE0, 0XB7E0, 0XB7E0, 0XBFE0, 0XC7E0, 0XCFE0, 0XD7E0, 
 0XD7E0, 0XDFE0, 0XE7E0, 0XEFE0, 0XF7E0, 0XFFE0, 0XFFC0, 0XFFA0, 0XFF80, 0XFF60, 
 0XFF40, 0XFF20, 0XFF00, 0XFEE0, 0XFEC0, 0XFEA0, 0XFEA0, 0XFE80, 0XFE60, 0XFE40, 
 0XFE20, 0XFE00, 0XFDE0, 0XFDC0, 0XFDA0, 0XFD80, 0XFD60, 0XFD60, 0XFD40, 0XFD20, 
 0XFD00, 0XFCE0, 0XFCC0, 0XFCA0, 0XFC80, 0XFC60, 0XFC40, 0XFC40, 0XFC00, 0XFBE0, 
 0XFBC0, 0XFBA0, 0XFB80, 0XFB60, 0XFB40, 0XFB20, 0XFB00, 0XFAE0, 0XFAC0, 0XFAA0, 
 0XFA80, 0XFA60, 0XFA40, 0XFA20, 0XF9E0, 0XF9C0, 0XF9A0, 0XF980, 0XF960, 0XF940, 
 0XF920, 0XF900, 0XF8E0, 0XF8C0, 0XF8A0, 0XF880, 0XF860, 0XF840, 0XF820, 0XF800, 
 0XF800, 0XF800, 0XF801, 0XF801, 0XF802, 0XF802, 0XF803, 0XF803, 0XF804, 0XF804, 
 0XF805, 0XF805, 0XF806, 0XF806, 0XF807, 0XF807, 0XF807, 0XF808, 0XF808, 0XF809, 
 0XF809, 0XF80A, 0XF80A, 0XF80B, 0XF80B, 0XF80C, 0XF80C, 0XF80D, 0XF80D, 0XF80E, 
 0XF80E, 0XF80F};

//decode array Ascii Characters in valid 4 from 8 order. 0 = bad 4 from 8 decode
uint8_t decode4from8[256] = {0,0,0,0,0,0,0,0,0,0,                 //0
                             0,0,0,0,0,13,0,0,0,0,                //10
                             0,0,0,32,0,0,0,33,0,34,              //20
                             35,0,0,0,0,0,0,0,0,36,               //30
                             0,0,0,37,0,38,39,0,0,0,              //40
                             0,40,0,41,42,0,0,43,44,0,            //50
                             45,0,0,0,0,0,0,0,0,0,                //60
                             0,46,0,0,0,47,0,48,49,0,             //70
                             0,0,0,50,0,51,52,0,0,53,             //80
                             54,0,55,0,0,0,0,0,0,56,              //90
                             0,57,58,0,0,59,60,0,61,0,            //100
                             0,0,0,62,63,0,64,0,0,0,              //110
                             65,0,0,0,0,0,0,0,0,0,                //120
                             0,0,0,0,0,66,0,0,0,67,               //130
                             0,68,69,0,0,0,0,70,0,71,             //140
                             72,0,0,73,74,0,75,0,0,0,             //150
                             0,0,0,76,0,77,78,0,0,79,             //160
                             80,0,81,0,0,0,0,82,83,0,             //170
                             84,0,0,0,85,0,0,0,0,0,               //180
                             0,0,0,0,0,86,0,87,88,0,              //190
                             0,89,90,0,91,0,0,0,0,92,             //200
                             93,0,94,0,0,0,95,0,0,0,              //210
                             0,0,0,0,0,126,126,0,126,0,           //220
                             0,0,126,0,0,0,0,0,0,0,               //230
                             126,0,0,0,0,0,0,0,0,0,               //240
                             0,0,0,0,0,0};                        //250

//JT4 Sync Vector table
const uint8_t JT4syncVector[JT4SYMBOLCOUNT] =
	{0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 0,
   0, 0, 0, 0, 1, 1, 0, 0, 0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,0 ,1 ,0 ,1 ,1,
   0, 1, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0,
   1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0,
   0, 1, 0, 0, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0,
   1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1,
   1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1,
   0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1,
   1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1,
   0, 1, 1, 1, 1, 0, 1, 0, 1};

//PI4 Sync Vector table
const uint8_t PI4syncVector[PI4SYMBOLCOUNT] =
	{0,0,1,0,0,1,1,1,1,0,1,0,1,0,1,0,0,1,0,0,0,1,0,0,0,1,1,0,0,1,1,1,1,0,0,1,1,1,1,1,0,0,1,1,0,1,1,1,1,0,1,0,1,1,0,1,1,0,1,0,
0,0,0,0,1,1,1,1,1,0,1,0,1,0,0,0,0,0,1,1,1,1,1,0,1,0,0,1,0,0,1,0,1,0,0,0,0,1,0,0,1,1,0,0,0,0,0,1,1,0,0,0,0,1,1,0,0,1,1,1,
0,1,1,1,0,1,1,0,1,0,1,0,1,0,0,0,0,1,1,1,0,0,0,0,1,};

//JT4 interpolation array
const uint8_t jt4di[JT4BITCOUNT] = 
{
0x00, 0x67, 0x34, 0x9B, 0x1A, 0x81, 0x4E, 0xB5, 0x0D, 0x74, 0x41, 0xA8, 0x27, 0x8E, 0x5B, 0xC2, 
0x07, 0x6E, 0x3B, 0xA2, 0x21, 0x88, 0x55, 0xBC, 0x14, 0x7B, 0x48, 0xAF, 0x2E, 0x95, 0x61, 0xC8, 
0x04, 0x6B, 0x38, 0x9F, 0x1E, 0x85, 0x52, 0xB9, 0x11, 0x78, 0x45, 0xAC, 0x2B, 0x92, 0x5E, 0xC5, 
0x0A, 0x71, 0x3E, 0xA5, 0x24, 0x8B, 0x58, 0xBF, 0x17, 0x7E, 0x4B, 0xB2, 0x31, 0x98, 0x64, 0xCB, 
0x02, 0x69, 0x36, 0x9D, 0x1C, 0x83, 0x50, 0xB7, 0x0F, 0x76, 0x43, 0xAA, 0x29, 0x90, 0x5D, 0xC4, 
0x09, 0x70, 0x3D, 0xA4, 0x23, 0x8A, 0x57, 0xBE, 0x16, 0x7D, 0x4A, 0xB1, 0x30, 0x97, 0x63, 0xCA, 
0x06, 0x6D, 0x3A, 0xA1, 0x20, 0x87, 0x54, 0xBB, 0x13, 0x7A, 0x47, 0xAE, 0x2D, 0x94, 0x60, 0xC7, 
0x0C, 0x73, 0x40, 0xA7, 0x26, 0x8D, 0x5A, 0xC1, 0x19, 0x80, 0x4D, 0xB4, 0x33, 0x9A, 0x66, 0xCD, 
0x01, 0x68, 0x35, 0x9C, 0x1B, 0x82, 0x4F, 0xB6, 0x0E, 0x75, 0x42, 0xA9, 0x28, 0x8F, 0x5C, 0xC3, 
0x08, 0x6F, 0x3C, 0xA3, 0x22, 0x89, 0x56, 0xBD, 0x15, 0x7C, 0x49, 0xB0, 0x2F, 0x96, 0x62, 0xC9, 
0x05, 0x6C, 0x39, 0xA0, 0x1F, 0x86, 0x53, 0xBA, 0x12, 0x79, 0x46, 0xAD, 0x2C, 0x93, 0x5F, 0xC6, 
0x0B, 0x72, 0x3F, 0xA6, 0x25, 0x8C, 0x59, 0xC0, 0x18, 0x7F, 0x4C, 0xB3, 0x32, 0x99, 0x65, 0xCC, 
0x03, 0x6A, 0x37, 0x9E, 0x1D, 0x84, 0x51, 0xB8, 0x10, 0x77, 0x44, 0xAB, 0x2A, 0x91
}; 

//PI4 interpolation array
const uint8_t pi4di[PI4BITCOUNT] = 
{
0,73,37,110,19,92,55,128,10,83,46,119,28,101,64,137,5,78,42,115,24,97,60,133,15,88,51,124,33,106,
69,142,3,76,40,113,22,95,58,131,13,86,49,122,31,104,67,140,8,81,44,117,26,99,62,135,17,90,53,126,
35,108,71,144,2,75,39,112,21,94,57,130,12,85,48,121,30,103,66,139,7,80,43,116,25,98,61,134,16,89,
52,125,34,107,70,143,4,77,41,114,23,96,59,132,14,87,50,123,32,105,68,141,9,82,45,118,27,100,63,136,
18,91,54,127,36,109,72,145,1,74,38,111,20,93,56,129,11,84,47,120,29,102,65,138,6,79
}; 

