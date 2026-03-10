#pragma once

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Tunable constants  (ported from Python StreamingMorseDecoder)
// ---------------------------------------------------------------------------
static constexpr int   MORS_MIN_ACQUIRE_MARK_RUNS  = 3;
static constexpr int   MORS_REESTIMATE_INTERVAL    = 6;
static constexpr int   MORS_ACQUIRE_RING_SIZE      = 400;   // ~11s at 36fps
static constexpr float MORS_LOCK_THRESHOLD         = 0.65f;
static constexpr float MORS_LOCK_THRESHOLD_FAST    = 0.55f;

// ---------------------------------------------------------------------------
// Dual-envelope AGC constants
//
// The envelope tracker replaces the old P20 histogram.  Two leaky-integrator
// envelopes are run in parallel:
//
//   _envPeak  – fast attack, slow decay  → tracks the signal ceiling
//   _envValley– slow attack, fast decay  → tracks the noise floor
//
// Both use asymmetric alphas so they respond quickly in the "grabbing" direction
// but bleed off slowly in the "releasing" direction.
//
// Peak envelope
//   attack  : ~1 frame   (new peak grabbed almost instantly)
//   release : τ ≈ 1/0.002 = 500 frames ≈ 14 s at 36 fps
// Peak envelope
//   attack  : ~2 frames      — grabs new peaks almost instantly (sub-dit)
//   decay   : τ ≈ 1/0.012 ≈ 83 frames ≈ 2.3 s — releases over a few seconds
//             of silence; fast enough to follow a fading/stopping signal
// Valley envelope
//   attack (downward) : τ ≈ 1/0.020 = 50 frames ≈ 1.4 s
//             Responds within 2–3 word spaces at 20 WPM (word space ≈ 15 frames)
//             so the noise floor estimate settles quickly after signal starts.
//   decay  (upward)   : τ ≈ 1/0.001 = 1000 frames ≈ 28 s
//             A 20 WPM dash is ~7 frames → valley creeps up only 0.7% of range
//             per dash, so marks barely disturb it. Recovers from noise dips
//             in ~30 s which is acceptable.
//   Ratio attack/decay = 20× ensures valley stays anchored to noise floor
//   even during long runs of dashes.
// ---------------------------------------------------------------------------
static constexpr float MORS_ENV_PEAK_ATTACK        = 0.50f;   // fraction toward new peak per frame
static constexpr float MORS_ENV_PEAK_DECAY         = 0.012f;  // τ ≈ 83 frames ≈ 2.3 s
static constexpr float MORS_ENV_VALLEY_ATTACK      = 0.020f;  // τ ≈ 50 frames ≈ 1.4 s  (downward)
static constexpr float MORS_ENV_VALLEY_DECAY       = 0.001f;  // τ ≈ 1000 frames ≈ 28 s (upward)

// Minimum peak/valley ratio before the Schmitt trigger is declared valid.
// Lowered from the old 6.0 to work better on weak signals.
static constexpr float MORS_ENV_MIN_SNR            = 2.5f;

// Schmitt hysteresis expressed as a fraction of the peak-to-valley range.
// Wider hysteresis → noisier signals need a cleaner transition.
// 0.10–0.15 is a good range; 0.12 is unchanged from before.
static constexpr float MORS_SCHMITT_HYST_FRAC      = 0.12f;

// Fraction of (peak−valley) by which the Schmitt midpoint is shifted.
// Negative = shift threshold down toward the valley, making it easier to
// detect marks on signals that spend more time high than low (typical CW).
// Zero = symmetric midpoint. Keep the magnitude small (0.0–0.10).
static constexpr float MORS_SCHMITT_CENTRE_BIAS    = -0.05f;

// Number of frames spent in the seeding phase before the leaky integrators
// take over.  Should cover at least 1–2 dits at the slowest expected speed
// so both a mark and a space are observed.
// At 10 WPM / 36 fps: 1 dit ≈ 4.3 frames → 10 frames covers ~2 dits.
static constexpr int   MORS_ENV_SEED_FRAMES        = 10;

// --- the rest of the constants are unchanged ---
static constexpr float MORS_MORPH_THRESH_FRAC      = 0.38f;

static constexpr float MORS_SPACE_WORD_WEIGHT      = 0.15f;
static constexpr float MORS_SPACE_LETTER_WEIGHT    = 0.30f;
static constexpr float MORS_HIST_REWARD            = 0.40f;
static constexpr float MORS_HIST_TOL_FRAC          = 0.35f;

static constexpr float MORS_ALPHA_MARK             = 0.12f;
static constexpr float MORS_ALPHA_DOT              = 0.08f;
static constexpr float MORS_ALPHA_DASH             = 0.20f;
static constexpr float MORS_ALPHA_SPACE            = 0.06f;
static constexpr float MORS_ALPHA_INTRA_SPACE      = 0.03f;
static constexpr float MORS_PLL_LO_FRAC            = 0.70f;
static constexpr float MORS_PLL_HI_FRAC            = 1.40f;

static constexpr float MORS_WORD_GAP_THR           = 6.0f;
static constexpr int   MORS_LOST_TIMEOUT_DITS      = 60;

// ---------------------------------------------------------------------------
// Fixed-size circular buffer  (replaces Python deque)
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
    char     ch   = 0;     // CHAR / WORD_SEP payload
    float    wpm  = 0.0f;  // LOCKED payload
};

// ---------------------------------------------------------------------------
// Internal run entry
// ---------------------------------------------------------------------------
struct RunEntry {
    int8_t  state;
    int16_t len;
};

// ---------------------------------------------------------------------------
// Decoder state
// ---------------------------------------------------------------------------
enum class MorseState : uint8_t { ACQUIRE, LOCKED };

class MorseRxDecoder {
public:
    MorseRxDecoder() {}

    void begin(int frameRate, float wpmMin, float wpmMax, int toneBin);
    void reset();

    // Feed one FFT frame magnitude for the tone bin.
    // Returns the number of events generated (retrieve with event()).
    int        feed(float mag);
    MorseEvent event(int i) const;

    bool  isLocked()  const { return _state == MorseState::LOCKED; }
    float lockedWpm() const { return _lockedWpm; }

    // Diagnostics – read-only access to envelope state (useful for tuning)
    float envPeak()   const { return _envPeak; }
    float envValley() const { return _envValley; }

private:
    // --- config ---
    int   _frameRate = 36;
    float _wpmMin    = 5.0f;
    float _wpmMax    = 35.0f;
    int   _toneBin   = 22;

    // --- dual-envelope AGC (replaces _peakHold + P20 histogram) ---
    int   _envFrames  = 0;   // total frames seen since reset
    float _envPeak    = 0.0f;
    float _envValley  = 0.0f;

    // --- Schmitt trigger ---
    int   _schmittState = 0;
    float _schmittLo    = 0.0f;
    float _schmittHi    = 0.0f;
    bool  _schmittValid = false;
    int   _schmittFrame = 0;

    // --- run tracking ---
    int _curState = 0;
    int _curLen   = 0;

    MorsRing<RunEntry, 500>                  _runBuf;
    MorsRing<uint8_t,  MORS_ACQUIRE_RING_SIZE> _binaryHist;

    // --- state machine ---
    MorseState _state          = MorseState::ACQUIRE;
    int        _framesSinceAcq = 0;

    // --- PLL ---
    float _lockedWpm  = 0.0f;
    float _unitEst    = 0.0f;
    float _unitMin    = 0.0f;
    float _unitMax    = 0.0f;
    float _unitLocked = 0.0f;

    // --- symbol accumulation ---
    char _symbol[8] = {};
    int  _symLen    = 0;

    // --- lock-loss watchdog ---
    int _framesSinceMark = 0;

    // --- event queue (cleared each feed()) ---
    static constexpr int MAX_EVENTS = 8;
    MorseEvent _events[MAX_EVENTS];
    int        _evtCount = 0;
    int        _implausibleRuns = 0;  // consecutive runs inconsistent with unitEst

    // --- internal methods ---
    void  _updateEnvelope(float mag);   // NEW: replaces _updatePeak + _updateP20
    void  _updateSchmitt();
    int   _schmittStep(float val);
    bool  _updateRun(int bit, RunEntry &out);
    void  _acquireStep();
    void  _trackStep(int runState, int runLen);
    void  _declareLocked(float wpm);
    void  _declareLost();
    void  _resetToAcquire();
    void  _emitSymbol();
    void  _pushEvent(MorseEvent ev);

    float       _ditFrames(float wpm) const { return 1.2f / wpm * _frameRate; }
    void        _morphFilter(RunEntry *runs, int &count, int minRun);
    void        _estimateWpm(const RunEntry *runs, int count, float &bestWpm, float &bestConf);
    static char _morseToChar(const char *sym);
};
