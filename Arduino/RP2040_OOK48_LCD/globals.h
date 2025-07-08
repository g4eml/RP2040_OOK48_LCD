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
};

struct eepromstruct settings;

enum decodemodes {NORMALMODE,ALTMODE};

enum core1Message {GENPLOT,DRAWSPECTRUM,DRAWWATERFALL,REDLINE,CYANLINE,SHOWTONE0,SHOWTONE1,MESSAGE,TMESSAGE,ERROR};         //messages for control of Core 1 from Core 2


uint dma_chan;                        //DMA Channel Number
bool dmaReady;                        //Flag to indicate a DMA buffer is ready to be processed.
uint8_t bufIndex = 0;                 //Index to the current DMA buffer. Alternates 0/1.

uint8_t mode;
enum modes {RX,TX};

uint32_t sampleRate;                  //samples per second.
uint16_t rxTone;                      //tone in bins. 
uint16_t toneTolerance;             //Tone tolerance in bins. 
uint16_t cacheSize;                 // tone decode samples.
float hzPerBin;
uint16_t activeBins;
int overlap = 1;

float sigNoise;
float snBins;
double threshold;
double toneCache[1 + MAXTONETOLERANCE * 2][CACHESIZE *2];          // Array large enough for the biggest tone magnitude cache
uint16_t cachePoint;                  // Pointer to next cache entry. 
bool halfRate = false;

char decoded;                         //decoded  Message character

char gpsBuffer[256];                     //GPS data buffer
int gpsPointer;                          //GPS buffer pointer. 
char gpsCh;
bool gpsActive = false; 
int lastSec = 0;
int gpsSec = -1;                       //GPS clock time  -1 for GPS Invalid
int gpsMin = -1;
int gpsHr = -1;
uint8_t PPSActive = 0;
long lastTimeUpdate = 0;
double latitude = 0;
double longitude =0;
char qthLocator[12] = "----------";

uint16_t buffer[2][NUMBEROFSAMPLES];     //2 DMA buffers to allow one to be processed while the next is being received.
double vReal[NUMBEROFBINS];            //Real Array for FFT input and output
double vImag[NUMBEROFBINS];            //Imaginary Array for FFT input and output

uint16_t t_x = 0, t_y = 0;            // To store the touch coordinates
uint16_t textrow;                    //current row for text output
uint16_t textcol;                    //current colume position for text output
uint8_t waterRow;                 //Counter for current Waterfall display row. 
bool autolevel = true;

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

uint8_t plotData[PLOTPOINTS];        //Array of Plot points for spectrum display. Log scaled and offset to 0 - SPECHEIGHT and used to display new line.  
uint8_t lastplotData[PLOTPOINTS];    //Array of Plot points for last Spectrum display. Used to erase previous line.

uint8_t toneLegend[2];                  // start and end pixels for tone indicator legend

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



