//This file contains functions for ADC and DMA to digitise the incoming audio. 

//Interrupt Handler. Called whenever the DMA has filled a DMA buffer. 
void dma_handler(void)
{
     dma_hw->ints0 = 1u << dma_chan;                                     //clear the interrupt request
     dma_channel_set_write_addr(dma_chan, buffer[bufIndex++], true);     //reset the destination address to the next buffer and restart the transfer
     if(bufIndex >1) bufIndex = 0;                                       //Alternate between two buffers. 
     dmaReady = true;                                                    //Flag buffer is available
}

void dma_stop(void)
{
    dma_channel_set_irq0_enabled(dma_chan, false);                      //disable DMA interrupts
    dma_channel_abort(dma_chan);                                        //stop the DMA
    dma_hw->ints0 = 1u << dma_chan;                                     //clear any spurious IRQ 
    dma_channel_set_irq0_enabled(dma_chan, true);                       //re-enable interrupts                                 
}

void dma_halt(void)
{
    adc_run(false);
    dma_channel_set_irq0_enabled(dma_chan, false);                      //disable DMA interrupts
    dma_channel_abort(dma_chan);                                        //stop the DMA
    dma_hw->ints0 = 1u << dma_chan;                                     //clear any spurious IRQ 
    dma_channel_unclaim(dma_chan);   
}


//Initialise and start ADC and DMA transfers. 
void dma_init(void)
{
    adc_gpio_init(26 + ADC_CHAN);                                //Assign GPIO Pin to ADC for audio input
    adc_gpio_init(26 + ADC_VOLTS);                                //Assign GPIO Pin to ADC for audio input
    adc_init();                                                  //initialise the ADC hardware
    adc_select_input(ADC_CHAN);                                  //select the Analogue input channel.
    adc_fifo_setup(true,true,1,false,false);                     //Enable ADC FIFO. Raise DRQ when 1 conversion is available. No Error bit and no bit shifting. 
    adc_set_clkdiv(48000000/sampleRate);                         //Number of clock cycles between sample triggers. 0 = continuous. 48MHz clock. (minimumm allowed value = 98)
    uint dma_chan = dma_claim_unused_channel(true);              //Find unused DMA channel and save it. 
    dma_channel_config cfg = dma_channel_get_default_config(dma_chan);   //create a DMA config structure
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);    //DMA 16 bit transfers
    channel_config_set_read_increment(&cfg, false);              //Do not increment the read address after each transfer
    channel_config_set_write_increment(&cfg, true);              //Increment the Write address after each transfer
    channel_config_set_dreq(&cfg, DREQ_ADC);                     // Pace transfers based on availability of ADC samples
    dma_channel_configure(dma_chan, &cfg, buffer[bufIndex], &adc_hw->fifo, NUMBEROFOVERSAMPLES, false);    //configure DMA to copy ADC FIFO to buffer 
    dma_channel_set_irq0_enabled(dma_chan, true);                //Enable DMA IRQ 0. 
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);           //Link DMA IRQ0 to the interrupt handler
    irq_set_enabled(DMA_IRQ_0, true);                            //Enable the interrupt
    bufIndex=0;                                                  //select first buffer
    adc_run(true);                                               //start the ADC Capture
}

