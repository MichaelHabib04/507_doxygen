/**
 * @file motor_driver.c
 * @brief Implementation of the PWM motor driver helpers.
 *
 * @date Created on Jun 2, 2026
 * @author malaika
 */

#include "motor_driver.h"
#include "stdlib.h"

/**
 * @brief Start PWM output with interrupts enabled on both motor channels.
 *
 * @param p_mot Pointer to the motor configuration.
 */
void motor_chan_enable(motor_t* p_mot){
	  HAL_TIM_PWM_Start_IT(p_mot->htim, p_mot->motor_out_1);
	  HAL_TIM_PWM_Start_IT(p_mot->htim, p_mot->motor_out_2);
}

/**
 * @brief Set the motor drive duty cycle.
 *
 * The duty cycle is converted from a percentage to a timer compare value using
 * the timer auto-reload register. The second output channel is set to zero.
 *
 * @param p_mot Pointer to the motor configuration.
 * @param duty_cycle Duty cycle percentage from 0 to 100.
 */
void motor_set_duty_cycle(motor_t* p_mot, int8_t duty_cycle){
	__HAL_TIM_SET_COMPARE(p_mot->htim, p_mot->motor_out_1, (duty_cycle*p_mot->htim->Instance->ARR)/100);
	__HAL_TIM_SET_COMPARE(p_mot->htim, p_mot->motor_out_2, 0);
}

/**
 * @brief Stop the motor by clearing both PWM compare registers.
 *
 * @param p_mot Pointer to the motor configuration.
 */
void motor_stop(motor_t* p_mot){
	__HAL_TIM_SET_COMPARE(p_mot->htim, p_mot->motor_out_1, 0);
	__HAL_TIM_SET_COMPARE(p_mot->htim, p_mot->motor_out_2, 0);
}
