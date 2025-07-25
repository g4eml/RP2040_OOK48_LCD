// create a USB Mass Storage device and link it to the SD Card
// Originated from Adafruit msc_sdfat example
// Must run undisturbed. LCD and touchscreen share the SPI Bus and cannot be used at the same time as the USB access to the SD card. 
//

void doUSBDrive(void)
{
  bool done = false;

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(50,50);
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.println("USB Drive Mode");
  tft.setCursor(0,150);
  tft.println("Power off when done");
  tft.setCursor(0,250);
  tft.println("Connect to PC ------->");   
   

  initUSBDrive();
  while(!done)
   {
     // endless loop while USB transfers are in use. 
   }

}


void initUSBDrive(void)
{ 
  usb_msc.setID("OOK48", "SD Card", "1.0");       // Set disk vendor id, product id and revision with string up to 8, 16, 4 characters respectively 
  usb_msc.setReadWriteCallback(msc_read_cb, msc_write_cb, msc_flush_cb);      // Set read write callback
  usb_msc.setUnitReady(false);      // Still initialize MSC but tell usb stack that MSC is not ready to read/write
  usb_msc.begin();                  // If we don't initialize, board will be enumerated as CDC only

  if (TinyUSBDevice.mounted())      // If already enumerated, additional class driverr begin() e.g msc, hid, midi won't take effect until re-enumeration
  {
    TinyUSBDevice.detach();
    delay(10);
    TinyUSBDevice.attach();
  } 
 
  uint32_t block_count = sd.card()->sectorCount();     // Size in blocks (512 bytes)
  usb_msc.setCapacity(block_count, 512);              // Set disk size, SD block size is always 512
  usb_msc.setUnitReady(true);                         // MSC is ready for read/write
}

// Callback invoked when received READ10 command.
// Copy disk's data to buffer (up to bufsize) and
// return number of copied bytes (must be multiple of block size)
int32_t msc_read_cb (uint32_t lba, void* buffer, uint32_t bufsize) 
{
  bool rc;
  rc = sd.card()->readSectors(lba, (uint8_t*) buffer, bufsize/512);
  return rc ? bufsize : -1;
}

// Callback invoked when received WRITE10 command.
// Process data in buffer to disk's storage and 
// return number of written bytes (must be multiple of block size)
int32_t msc_write_cb (uint32_t lba, uint8_t* buffer, uint32_t bufsize) 
{
  bool rc;
  rc = sd.card()->writeSectors(lba, buffer, bufsize/512);
  return rc ? bufsize : -1;
}

// Callback invoked when WRITE10 command is completed (status received and accepted by host).
// used to flush any pending cache.
void msc_flush_cb (void) 
{
  sd.card()->syncDevice();
  sd.cacheClear();              // clear file system's cache to force refresh
}
