#pragma once

#include "stm32f4xx_hal.h"
#include <cstdint>
#include <cstddef>

class ST7789 {
public:
    static constexpr uint16_t WIDTH  = 240;
    static constexpr uint16_t HEIGHT = 320;

    ST7789(GPIO_TypeDef* sckPort,
           uint16_t sckPin,
           GPIO_TypeDef* mosiPort,
           uint16_t mosiPin,
           GPIO_TypeDef* rstPort,
           uint16_t rstPin);

    void init();

    void drawPixel(uint16_t x, uint16_t y, uint16_t color);
    void fillScreen(uint16_t color);
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

    void drawImage(uint16_t x,
                   uint16_t y,
                   uint16_t w,
                   uint16_t h,
                   const uint16_t* image565);

private:
    GPIO_TypeDef* _sckPort;
    GPIO_TypeDef* _mosiPort;
    GPIO_TypeDef* _rstPort;

    uint16_t _sckPin;
    uint16_t _mosiPin;
    uint16_t _rstPin;

    void reset();

    void write9(bool isData, uint8_t value);
    void command(uint8_t cmd);
    void data(uint8_t value);
    void data16(uint16_t value);

    void setAddressWindow(uint16_t x0,
                          uint16_t y0,
                          uint16_t x1,
                          uint16_t y1);

    void pulseClock();
};
