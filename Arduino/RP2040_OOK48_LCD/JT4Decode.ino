// This file contains the functions to decode the received JT4 symbols. 

void JT4Init(void)
{
  sampleRate = JT4OVERSAMPLERATE;                  //samples per second.
  toneSpacing = JT4TONESPACING;                //tone spacing in number of bins.
  tone0 = JT4TONE0;                            //tone zero in bins. 
  toneTolerance = JT4TONETOLERANCE;            //Tone tolerance in bins. 
  cacheSize = JT4CACHESIZE;                    // tone decode samples.
  symbolCount = JT4SYMBOLCOUNT;               //number of symbols message.
  bitCount = JT4BITCOUNT;                     //number of bits in Message. 
  hzPerBin = JT4HZPERBIN;
  snBins = JT4SNBINS;
  numberOfTones = 4;
  numberOfBins = JT4NUMBEROFBINS;
  startBin = JT4STARTBIN;
  dmaTransferCount = NUMBEROFOVERSAMPLES;
  
  dma_init();                       //Initialise and start ADC conversions and DMA transfers. 
  dma_handler();                    //call the interrupt handler once to start transfers
  dmaReady = false;                 //reset the transfer ready flag
  cachePoint = 0;                   //zero the data received cache
  calcLegend();
}

void beaconTick(void)
{
  uint8_t tn;

  if(gpsMin != lastmin)                                         //every minute
    {
      lastmin = gpsMin;                                        //Save for next minute 
      cachePoint =0;                                            //Reset the sample cache ready for another period of received data
      rp2040.fifo.push(CYANLINE);                               //Ask core 1 to draw a Cyan horizontal line in the spectrum to mark the start of the minute.  
      sigNoise = -100.00; 
    }
   
   if(dmaReady)                                                 //Do we have a complete buffer of ADC samples ready?
    {
      calcSpectrum();                                           //Perform the FFT of the data
      rp2040.fifo.push(GENPLOT);                                //Ask Core 1 to generate data for the Displays from the FFT results.  
      rp2040.fifo.push(DRAWSPECTRUM);                           //Ask core 1 to draw the Spectrum Display
      rp2040.fifo.push(DRAWWATERFALL);                          //Ask core 1 to draw the Waterfall Display      
      tn=toneDetect();                                          //Detect which tone is present

        BeaconToneCache[cachePoint++] = tn;                     //Add the tone index to the Tone cache 

        if(cachePoint == cacheSize)                               //If the Cache is full (54 seconds of tones)
          {
            rp2040.fifo.push(REDLINE);                            //Ask core 1 to draw a Red horizontal line in the spectrum to mark the end of the Rx Period.. 
            cachePoint =0;                                        //Reset ready for the next period 
            if(beaconMode == JT4)
            {
              if(JT4decodeCache())                                     //Try to extract the JT4 message from the 54 seconds of Tone cache.
                {
                rp2040.fifo.push(JTMESSAGE);                         //Successfull decode. Ask Core 1 to display the received message  
                }  
            }

            if(beaconMode == PI4)
            {
              if(PI4decodeCache())                                     //Try to extract the JT4 message from the 54 seconds of Tone cache.
                {
                  rp2040.fifo.push(PIMESSAGE);                         //Successfull decode. Ask Core 1 to display the received message 
                }  
            }
          }                                  
      dmaReady = false;                                         //Clear the flag ready for next time     
    }
}


//Search the 54 seconds of Tone Cache to try to decode the FT4 message. 
bool JT4decodeCache(void)
{
uint8_t bestStartIndex;                     //start of the best received symbol pattern
uint8_t bits[bitCount];                 //storage for received bits
bool decodedOK = false;
unsigned char dec[14];                      //decoded message before JT4unpacking

  bestStartIndex = JT4findSync();              //search the received symbols for the best match to the sync vector
  JT4extractBits(bestStartIndex, bits);        //extract the best match found and store the bits, one bit per byte
  JT4deInterleave(bits);                       //deinterleave the bits
  decodedOK = decodeFT4(bits,dec);          //decode using the fano algorithm to error correct from the received bits.
  if(decodedOK)                             ///if it looks like a successful decode
   {
    JT4unpack(dec);                            //JT4unpack back to the original JT4 message
   }  

  return decodedOK;
}

//search the received symbol cache and return the index of the best match to the sync vector
uint8_t JT4findSync()
{
uint8_t syncErrorCount;
uint8_t bestErrorCount;
uint8_t bestStartIndex;
uint8_t equalBestCount;
bestErrorCount = 255;

for(int i = 0; i < cacheSize - (symbolCount * overlap) ; i++)       //starting index for the Sync test. No point in testing after the first few seconds as the whole message wont have been captured. 
 {
  syncErrorCount = 0;
  for(int s = 0; s < symbolCount; s++)          //test each symbol in the JT4syncVector against the received symbols. Sync pattern is in bit 0 of symbol
   {
      if(JT4syncVector[s] != (BeaconToneCache[i + s * overlap] & 0x01) )    syncErrorCount++;
   }

  if(syncErrorCount < bestErrorCount)                     //Have we found a better match to the sync vector?
   {
    bestStartIndex = i;                                  //save this start index as the best found so far.
    bestErrorCount = syncErrorCount;
   }
 }

  return bestStartIndex;
}

// extract the bits from bit 1 of the symbols. Check and Ignore first bit which should be a zero.
void JT4extractBits(uint8_t bestStartIndex, uint8_t *bits)
{ 
for(int i = 0; i < bitCount; i++)
   {
     bits[i] = (BeaconToneCache[bestStartIndex + i* overlap + overlap] >> 1);
   }
}

void JT4deInterleave(uint8_t *bits)
{
  uint8_t d[bitCount];           //temporary aray for de-interleaving

  for(int i = 0; i < bitCount; i++)
  {
    d[jt4di[i]] = bits[i];
  }
  memcpy(bits, d, bitCount);
}

bool decodeFT4(uint8_t *bits, unsigned char *dec)
{
 //decode using Fano Algorithm
  unsigned int metric;
  unsigned long cycles;
  unsigned int maxnp;

  for(int i =0;i<bitCount;i++)
   {
    bits[i]=bits[i]*255;
   }

 int notDecoded = fano(&metric,&cycles,&maxnp, dec, bits, 104,60,20000);
 return !notDecoded;
}

 //Unpack message

void JT4unpack(unsigned char *dec)
{
  uint32_t n1 , n2 , n3;

  n3 = (dec[7] << 8) + dec[8];
  n2 = ((dec[3] & 0x0F) << 24) + (dec[4] << 16) + (dec[5] <<8 ) + dec[6];
  n1 = (dec[0] << 20) + (dec[1] << 12) + (dec[2] << 4) + (dec[3] >> 4);

  n3 = (n3 & 0x7FFF) | ((n1 & 0x01) <<15) | ((n2 & 0x01) << 16);
  n2 = n2 >> 1;
  n1 = n1 >> 1;

  JTmessage[4] = n1 % 42;
  n1 = n1 / 42;
  JTmessage[3] = n1 % 42;
  n1 = n1 / 42;
  JTmessage[2] = n1 % 42;
  n1 = n1 / 42;  
  JTmessage[1] = n1 % 42;
  n1 = n1 / 42;
  JTmessage[0] = n1;  

  JTmessage[9] = n2 % 42;
  n2 = n2 / 42;
  JTmessage[8] = n2 % 42;
  n2 = n2 / 42;
  JTmessage[7] = n2 % 42;
  n2 = n2 / 42;  
  JTmessage[6] = n2 % 42;
  n2 = n2 / 42;
  JTmessage[5] = n2; 

  JTmessage[12] = n3 % 42;
  n3 = n3 / 42;  
  JTmessage[11] = n3 % 42;
  n3 = n3 / 42;
  JTmessage[10] = n3; 

  JTmessage[13] = 0;    //terminate the string
  //convert back to ascii

  for(int i = 0; i<13 ; i++)
   {
    switch(JTmessage[i])
    {
      case 0 ... 9:
      JTmessage[i] = JTmessage[i] + '0';
      break;
      case 10 ... 35:
      JTmessage[i] = JTmessage[i] + 'A' - 10;
      break;
      case 36:
      JTmessage[i] = ' ';
      break;
      case 37:
      JTmessage[i] = '+';
      break;
      case 38:
      JTmessage[i] = '-';
      break;
      case 39:
      JTmessage[i] = '.';
      break;
      case 40:
      JTmessage[i] = '/';
      break;
      case 41:
      JTmessage[i] = '?';
      break;
    }
   }

}

//Find the FFT bin for each of the 4 tones and then determine which tone has the largest signal. Return the corresponding tone index 0 - 4. 
uint8_t toneDetect(void)
{
  double toneMag[4];
  uint16_t toneBin[4];
  double sn[4];
  double signz;
  uint8_t tone;
  double mx;

  //search around each averaged tone to find largest magnitude and s/n 
  for(int i =0; i<4; i++)
    {
      findMax( i, &toneMag[i], &sn[i]);
    }


  //find the tone with the best s/n (using s/n compensates for any amplitude slew between tones.)
  mx = 0;
  for(int i =0;i<4;i++)
    {
      if(sn[i] > mx)
       {
        mx=sn[i];
        tone = i;
       }
    }
  
  signz = 10 * log10(mx / snBins);         //calculate 2.5KHz s/n for this signal . 
  if(signz > sigNoise) sigNoise = signz;      //and record the best value

  return tone;
}

//Search the averaged FFT Bins for the given tone. Return the maximum Magnitude found, its associated bin number and its s/n relative to the adjacent bins.
void findMax(int tone, double * maxval, double * sn)
{
  double tstval=0;
  double avg=0;
  uint16_t binnumber=0;

  // find the bin in the tolerance range with the highest signal.
  for(int i = 0 ; i < (1+toneTolerance *2) ; i++ )
  {
    int bin = tone0 + tone*toneSpacing - toneTolerance + i;
    if(magnitude[bin] > tstval)
     {
      tstval=magnitude[bin];
      binnumber = bin;
     }
  }
  
  //average a few bins either side of the tone to get a noise floor
   for(int i =0;i < 4;i++ )
   {
    int bin = tone0 + tone * toneSpacing - toneTolerance - 5 + i;  //below the current tone band
    avg = avg + magnitude[bin];
    bin = tone0 + tone * toneSpacing + toneTolerance + 1 + i;  //above the current tone band 
    avg = avg + magnitude[bin];
   }

  avg = avg /8;

  *maxval = magnitude[binnumber];
  *sn = magnitude[binnumber] / avg;                  //calculate and return the S/N ratio for the max tone.
}