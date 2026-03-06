#include <ctype.h>
#include "defines.h"
#include "morse_tx.h"
#include <Arduino.h>

#define TMESSAGE 6

extern char TxCharSent;

struct MorseMap
{
    char c;
    const char *pattern;
};

static const MorseMap MORSE_TABLE[] = {
    {'A', ".-"},    {'B', "-..."},  {'C', "-.-."},  {'D', "-.."},   {'E', "."},
    {'F', "..-."},  {'G', "--."},   {'H', "...."},  {'I', ".."},    {'J', ".---"},
    {'K', "-.-"},   {'L', ".-.."},  {'M', "--"},    {'N', "-."},    {'O', "---"},
    {'P', ".--."},  {'Q', "--.-"},  {'R', ".-."},   {'S', "..."},   {'T', "-"},
    {'U', "..-"},   {'V', "...-"},  {'W', ".--"},   {'X', "-..-"},  {'Y', "-.--"},
    {'Z', "--.."},
    {'0', "-----"}, {'1', ".----"}, {'2', "..---"}, {'3', "...--"}, {'4', "....-"},
    {'5', "....."}, {'6', "-...."}, {'7', "--..."}, {'8', "---.."}, {'9', "----."},
    {'/', "-..-."}, {'?', "..--.."}, {'.', ".-.-.-"}, {',', "--..--"},
    {'-', "-....-"}, {'+', ".-.-."}, {'=', "-...-"}
};

MorseTx::MorseTx() :
    mode_(NONE),
    completeRequest_(false),
    seqLen_(0),
    seqPos_(0),
    unitsLeft_(0),
    currentKey_(false),
    unitUs_(1200000UL / MORSE_DEFAULT_WPM),
    dashPhase_(0)
{
}

void MorseTx::setWpm(uint8_t wpm)
{
    if (wpm < MORSE_MIN_WPM) wpm = MORSE_MIN_WPM;
    if (wpm > MORSE_MAX_WPM) wpm = MORSE_MAX_WPM;
    unitUs_ = 1200000UL / (uint32_t)wpm;
}

uint32_t MorseTx::unitUs() const
{
    return unitUs_;
}

const char *MorseTx::patternForChar(char c) const
{
    char u = (char)toupper((unsigned char)c);
    for (uint16_t i = 0; i < (sizeof(MORSE_TABLE) / sizeof(MORSE_TABLE[0])); i++)
    {
        if (MORSE_TABLE[i].c == u) return MORSE_TABLE[i].pattern;
    }
    return nullptr;
}

bool MorseTx::appendUnits(int8_t units)
{
    if (units == 0) return true;
    if (seqLen_ >= MAX_UNITS) return false;
    seq_[seqLen_++] = units;
    return true;
}

bool MorseTx::buildSequence(const char *text)
{
    seqLen_ = 0;
    uint8_t pendingGap = 0;

    for (uint16_t i = 0; text[i] != 0; i++)
    {
        char c = text[i];
        if (c == ' ' || c == '\t')
        {
            if (seqLen_ > 0 && pendingGap < 7) pendingGap = 7;
            continue;
        }

        const char *pattern = patternForChar(c);
        if (pattern == nullptr) continue;

        if (seqLen_ > 0)
        {
            uint8_t letterGap = (pendingGap > 0) ? pendingGap : 3;
            if (!appendUnits(-(int8_t)letterGap)) return false;
            if(pendingGap > 6) 
            {
                appendUnits(' ');                           //add a space for the display
            }
        }
        pendingGap = 0;

        for (uint8_t p = 0; pattern[p] != 0; p++)
        {
            uint8_t onUnits = (pattern[p] == '-') ? 3 : 1;
            if (!appendUnits((int8_t)onUnits)) return false;
            if (pattern[p + 1] != 0)
            {
                if (!appendUnits(-1)) return false;
            }
        }
        appendUnits(c);                         //embed the actual character in the sequence
    }

    appendUnits(-10);                   //add a long gap at the end of the message
    appendUnits(13);                     //add a CR for the display
    appendUnits(-1);

    return seqLen_ > 0;
}

void MorseTx::start()
{
    mode_ = MORSE;
    completeRequest_ = false;
    seqPos_ = 0;
    unitsLeft_ = 0;
    currentKey_ = false;
    dashPhase_ = 0;
}

void MorseTx::startDashes()
{
    mode_ = DASH;
    completeRequest_ = false;
    seqPos_ = 0;
    unitsLeft_ = 0;
    currentKey_ = false;
    dashPhase_ = 0;
}

void MorseTx::stop()
{
    mode_ = NONE;
    completeRequest_ = false;
    seqPos_ = 0;
    unitsLeft_ = 0;
    currentKey_ = false;
    dashPhase_ = 0;
}

bool MorseTx::isActive() const
{
    return mode_ != NONE;
}

bool MorseTx::isMorseActive() const
{
    return mode_ == MORSE;
}

bool MorseTx::isDashActive() const
{
    return mode_ == DASH;
}

uint32_t MorseTx::intervalUs() const
{
    if (mode_ == DASH) return DASH_UNIT_US;
    return unitUs_;
}

void MorseTx::tick(bool inTxMode, bool &keyOut)
{
    if (mode_ == NONE || !inTxMode)
    {
        keyOut = 0;
        return;
    }

    if (mode_ == DASH)
    {
        keyOut = (dashPhase_ < DASH_ON_UNITS);
        dashPhase_++;
        if (dashPhase_ >= (DASH_ON_UNITS + DASH_OFF_UNITS)) dashPhase_ = 0;
        return;
    }

    if (unitsLeft_ == 0)
    {
        if (seqPos_ >= seqLen_)
        {
            seqPos_ = 0;
            return;
        }

        int8_t seg = seq_[seqPos_++];
        while(seg > 10)
         {
           TxCharSent=seg;                      //this is the actual character embeded in the morse sequence. 
           rp2040.fifo.push(TMESSAGE);          //Ask Core 1 to display it. 
           seg = seq_[seqPos_++];
         }
        currentKey_ = (seg > 0);
        unitsLeft_ = (uint8_t)(seg > 0 ? seg : -seg);
    }

    keyOut = currentKey_;
    unitsLeft_--;
}

bool MorseTx::consumeCompleteRequest()
{
    if (!completeRequest_) return false;
    completeRequest_ = false;
    return true;
}
