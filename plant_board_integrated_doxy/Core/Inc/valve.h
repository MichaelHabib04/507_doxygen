/**
 * @file valve.h
 * @brief Relay-controlled valve interface.
 *
 * This module wraps GPIO writes used to open and close solenoid valves through
 * relay outputs.
 *
 * @date Created on Jun 4, 2026
 * @author malaika
 */

#ifndef INC_VALVE_H_
#define INC_VALVE_H_

#include "stm32f4xx_hal.h"

/**
 * @brief GPIO configuration for a relay-controlled valve.
 */
typedef struct{
	/** @brief GPIO port connected to the relay control input. */
	GPIO_TypeDef* gpio_Relay;

	/** @brief GPIO pin connected to the relay control input. */
	uint16_t pin_Relay;
//	GPIO_TypeDef* Relay2_GPIO_Port;
//	uint16_t Relay2_Pin;
} valve_t;

/**
 * @brief Open the valve by setting the relay GPIO pin high.
 *
 * @param p_valve Pointer to the valve GPIO configuration.
 */
void valve_open(valve_t* p_valve);

/**
 * @brief Close the valve by setting the relay GPIO pin low.
 *
 * @param p_valve Pointer to the valve GPIO configuration.
 */
void valve_close(valve_t* p_valve);

#endif /* INC_VALVE_H_ */
