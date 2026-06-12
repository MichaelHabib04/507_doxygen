/**
 * @file motor_driver.h
 * @brief PWM motor driver interface for a two-channel motor output.
 *
 * This module controls a motor using STM32 timer PWM channels. One channel is
 * driven with the requested duty cycle while the second channel is held low.
 *
 * @date Created on Jun 2, 2026
 * @author malaika
 */

#ifndef INC_MOTOR_DRIVER_H_
#define INC_MOTOR_DRIVER_H_

#include "stm32f4xx_hal.h"

/**
 * @brief Timer channel configuration for a PWM-controlled motor.
 *
 * The motor is represented by two timer output channels and the timer handle
 * that owns those channels.
 */
typedef struct {

	/** @brief Primary PWM timer channel used to drive the motor. */
	uint16_t    motor_out_1;

	/** @brief Secondary PWM timer channel held low for this drive direction. */
	uint16_t    motor_out_2;

	/** @brief STM32 HAL timer handle that owns the PWM channels. */
	TIM_HandleTypeDef* htim;


} motor_t;

/**
 * @brief Start PWM output on both configured motor channels.
 *
 * @param p_mot Pointer to the motor configuration.
 */
void motor_chan_enable(motor_t* p_mot);

/**
 * @brief Set the motor duty cycle as a percentage of the timer period.
 *
 * @param p_mot Pointer to the motor configuration.
 * @param duty_cycle Duty cycle percentage from 0 to 100.
 */
void motor_set_duty_cycle(motor_t* p_mot, int8_t duty_cycle);

/**
 * @brief Stop the motor by setting both PWM compare values to zero.
 *
 * @param p_mot Pointer to the motor configuration.
 */
void motor_stop(motor_t* p_mot);

#endif /* INC_MOTOR_DRIVER_H_ */
