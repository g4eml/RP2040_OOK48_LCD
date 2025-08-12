# RP2040 OOK48 LCD

## Description

This Project is an experimental test for a new Synchronous On Off Keying mode for use with weak signal microwave contacts.
The protocol is based on an idea from Andy G4JNT with input from several other UK Microwave Group Members. 
It is described in the document https://github.com/g4eml/RP2040_OOK48_LCD/blob/main/Documents/OOK48%20Protocol.pdf.

## Features

- Uses On/Off keying which is easier to implement than Frequency Shift keying on the higher microwave bands. 

- The data rate is similar to Morse code, so the module can be connected directly to the CW key input of any radio. 

- Real time visual confirmation of the message, character by character. No need to wait 30 seconds to see if the reception has succeeded.

- Uses the GPS 1 Pulse per second signal for accurate character synchronisation.  

- Optional 2 Seconds per character mode which increases the decode sensitiviy slightly. 

- Stand alone Device with LCD touch screen display. 

- No special programming hardware or software required. 

- Programming using the RP2040s built in standard boot loader. 

- 10 Preset Messages are saved to EEPROM for automatic load on power on. 

- Automatic calculation of QTH Lacator. Selectable for 6, 8 or 10 character accuracy. This locator can be inserted in any of the 10 Preset Messages. 

- Recording of messages to SD card for later viewing on a PC.

- USB Drive mode to allow downloading of SD card files to a PC. 

## Operation Description

### Display
The display is split into 4 main areas. On the left are the Spectrum display and Waterfall. These are used to tune the reciever to the correct frequency. 

The Spectrum and Waterfall span from 500 Hz to 1100 Hz. Underneath the spectrum display there is an orange band which indicates the correct frequency range for decoding (centred on 800 Hz) . The receiver needs to be tuned such that the received tone falls withing this orange band. 

Received messages will appear at one character per second on the right hand side of the display. 

When Transmitting the Spectrum display is replaced with a RED box and the Text 'TX'. The transmitted message appears on the right hand side in red as it is sent. 

### Controls

At the bottom of the screen there are 6 touch buttons. Only 5 of these are currently in use. 

Clear.   Clears the Message display.

Config. Displays a config page which allows settings to be changed.  

Record Button (Red Circle).  Displayed when a FAT formatted SD card is fitted. Click to start recording of text to the SD card. (GPS must be active). The filename will be based on the current date and time DDMMYY-HHMMSS.txt.

Stop Button (White Square). Displayed when recording is in progress. Click to stop recording. 

Set Tx.  This shows a menu of the 10 stored messages to be used for transmit. Selecting a messsage allows it to be edited on the next screen. Pressing the Enter Button saves the message.

Tx / Rx   Starts and stops the transmission of the currently selected Message. 

Touching the Waterfall display area of the screen will cycle through a selection of Tone detection ranges indicated by the width of the orange band. 
Available ranges are +-50 Hz, +_ 100Hz and the full width 500-1100 Hz.
Wider ranges require less accurate tuning but have a slightly higher chance of errors with weak signals. 

A 'Factory Reset' can be done by holding your finger on the display while powering on. This will clear the EEPROM to its default values and run the screen calibration routine. 

## USB Drive Mode. 

If an SD card is present there will be an additonal line visible on the Config Page. 'Activate USB Drive Mode" .  Clicking on this button will activate the USB Memory stick emulation mode. You can then connect the HMI module to a PC and the contents of its SD card will appear as a USB drive. This allows the download and deletion of the saved text files without removing the SD card. 

This mode can pnly be exited by powering off the module. 

## Config page settings

- Set Locator Length.  Allows selection of 6, 8 or 10 digit locators.

- Character Period. Selects the normal 1S per character or the optional 2 Seconds per character. Both ends of the link need to have the same settings. 

- GPS Baud Rate. Selects 9600 Baud for the older GPS modules or 38400 for more recent modules. 

- Decode Mode. Selects Normal or Alternate mode. Normal mode is tolerant of frequency drift and chirping. Alternate mode needs a very stable drift free tone but is slightly more sensitive.

- Tx Timing Advance. Allows a fixed delay to be added to compensate for signal processing delays in the Tx chain. (such as DSP audio processing)

- Rx Timing Retard. Allows a fixed delay to be added to compensate for any signal processing delays in the Rx chain. (such as noise reduction)

- Activate USB Drive Mode.  Allows the module to be connected to a PC where it will appear as a USB drive allowing the contents of the SD card to be read or deleted. The module must be powered off to exit this mode. 

- Exit. Return to the previous screen.

- Version Number. 

- Voltage Display. This shows the current battery voltage.  This reading may need calibrating for each module. Power the module using the USB port and disconnect or turn off the battery. The voltage should then read 4.20V. 
If calibration is required, just touch the voltage area of the screen and it will automatically calibrate to read 4.20V. Reconnect the battery to view the battery voltage. 


## Hardware Requirements

This code is designed to work with the Elecrow CrowPanel Pico-3.5 inch 480x320 TFT LCD HMI Module. https://www.aliexpress.com/item/1005007250778536.html 

![hmi](https://github.com/user-attachments/assets/27250811-edb9-4df4-908e-7b8d27edb42c)


Note:- similar HMI Panels are available using the ESP32 processor chip. Make sure that you are purchasing the RP2040 version. 

A GPS module is also essential and must have a 1 Pulse per second output. This pulse is used to synchronise the start of each character. 

![gps](https://github.com/user-attachments/assets/09e46324-8409-4898-bb8c-1557b216c92c)


## Connecting

The receiver is connected to the HMI module via a simple level shifting interface. Details are shown in this document [Connections](Documents/Schematic.pdf)

The  GPS module can be connected the Connector on the top of the module as per the schematic. Alternatively it can be connected to the UART1 connector on the lower edge of the MHI Module. However the 1PPS signal will still need to be connected to the top connector. 

The power is provided by the USB-C connector marked 'USB' on the end of the HMI module, or optionally by a 3.7V lithium cell connected to the BAT connector. 


## Programming or updating the HMI Module (quick method) 

1. Locate the latest compiled firmware file 'RP2040_OOK48_LCD.uf2' which will be found here https://github.com/g4eml/RP2040_OOK48_LCD/releases and save it to your desktop. 

2. Connect the HMI Module to your PC using the USB-C port on the side. 

3. Hold down the BOOT button on the back of the HMI module and briefly press the Reset button. The RP2040 should appear as a USB disk drive on your PC.

3. Copy the .uf2 file onto the USB drive. The RP2040 will recognise the file and immediately update its firmware and reboot.

## Building your own version of the firmware (longer method and not normally required unless you need to make changes)

The RP2040 is programmed using the Arduino IDE with the Earl F. Philhower, III  RP2040 core. 

#### Installing the Arduino IDE

1. Download and Install the Arduino IDE 2.3.0 from here https://www.arduino.cc/en/software

2. Open the Arduino IDE and go to File/Preferences.

3. in the dialog enter the following URL in the 'Additional Boards Manager URLs' field: https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json

4. Hit OK to close the Dialog.

5. Go to Tools->Board->Board Manager in the IDE.

6. Type “RP2040” in the search box.

7. Locate the entry for 'Raspberry Pi Pico/RP2040 by Earle F. Philhower, III' and click 'Install'

### Installing the required libraries

1. From the Arduino IDE select (Tools > Manage Libraries)
2. Search for 'arduinoFFT' Scroll down to find the arduinoFFT library by Enrique Condes.
3. Click Install
4. Now search for 'TFT_eSPI' and find the TFT graphics library by Bodmer.
5. Click Install
6. Now search for 'SdFat - Adafruit Fork" and find the library by Bill Greiman.
7. Click Install

#### Downloading the Software.

1. Download the latest released source code .zip file from https://github.com/g4eml/RP2040_OOK48_LCD/releases

2. Save it to a convenient location and then unzip it. 

The TFT_eSPI Library is unusual in that it needs to be configured to the TFT display in use by modifying library files. 
The required modified versions of the files are located in the 'LCD-eSPI Settings' folder of this repository. 
Copy the files 'User_Setup.h' and 'User_Setup_Select.h' from  the downloaded 'LCD-eSPI Settings' folder to your Arduino libraries directory.
This will normaly be found at 'Documents/Arduino/libraries/TFT_eSPI'

#### Programming the RP2040

1. Open the Arduino IDE and click File/Open

2. Navigate to the File RP2404Synth/RP2040_OOK48_LCD.ino (downloaded in the previous step) and click Open.

3. Select Tools and make the following settings.

   Board: "Raspberry Pi Pico"

   Port: "UF2 Board"
   
   Debug Level: "None"

   Debug Port: "Disabled"

   C++ Exceptions: "Disabled"

   Flash Size: "2Mb (no FS)"

   CPU Speed: "200MHz"

   IP/Bluetooth Stack: "IPV4 Only"

   Optimise: "Small (-Os)(standard)"

   RTTI: "Disabled"

   Stack Protection: "Disabled"

   Upload Method: "Default (UF2)"

   USB Stack: "Adafruit TinyUSB"  

5. Connect the HMI Module to the USB port, hold down the BOOT button and briefly press the reset Button. 

6. Click Sketch/Upload.

The Sketch should compile and upload automatically to the Pico. If the upload fails you may need to disconnect the module and then hold down the BOOT button while reconnecting. 

## Connections

The receiver audio is connected using a simple CR network to GPIO Pins GND, 3V3 and 28 on the top edge of the HMI Module. 
Details of this interface are in this file.  ![Interface](Documents/Schematic.pdf)

IO Pin 6 is connected to to gate of an N channel Mosfet. The drain of this Mosfet is connected to the PTT input of the radio.
IO Pin 7 is connected to to gate of an N channel Mosfet. The drain of this Mosfet is connected to the Key input of the radio.   

The firmware requires the connection of a GPS module. This is used to accurately set the time and to generate the 1 Pulse per second signal used to synchromise the satrt of each character. Any GPS module with a 3V3 output and a 1PPS output can be used. It needs to output NMEA data at 9600 Baud or 38400 Baud. One of the G10A-F30 modules was used for development.
GPS data to the HMI module is connected to IO pin 5.
GPS data from the HMI module is connected to IO pin 4.
1 PPS pulse from the GPS module is conneced to IO pin 3.


## 3D Printed Enclosure

Files for a 3D printable enclosure are included in the Enclosure directory. 

Additional parts required.
- 3 Phono Sockets for Audio Input, Key Output and PTT Output
- PAM8403 amplifier module with volume control. Wired in parallel with input audio to allow monitoring of the signal 
- 28mm Speaker 
- 18650 Lithium Cell. (can be switched with the audio amp voulme control on/off switch)

  
![Pam2303](https://github.com/user-attachments/assets/8c209f55-b711-41c5-a5d7-33df65cf7edd)
![Speaker](https://github.com/user-attachments/assets/70e4076a-ec9e-46bb-a499-0e55d69f436d) 

