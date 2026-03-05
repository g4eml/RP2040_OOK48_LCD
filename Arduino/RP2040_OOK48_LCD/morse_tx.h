#pragma once

#include <stdint.h>
#include <stdbool.h>

class MorseTx
{
public:
    MorseTx();

    void setWpm(uint8_t wpm);
    uint32_t unitUs() const;

    bool buildSequence(const char *text);
    void start();
    void startDashes();
    void stop();
    bool isActive() const;
    bool isMorseActive() const;
    bool isDashActive() const;
    uint32_t intervalUs() const;

    void tick(bool inTxMode, bool &keyOut);
    bool consumeCompleteRequest();

private:
    enum Mode : uint8_t { NONE = 0, MORSE = 1, DASH = 2 };
    bool appendUnits(int8_t units);
    const char *patternForChar(char c) const;

    static constexpr uint16_t MAX_UNITS = 512;
    static constexpr uint32_t DASH_UNIT_US = 100000;
    static constexpr uint8_t DASH_ON_UNITS = 3;
    static constexpr uint8_t DASH_OFF_UNITS = 1;

    volatile Mode mode_;
    volatile bool completeRequest_;
    volatile uint16_t seqLen_;
    volatile uint16_t seqPos_;
    volatile uint8_t unitsLeft_;
    volatile bool currentKey_;
    volatile uint32_t unitUs_;
    volatile uint8_t dashPhase_;
    volatile int8_t seq_[MAX_UNITS];
};
