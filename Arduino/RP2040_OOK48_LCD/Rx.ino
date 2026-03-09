//Receive routines for an OOK48 message

void RxInit(void)
{
  sampleRate = OVERSAMPLERATE;

  if (settings.app == MORSE)
  {
    dmaTransferCount = MORSE_FRAME_SAMPLES;      // 2048 ADC samples → ~36 fps
    numberOfBins     = MORSE_FFT_BINS;
    startBin         = MORSESTARTBIN;
    rxToneBin           = MORSE_TONE_BIN;
    toneTolerance    = 3;
    numberOfTones    = 1;
    calcLegend();
    morseDecoder.begin(MORSE_FRAME_RATE, MORSE_MIN_WPM, MORSE_MAX_WPM, MORSE_TONE_BIN);
    morseCentroidHz = (float)(MORSE_TONE_BIN + MORSESTARTBIN * (SAMPLERATE / MORSE_FFT_SIZE));
  }
  else
  {
    dmaTransferCount = NUMBEROFOVERSAMPLES;
    cacheSize        = CACHESIZE;
    if (halfRate) cacheSize = CACHESIZE * 2;
    rxToneBin           = TONE800;
    toneTolerance    = TONETOLERANCE;
    numberOfTones    = 1;
    numberOfBins     = OOKNUMBEROFBINS;
    startBin         = OOKSTARTBIN;
    calcLegend();
  }
  dma_init();                       //Initialise and start ADC conversions and DMA transfers. 
  dma_handler();                    //call the interrupt handler once to start transfers
  dmaReady = false;                 //reset the transfer ready flag
  cachePoint = 0;                   //zero the data received cache
}

// Waterfall accumulator for morse mode (file-static, persists across calls)
static float  morseWfAccum[MORSE_FFT_BINS] = {};
static uint8_t morseWfCount = 0;
static float morseCentroidFiltBin = (float)MORSE_TONE_BIN;

void RxTick(void)
{
  uint8_t tn;
  static unsigned long lastDma;

    if (settings.app == MORSE)
  {
    if (!dmaReady) return;
    lastDma = millis();
    calcMorseSpectrum();

    const float binHz = (float)SAMPLERATE / (float)MORSE_FFT_SIZE;
    bool morseRainscatter = (settings.morseDecodeMode == RAINSCATTERMODE);

    if (!morseRainscatter)
    {
      // Track centroid around expected Morse tone and smooth for stable GUI marker.
      // Use local energy weighting in a narrow band to avoid whole-spectrum bias.
      int lo = MORSE_TONE_BIN - 10;
      int hi = MORSE_TONE_BIN + 10;
      if (lo < MORSESTARTBIN) lo = MORSESTARTBIN;
      if (hi > MORSE_FFT_BINS - 2) hi = MORSE_FFT_BINS - 2;

      float weighted = 0.0f;
      float sumMag = 0.0f;
      for (int b = lo; b <= hi; b++)
      {
        float m = magnitude[b];
        if (m < 0.0f) m = 0.0f;
        weighted += (float)b * m;
        sumMag += m;
      }

      if (sumMag > 0.0f)
      {
        float centroidBin = weighted / sumMag;
        morseCentroidFiltBin = morseCentroidFiltBin * 0.75f + centroidBin * 0.25f;
      }

      morseCentroidHz = morseCentroidFiltBin+startBin * binHz;
    }
    else
    {
      // In rainscatter mode we deliberately avoid single-tone tracking.
      morseCentroidHz = 0.0f;
    }

    // Accumulate bin magnitudes for waterfall
    for (int i = 0; i < MORSE_FFT_BINS; i++)
      morseWfAccum[i] += magnitude[i];

    // Feed morse decoder:
    //  - normal mode: largest magnitude in the tolerance range
    //  - rainscatter mode: sum of entire tolerance range
    float decoderMag;
    if (!morseRainscatter)
    {
      decoderMag = magnitude[rxToneBin];
    }
    else
    {
      float p = 0.0f;
      for (int b =rxToneBin - toneTolerance; b <= rxToneBin + toneTolerance;b++)
      {
        float m = magnitude[b];
        if (m > 0.0f) p += m;
      }
      decoderMag = p;
    }

    int n = morseDecoder.feed(decoderMag);
    for (int i = 0; i < n; i++)
    {
      MorseEvent ev = morseDecoder.event(i);
      if ((ev.kind == MorseEvt::CHAR)||(ev.kind == MorseEvt::WORD_SEP)) 
      {
        rp2040.fifo.push(MORSEMESSAGE + (ev.ch <<16));
      }
      else if (ev.kind == MorseEvt::LOCKED)
      {
        morseWpmEst = ev.wpm;
        rp2040.fifo.push(MORSELOCKED);
      }
      else if (ev.kind == MorseEvt::LOST)
      {
        rp2040.fifo.push(MORSELOST);
      }
    }

    // Send waterfall every MORSE_WF_FRAMES frames (~9/sec)
    if (++morseWfCount >= MORSE_WF_FRAMES)
    {
      morseWfCount = 0;
      for (int i = 0; i < MORSE_FFT_BINS; i++)
        magnitude[i] = morseWfAccum[i];
      rp2040.fifo.push(GENPLOT);
      rp2040.fifo.push(DRAWSPECTRUM);
      rp2040.fifo.push(DRAWWATERFALL);
      memset(morseWfAccum, 0, sizeof(morseWfAccum));
    }

    dmaReady = false;
    return;
  }

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
            rp2040.fifo.push(MESSAGE + decoded << 16);          //Ask Core 1 to display it 
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
  for(int b=rxToneBin - toneTolerance ; b < rxToneBin + toneTolerance; b++)        //search each possible bin in the search range
    {
      max = 0 - FLT_MAX;
      min = FLT_MAX;
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
  max = 0 - FLT_MAX;
  for(int b=rxToneBin - toneTolerance ; b < rxToneBin + toneTolerance; b++)        //search each possible bin in the search range to find the largest magnitude
    {
      if(toneCache[b][timeslot] > max) max = toneCache[b][timeslot];
    }
  return max;
}

// Sum magnitude across all OOK bins for one symbol time slot.
// Used by rainscatter mode where energy is spread across frequency.
float findWidebandPower(int timeslot)
{
  float sum = 0.0f;
  for(int b = 0; b < numberOfBins; b++)
    sum += toneCache[b][timeslot];
  return sum;
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



