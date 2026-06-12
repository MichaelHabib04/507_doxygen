#include "button.h"

Button::Button(GPIO_TypeDef* port, uint16_t pin, bool holdable)
    : gpioPort(port),
      gpioPin(pin),
	  isHoldable(holdable),
	  isPressed(false),
	  currentState(INACTIVE),
	  delayDB(0)
{
}

void Button::tick(){
	if(delayDB>0){
		delayDB--;
	}
}

bool Button::easy_update(){
	bool pinPressed = readPin();
	return pinPressed;
}

void Button::update()
{
    bool pinPressed = readPin();

    switch (currentState) {
    case INACTIVE:
        isPressed = false;

        if (pinPressed) {
            delayDB = debounceTimeMs;
            currentState = DB_PRESS;
        }
        break;

    case DB_PRESS:
        if (!pinPressed) {
            currentState = INACTIVE;
            isPressed = false;
        } else if (delayDB == 0) {
            isPressed = true;
            currentState = ACTIVE;
        }
        break;

    case ACTIVE:
        if (isHoldable) {
            if (pinPressed) {
                isPressed = true;
            } else {
                delayDB = debounceTimeMs;
                currentState = DB_RELEASE;
            }
        } else {
            isPressed = true;
            currentState = WAIT_RELEASE;
        }
        break;

    case WAIT_RELEASE:
        isPressed = false;

        if (!pinPressed) {
            delayDB = debounceTimeMs;
            currentState = DB_RELEASE;
        }
        break;

    case DB_RELEASE:
        if (pinPressed) {
            if (isHoldable) {
                currentState = ACTIVE;
                isPressed = true;
            } else {
                currentState = WAIT_RELEASE;
                isPressed = false;
            }
        } else if (delayDB == 0) {
            isPressed = false;
            currentState = INACTIVE;
        }
        break;
    }
}

bool Button::pressed() const {
	return isPressed;
}

Button::State Button::state() const {
	return currentState;
}

bool Button::readPin() const {
    return HAL_GPIO_ReadPin(gpioPort, gpioPin) == GPIO_PIN_SET;
}
