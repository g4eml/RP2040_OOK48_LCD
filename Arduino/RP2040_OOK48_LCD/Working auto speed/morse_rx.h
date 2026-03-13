#pragma once

#include <stdint.h>
#include <string.h>

extern float morseWpmCurrent;

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
// Speed tracking (PLL)
// ---------------------------------------------------------------------------
// How often (in completed runs) the background WPM estimator is re-run.
static constexpr int   MORS_REESTIMATE_INTERVAL    = 4;

// Recent-run ring used by the background estimator.
static constexpr int   MORS_RUN_RING_SIZE          = 64;

// Alpha filters for PLL correction.
static constexpr float MORS_ALPHA_DOT              = 0.08f;
static constexpr float MORS_ALPHA_DASH             = 0.20f;
static constexpr float MORS_ALPHA_SPACE            = 0.06f;
static constexpr float MORS_ALPHA_INTRA_SPACE      = 0.03f;

// PLL clamp: normal operating range.
static constexpr float MORS_PLL_LO_FRAC            = 0.70f;
static constexpr float MORS_PLL_HI_FRAC            = 1.40f;

// Wider clamp used while coasting (low confidence).
// Lets the PLL drift enough to re-converge on a speed change.
static constexpr float MORS_COAST_LO_FRAC          = 0.40f;
static constexpr float MORS_COAST_HI_FRAC          = 2.50f;

// ---------------------------------------------------------------------------
// Confidence
// ---------------------------------------------------------------------------
// Confidence is a 0..1 score maintained per run.
//   >= CONF_HIGH  →  isLocked() = true, tight PLL clamp
//   <  CONF_HIGH  →  coasting: wide PLL clamp, background re-estimator active
//   == 0          →  LOST event emitted; _unitEst preserved for fast re-lock
//
// Each plausible run raises confidence by CONF_RISE (capped at 1.0).
// Each implausible run lowers it by CONF_FALL.
static constexpr float MORS_CONF_HIGH              = 0.55f;
static constexpr float MORS_CONF_RISE              = 0.12f;
static constexpr float MORS_CONF_FALL              = 0.18f;

// After this many dit-lengths with no mark, confidence is forced to zero
// and a LOST event is emitted. _unitEst is kept.
// 120 dits @ 20 WPM / 36 fps ≈ 7 s — survives a deliberate pause between
// overs without falsely declaring lost.
static constexpr int   MORS_LOST_TIMEOUT_DITS      = 120;

// ---------------------------------------------------------------------------
// Speed-change detection (bias streak)
// ---------------------------------------------------------------------------
// Each mark PLL correction votes +1 (unitEst moving up) or -1 (moving down).
// When the streak magnitude reaches BIAS_TRIGGER the decoder treats it as a
// genuine speed change: it immediately runs an aggressive re-estimate and
// widens the PLL clamp in the direction of the bias so _unitEst can travel
// the full distance in one step.
//
// BIAS_TRIGGER = 3 means three consecutive same-direction mark corrections
// trigger the fast response — roughly one full character (e.g. 'S' = 3 dots).
// BIAS_BLEND_BASE is the re-estimate blend fraction at trigger; it increases
// by BIAS_BLEND_STEP for each additional streak run beyond the trigger.
// Capped at BIAS_BLEND_MAX so we never make a completely unconstrained jump.
static constexpr int   MORS_BIAS_TRIGGER           = 3;
static constexpr float MORS_BIAS_BLEND_BASE        = 0.50f;
static constexpr float MORS_BIAS_BLEND_STEP        = 0.10f;
static constexpr float MORS_BIAS_BLEND_MAX         = 0.85f;


static constexpr float MORS_MORPH_THRESH_FRAC      = 0.38f;
static constexpr float MORS_SPACE_WORD_WEIGHT      = 0.15f;
static constexpr float MORS_SPACE_LETTER_WEIGHT    = 0.30f;
static constexpr float MORS_HIST_REWARD            = 0.40f;
static constexpr float MORS_HIST_TOL_FRAC          = 0.35f;

// ---------------------------------------------------------------------------
// Word-gap threshold (in dit units)
// ---------------------------------------------------------------------------
static constexpr float MORS_WORD_GAP_THR           = 6.0f;

// ---------------------------------------------------------------------------
// Fixed-size circular buffer
// ---------------------------------------------------------------------------
template<typename T, int N>
struct MorsRing {
    T   data[N];
    int head = 0;
    int cnt  = 0;

    void push(T v) {
        data[(head + cnt) % N] = v;
        if (cnt < N) cnt++;
        else          head = (head + 1) % N;
    }
    T    operator[](int i) const { return data[(head + i) % N]; }
    void clear() { head = 0; cnt = 0; }
    int  size()  const { return cnt; }
};

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------
enum class MorseEvt : uint8_t { NONE, CHAR, WORD_SEP, LOCKED, LOST };

struct MorseEvent {
    MorseEvt kind = MorseEvt::NONE;
    char     ch   = 0;
    float    wpm  = 0.0f;
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

    void begin(int frameRate, float wpmMin, float wpmMax, int toneBin);
    void reset();

    // Feed one FFT-bin magnitude per frame.
    // Returns number of events generated; retrieve with event().
    int        feed(float mag);
    MorseEvent event(int i) const;

    // isLocked() is true when confidence >= MORS_CONF_HIGH.
    bool  isLocked()   const { return _confidence >= MORS_CONF_HIGH; }
    float lockedWpm()  const { return _lockedWpm; }

    // Diagnostics
    float envPeak()    const { return _envPeak; }
    float envValley()  const { return _envValley; }
    float confidence() const { return _confidence; }

private:
    // --- config ---
    int   _frameRate = 36;
    float _wpmMin    = 5.0f;
    float _wpmMax    = 35.0f;
    int   _toneBin   = 22;

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

    // --- run tracking ---
    int _curState = 0;
    int _curLen   = 0;
    int _runCount = 0;   // total runs since Schmitt became valid; drives re-estimator

    MorsRing<RunEntry, MORS_RUN_RING_SIZE> _runBuf;

    // --- speed PLL ---
    float _unitEst   = 0.0f;   // current dit-length in frames (persists across LOST)
    float _unitMin   = 0.0f;
    float _unitMax   = 0.0f;
    float _lockedWpm = 0.0f;

    // --- confidence ---
    float _confidence = 0.0f;
    bool  _wasLocked  = false;  // for LOCKED event edge detection

    // --- symbol accumulation ---
    char _symbol[8] = {};
    int  _symLen    = 0;

    // --- silence watchdog ---
    int _framesSinceMark = 0;

    // --- speed-change bias streak ---
    // Signed counter of consecutive mark PLL corrections in the same direction.
    // Positive = corrections pushing unitEst up (speed went down).
    // Negative = corrections pushing unitEst down (speed went up).
    // Reset to zero on any direction reversal or after an aggressive re-estimate.
    int _biasStreak = 0;

    // --- event queue (cleared at start of each feed()) ---
    static constexpr int MAX_EVENTS = 8;
    MorseEvent _events[MAX_EVENTS];
    int        _evtCount = 0;

    // --- internal methods ---
    void  _updateEnvelope(float mag);
    void  _updateSchmitt();
    int   _schmittStep(float val);
    bool  _updateRun(int bit, RunEntry &out);
    void  _trackStep(int runState, int runLen);
    void  _reestimateSpeed(bool aggressive);
    void  _applySpeedEstimate(float wpm);
    void  _emitSymbol();
    void  _pushEvent(MorseEvent ev);

    float       _ditFrames(float wpm) const { return 1.2f / wpm * _frameRate; }
    void        _morphFilter(RunEntry *runs, int &count, int minRun);
    void        _estimateWpm(const RunEntry *runs, int count, float &bestWpm, float &bestConf);
    static char _morseToChar(const char *sym);
};
