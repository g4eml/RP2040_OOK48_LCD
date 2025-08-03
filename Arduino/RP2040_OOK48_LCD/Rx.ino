//Receive routines for an OOK48 message

void RxInit(void)
{
  sampleRate = OVERSAMPLERATE;                  //samples per second.  
  cacheSize = CACHESIZE;                    // tone decode samples.
  rxTone = TONE800;
  toneTolerance = TONETOLERANCE;
  calcLegend();
  dma_init();                       //Initialise and start ADC conversions and DMA transfers. 
  dma_handler();                    //call the interrupt handler once to start transfers
  dmaReady = false;                 //reset the transfer ready flag
  cachePoint = 0;                   //zero the data received cache
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
      cachePoint++;
      if(cachePoint == cacheSize)                               //If the Cache is full (8 bits of data)
        {
          if(PPSActive)                                         //decodes are only valid if the PPS Pulse is present
          { 
            decodeCache();                                      //extract the character
            rp2040.fifo.push(MESSAGE);                         //Ask Core 1 to display it 
          }
        }                                  
      dmaReady = false;                                         //Clear the flag ready for next time     
    }
}

//search the FFT cache to find the bin containing the tone. Use the bin with the greatest max to min range 
int findBestBin(void)
{
  float max;
  float min;
  float range;
  float bestRange;
  int topBin;

  bestRange =0;
  topBin = 0;
  for(int b=rxTone - toneTolerance ; b < rxTone + toneTolerance; b++)        //search each possible bin in the search range
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

//search the magnitude cache to find the magnitude of the largest tone tone. 
float findLargest(int timeslot)
{
  float max;
  max = 0 - DBL_MAX;
  for(int b=rxTone - toneTolerance ; b < rxTone + toneTolerance; b++)        //search each possible bin in the search range to find the largest magnitude
    {
      if(toneCache[b][timeslot] > max) max = toneCache[b][timeslot];
    }
  return max;
}


//Search the Tone Cache to try to decode the character. Pick the four largest magnitudes and set them to one. This will always result in a valid value.  
bool decodeCache(void)
{
  uint8_t dec;
  float largest;
  int bestbin;
  uint8_t largestbits[4];
  float temp[CACHESIZE*2];         //temporary array for finding the largest magnitudes


  if(settings.decodeMode == ALTMODE)
   {
    bestbin = findBestBin(); 
    for(int i =0; i< cacheSize; i++)
     {
      temp[i] = toneCache[bestbin][i];
     }
   }
  else 
   {
      for(int i =0; i< cacheSize; i++)
     {
      temp[i] = findLargest(i);
     }
   }

  if(halfRate)                        //half rate decoding. Sum the two received chars into one. 
   {
    for(int i=0; i<CACHESIZE ; i++)
      {
        temp[i] = temp[i] + temp[i+8];
      }
   }

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
   return 1;
}



