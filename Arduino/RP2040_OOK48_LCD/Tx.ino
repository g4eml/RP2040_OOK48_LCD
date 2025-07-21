/* 
 * Sends an OOK48 encoded message.
 *
*/


//all valid 4 from 8 values.
uint8_t encode4from8[70] = { 15, 23, 27, 29, 30, 39, 43, 45, 46, 51,
                       53, 54, 57, 58, 60, 71, 75, 77, 78, 83,
                       85, 86, 89, 90, 92, 99, 101, 102, 105, 106,
                       108, 113, 114, 116, 120, 135, 139, 141, 142, 147,
                       149, 150, 153, 154, 156, 163, 165, 166, 169, 170,
                       172, 177, 178, 180, 184, 195, 197, 198, 201, 202,
                       204, 209, 210, 212, 216, 225, 226, 228, 232, 240 };


void TxInit(void) 
{
  replaceToken(visualTxMessage+1,settings.TxMessage[TxMessNo],LOCTOKEN, qthLocator ); 
  visualTxMessage[0]=13;
  TxMessLen = encode(visualTxMessage,strlen(visualTxMessage) , TxBuffer);
  TxPointer = 0;                   
  TxBitPointer = 0;
}

void TxSymbol(void) 
{
  if (mode == TX) 
  {
    if (TxPointer == TxMessLen) 
    {
      TxPointer = 1;                //don't repeat the first character as this is for the first transmission only
      TxBitPointer = 0;
    }
    if (TxBitPointer == 8) 
    {
      Key = 0;
      cancel_repeating_timer(&TxIntervalTimer);
      TxCharSent = visualTxMessage[TxPointer];
      if((halfRate == false ) || (halfRate & (gpsSec & 0x01) )) TxSent = true;
    } 
    else 
    {
      Key = (TxBuffer[TxPointer] << TxBitPointer) & 0x80;
    }
    TxBitPointer++;
    if (TxBitPointer > 8) 
    {
      TxBitPointer = 0;
      if((halfRate == false ) || (halfRate & (gpsSec & 0x01) )) TxPointer++;        //repeat the character if half Rate and the second is even
    }
  }
}

void TxTick(void)
  {
    static bool lastKey;
    if(Key != lastKey)
     {
      digitalWrite(KEYPIN, Key);
      lastKey = Key;
     }

    if(TxSent)        //if we have just finished sending a character 
     {
        if(! messageChanging) rp2040.fifo.push(TMESSAGE);          //Ask Core 1 to display it. 
        TxSent = false;
     }
  }
//

uint8_t encode(const char* msg, uint8_t len, uint8_t* symbols) 
{
  uint8_t v;
  for (int i = 0; i < len; i++) 
  {
    v = 69;  //default to null
    switch (msg[i]) 
    {
      case 13:                //Carriage return or line feed = end of message
      case 10:
      v=0;
      break;

      case 32 ... 95:
        v = msg[i] - 31;  //all upper case Letters, numbers and punctuation encoded to 1 - 64
        break;

      case 97 ... 122:
        v = msg[i] - 63;  //lower case letters map to upper case.
        break;
    }
    symbols[i] = encode4from8[v];
  }

  return len;
}
