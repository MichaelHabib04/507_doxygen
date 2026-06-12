#pragma once
#ifndef BUTTON_H_
#define BUTTON_H_

#include <stdint.h>
#include "main.h"

/**
 * Button driver object class. Enables button object with built-in DB.
 */

class Button{
public:
	enum State{
		INACTIVE,
		DB_PRESS,
		ACTIVE,
		WAIT_RELEASE,
		DB_RELEASE
	};

	Button(GPIO_TypeDef* port, uint16_t pin, bool holdable);
	bool easy_update();
	void update();
	void tick();

	bool pressed() const;
	State state() const;

private:
	GPIO_TypeDef* gpioPort;
	uint16_t gpioPin;
	bool isHoldable; // can hold button down
	bool isPressed; // set HI on first contact, set LO on first break
	bool readPin() const;
	char bPin; // pointer to GPIO pin, can be reassigned

	State currentState;

	uint32_t delayDB;
	static const uint32_t debounceTimeMs = 20;
};
#endif
