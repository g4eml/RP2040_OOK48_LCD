//This file contains the functions used to frequency sample and analyse the incoming audio


//Perform Goertzel function on the ADC sample buffer. Calculate the magnitude of each frequency bin. Results are in the magnitude[] array. 
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
  }
  
  //Averaged samples are now in the sample[] array calculate the magnitude of each of the scanned frequencies
   for (int m=0 ; m < NUMBEROFBINS ; m++)
    {
      magnitude[m]= goertzel(sample , NUMBEROFSAMPLES, STARTFREQ + m*9);
    }
  //Spectrum magnitudes are now in the magnitude[] array.                     
}

// Goertzel algorithm implementation
float goertzel(float* samples, int N, int targetFreq) 
{
  float omega = 2.0 * PI * targetFreq / SAMPLERATE;
  float coeff = 2.0 * cos(omega);
  float q1 = 0, q2 = 0;

  for (int i = 0; i < N; i++) {
    float q0 = coeff * q1 - q2 + samples[i];
    q2 = q1;
    q1 = q0;
  }

  return q1 * q1 + q2 * q2 - q1 * q2 * coeff;
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
      db[p]=(20*(log10(magnitude[p] / vref)));               //calculate bin amplitude relative to FS in dB
 
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
