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

void MorseRxDecoder::begin(int frameRate, float wpm, int toneBin)
{
    _frameRate = frameRate;
    _toneBin   = toneBin;
    _wpm       = wpm;
    _unitFrames = _ditFrames(wpm);
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

    _symLen          = 0;
    _confidence      = 0.0f;
    _wasActive       = false;
    _framesSinceMark = 0;
    _evtCount        = 0;
}

void MorseRxDecoder::setWpm(float wpm)
{
    // Flush any partial symbol so old-speed elements are not mixed with
    // new-speed elements in the same symbol buffer.
    if (_symLen > 0) _emitSymbol();

    _wpm        = wpm;
    _unitFrames = _ditFrames(wpm);
}

int MorseRxDecoder::feed(float mag)
{
    _evtCount = 0;
    _envFrames++;

    // 1. Envelope AGC
    _updateEnvelope(mag);

    // 2. Update Schmitt thresholds
    _schmittFrame++;
    if (_schmittFrame % 4 == 0)
        _updateSchmitt();

    if (!_schmittValid)
        return 0;

    // 3. Schmitt → binary bit
    int bit = _schmittStep(mag);

    // 4. Run-length encode; decode each completed run
    RunEntry completed;
    if (_updateRun(bit, completed))
        _decodeRun(completed.state, completed.len);

    // 5. Word-gap detection on the in-progress space run
    if (_curState == 0 && _symLen > 0 && _unitFrames > 1e-6f)
    {
        float spaceUnits = (float)_curLen / _unitFrames;
        if (spaceUnits >= MORS_WORD_GAP_THR)
            _emitSymbol();
    }

    // 6. Silence watchdog
    if (bit == 1)
        _framesSinceMark = 0;
    else
        _framesSinceMark++;

    if (_unitFrames > 1e-6f)
    {
        int lostTimeout = (int)(MORS_LOST_TIMEOUT_DITS * _unitFrames);
        if (_framesSinceMark > lostTimeout && _confidence > 0.0f)
        {
            if (_symLen > 0) _emitSymbol();
            _confidence = 0.0f;
        }
    }

    // 7. SIGNAL_LOST edge: confidence just reached zero
    bool nowActive = isActive();
    if (_confidence == 0.0f && _wasActive)
    {
        MorseEvent ev; ev.kind = MorseEvt::SIGNAL_LOST; ev.ch = 0; ev.wpm = 0;
        _pushEvent(ev);
        _wasActive = false;
    }

    // 8. SIGNAL_ACQUIRED edge: confidence just crossed CONF_HIGH
    if (nowActive && !_wasActive)
    {
        MorseEvent ev; ev.kind = MorseEvt::SIGNAL_ACQUIRED; ev.ch = 0; ev.wpm = _wpm;
        _pushEvent(ev);
    }
    _wasActive = nowActive;

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
// Run decoder
// ---------------------------------------------------------------------------
// With speed fixed, run classification is straightforward: measure the run
// in units of _unitFrames and apply fixed thresholds.
//
// Mark runs:
//   < MARK_MIN_FRAC units  → implausible (noise), penalise confidence
//   < DOT_DASH_BOUNDARY    → dot
//   >= DOT_DASH_BOUNDARY   → dash
//   > MARK_MAX_FRAC units  → implausible (too long), penalise confidence
//
// Space runs (any length is valid — long spaces are just pauses):
//   >= WORD_GAP_THR units  → word gap: emit current symbol + WORD_SEP event
//   >= INTER_CHAR_UNITS    → inter-character gap: emit current symbol
//   < INTER_CHAR_UNITS     → intra-character gap: continue accumulating
// ---------------------------------------------------------------------------

void MorseRxDecoder::_decodeRun(int runState, int runLen)
{
    if (_unitFrames <= 1e-6f) return;

    float unitsF = (float)runLen / _unitFrames;

    if (runState == 1)
    {
        // --- Mark ---
        bool plausible = (unitsF >= MORS_MARK_MIN_FRAC &&
                          unitsF <= MORS_MARK_MAX_FRAC);

        if (plausible)
        {
            _confidence += MORS_CONF_RISE;
            if (_confidence > 1.0f) _confidence = 1.0f;

            if (_symLen >= 7) _emitSymbol();   // buffer full — flush
            _symbol[_symLen++] = (unitsF < MORS_DOT_DASH_BOUNDARY) ? '.' : '-';
        }
        else
        {
            _confidence -= MORS_CONF_FALL;
            if (_confidence < 0.0f) _confidence = 0.0f;
            // Implausible mark: discard, do not add to symbol buffer
        }
    }
    else
    {
        // --- Space ---
        // Long spaces are valid pauses — never penalise confidence.
        if (unitsF >= MORS_WORD_GAP_THR)
        {
            if (_symLen > 0) _emitSymbol();
            MorseEvent ev; ev.kind = MorseEvt::WORD_SEP; ev.ch = ' '; ev.wpm = 0;
            _pushEvent(ev);
        }
        else if (unitsF >= MORS_INTER_CHAR_UNITS)
        {
            if (_symLen > 0) _emitSymbol();
        }
        // else: intra-character gap — keep accumulating dots/dashes
    }
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
