/**
 * @file valve.c
 * @brief Implementation of relay-controlled valve helpers.
 *
 * @date Created on Jun 4, 2026
 * @author malaika
 */

#include "valve.h"

/**
 * @brief Open the valve by setting the configured relay output high.
 *
 * @param p_valve Pointer to the valve GPIO configuration.
 */
void valve_open(valve_t* p_valve){
	HAL_GPIO_WritePin(p_valve->gpio_Relay, p_valve->pin_Relay, GPIO_PIN_SET);
}

/**
 * @brief Close the valve by setting the configured relay output low.
 *
 * @param p_valve Pointer to the valve GPIO configuration.
 */
void valve_close(valve_t* p_valve){
	HAL_GPIO_WritePin(p_valve->gpio_Relay, p_valve->pin_Relay, GPIO_PIN_RESET);
}
