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
    
    for(int m=0 ; m < NUMBEROFBINS ; m++)
      {
        magnitude[m] = sample[STARTBIN + m];                  //copy the bins for the band of interest to the magnitude[] array
      }

}


//Generate the display output array from the magnitude array with log scaling. Add offset and gain to the values.
void generatePlotData(void)
{
  float db[NUMBEROFBINS];
  float vref = 2048.0;
  static float baselevel;


    if(autolevel)
    {
    baselevel = 0;
    }

    for(int p =0;p < NUMBEROFBINS; p++)                         
    {
      db[p]=2*(20*(log10(magnitude[p] / vref)));               //calculate bin amplitude relative to FS in dB
 
    if(autolevel)
      {
      baselevel = baselevel + db[p];
      }  
    }
    
    if(autolevel)
    {
      baselevel = baselevel/NUMBEROFBINS;                             //use the average level for the baseline.
    }

    for(int p=0;p<NUMBEROFBINS;p++)
    {
      plotData[p]= uint8_t (db[p] - baselevel);  
    }
 
}

void saveCache(void)
{
  for(int i = 0 ; i < NUMBEROFBINS ; i++ )
  {
     toneCache[i][cachePoint]= magnitude[i];
  }
}
