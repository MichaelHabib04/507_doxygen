#include "IS31FL3726A.h"

IS31FL3726A::IS31FL3726A(GPIO_TypeDef* serInPort,
                         uint16_t serInPin,
                         GPIO_TypeDef* clkPort,
                         uint16_t clkPin,
                         GPIO_TypeDef* latchPort,
                         uint16_t latchPin)
    : serInPort_(serInPort),
      serInPin_(serInPin),
      clkPort_(clkPort),
      clkPin_(clkPin),
      latchPort_(latchPort),
      latchPin_(latchPin)
{
}

void IS31FL3726A::begin(bool clearLeds)
{
    HAL_GPIO_WritePin(serInPort_, serInPin_, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(clkPort_, clkPin_, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(latchPort_, latchPin_, GPIO_PIN_RESET);

    if (clearLeds)
    {
        clear();
    }
    else
    {
        show();
    }
}

void IS31FL3726A::clear()
{
    state_ = 0x0000;
    show();
}

void IS31FL3726A::allOn()
{
    state_ = 0xFFFF;
    show();
}

void IS31FL3726A::setLed(uint8_t row, uint8_t column, bool on)
{
    if (!validLedAddress(row, column))
    {
        return;
    }

    const uint16_t mask = maskForLed(row, column);

    if (on)
    {
        state_ |= mask;
    }
    else
    {
        state_ &= static_cast<uint16_t>(~mask);
    }

    show();
}

void IS31FL3726A::toggleLed(uint8_t row, uint8_t column)
{
    if (!validLedAddress(row, column))
    {
        return;
    }

    state_ ^= maskForLed(row, column);
    show();
}

void IS31FL3726A::setRow(uint8_t row, bool on)
{
    if (!validRow(row))
    {
        return;
    }

    const uint16_t mask = maskForRow(row);

    if (on)
    {
        state_ |= mask;
    }
    else
    {
        state_ &= static_cast<uint16_t>(~mask);
    }

    show();
}

void IS31FL3726A::setColumn(uint8_t column, bool on)
{
    if (!validColumn(column))
    {
        return;
    }

    const uint16_t mask = maskForColumn(column);

    if (on)
    {
        state_ |= mask;
    }
    else
    {
        state_ &= static_cast<uint16_t>(~mask);
    }

    show();
}

void IS31FL3726A::toggleRow(uint8_t row)
{
    if (!validRow(row))
    {
        return;
    }

    state_ ^= maskForRow(row);
    show();
}

void IS31FL3726A::toggleColumn(uint8_t column)
{
    if (!validColumn(column))
    {
        return;
    }

    state_ ^= maskForColumn(column);
    show();
}

void IS31FL3726A::setRaw(uint16_t value)
{
    state_ = value;
    show();
}

uint16_t IS31FL3726A::raw() const
{
    return state_;
}

void IS31FL3726A::show()
{
    shift16(state_);
    pulseLatch();
}

bool IS31FL3726A::validLedAddress(uint8_t row, uint8_t column)
{
    return validRow(row) && validColumn(column);
}

bool IS31FL3726A::validRow(uint8_t row)
{
    return row >= 1 && row <= ROW_COUNT;
}

bool IS31FL3726A::validColumn(uint8_t column)
{
    return column >= 1 && column <= COLUMN_COUNT;
}

/*
    Logical LED map, using 1-based row/column addresses:

             Column 1   Column 2   Column 3   Column 4
    Row 1    OUT0       OUT4       OUT8       OUT12
    Row 2    OUT1       OUT5       OUT9       OUT13
    Row 3    OUT2       OUT6       OUT10      OUT14
    Row 4    OUT3       OUT7       OUT11      OUT15

    TSSOP-24-EP package pin map:

    Column 1: pins  5,  6,  7,  8
    Column 2: pins  9, 10, 11, 12
    Column 3: pins 13, 14, 15, 16
    Column 4: pins 17, 18, 19, 20

    Therefore:
    Row 1 = pins 5, 9, 13, 17
    Row 2 = pins 6, 10, 14, 18
    Row 3 = pins 7, 11, 15, 19
    Row 4 = pins 8, 12, 16, 20
*/
uint8_t IS31FL3726A::outputIndex(uint8_t row, uint8_t column)
{
    return static_cast<uint8_t>((column - 1U) * ROW_COUNT + (row - 1U));
}

uint16_t IS31FL3726A::maskForLed(uint8_t row, uint8_t column)
{
    return static_cast<uint16_t>(1U << outputIndex(row, column));
}

uint16_t IS31FL3726A::maskForRow(uint8_t row)
{
    uint16_t mask = 0x0000;

    for (uint8_t column = 1; column <= COLUMN_COUNT; ++column)
    {
        mask |= maskForLed(row, column);
    }

    return mask;
}

uint16_t IS31FL3726A::maskForColumn(uint8_t column)
{
    uint16_t mask = 0x0000;

    for (uint8_t row = 1; row <= ROW_COUNT; ++row)
    {
        mask |= maskForLed(row, column);
    }

    return mask;
}

void IS31FL3726A::shift16(uint16_t value)
{
    /*
        The Lumissil example shifts MSB first:
        bit 15 first, bit 0 last.
    */
    for (int8_t bit = 15; bit >= 0; --bit)
    {
        const bool bitSet = (value & static_cast<uint16_t>(1U << bit)) != 0;

        HAL_GPIO_WritePin(serInPort_,
                          serInPin_,
                          bitSet ? GPIO_PIN_SET : GPIO_PIN_RESET);

        pulseClock();
    }

    HAL_GPIO_WritePin(serInPort_, serInPin_, GPIO_PIN_RESET);
}

void IS31FL3726A::pulseClock()
{
    HAL_GPIO_WritePin(clkPort_, clkPin_, GPIO_PIN_SET);
    HAL_GPIO_WritePin(clkPort_, clkPin_, GPIO_PIN_RESET);
}

void IS31FL3726A::pulseLatch()
{
    HAL_GPIO_WritePin(latchPort_, latchPin_, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(latchPort_, latchPin_, GPIO_PIN_RESET);
    HAL_Delay(1);
}
