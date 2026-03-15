//This file contains the functions used to frequency sample and analyse the incoming audio

//Perform an FFT on the ADC sample buffer. Calculate the magnitude of each frequency bin. Results are in the first half of the vReal[] array. 
void calcSpectrum(void)
{
  int bin;
  for(int i = 0;i < (NUMBEROFOVERSAMPLES); i=i+OVERSAMPLE)                       //for each of the oversamples calculate the average value and save inb the sample[] array
  {
    bin = i/OVERSAMPLE;
    sample[bin]=0;
    for(int s=0;s<OVERSAMPLE;s++)
    {
      sample[bin] += buffer[bufIndex][i+s] - 2048;     //average the samples and copy the result into the Real array. Offsetting to allow for ADC bias point
    }
    sample[bin] = sample[bin]/OVERSAMPLE;
    sampleI[bin] = 0;
  }
    FFT.windowing(FFTWindow::Hann, FFTDirection::Forward);     //weight the data to reduce discontinuities. Hann window seems to work the best.
    FFT.compute(FFTDirection::Forward);                        //calculate the FFT. Results are now in the vReal and vImag arrays.
    FFT.complexToMagnitude();                                  //calculate the magnitude of each bin. FFT magnitude results are now in the first half of the sample[] array.
    
    for(int m=0 ; m < numberOfBins ; m++)
      {
        magnitude[m] = sample[startBin + m];                  //copy the bins for the band of interest to the magnitude[] array
      }

}

static ArduinoFFT<float> MorseFFT(sample, sampleI, MORSE_FFT_SIZE, (float)SAMPLERATE);

void calcMorseSpectrum(void)
{
  int bin;
    for (int i = 0; i < MORSE_FRAME_SAMPLES; i += OVERSAMPLE)
    {
        bin = i / OVERSAMPLE;
        sample[bin] = 0;
        for (int s = 0; s < OVERSAMPLE; s++)
        {
          sample[bin] += buffer[bufIndex][i + s] - 2048.0f;
        }
        sample[bin] = sample[bin]/OVERSAMPLE;
        sampleI[bin] = 0;
    }

    MorseFFT.windowing(FFTWindow::Hann, FFTDirection::Forward);
    MorseFFT.compute(FFTDirection::Forward);
    MorseFFT.complexToMagnitude();

    for (int m = 0; m < MORSE_FFT_BINS; m++)
    {
        magnitude[m] = sample[startBin + m];
    }

}

//Generate the display output array from the magnitude array with log scaling. Add offset and gain to the values.
void generatePlotData(void)
{
  float db[numberOfBins];
  float vref = 2048.0;
  static float baselevel;


    if(autolevel)
    {
    baselevel = 0;
    }

    for(int p =0;p < numberOfBins; p++)                         
    {
      db[p]=2*(20*(log10(magnitude[p] / vref)));               //calculate bin amplitude relative to FS in dB
    if(autolevel)
      {
      baselevel = baselevel + db[p];
      }  
    }
    
    if(autolevel)
    {
      baselevel = baselevel/numberOfBins;                             //use the average level for the baseline.
    }

    for(int p =0;p<numberOfBins;p++)
    {
      db[p] = uint8_t (db[p] - baselevel);
    }

//map the bins to fill the available display pixels

    for (int x = 0; x < SPECWIDTH; x++) 
       {
        // Calculate start and end bin using integer math
        int strtBin = (long)x * numberOfBins / SPECWIDTH;
        int endBin = (long)(x + 1) * numberOfBins / SPECWIDTH - 1;
        if (endBin >= numberOfBins) endBin = numberOfBins - 1;
        if (endBin < 0) endBin = 0;

        // Find maximum value in this range (float)
        uint8_t maxVal = db[strtBin];
        for (int i = strtBin + 1; i <= endBin; i++) 
        {
          if (db[i] > maxVal) maxVal = db[i];
        }

        plotData[x] = maxVal; 
    }
}


void saveCache(void)
{
  for(int i = 0 ; i < numberOfBins ; i++ )
  {
     toneCache[i][cachePoint]= magnitude[i];
  }
}
