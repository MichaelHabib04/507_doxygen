/**
 * @file probe.h
 * @brief Interface for reading and reporting TDS probe measurements.
 *
 * This module reads a TDS probe through an ADC channel, converts the ADC count
 * to voltage, applies temperature compensation, estimates parts-per-million,
 * and can print the latest reading over UART.
 *
 * @date Created on Jun 11, 2026
 * @author malai
 */

#ifndef INC_PROBE_H_
#define INC_PROBE_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

/**
 * @brief Runtime state and peripheral handles for a TDS probe.
 */
typedef struct {
    /** @brief ADC handle used to sample the probe voltage. */
    ADC_HandleTypeDef *hadc;

    /** @brief UART handle used to print debug or telemetry messages. */
    UART_HandleTypeDef *huart;

    /** @brief Most recent raw 12-bit ADC conversion value. */
    uint32_t adc_value;

    /** @brief Most recent probe voltage in volts. */
    float voltage;

    /** @brief Most recent estimated total dissolved solids value in ppm. */
    float ppm;

    /** @brief Water temperature used for compensation in degrees Celsius. */
    float temperature_c;

    /** @brief ADC reference voltage in volts. */
    float vref;
} tds_t;

/**
 * @brief Initialize a TDS probe state structure.
 *
 * @param tds Pointer to the TDS state structure to initialize.
 * @param hadc ADC handle used for probe sampling.
 * @param huart UART handle used for reporting probe values.
 * @param vref ADC reference voltage in volts.
 */
void tds_init(tds_t *tds, ADC_HandleTypeDef *hadc, UART_HandleTypeDef *huart, float vref);

/**
 * @brief Read the TDS probe and calculate the compensated ppm value.
 *
 * @param tds Pointer to the initialized TDS state structure.
 * @return Latest TDS estimate in parts per million.
 */
float tds_read_ppm(tds_t *tds);

/**
 * @brief Print the most recent TDS reading over UART.
 *
 * @param tds Pointer to the initialized TDS state structure.
 */
void tds_uart_print(tds_t *tds);

#endif /* INC_PROBE_H_ */
