#include "ST7789.h"

/*
 * ST7789 command set
 */
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVOFF   0x20
#define ST7789_INVON    0x21
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A

ST7789::ST7789(GPIO_TypeDef* sckPort,
               uint16_t sckPin,
               GPIO_TypeDef* mosiPort,
               uint16_t mosiPin,
               GPIO_TypeDef* rstPort,
               uint16_t rstPin)
    : _sckPort(sckPort),
      _mosiPort(mosiPort),
      _rstPort(rstPort),
      _sckPin(sckPin),
      _mosiPin(mosiPin),
      _rstPin(rstPin)
{
}

/*
 * Hardware reset.
 *
 * TFT /RES is active LOW.
 */
void ST7789::reset()
{
    HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_SET);
    HAL_Delay(10);

    HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_RESET);
    HAL_Delay(20);

    HAL_GPIO_WritePin(_rstPort, _rstPin, GPIO_PIN_SET);
    HAL_Delay(150);
}

/*
 * One clock pulse.
 *
 * This is SPI mode 0 style:
 *   - clock idles LOW
 *   - data is changed while clock is LOW
 *   - display samples on rising edge
 */
void ST7789::pulseClock()
{
    __NOP();
    __NOP();

    HAL_GPIO_WritePin(_sckPort, _sckPin, GPIO_PIN_SET);

    __NOP();
    __NOP();

    HAL_GPIO_WritePin(_sckPort, _sckPin, GPIO_PIN_RESET);

    __NOP();
    __NOP();
}

/*
 * Write one 9-bit frame for 3-line serial mode.
 *
 * Frame format:
 *
 *   bit 8    = D/C bit
 *              0 = command
 *              1 = data
 *
 *   bits 7:0 = payload byte
 *
 * Physical D/C pin on the TFT must be grounded in this mode.
 * Physical /CS pin is assumed to be grounded.
 */
void ST7789::write9(bool isData, uint8_t value)
{
    uint16_t frame = static_cast<uint16_t>(((isData ? 1U : 0U) << 8) | value);

    for (int bit = 8; bit >= 0; --bit)
    {
        HAL_GPIO_WritePin(_sckPort, _sckPin, GPIO_PIN_RESET);

        GPIO_PinState pinState =
            (frame & (1U << bit)) ? GPIO_PIN_SET : GPIO_PIN_RESET;

        HAL_GPIO_WritePin(_mosiPort, _mosiPin, pinState);

        pulseClock();
    }
}

void ST7789::command(uint8_t cmd)
{
    write9(false, cmd);
}

void ST7789::data(uint8_t value)
{
    write9(true, value);
}

/*
 * ST7789 expects RGB565 as high byte, then low byte.
 */
void ST7789::data16(uint16_t value)
{
    data(static_cast<uint8_t>(value >> 8));
    data(static_cast<uint8_t>(value & 0xFF));
}

/*
 * Basic ST7789 initialization.
 *
 * This is intentionally conservative and minimal.
 * Once communication is confirmed, this can be expanded with
 * Newhaven-specific gamma/power commands if needed.
 */
void ST7789::init()
{
    /*
     * Idle states before reset.
     */
    HAL_GPIO_WritePin(_sckPort, _sckPin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(_mosiPort, _mosiPin, GPIO_PIN_RESET);

    reset();

    command(ST7789_SWRESET);
    HAL_Delay(150);

    command(ST7789_SLPOUT);
    HAL_Delay(150);

    /*
     * Interface pixel format.
     * 0x55 = 16 bits per pixel, RGB565.
     */
    command(ST7789_COLMOD);
    data(0x55);
    HAL_Delay(10);

    /*
     * Memory access control.
     *
     * 0x00 = normal portrait orientation.
     * If colors are swapped, try 0x08.
     * If orientation is wrong, MADCTL can be adjusted later.
     */
    command(ST7789_MADCTL);
    data(0x00);

    /*
     * Many ST7789 panels expect inversion ON.
     * If the image looks strange later, try INVOFF instead.
     */
    command(ST7789_INVON);
    HAL_Delay(10);

    command(ST7789_NORON);
    HAL_Delay(10);

    command(ST7789_DISPON);
    HAL_Delay(150);
}

/*
 * Set the rectangular RAM write region.
 *
 * ST7789 uses:
 *   CASET = column address set
 *   RASET = row address set
 *   RAMWR = begin memory write
 */
void ST7789::setAddressWindow(uint16_t x0,
                              uint16_t y0,
                              uint16_t x1,
                              uint16_t y1)
{
    command(ST7789_CASET);
    data16(x0);
    data16(x1);

    command(ST7789_RASET);
    data16(y0);
    data16(y1);

    command(ST7789_RAMWR);
}

void ST7789::drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= WIDTH || y >= HEIGHT)
    {
        return;
    }

    setAddressWindow(x, y, x, y);
    data16(color);
}

void ST7789::fillRect(uint16_t x,
                      uint16_t y,
                      uint16_t w,
                      uint16_t h,
                      uint16_t color)
{
    if (x >= WIDTH || y >= HEIGHT)
    {
        return;
    }

    if ((x + w) > WIDTH)
    {
        w = WIDTH - x;
    }

    if ((y + h) > HEIGHT)
    {
        h = HEIGHT - y;
    }

    setAddressWindow(x, y, x + w - 1, y + h - 1);

    uint32_t pixelCount = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);

    for (uint32_t i = 0; i < pixelCount; ++i)
    {
        data16(color);
    }
}

void ST7789::fillScreen(uint16_t color)
{
    fillRect(0, 0, WIDTH, HEIGHT, color);
}

void ST7789::drawImage(uint16_t x,
                       uint16_t y,
                       uint16_t w,
                       uint16_t h,
                       const uint16_t* image565)
{
    if (image565 == nullptr)
    {
        return;
    }

    if (x >= WIDTH || y >= HEIGHT)
    {
        return;
    }

    if ((x + w) > WIDTH)
    {
        w = WIDTH - x;
    }

    if ((y + h) > HEIGHT)
    {
        h = HEIGHT - y;
    }

    setAddressWindow(x, y, x + w - 1, y + h - 1);

    uint32_t pixelCount = static_cast<uint32_t>(w) * static_cast<uint32_t>(h);

    for (uint32_t i = 0; i < pixelCount; ++i)
    {
        data16(image565[i]);
    }
}
