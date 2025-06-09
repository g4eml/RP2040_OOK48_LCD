//Receive routines for an OOK48 message

void RxInit(void)
{
  sampleRate = SAMPLERATE;                  //samples per second.
  rxTone = TONE800;                              //tone in bins. 
  toneTolerance = TONETOLERANCE;            //Tone tolerance in bins. 
  cacheSize = CACHESIZE;                    // tone decode samples.
  hzPerBin = HZPERBIN;
  snBins = SNBINS;
  calcLegend();
  dma_init();                       //Initialise and start ADC conversions and DMA transfers. 
  dma_handler();                    //call the interrupt handler once to start transfers
  dmaReady = false;                 //reset the transfer ready flag
  cachePoint = 0;                   //zero the data received cache
  sigNoise = -100.00;
}

void RxTick(void)
{
  uint8_t tn;
  static unsigned long lastDma;

  if((millis() - lastDma) > 250) cachePoint = 0;                //if we have not had a DAm tranfer recently reset the pointer. (allows the spectrum to freerun with no GPS signal)

   if((dmaReady) && (cachePoint < cacheSize))                                                 //Do we have a complete buffer of ADC samples ready?
    {
      lastDma = millis();
      calcSpectrum();                                           //Perform the FFT of the data
      rp2040.fifo.push(GENPLOT);                                //Ask Core 1 to generate data for the Displays from the FFT results.  
      rp2040.fifo.push(DRAWSPECTRUM);                           //Ask core 1 to draw the Spectrum Display
      rp2040.fifo.push(DRAWWATERFALL);                          //Ask core 1 to draw the Waterfall Display      
      saveCache();                                              //save the FFT magnitudes to the cache.
      if(toneCache[bestBin][cachePoint++] > threshold)
       {
         rp2040.fifo.push(SHOWTONE1);     //Ask Core 1 to highlight the Tone Green. 
       }
       else 
       {
         rp2040.fifo.push(SHOWTONE0);     //Ask Core 1 to highlight the Tone Orange.
       }

      if(cachePoint == cacheSize)                               //If the Cache is full (8 bits of data)
        {
          if(PPSActive)                                         //decodes are only valid if the PPS Pulse is present
          { 
           bestBin = findBestBin();                             //search the cached bins to find the bin containing the tone. 
           if(decodeCache(bestBin))                              //Try to extract the character
             {
               rp2040.fifo.push(MESSAGE);                         //Successful 1st decode. Ask Core 1 to display it 
             } 
            else 
             {
                rp2040.fifo.push(ERROR);                         //Unsuccessful 1st decode. Ask Core 1 to display the possible char 
             } 
          }
        }                                  
      dmaReady = false;                                         //Clear the flag ready for next time     
    }
}

//search the FFT cache to find the bin containing the tone. Use the bin with the greatest max to min range 
int findBestBin(void)
{
  double max;
  double min;
  double range;
  double bestRange;
  int topBin;

  bestRange =0;
  topBin = 0;
  for(int b=0 ; b < 1 + toneTolerance *2; b++)        //search each possible bin in the search range
    {
      max = 0 - DBL_MAX;
      min = DBL_MAX;
      for(int s=0; s < cacheSize ; s++)               //search all 8 or 16 symbols in this bin to find the largest and smallest
        {
          if(toneCache[b][s] > max) max = toneCache[b][s];
          if(toneCache[b][s] < min) min = toneCache[b][s];
        }
      range = max - min;                //calculate the signal to noise for this bin
      if(range > bestRange)             //if this bin is a better choice than previous (larger signal to noise)
        {
          bestRange = range;            //make it the chosen one. 
          topBin = b;
        }

    }
  return topBin;
}


//Search the Tone Cache to try to decode the character. 
bool decodeCache(int bin)
{
  if(halfRate)                        //half rate decoding. Sum the two received chars into one. 
   {
    for(int i=0; i<CACHESIZE ; i++)
      {
        toneCache[bin][i] = toneCache[bin][i] + toneCache[bin][i+8];
      }
   }

//calculate the threshold as the mean of the received tone magnitudes. 
  threshold=0;
  for(int i = 0; i < CACHESIZE ; i++)
    {
      threshold=threshold + toneCache[bin][i];
    }
  threshold = threshold / CACHESIZE;

// now take each received symbol and determine if it is a 1 or a zero
  uint8_t dec = 0;
  for(int i = 0; i < CACHESIZE; i++)
    {
      if(toneCache[bin][i] > threshold) 
        {
          dec = dec | (0x80 >> i);        //add a one bit if detected. 
        }
    }


  decoded = decode4from8[dec];           //use the decode array to recover the original Character

  if(decoded > 0)
    {
       return 1;                     // return 1 if it is a valid 4 from 8 character
    }
  else 
    {
      force4from8(bin);                    //try to recover the best 4 from 8 character. 
      return 0;                         //unreliable character
    }
}

//crude error correction. Picks the highest 4 Magnitudes and sets them to 1 creating a 4 from 8 character.
void force4from8(int bin)
{
  uint8_t dec;
  double largest;
  uint8_t largestbits[4];
  double temp[CACHESIZE];         //temporary array for finding the largest magnitudes

  
  memcpy(temp,toneCache[bin],sizeof(temp));        //make a copy of the tone cache

  //find the four largest magnitudes and save their bit positions.
  for(int l= 0; l < 4; l++)
    {
      largest = 0;
      for(int i = 0 ; i < CACHESIZE ; i++)
      {
        if(temp[i] > largest)
        {
        largest=temp[i];
        largestbits[l]=i;
        }
      }
      temp[largestbits[l]] = 0;
    }

  //convert the 4 bit positions to a valid 4 from 8 char
    for(int l = 0;l<4;l++)
    {
      dec = dec | (0x80 >> largestbits[l]);        //add a one bit. 
    }

   decoded = decode4from8[dec];           //use the decode array to recover the original Character
}