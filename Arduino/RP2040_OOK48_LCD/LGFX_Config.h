// LovyanGFX hardware configuration for the Elecraft HMI board.
//
// This replaces the TFT_eSPI "Setup_Elecraft_HMI.h" user setup file. Unlike
// TFT_eSPI, LovyanGFX is configured here in the sketch, so no library files
// need to be edited.
//
// Hardware (taken from Setup_Elecraft_HMI.h):
//   Display  : ILI9488 320x480, SPI
//   SPI port : spi1  (TFT_SPI_PORT 1)
//   SCLK=10  MOSI=11  MISO=12  CS=9  DC=8  RST=15
//   Backlight: GPIO 18, active HIGH
//   Touch    : XPT2046 on the same SPI bus, T_CS=16
//   SPI clock: 80MHz write (RP2040 rounds down to the nearest achievable rate),
//              20MHz read, 2.5MHz for the touch controller.
//   SD card  : also on spi1 (CS=22), driven by SdFat through the Arduino SPI1 object.

#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ---------------------------------------------------------------------------
// Sharing spi1 with the SD card.
//
// SdFat talks to the SD card through the Arduino SPI1 object. That object
// remembers the clock/format it last set and does not set them again if the
// next transaction asks for the same settings. TFT_eSPI went through SPI1 as
// well, so this worked. LovyanGFX programs the spi1 hardware directly, so
// after drawing, the SD card would be clocked at the display's speed.
//
// To keep them apart, the display bus and the touch driver save the spi1
// clock/format registers before each transaction and put them back afterwards
// (also emptying any unread bytes out of the receive FIFO), so SPI1 always
// finds the hardware exactly as it left it.
// ---------------------------------------------------------------------------
struct SpiRegState
{
  uint32_t cr0;
  uint32_t cpsr;
};

static inline spi_hw_t* lgfxSpiHw(int host) { return host ? spi1_hw : spi0_hw; }

static inline SpiRegState spiSaveRegs(int host)
{
  spi_hw_t* hw = lgfxSpiHw(host);
  return { hw->cr0, hw->cpsr };
}

static inline void spiRestoreRegs(int host, const SpiRegState& st)
{
  spi_hw_t* hw = lgfxSpiHw(host);
  while (hw->sr & SPI_SSPSR_BSY_BITS) {}              // let the last transfer finish
  while (hw->sr & SPI_SSPSR_RNE_BITS) (void)hw->dr;   // discard unread receive data
  hw->icr  = SPI_SSPICR_RORIC_BITS;                   // clear any receive overrun
  hw->cpsr = st.cpsr;
  hw->cr0  = st.cr0;
}

class Bus_SPI_Shared : public lgfx::Bus_SPI
{
public:
  void beginTransaction(void) override
  {
    _saved = spiSaveRegs(config().spi_host);
    lgfx::Bus_SPI::beginTransaction();
  }

  void endTransaction(void) override
  {
    lgfx::Bus_SPI::endTransaction();
    spiRestoreRegs(config().spi_host, _saved);
  }

private:
  SpiRegState _saved = {0, 0};
};

// ---------------------------------------------------------------------------
// XPT2046 touch driver using the same touch validation as TFT_eSPI.
//
// The stock LovyanGFX XPT2046 driver reports a touch as soon as there is any
// contact. On a resistive panel a light touch gives a skewed X/Y reading, so
// the start of a press can land on the wrong key (e.g. "5" entered as "45").
// TFT_eSPI's getTouch()/validTouch() avoided this, and this class copies it:
//   1. wait until the pressure (Z) stops rising, so the reading is taken once
//      the finger has settled, not while it is still pressing down,
//   2. reject the touch unless Z is above a threshold (TFT_eSPI default 600),
//   3. take two readings a few ms apart and reject the touch unless they
//      agree to within TOUCH_RAW_ERR raw counts (TFT_eSPI _RAWERR = 20),
//   4. once a touch is accepted, drop the threshold to 20 for 50ms so a
//      held press doesn't flicker off/on (which could enter a key twice).
// Raise TOUCH_Z_THRESHOLD if light touches still register, lower it if
// firm presses are being missed.
//
// During touch calibration the threshold is lowered to TOUCH_Z_CALIBRATE, as
// TFT_eSPI did (Z_THRESHOLD / 2). The calibration targets sit right in the
// screen corners, where this simple pressure estimate reads low, so at the
// normal threshold the last corner could not be registered.
// ---------------------------------------------------------------------------
#define TOUCH_Z_THRESHOLD  600    // minimum pressure, same scale as TFT_eSPI getTouchRawZ()
#define TOUCH_Z_CALIBRATE  175    // threshold used while calibrating (TFT_eSPI: 350 / 2)
#define TOUCH_Z_HELD       20     // threshold while a press is being held
#define TOUCH_HOLD_MS      50     // how long the held threshold applies after a valid read
#define TOUCH_RAW_ERR      20     // max raw difference between the two readings

class Touch_XPT2046_Filtered : public lgfx::Touch_XPT2046
{
public:
  void setThreshold(uint16_t z) { _zThreshold = z; }

  // Pressure only, equivalent to TFT_eSPI getTouchRawZ(). 0 = not touched.
  uint16_t readZ(void)
  {
    if (!_inited) return 0;
    int x, y, z;
    sample(&x, &y, &z);          // z is valid even when x/y are not
    return (z < 0) ? 0 : z;
  }

  uint_fast8_t getTouchRaw(lgfx::touch_point_t* tp, uint_fast8_t count) override
  {
    if (!_inited || count == 0) return 0;
    tp->size = 0;

    int threshold = (millis() < _pressTime) ? TOUCH_Z_HELD : _zThreshold;

    int x, y, z;
    bool valid = false;
    for (int n = 0; n < 5 && !valid; ++n)    // TFT_eSPI tries up to 5 times
    {
      valid = validTouch(&x, &y, &z, threshold);
    }
    if (!valid)
    {
      _pressTime = 0;
      return 0;
    }
    _pressTime = millis() + TOUCH_HOLD_MS;

    tp->x    = x;
    tp->y    = y;
    tp->size = z;
    return 1;
  }

private:
  uint32_t _pressTime = 0;
  uint16_t _zThreshold = TOUCH_Z_THRESHOLD;

  bool validTouch(int* x, int* y, int* z, int threshold)
  {
    int x1, y1, z1, x2, y2, z2;

    // Wait until pressure stops increasing (limited so it can't hang)
    int zPrev = -1;
    z1 = 0;
    for (int i = 0; i < 20; ++i)
    {
      if (!sample(&x1, &y1, &z1)) return false;
      if (z1 <= zPrev) break;
      zPrev = z1;
      delay(1);
    }
    if (z1 <= threshold) return false;

    delay(1);
    if (!sample(&x1, &y1, &z1) || z1 <= threshold) return false;
    delay(2);
    if (!sample(&x2, &y2, &z2) || z2 <= threshold) return false;

    if (abs(x1 - x2) > TOUCH_RAW_ERR || abs(y1 - y2) > TOUCH_RAW_ERR) return false;

    *x = (x1 + x2) / 2;
    *y = (y1 + y2) / 2;
    *z = (z1 + z2) / 2;
    return true;
  }

  // One burst of 7 Y/Z1/X/Z2 conversions from the XPT2046.
  void burst(uint8_t* data)
  {
    memset(data, 0, 8);
    data[0] = 0x91;   // Y
    data[2] = 0xB1;   // Z1
    data[4] = 0xD1;   // X
    data[6] = 0xC1;   // Z2
    data[56] = 0x80;  // power down after the last conversion
    memcpy(&data[ 8], data,  8);
    memcpy(&data[16], data, 16);
    memcpy(&data[32], data, 24);

    SpiRegState saved = spiSaveRegs(_cfg.spi_host);   // keep the SD card's SPI settings
    lgfx::spi::beginTransaction(_cfg.spi_host, _cfg.freq, 0);
    if (_cfg.pin_cs >= 0) lgfx::gpio_lo(_cfg.pin_cs);
    lgfx::spi::readBytes(_cfg.spi_host, data, 57);
    if (_cfg.pin_cs >= 0) lgfx::gpio_hi(_cfg.pin_cs);
    lgfx::spi::endTransaction(_cfg.spi_host);
    spiRestoreRegs(_cfg.spi_host, saved);
  }

  // One burst, returning the median X, Y and Z.
  // Z is calculated the same way as TFT_eSPI: 4095 + Z1 - Z2 (0 = no touch).
  bool sample(int* x, int* y, int* z)
  {
    uint8_t data[57];
    burst(data);

    int xt[7], yt[7], zt[7];
    int ix = 0, iy = 0, iz = 0;
    for (int j = 0; j < 7; ++j)
    {
      const uint8_t* d = &data[j * 8];
      int ry  = (d[1] << 8 | d[2]) >> 3;
      int rz1 = (d[3] << 8 | d[4]) >> 3;
      int rx  = (d[5] << 8 | d[6]) >> 3;
      int rz2 = (d[7] << 8 | d[8]) >> 3;
      if (rx > 128 && rx <= 3968) xt[ix++] = rx;
      if (ry > 128 && ry <= 3968) yt[iy++] = ry;
      int rz = 4095 + rz1 - rz2;
      if (rz >= 4095) rz = 0;            // as TFT_eSPI: 4095 means no touch
      zt[iz++] = rz;
    }
    *z = median(zt, iz);
    if (ix < 3 || iy < 3) return false;

    *x = median(xt, ix);
    *y = median(yt, iy);
    return true;
  }

  static int median(int* v, int n)
  {
    for (int i = 1; i < n; ++i)           // insertion sort, n <= 7
    {
      int t = v[i], k = i - 1;
      while (k >= 0 && v[k] > t) { v[k + 1] = v[k]; --k; }
      v[k + 1] = t;
    }
    return v[n >> 1];
  }
};

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9488   _panel_instance;
  Bus_SPI_Shared        _bus_instance;     // spi1 bus that leaves the SD card's settings alone
  lgfx::Light_PWM       _light_instance;
  Touch_XPT2046_Filtered _touch_instance;   // filtered XPT2046 driver (see above)

public:
  // Change the touch pressure threshold (e.g. lower it while calibrating)
  void setTouchThreshold(uint16_t z) { _touch_instance.setThreshold(z); }

  // LovyanGFX resets the SPI peripheral during init, so keep the SD card's
  // spi1 settings (SdFat has already started it by now) across init().
  bool init(void)
  {
    SpiRegState saved = spiSaveRegs(1);
    bool ok = lgfx::LGFX_Device::init();
    spiRestoreRegs(1, saved);
    return ok;
  }

  // TFT_eSPI-compatible touch helpers used by this sketch
  uint16_t getTouchRawZ(void) { return _touch_instance.readZ(); }

  using lgfx::LGFX_Device::getTouch;
  uint_fast8_t getTouch(uint16_t* x, uint16_t* y, uint16_t threshold)
  {
    if (threshold < 20) threshold = 20;
    _touch_instance.setThreshold(threshold);
    uint_fast8_t r = lgfx::LGFX_Device::getTouch(x, y);
    _touch_instance.setThreshold(TOUCH_Z_THRESHOLD);
    return r;
  }

  LGFX(void)
  {
    { // SPI bus
      auto cfg = _bus_instance.config();
      cfg.spi_host   = 1;            // RP2040: 0 = spi0, 1 = spi1
      cfg.spi_mode   = 0;
      cfg.freq_write = 80000000;     // SPI_FREQUENCY
      cfg.freq_read  = 20000000;     // SPI_READ_FREQUENCY
      cfg.pin_sclk   = 10;           // TFT_SCLK
      cfg.pin_mosi   = 11;           // TFT_MOSI
      cfg.pin_miso   = 12;           // TFT_MISO
      cfg.pin_dc     = 8;            // TFT_DC
      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    { // Display panel
      auto cfg = _panel_instance.config();
      cfg.pin_cs           = 9;      // TFT_CS
      cfg.pin_rst          = 15;     // TFT_RST
      cfg.pin_busy         = -1;
      cfg.panel_width      = 320;    // TFT_WIDTH
      cfg.panel_height     = 480;    // TFT_HEIGHT
      cfg.memory_width     = 320;
      cfg.memory_height    = 480;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;      // adjust (0-7) if the picture is rotated/mirrored
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable         = true;
      cfg.invert           = false;  // set true if white shows as black
      cfg.rgb_order        = false;  // set true if red and blue are swapped
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;   // the touch controller shares this SPI bus
      _panel_instance.config(cfg);
    }

    { // Backlight
      auto cfg = _light_instance.config();
      cfg.pin_bl      = 18;          // TFT_BL
      cfg.invert      = false;       // TFT_BACKLIGHT_ON HIGH
      cfg.freq        = 44100;       // not used by the RP2040 driver
      cfg.pwm_channel = 0;           // GPIO18 is PWM slice 1 channel A (0)
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    { // Touch controller (XPT2046)
      auto cfg = _touch_instance.config();
      cfg.x_min           = 0;       // raw range before calibration; the
      cfg.x_max           = 4095;    // calibration stored in EEPROM replaces this
      cfg.y_min           = 0;
      cfg.y_max           = 4095;
      cfg.pin_int         = -1;
      cfg.bus_shared      = true;
      cfg.offset_rotation = 0;
      cfg.spi_host        = 1;       // same SPI port as the display
      cfg.freq            = 2500000; // SPI_TOUCH_FREQUENCY
      cfg.pin_sclk        = 10;
      cfg.pin_mosi        = 11;
      cfg.pin_miso        = 12;
      cfg.pin_cs          = 16;      // TOUCH_CS
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};
