#pragma once

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Dual-envelope AGC
// ---------------------------------------------------------------------------
// Peak envelope
//   attack  : ~2 frames      — grabs new peaks almost instantly (sub-dit)
//   decay   : τ ≈ 1/0.012 ≈ 83 frames ≈ 2.3 s
// Valley envelope
//   attack (downward) : τ ≈ 1/0.020 = 50 frames ≈ 1.4 s
//   decay  (upward)   : τ ≈ 1/0.001 = 1000 frames ≈ 28 s
//   Ratio 20:1 keeps valley anchored to noise floor during long dash runs.
// ---------------------------------------------------------------------------
static constexpr float MORS_ENV_PEAK_ATTACK        = 0.50f;
static constexpr float MORS_ENV_PEAK_DECAY         = 0.012f;
static constexpr float MORS_ENV_VALLEY_ATTACK      = 0.020f;
static constexpr float MORS_ENV_VALLEY_DECAY       = 0.001f;

// Frames spent collecting hard min/max before leaky integrators take over.
// 10 frames @ 36 fps = 0.28 s — covers ~2 dits at 10 WPM.
static constexpr int   MORS_ENV_SEED_FRAMES        = 10;

// Minimum peak/valley ratio for Schmitt trigger to be valid.
static constexpr float MORS_ENV_MIN_SNR            = 2.5f;

// ---------------------------------------------------------------------------
// Schmitt trigger
// ---------------------------------------------------------------------------
// Hysteresis as a fraction of peak-to-valley range.
static constexpr float MORS_SCHMITT_HYST_FRAC      = 0.12f;

// Midpoint bias: negative shifts threshold down, favouring mark-heavy signals.
static constexpr float MORS_SCHMITT_CENTRE_BIAS    = -0.05f;

// ---------------------------------------------------------------------------
// Decode thresholds
// ---------------------------------------------------------------------------
// A mark run measuring less than this fraction of _unitFrames is treated as
// noise and ignored (confidence penalty but no symbol output).
static constexpr float MORS_MARK_MIN_FRAC          = 0.4f;

// A mark run above this multiple of _unitFrames is implausible (longer than
// a dash with generous tolerance).
static constexpr float MORS_MARK_MAX_FRAC          = 4.5f;

// Dot/dash boundary: runs below this threshold (in units) are dots.
static constexpr float MORS_DOT_DASH_BOUNDARY      = 2.0f;

// Space runs shorter than this (in units) are intra-character gaps.
// At or above this threshold the current symbol is emitted (inter-char gap).
static constexpr float MORS_INTER_CHAR_UNITS       = 2.0f;

// Space runs at or above this threshold are word gaps.
static constexpr float MORS_WORD_GAP_THR           = 6.0f;

// ---------------------------------------------------------------------------
// Confidence
// ---------------------------------------------------------------------------
// Confidence is a 0..1 score updated on each completed run.
//   >= CONF_HIGH  →  isActive() returns true
//   == 0          →  SIGNAL_LOST event emitted
//
// Each plausible mark run raises confidence by CONF_RISE.
// Each implausible mark run lowers it by CONF_FALL.
// Space runs of any length do not affect confidence.
static constexpr float MORS_CONF_HIGH              = 0.55f;
static constexpr float MORS_CONF_RISE              = 0.12f;
static constexpr float MORS_CONF_FALL              = 0.18f;

// After this many dit-lengths with no mark the signal is considered lost.
// 120 dits @ 20 WPM / 36 fps ≈ 7 s — survives pauses between overs.
static constexpr int   MORS_LOST_TIMEOUT_DITS      = 120;

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------
enum class MorseEvt : uint8_t { NONE, CHAR, WORD_SEP, SIGNAL_ACQUIRED, SIGNAL_LOST };

struct MorseEvent {
    MorseEvt kind = MorseEvt::NONE;
    char     ch   = 0;      // valid for CHAR and WORD_SEP
    float    wpm  = 0.0f;   // valid for SIGNAL_ACQUIRED
};

// ---------------------------------------------------------------------------
// Internal run entry
// ---------------------------------------------------------------------------
struct RunEntry {
    int8_t  state;
    int16_t len;
};

// ---------------------------------------------------------------------------
// Decoder
// ---------------------------------------------------------------------------
class MorseRxDecoder {
public:
    MorseRxDecoder() {}

    // Call once before use.
    //   frameRate : FFT frames per second (typically 36)
    //   wpm       : expected signal speed in words per minute
    //   toneBin   : FFT bin index of the target tone (informational only)
    void begin(int frameRate, float wpm, int toneBin);

    // Full reset — clears envelope state, symbol buffer and confidence.
    void reset();

    // Change the expected speed at any time.
    // Takes effect immediately; any partially-accumulated symbol is flushed.
    void setWpm(float wpm);

    // Feed one FFT-bin magnitude per frame.
    // Returns the number of events generated; retrieve with event().
    int        feed(float mag);
    MorseEvent event(int i) const;

    // True when confidence >= MORS_CONF_HIGH (signal present and decoding).
    bool  isActive()    const { return _confidence >= MORS_CONF_HIGH; }
    float wpm()         const { return _wpm; }

    // Diagnostics
    float envPeak()     const { return _envPeak; }
    float envValley()   const { return _envValley; }
    float confidence()  const { return _confidence; }

private:
    // --- config ---
    int   _frameRate   = 36;
    float _wpm         = 12.0f;
    float _unitFrames  = 0.0f;   // dit length in frames = 1.2 / wpm * frameRate
    int   _toneBin     = 22;

    // --- dual-envelope AGC ---
    int   _envFrames = 0;
    float _envPeak   = 0.0f;
    float _envValley = 0.0f;

    // --- Schmitt trigger ---
    int   _schmittState = 0;
    float _schmittLo    = 0.0f;
    float _schmittHi    = 0.0f;
    bool  _schmittValid = false;
    int   _schmittFrame = 0;

    // --- run-length encoder ---
    int _curState = 0;
    int _curLen   = 0;

    // --- symbol accumulation ---
    char _symbol[8] = {};
    int  _symLen    = 0;

    // --- confidence ---
    float _confidence  = 0.0f;
    bool  _wasActive   = false;   // for event edge detection

    // --- silence watchdog ---
    int _framesSinceMark = 0;

    // --- event queue (cleared at start of each feed()) ---
    static constexpr int MAX_EVENTS = 8;
    MorseEvent _events[MAX_EVENTS];
    int        _evtCount = 0;

    // --- internal methods ---
    void  _updateEnvelope(float mag);
    void  _updateSchmitt();
    int   _schmittStep(float val);
    bool  _updateRun(int bit, RunEntry &out);
    void  _decodeRun(int runState, int runLen);
    void  _emitSymbol();
    void  _pushEvent(MorseEvent ev);

    float       _ditFrames(float wpm) const { return 1.2f / wpm * (float)_frameRate; }
    static char _morseToChar(const char *sym);
};
