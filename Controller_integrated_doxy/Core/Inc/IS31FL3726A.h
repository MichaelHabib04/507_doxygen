#pragma once

#include "main.h"
#include <cstdint>

class IS31FL3726A
{
public:
    static constexpr uint8_t ROW_COUNT = 4;
    static constexpr uint8_t COLUMN_COUNT = 4;

    /*
        Constructor order matches the real IS31FL3726A control pins:

        serIn  -> IS31FL3726A SERIAL_IN
        clk    -> IS31FL3726A CLK
        latch  -> IS31FL3726A LATCH

        Current CubeMX mapping:
        LED_LATCH  = PE12
        LED_SER_IN = PE13
        LED_CLK    = PE14
    */
    IS31FL3726A(GPIO_TypeDef* serInPort,
                uint16_t serInPin,
                GPIO_TypeDef* clkPort,
                uint16_t clkPin,
                GPIO_TypeDef* latchPort,
                uint16_t latchPin);

    void begin(bool clearLeds = true);

    void clear();
    void allOn();

    void setLed(uint8_t row, uint8_t column, bool on);
    void toggleLed(uint8_t row, uint8_t column);

    void setRow(uint8_t row, bool on);
    void setColumn(uint8_t column, bool on);

    void toggleRow(uint8_t row);
    void toggleColumn(uint8_t column);

    void setRaw(uint16_t value);
    uint16_t raw() const;

    void show();

private:
    GPIO_TypeDef* serInPort_;
    uint16_t serInPin_;

    GPIO_TypeDef* clkPort_;
    uint16_t clkPin_;

    GPIO_TypeDef* latchPort_;
    uint16_t latchPin_;

    uint16_t state_ = 0x0000;

    static bool validLedAddress(uint8_t row, uint8_t column);
    static bool validRow(uint8_t row);
    static bool validColumn(uint8_t column);

    static uint8_t outputIndex(uint8_t row, uint8_t column);
    static uint16_t maskForLed(uint8_t row, uint8_t column);
    static uint16_t maskForRow(uint8_t row);
    static uint16_t maskForColumn(uint8_t column);

    void shift16(uint16_t value);
    void pulseClock();
    void pulseLatch();
};
