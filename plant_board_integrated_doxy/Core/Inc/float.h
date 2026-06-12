/**
 * @file float.h
 * @brief Interface for reading the water-level float switch.
 *
 * This module wraps the STM32 HAL GPIO read used by the float switch so the
 * application can treat the switch as a small reusable sensor object.
 *
 * @date Created on Jun 9, 2026
 * @author malaika
 */

#ifndef INC_FLOAT_H_
#define INC_FLOAT_H_

#include "stm32f4xx_hal.h"

/**
 * @brief GPIO configuration for a float switch input.
 *
 * The structure stores the GPIO port and pin connected to the float switch.
 * Pass an initialized instance to read_float() to read the switch state.
 */
typedef struct{
	/** @brief GPIO port connected to the float switch input. */
	GPIO_TypeDef* gpio_float;

	/** @brief GPIO pin connected to the float switch input. */
	uint16_t pin_float;

} float_t;

/**
 * @brief Read the current state of the float switch input pin.
 *
 * @param p_float Pointer to the float switch configuration.
 * @return GPIO_PinState Current GPIO state returned by HAL_GPIO_ReadPin().
 */
GPIO_PinState read_float(float_t* p_float);

#endif /* INC_FLOAT_H_ */
