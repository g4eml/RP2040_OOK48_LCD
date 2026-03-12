#include "morse_rx.h"

// ---------------------------------------------------------------------------
// Morse lookup table
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
// Public API
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
    _envFrames = 0;
    _envPeak   = 0.0f;
    _envValley = 0.0f;

    _schmittState = 0;
    _schmittLo    = 0.0f;
    _schmittHi    = 0.0f;
    _schmittValid = false;
    _schmittFrame = 0;

    _curState = 0;
    _curLen   = 0;
    _runCount = 0;
    _runBuf.clear();

    // Seed unit estimate at the midpoint of the expected WPM range.
    // This gives _trackStep something to work with from the very first run,
    // even before the background estimator has had a chance to refine it.
    float midWpm = 0.5f * (_wpmMin + _wpmMax);
    _applySpeedEstimate(midWpm);

    _confidence      = 0.0f;
    _wasLocked       = false;
    _symLen          = 0;
    _framesSinceMark = 0;
    _biasStreak      = 0;
    _evtCount        = 0;
}

int MorseRxDecoder::feed(float mag)
{
    _evtCount = 0;
    _envFrames++;

    // 1. Envelope AGC
    _updateEnvelope(mag);

    // 2. Update Schmitt thresholds (more often while confidence is low)
    _schmittFrame++;
    int schmittInterval = isLocked() ? 8 : 4;
    if (_schmittFrame % schmittInterval == 0)
        _updateSchmitt();

    if (!_schmittValid)
        return 0;

    // 3. Schmitt → binary bit
    int bit = _schmittStep(mag);

    // 4. Run-length encode; process each completed run
    RunEntry completed;
    if (_updateRun(bit, completed))
    {
        _runBuf.push(completed);
        _runCount++;

        _trackStep(completed.state, completed.len);

        // Background speed re-estimator — runs periodically, more aggressively
        // while coasting (confidence low) so we re-converge quickly after a
        // speed change or brief loss of signal.
        int interval = isLocked() ? MORS_REESTIMATE_INTERVAL * 2
                                  : MORS_REESTIMATE_INTERVAL;
        if (_runCount % interval == 0)
            _reestimateSpeed(false);
    }

    // 5. Word-gap detection on in-progress space run
    if (_curState == 0 && _symLen > 0 && _unitEst > 1e-6f)
    {
        float spaceUnits = (float)_curLen / _unitEst;
        if (spaceUnits >= MORS_WORD_GAP_THR)
            _emitSymbol();
    }

    // 6. Silence watchdog
    if (bit == 1)
        _framesSinceMark = 0;
    else
        _framesSinceMark++;

    if (_unitEst > 1e-6f)
    {
        int lostTimeout = (int)(MORS_LOST_TIMEOUT_DITS * _unitEst);
        if (_framesSinceMark > lostTimeout && _confidence > 0.0f)
        {
            // Silence timeout — force confidence to zero
            if (_symLen > 0) _emitSymbol();
            _confidence = 0.0f;
        }
    }

    // LOST event: fire on any falling edge to zero confidence, regardless
    // of cause (silence timeout or sustained implausible runs).
    bool nowLocked = isLocked();
    if (_confidence == 0.0f && _wasLocked)
    {
        MorseEvent ev; ev.kind = MorseEvt::LOST; ev.ch = 0; ev.wpm = 0;
        _pushEvent(ev);
        _wasLocked = false;
    }

    // LOCKED event: fire on rising edge of confidence crossing CONF_HIGH.
    if (nowLocked && !_wasLocked)
    {
        MorseEvent ev; ev.kind = MorseEvt::LOCKED; ev.ch = 0; ev.wpm = _lockedWpm;
        _pushEvent(ev);
    }
    _wasLocked = nowLocked;

    return _evtCount;
}

MorseEvent MorseRxDecoder::event(int i) const
{
    if (i < 0 || i >= _evtCount) return MorseEvent{};
    return _events[i];
}

// ---------------------------------------------------------------------------
// Dual-envelope AGC
// ---------------------------------------------------------------------------

void MorseRxDecoder::_updateEnvelope(float mag)
{
    // Seeding phase: collect hard min/max for the first N frames so both
    // envelopes start from realistic values rather than zero.
    if (_envFrames <= MORS_ENV_SEED_FRAMES)
    {
        if (_envFrames == 1)
        {
            _envPeak   = mag;
            _envValley = mag;
        }
        else
        {
            if (mag > _envPeak)   _envPeak   = mag;
            if (mag < _envValley) _envValley = mag;
        }
        // On the last seed frame, nudge valley slightly below observed min
        // so the SNR gate passes cleanly from the next frame onward.
        if (_envFrames == MORS_ENV_SEED_FRAMES)
            _envValley *= 0.85f;
        return;
    }

    // Peak: fast attack, slow decay
    if (mag >= _envPeak)
        _envPeak += MORS_ENV_PEAK_ATTACK * (mag - _envPeak);
    else
        _envPeak -= MORS_ENV_PEAK_DECAY  * _envPeak;

    // Valley: tracks noise floor.
    // Moves down quickly when a sample is below it (space/noise region).
    // Creeps up very slowly when samples are above it (during marks) so it
    // can follow a genuine rise in the noise floor without being dragged up
    // by the signal itself.
    if (mag < _envValley)
        _envValley += MORS_ENV_VALLEY_ATTACK * (mag - _envValley);  // pull down
    else
        _envValley += MORS_ENV_VALLEY_DECAY  * (mag - _envValley);  // creep up

    // Hard ceiling: valley must remain well below peak
    if (_envValley > _envPeak * 0.80f)
        _envValley = _envPeak * 0.80f;

    if (_envValley < 1e-12f) _envValley = 1e-12f;
    if (_envPeak   < 1e-12f) _envPeak   = 1e-12f;
}

// ---------------------------------------------------------------------------
// Schmitt trigger
// ---------------------------------------------------------------------------

void MorseRxDecoder::_updateSchmitt()
{
    if (_envFrames <= MORS_ENV_SEED_FRAMES) { _schmittValid = false; return; }

    float peak   = _envPeak;
    float valley = _envValley;
    float range  = peak - valley;

    if (valley <= 0.0f || (peak / valley) < MORS_ENV_MIN_SNR || range < 1e-9f)
    {
        _schmittValid = false;
        _schmittState = 0;
        return;
    }

    float mid  = valley + range * (0.5f + MORS_SCHMITT_CENTRE_BIAS);
    float hyst = MORS_SCHMITT_HYST_FRAC * range;
    _schmittLo    = mid - hyst;
    _schmittHi    = mid + hyst;
    _schmittValid = true;
}

int MorseRxDecoder::_schmittStep(float val)
{
    if      (_schmittState == 0 && val >= _schmittHi) _schmittState = 1;
    else if (_schmittState == 1 && val <= _schmittLo) _schmittState = 0;
    return _schmittState;
}

// ---------------------------------------------------------------------------
// Run-length encoder
// ---------------------------------------------------------------------------

bool MorseRxDecoder::_updateRun(int bit, RunEntry &out)
{
    if (bit == _curState) { _curLen++; return false; }
    bool has = (_curLen > 0);
    if (has) { out.state = (int8_t)_curState; out.len = (int16_t)_curLen; }
    _curState = bit;
    _curLen   = 1;
    return has;
}

// ---------------------------------------------------------------------------
// Tracking — called on every completed run
// ---------------------------------------------------------------------------
//
// There is no separate ACQUIRE state any more.  _trackStep runs from the
// first completed run.  Early on, _unitEst is seeded at the mid-WPM guess
// and the PLL clamps are wide (COAST values), so the first few runs will
// correct it rapidly.  Confidence rises with each plausible run and falls
// with each implausible one.  The caller (feed()) emits characters
// regardless of confidence level — spurious early output is acceptable.
// ---------------------------------------------------------------------------

void MorseRxDecoder::_trackStep(int runState, int runLen)
{
    if (_unitEst <= 1e-6f) return;

    float uf     = _unitEst;
    float unitsF = (float)runLen / uf;

    // Plausibility check.
    // For marks: valid range is 0.4..4.5 units (dot=1, dash=3 with tolerance).
    //   A mark longer than ~4.5 units cannot be a valid Morse element.
    // For spaces: any length >= 0.4 units is plausible. A very long space is
    //   just an extended pause and must never penalise confidence — it was
    //   the cause of losing the first letters after a double word gap.
    bool plausible;
    if (runState == 1)
        plausible = (unitsF >= 0.4f && unitsF <= 4.5f);
    else
        plausible = (unitsF >= 0.4f);

    if (plausible)
    {
        _confidence += MORS_CONF_RISE;
        if (_confidence > 1.0f) _confidence = 1.0f;
    }
    else
    {
        _confidence -= MORS_CONF_FALL;
        if (_confidence < 0.0f) _confidence = 0.0f;
        // Don't try to decode an implausible run — just return.
        // The background re-estimator will correct the speed.
        return;
    }

    // Update PLL clamps based on current confidence
    if (isLocked())
    {
        _unitMin = MORS_PLL_LO_FRAC   * _unitEst;
        _unitMax = MORS_PLL_HI_FRAC   * _unitEst;
    }
    else
    {
        // Coasting: wide clamp so PLL can drift to find the right speed
        _unitMin = MORS_COAST_LO_FRAC * _unitEst;
        _unitMax = MORS_COAST_HI_FRAC * _unitEst;
    }

    int units = (int)(unitsF + 0.5f);
    if (units < 1) units = 1;

    if (runState == 1)
    {
        // Mark: dot or dash
        bool isDash = (unitsF >= 2.0f);
        if (_symLen >= 7) _emitSymbol();   // buffer full — flush
        _symbol[_symLen++] = isDash ? '-' : '.';

        float target = isDash ? 3.0f : 1.0f;
        float obs    = (float)runLen / target;
        float alpha  = isDash ? MORS_ALPHA_DASH : MORS_ALPHA_DOT;
        float newEst = (1.0f - alpha) * uf + alpha * obs;

        // Track direction of this correction for bias-streak detection.
        // Only mark runs vote — spaces are too variable to give a clean signal.
        int vote = (newEst > uf) ? +1 : (newEst < uf) ? -1 : 0;
        if (vote != 0)
        {
            // Same direction as current streak → extend it; opposite → reset
            if (_biasStreak == 0 || (vote > 0) == (_biasStreak > 0))
                _biasStreak += vote;
            else
                _biasStreak = vote;   // reversal: start fresh in new direction
        }

        // If the streak has reached the trigger threshold, call for an
        // aggressive re-estimate immediately rather than waiting for the
        // periodic interval.  The blend fraction grows with streak length
        // so a longer sustained bias causes a larger single correction.
        int absStreak = _biasStreak < 0 ? -_biasStreak : _biasStreak;
        if (absStreak >= MORS_BIAS_TRIGGER)
        {
            // Temporarily widen the PLL clamp in the direction of the bias
            // so _unitEst can actually travel the full required distance.
            if (_biasStreak > 0)
                _unitMax = MORS_COAST_HI_FRAC * _unitEst;   // speed went down
            else
                _unitMin = MORS_COAST_LO_FRAC * _unitEst;   // speed went up

            _reestimateSpeed(true /* aggressive */);
            _biasStreak = 0;   // consumed — start counting again
        }

        _unitEst = newEst;
    }
    else
    {
        // Space: intra-char, inter-char, or word gap
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
            _unitEst = (1.0f - MORS_ALPHA_INTRA_SPACE) * uf + MORS_ALPHA_INTRA_SPACE * obs;
        }
    }

    // Clamp PLL
    if (_unitEst < _unitMin) _unitEst = _unitMin;
    if (_unitEst > _unitMax) _unitEst = _unitMax;

    // Update reported WPM
    _lockedWpm = 1.2f / (_unitEst / (float)_frameRate);
}

// ---------------------------------------------------------------------------
// Background speed re-estimator
// ---------------------------------------------------------------------------
//
// Runs periodically and also on demand when a bias streak is detected.
// aggressive=true uses a larger blend fraction so _unitEst jumps most of the
// way to the new estimate in a single call — used for speed-change response.
// aggressive=false uses a softer blend to avoid disrupting a stable lock.
// ---------------------------------------------------------------------------

void MorseRxDecoder::_reestimateSpeed(bool aggressive)
{
    int count = _runBuf.size();
    if (count < 4) return;

    int markCount = 0;
    for (int i = 0; i < count; i++)
        if (_runBuf[i].state == 1) markCount++;
    if (markCount < 2) return;

    static RunEntry runs[MORS_RUN_RING_SIZE];
    for (int i = 0; i < count; i++) runs[i] = _runBuf[i];

    int minRun = (int)(MORS_MORPH_THRESH_FRAC * _unitEst + 0.5f);
    if (minRun < 2) minRun = 2;
    _morphFilter(runs, count, minRun);

    float bestWpm, bestConf;
    _estimateWpm(runs, count, bestWpm, bestConf);

    if (bestConf < 0.3f) return;

    float newUnit = _ditFrames(bestWpm);
    morseWpmCurrent = bestWpm;

    float blend;
    if (aggressive)
    {
        // Blend fraction scales with how far the streak has already grown,
        // captured via the current |_biasStreak| before it was zeroed.
        // Since streak is reset before this is called from _trackStep, use
        // a fixed aggressive fraction here; the caller already widened the clamp.
        blend = MORS_BIAS_BLEND_BASE;
    }
    else if (isLocked())
    {
        blend = 0.15f;   // soft — don't disrupt a stable lock
    }
    else
    {
        blend = 0.70f;   // coasting — jump most of the way immediately
    }

    _unitEst = _unitEst * (1.0f - blend) + newUnit * blend;

    // Re-clamp after adjustment
    float lo = isLocked() ? MORS_PLL_LO_FRAC  * _unitEst
                          : MORS_COAST_LO_FRAC * _unitEst;
    float hi = isLocked() ? MORS_PLL_HI_FRAC  * _unitEst
                          : MORS_COAST_HI_FRAC * _unitEst;
    if (_unitEst < lo) _unitEst = lo;
    if (_unitEst > hi) _unitEst = hi;

    _lockedWpm = 1.2f / (_unitEst / (float)_frameRate);
}

// ---------------------------------------------------------------------------
// Apply a WPM estimate — sets _unitEst and the PLL clamps
// ---------------------------------------------------------------------------

void MorseRxDecoder::_applySpeedEstimate(float wpm)
{
    if (wpm < _wpmMin) wpm = _wpmMin;
    if (wpm > _wpmMax) wpm = _wpmMax;
    float uf   = _ditFrames(wpm);
    _unitEst   = uf;
    _unitMin   = MORS_COAST_LO_FRAC * uf;   // start with wide clamps
    _unitMax   = MORS_COAST_HI_FRAC * uf;
    _lockedWpm = wpm;
}

// ---------------------------------------------------------------------------
// Symbol emit
// ---------------------------------------------------------------------------

void MorseRxDecoder::_emitSymbol()
{
    _symbol[_symLen] = '\0';
    char ch = _morseToChar(_symbol);
    MorseEvent ev; ev.kind = MorseEvt::CHAR; ev.ch = ch; ev.wpm = 0.0f;
    _pushEvent(ev);
    _symLen = 0;
}

// ---------------------------------------------------------------------------
// Morphological filter
// ---------------------------------------------------------------------------

void MorseRxDecoder::_morphFilter(RunEntry *runs, int &count, int minRun)
{
    if (count <= 0 || minRun <= 1) return;

    static RunEntry tmp[MORS_RUN_RING_SIZE];
    static RunEntry merged[MORS_RUN_RING_SIZE];

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
                    tmp[tmpCount-1].len += (int16_t)n;
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
    static int markRuns[MORS_RUN_RING_SIZE];
    int markCount = 0;
    for (int i = 0; i < count; i++)
        if (runs[i].state == 1 && runs[i].len >= 2)
            markRuns[markCount++] = runs[i].len;

    if (markCount == 0) { bestWpm = _wpmMin; bestConf = 0.0f; return; }

    bestWpm  = _wpmMin;
    bestConf = 0.0f;
    float bestScore = -1e9f;

    // While coasting use a coarser step to search faster
    float step = isLocked() ? 0.5f : 1.0f;

    for (float wpm = _wpmMin; wpm <= _wpmMax + 1e-4f; wpm += step)
    {
        float uf = _ditFrames(wpm);
        if (uf < 0.5f) uf = 0.5f;

        int subThresh = 0;
        for (int i = 0; i < count; i++)
            if ((float)runs[i].len / uf < 0.5f) subThresh++;
        float subFrac = (float)subThresh / (float)(count > 0 ? count : 1);

        float pen = 0.0f, tw = 0.0f;
        for (int i = 0; i < count; i++)
        {
            int   s = runs[i].state, n = runs[i].len;
            float units = (float)n / uf;
            if (units < 0.5f)
            {
                pen += (float)n * 2.0f;
                tw  += (float)n;
                continue;
            }
            float weight = (n < 10.0f*uf ? (float)n : 10.0f*uf);
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

        float tol   = MORS_HIST_TOL_FRAC * uf;
        float dashF = 3.0f * uf;
        int hits = 0;
        for (int i = 0; i < markCount; i++)
        {
            float d1 = (float)markRuns[i] - uf;    if (d1 < 0) d1 = -d1;
            float d3 = (float)markRuns[i] - dashF; if (d3 < 0) d3 = -d3;
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
