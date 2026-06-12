/**
 * @file float.c
 * @brief Implementation of the float switch GPIO read helper.
 *
 * @date Created on Jun 9, 2026
 * @author malaika
 */

#include "float.h"

/**
 * @brief Read the current state of the float switch input pin.
 *
 * @param p_float Pointer to the float switch configuration.
 * @return GPIO_PIN_SET if the configured input is high; GPIO_PIN_RESET if low.
 */
GPIO_PinState read_float(float_t* p_float){
	    return HAL_GPIO_ReadPin(p_float->gpio_float, p_float->pin_float);
}
