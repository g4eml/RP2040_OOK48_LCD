#pragma once

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Tunable constants  (ported from Python StreamingMorseDecoder)
// ---------------------------------------------------------------------------
static constexpr int   MORS_MIN_ACQUIRE_MARK_RUNS  = 20;
static constexpr int   MORS_REESTIMATE_INTERVAL    = 6;
static constexpr int   MORS_ACQUIRE_RING_SIZE      = 400;   // ~11s at 36fps
static constexpr float MORS_LOCK_THRESHOLD         = 0.65f;

static constexpr float MORS_SCHMITT_HYST_FRAC      = 0.12f;

static constexpr float MORS_PEAK_DECAY_SLOW        = 0.9995f;
static constexpr float MORS_PEAK_DECAY_FAST        = 0.985f;
static constexpr int   MORS_PEAK_FAST_ONSET        = 120;

static constexpr int   MORS_P20_HIST_BINS          = 256;
static constexpr int   MORS_P20_HIST_WINDOW        = 128;

static constexpr float MORS_MORPH_THRESH_FRAC      = 0.38f;

static constexpr float MORS_SPACE_WORD_WEIGHT      = 0.15f;
static constexpr float MORS_SPACE_LETTER_WEIGHT    = 0.30f;
static constexpr float MORS_HIST_REWARD            = 0.40f;
static constexpr float MORS_HIST_TOL_FRAC          = 0.35f;

static constexpr float MORS_ALPHA_MARK             = 0.12f;
static constexpr float MORS_ALPHA_SPACE            = 0.06f;
static constexpr float MORS_PLL_LO_FRAC            = 0.60f;
static constexpr float MORS_PLL_HI_FRAC            = 1.55f;

static constexpr float MORS_WORD_GAP_THR           = 5.5f;
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

private:
    // --- config ---
    int   _frameRate = 36;
    float _wpmMin    = 5.0f;
    float _wpmMax    = 35.0f;
    int   _toneBin   = 22;

    // --- asymmetric AGC ---
    int   _envFrames     = 0;
    float _peakHold      = 0.0f;
    int   _peakLowFrames = 0;

    // --- P20 histogram noise floor ---
    uint16_t                          _p20Hist[MORS_P20_HIST_BINS] = {};
    MorsRing<uint8_t, MORS_P20_HIST_WINDOW> _p20Ring;
    float    _p20Scale      = 0.0f;
    int      _p20Total      = 0;
    float    _noiseFloor    = 0.0f;
    float    _noiseFloorMin = 0.0f;

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
    float _lockedWpm = 0.0f;
    float _unitEst   = 0.0f;
    float _unitMin   = 0.0f;
    float _unitMax   = 0.0f;

    // --- symbol accumulation ---
    char _symbol[8] = {};
    int  _symLen    = 0;

    // --- lock-loss watchdog ---
    int _framesSinceMark = 0;

    // --- event queue (cleared each feed()) ---
    static constexpr int MAX_EVENTS = 8;
    MorseEvent _events[MAX_EVENTS];
    int        _evtCount = 0;

    // --- internal methods ---
    void  _updatePeak(float mag);
    void  _updateP20(float mag);
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
