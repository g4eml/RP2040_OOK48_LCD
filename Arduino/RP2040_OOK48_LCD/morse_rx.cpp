#include "morse_rx.h"

// ---------------------------------------------------------------------------
// Morse code lookup table  (pattern → character)
// ---------------------------------------------------------------------------
static const struct { const char *pat; char ch; } MORSE_TABLE[] = {
    {".-",    'A'}, {"-...",  'B'}, {"-.-.",  'C'}, {"-..",   'D'},
    {".",     'E'}, {"..-.",  'F'}, {"--.",   'G'}, {"....",  'H'},
    {"..",    'I'}, {".---",  'J'}, {"-.-",   'K'}, {".-..",  'L'},
    {"--",    'M'}, {"-.",    'N'}, {"---",   'O'}, {".--.",  'P'},
    {"--.-",  'Q'}, {".-.",   'R'}, {"...",   'S'}, {"-",     'T'},
    {"..-",   'U'}, {"...-",  'V'}, {".--",   'W'}, {"-..-",  'X'},
    {"-.--",  'Y'}, {"--..",  'Z'},
    {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'},
    {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'},
    {"---..", '8'}, {"----.", '9'},
    {".-.-.-",'.'}, {"--..--",','}, {"..--..",'?'}, {"-....-",'-'},
    {"-..-..", '/'}, {".-.-.", '+'}, {"-...-", '='},
};
static constexpr int MORSE_TABLE_SIZE = sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0]);

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void MorseRxDecoder::begin(int frameRate, float wpmMin, float wpmMax, int toneBin)
{
    _frameRate = frameRate;
    _wpmMin    = wpmMin;
    _wpmMax    = wpmMax;
    _toneBin   = toneBin;
    reset();
}

void MorseRxDecoder::reset()
{
    _envFrames     = 0;
    _peakHold      = 0.0f;
    _peakLowFrames = 0;

    memset(_p20Hist, 0, sizeof(_p20Hist));
    _p20Ring.clear();
    _p20Scale      = 0.0f;
    _p20Total      = 0;
    _noiseFloor    = 0.0f;
    _noiseFloorMin = 0.0f;

    _schmittState  = 0;
    _schmittLo     = 0.0f;
    _schmittHi     = 0.0f;
    _schmittValid  = false;
    _schmittFrame  = 0;

    _resetToAcquire();
}

int MorseRxDecoder::feed(float mag)
{
    _evtCount = 0;
    _envFrames++;

    // 1. AGC
    _updatePeak(mag);
    _updateP20(mag);

    // 2. Update Schmitt thresholds every 8 frames
    _schmittFrame++;
    if (_schmittFrame % 8 == 0)
        _updateSchmitt();

    if (!_schmittValid)
        return 0;

    // 3. Schmitt → binary
    int bit = _schmittStep(mag);
    _binaryHist.push((uint8_t)bit);

    // 4. Run-length tracking
    RunEntry completed;
    if (_updateRun(bit, completed))
    {
        _runBuf.push(completed);
        _framesSinceAcq++;

        if (_state == MorseState::ACQUIRE)
            _acquireStep();
        else
            _trackStep(completed.state, completed.len);
    }

    // 5. Lock-loss watchdog
    if (_state == MorseState::LOCKED)
    {
        if (bit == 1) _framesSinceMark = 0;
        else          _framesSinceMark++;

        int lostTimeout = (int)(MORS_LOST_TIMEOUT_DITS * _unitEst);
        if (_framesSinceMark > lostTimeout)
            _declareLost();
        else if (_unitEst < _unitMin || _unitEst > _unitMax)
            _declareLost();
    }

    return _evtCount;
}

MorseEvent MorseRxDecoder::event(int i) const
{
    if (i < 0 || i >= _evtCount) return MorseEvent{};
    return _events[i];
}

// ---------------------------------------------------------------------------
// AGC
// ---------------------------------------------------------------------------

void MorseRxDecoder::_updatePeak(float mag)
{
    if (mag >= _peakHold)
    {
        _peakHold      = mag;
        _peakLowFrames = 0;
    }
    else
    {
        _peakLowFrames++;
        float decay = (_peakLowFrames > MORS_PEAK_FAST_ONSET)
                      ? MORS_PEAK_DECAY_FAST : MORS_PEAK_DECAY_SLOW;
        _peakHold *= decay;
    }
}

void MorseRxDecoder::_updateP20(float mag)
{
    if (_p20Scale == 0.0f && mag > 0.0f)
        _p20Scale = (float)(MORS_P20_HIST_BINS - 1) / (mag * 8.0f);
    if (_p20Scale <= 0.0f) return;

    int bucket = (int)(mag * _p20Scale);
    if (bucket >= MORS_P20_HIST_BINS) bucket = MORS_P20_HIST_BINS - 1;

    // Evict oldest sample when window is full
    if (_p20Ring.size() == MORS_P20_HIST_WINDOW)
    {
        int old = _p20Ring[0];
        if (_p20Hist[old] > 0) _p20Hist[old]--;
        _p20Total = MORS_P20_HIST_WINDOW;
    }

    _p20Ring.push((uint8_t)bucket);
    _p20Hist[bucket]++;
    if (_p20Total < MORS_P20_HIST_WINDOW) _p20Total++;

    // Walk histogram to find p20
    int target = (_p20Total * 20) / 100;
    if (target < 1) target = 1;
    int cum = 0, p20bucket = 0;
    for (int b = 0; b < MORS_P20_HIST_BINS; b++)
    {
        cum += _p20Hist[b];
        if (cum >= target) { p20bucket = b; break; }
    }

    float shortTerm = (float)p20bucket / (_p20Scale + 1e-12f);
    if (shortTerm > _noiseFloorMin)
        _noiseFloorMin += 0.001f * (shortTerm - _noiseFloorMin);
    _noiseFloor = (shortTerm > _noiseFloorMin) ? shortTerm : _noiseFloorMin;
}

// ---------------------------------------------------------------------------
// Schmitt trigger
// ---------------------------------------------------------------------------

void MorseRxDecoder::_updateSchmitt()
{
    if (_envFrames < 20) { _schmittValid = false; return; }

    float peak  = _peakHold;
    float noise = _noiseFloor;

    if (noise <= 0.0f || peak / (noise + 1e-9f) < 6.0f)
    {
        _schmittValid = false;
        return;
    }

    float mid  = 0.5f * (noise + peak);
    float hyst = MORS_SCHMITT_HYST_FRAC * (peak - noise);
    _schmittLo    = mid - hyst;
    _schmittHi    = mid + hyst;
    _schmittValid = true;
}

int MorseRxDecoder::_schmittStep(float val)
{
    if (_schmittState == 0 && val >= _schmittHi) _schmittState = 1;
    else if (_schmittState == 1 && val <= _schmittLo) _schmittState = 0;
    return _schmittState;
}

// ---------------------------------------------------------------------------
// Run-length tracking
// ---------------------------------------------------------------------------

bool MorseRxDecoder::_updateRun(int bit, RunEntry &out)
{
    if (bit == _curState)
    {
        _curLen++;
        return false;
    }
    bool has = (_curLen > 0);
    if (has) { out.state = (int8_t)_curState; out.len = (int16_t)_curLen; }
    _curState = bit;
    _curLen   = 1;
    return has;
}

// ---------------------------------------------------------------------------
// Morphological filter  (merge runs shorter than minRun into neighbours)
// Uses static scratch buffers — never called re-entrantly.
// ---------------------------------------------------------------------------

void MorseRxDecoder::_morphFilter(RunEntry *runs, int &count, int minRun)
{
    if (count <= 0 || minRun <= 1) return;

    static RunEntry tmp[500];
    static RunEntry merged[500];

    bool changed = true;
    while (changed)
    {
        changed = false;
        int tmpCount = 0;
        int i = 0;

        while (i < count)
        {
            int s = runs[i].state, n = runs[i].len;
            if (n < minRun && count > 1)
            {
                if (i == 0)
                {
                    tmp[tmpCount++] = { (int8_t)runs[i+1].state,
                                        (int16_t)(n + runs[i+1].len) };
                    i += 2;
                }
                else if (i == count - 1)
                {
                    tmp[tmpCount - 1].len += (int16_t)n;
                    i++;
                }
                else
                {
                    int pn = tmp[tmpCount-1].len;
                    int ns = runs[i+1].state, nn = runs[i+1].len;
                    if (pn >= nn)
                    {
                        tmp[tmpCount-1].len += (int16_t)n;
                        i++;
                    }
                    else
                    {
                        tmp[tmpCount++] = { (int8_t)ns, (int16_t)(n + nn) };
                        i += 2;
                    }
                }
                changed = true;
            }
            else
            {
                tmp[tmpCount++] = { (int8_t)s, (int16_t)n };
                i++;
            }
        }

        // Merge adjacent same-state runs
        int mergedCount = 0;
        for (int j = 0; j < tmpCount; j++)
        {
            if (mergedCount > 0 && merged[mergedCount-1].state == tmp[j].state)
                merged[mergedCount-1].len += tmp[j].len;
            else
                merged[mergedCount++] = tmp[j];
        }

        memcpy(runs, merged, mergedCount * sizeof(RunEntry));
        count = mergedCount;
    }
}

// ---------------------------------------------------------------------------
// WPM estimator
// ---------------------------------------------------------------------------

void MorseRxDecoder::_estimateWpm(const RunEntry *runs, int count,
                                   float &bestWpm, float &bestConf)
{
    static int markRuns[500];
    int markCount = 0;
    for (int i = 0; i < count; i++)
        if (runs[i].state == 1 && runs[i].len >= 2)
            markRuns[markCount++] = runs[i].len;

    if (markCount == 0) { bestWpm = _wpmMin; bestConf = 0.0f; return; }

    bestWpm  = _wpmMin;
    bestConf = 0.0f;
    float bestScore = -1e9f;

    for (float wpm = _wpmMin; wpm <= _wpmMax + 1e-4f; wpm += 0.5f)
    {
        int uf = (int)(_ditFrames(wpm) + 0.5f);
        if (uf < 1) uf = 1;

        // Sub-threshold fraction
        int subThresh = 0;
        for (int i = 0; i < count; i++)
            if ((float)runs[i].len / uf < 0.5f) subThresh++;
        float subFrac = (float)subThresh / (float)(count > 0 ? count : 1);

        // Weighted error score
        float pen = 0.0f, tw = 0.0f;
        for (int i = 0; i < count; i++)
        {
            int   s = runs[i].state, n = runs[i].len;
            float units = (float)n / (float)uf;
            if (units < 0.5f) continue;
            float weight = (float)(n < 10*uf ? n : 10*uf);
            float err, w;
            if (s == 1)
            {
                float e1 = units - 1.0f; if (e1 < 0) e1 = -e1;
                float e3 = units - 3.0f; if (e3 < 0) e3 = -e3;
                err = (e1 < e3) ? e1 : e3;
                w   = 1.0f;
            }
            else
            {
                if (units >= 6.0f)
                {
                    err = units - 7.0f; if (err < 0) err = -err;
                    w   = MORS_SPACE_WORD_WEIGHT;
                }
                else
                {
                    float e1 = units - 1.0f; if (e1 < 0) e1 = -e1;
                    float e3 = units - 3.0f; if (e3 < 0) e3 = -e3;
                    err = (e1 < e3) ? e1 : e3;
                    w   = MORS_SPACE_LETTER_WEIGHT;
                }
            }
            pen += weight * w * err;
            tw  += weight * w;
        }
        if (tw <= 1e-9f) continue;

        // Histogram alignment
        float tol   = MORS_HIST_TOL_FRAC * (float)uf;
        float dashF = 3.0f * (float)uf;
        int hits = 0;
        for (int i = 0; i < markCount; i++)
        {
            float d1 = (float)markRuns[i] - (float)uf;  if (d1 < 0) d1 = -d1;
            float d3 = (float)markRuns[i] - dashF;       if (d3 < 0) d3 = -d3;
            if (d1 <= tol || d3 <= tol) hits++;
        }
        float conf  = (float)hits / (float)markCount;
        float score = -(pen / tw) + MORS_HIST_REWARD * conf - 1.5f * subFrac;

        if (score > bestScore)
        {
            bestScore = score;
            bestWpm   = wpm;
            bestConf  = conf;
        }
    }
}

// ---------------------------------------------------------------------------
// Acquisition
// ---------------------------------------------------------------------------

void MorseRxDecoder::_acquireStep()
{
    int markCount = 0;
    for (int i = 0; i < _runBuf.size(); i++)
        if (_runBuf[i].state == 1) markCount++;
    if (markCount < MORS_MIN_ACQUIRE_MARK_RUNS) return;
    if (_framesSinceAcq % MORS_REESTIMATE_INTERVAL != 0) return;

    static RunEntry runs[500];
    int count = _runBuf.size();
    for (int i = 0; i < count; i++) runs[i] = _runBuf[i];

    float midWpm  = 0.5f * (_wpmMin + _wpmMax);
    int   coarseUf = (int)(_ditFrames(midWpm) + 0.5f);
    if (coarseUf < 1) coarseUf = 1;
    int minRun = (int)(MORS_MORPH_THRESH_FRAC * (float)coarseUf + 0.5f);
    if (minRun < 2) minRun = 2;
    _morphFilter(runs, count, minRun);

    float bestWpm, bestConf;
    _estimateWpm(runs, count, bestWpm, bestConf);

    if (bestConf >= MORS_LOCK_THRESHOLD)
        _declareLocked(bestWpm);
}

// ---------------------------------------------------------------------------
// Tracking (LOCKED state)
// ---------------------------------------------------------------------------

void MorseRxDecoder::_trackStep(int runState, int runLen)
{
    float uf = _unitEst;
    if (uf <= 1e-6f) return;

    float unitsF = (float)runLen / uf;
    int   units  = (int)(unitsF + 0.5f);
    if (units < 1) units = 1;

    if (runState == 1)
    {
        bool isDash = (units >= 2);
        if (_symLen < 7) _symbol[_symLen++] = isDash ? '-' : '.';
        float target = isDash ? 3.0f : 1.0f;
        float obs    = (float)runLen / target;
        _unitEst = (1.0f - MORS_ALPHA_MARK) * uf + MORS_ALPHA_MARK * obs;
        _framesSinceMark = 0;
    }
    else
    {
        if (unitsF >= MORS_WORD_GAP_THR)
        {
            if (_symLen > 0) _emitSymbol();
            MorseEvent ev; ev.kind = MorseEvt::WORD_SEP; ev.ch = ' '; ev.wpm = 0;
            _pushEvent(ev);
        }
        else if (units >= 3)
        {
            if (_symLen > 0) _emitSymbol();
            float obs = (float)runLen / 3.0f;
            _unitEst = (1.0f - MORS_ALPHA_SPACE) * uf + MORS_ALPHA_SPACE * obs;
        }
        else
        {
            float obs = (float)runLen / 1.0f;
            _unitEst = (1.0f - MORS_ALPHA_SPACE) * uf + MORS_ALPHA_SPACE * obs;
        }
    }

    // Clamp PLL
    if (_unitEst < _unitMin) _unitEst = _unitMin;
    if (_unitEst > _unitMax) _unitEst = _unitMax;
}

void MorseRxDecoder::_emitSymbol()
{
    _symbol[_symLen] = '\0';
    char ch = _morseToChar(_symbol);
    MorseEvent ev;
    ev.kind = MorseEvt::CHAR;
    ev.ch   = ch;
    ev.wpm  = 0.0f;
    _pushEvent(ev);
    _symLen = 0;
}

// ---------------------------------------------------------------------------
// State transitions
// ---------------------------------------------------------------------------

void MorseRxDecoder::_declareLocked(float wpm)
{
    _state     = MorseState::LOCKED;
    _lockedWpm = wpm;
    float uf   = _ditFrames(wpm);
    _unitEst   = uf;
    _unitMin   = MORS_PLL_LO_FRAC * uf;
    _unitMax   = MORS_PLL_HI_FRAC * uf;
    _symLen    = 0;
    _framesSinceMark = 0;

    MorseEvent ev; ev.kind = MorseEvt::LOCKED; ev.ch = 0; ev.wpm = wpm;
    _pushEvent(ev);

    // Re-play buffered runs through tracker now that we have a unit estimate
    for (int i = 0; i < _runBuf.size(); i++)
        _trackStep(_runBuf[i].state, _runBuf[i].len);
}

void MorseRxDecoder::_declareLost()
{
    MorseEvent ev; ev.kind = MorseEvt::LOST; ev.ch = 0; ev.wpm = 0;
    _pushEvent(ev);
    _resetToAcquire();
}

void MorseRxDecoder::_resetToAcquire()
{
    _state           = MorseState::ACQUIRE;
    _framesSinceAcq  = 0;
    _runBuf.clear();
    _symLen          = 0;
    _framesSinceMark = 0;
    _curState        = 0;
    _curLen          = 0;
    // Keep envelope/AGC state — helps Schmitt retrain quickly
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void MorseRxDecoder::_pushEvent(MorseEvent ev)
{
    if (_evtCount < MAX_EVENTS)
        _events[_evtCount++] = ev;
}

char MorseRxDecoder::_morseToChar(const char *sym)
{
    for (int i = 0; i < MORSE_TABLE_SIZE; i++)
        if (__builtin_strcmp(sym, MORSE_TABLE[i].pat) == 0)
            return MORSE_TABLE[i].ch;
    return '?';
}
