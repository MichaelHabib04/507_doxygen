/**
 * @file probe.c
 * @brief Implementation of TDS probe sampling, conversion, and UART reporting.
 *
 * @date Created on Jun 11, 2026
 * @author malai
 */

#include "probe.h"
#include <stdio.h>

/**
 * @brief Initialize a TDS probe state structure.
 *
 * The probe starts with zeroed ADC, voltage, and ppm values. Temperature is set
 * to 25 degrees Celsius, which is the reference temperature for the compensation
 * calculation used by tds_read_ppm().
 *
 * @param tds Pointer to the TDS state structure to initialize.
 * @param hadc ADC handle used for probe sampling.
 * @param huart UART handle used for reporting probe values.
 * @param vref ADC reference voltage in volts.
 */
void tds_init(tds_t *tds, ADC_HandleTypeDef *hadc, UART_HandleTypeDef *huart, float vref)
{
    tds->hadc = hadc;
    tds->huart = huart;
    tds->adc_value = 0;
    tds->voltage = 0.0f;
    tds->ppm = 0.0f;
    tds->temperature_c = 25.0f;
    tds->vref = vref;
}

/**
 * @brief Sample the ADC and calculate the compensated TDS value.
 *
 * The ADC reading is converted to volts using the configured reference voltage.
 * A temperature compensation factor is applied before the TDS polynomial is
 * evaluated.
 *
 * @param tds Pointer to the initialized TDS state structure.
 * @return Latest TDS estimate in parts per million.
 */
float tds_read_ppm(tds_t *tds)
{
    HAL_ADC_Start(tds->hadc);

    if (HAL_ADC_PollForConversion(tds->hadc, 100) == HAL_OK) {
        tds->adc_value = HAL_ADC_GetValue(tds->hadc);
    }

    HAL_ADC_Stop(tds->hadc);

    tds->voltage = ((float)tds->adc_value * tds->vref) / 4095.0f;

    float compensation = 1.0f + 0.02f * (tds->temperature_c - 25.0f);
    float compensated_voltage = tds->voltage / compensation;

    tds->ppm =
        (133.42f * compensated_voltage * compensated_voltage * compensated_voltage
        -255.86f * compensated_voltage * compensated_voltage
        +857.39f * compensated_voltage) * 0.5f;

    return tds->ppm;
}

/**
 * @brief Print the most recent ADC, voltage, and TDS values over UART.
 *
 * @param tds Pointer to the initialized TDS state structure.
 */
void tds_uart_print(tds_t *tds)
{
    char msg[100];

    uint32_t voltage_mv = (uint32_t)(tds->voltage * 1000.0f);
    uint32_t ppm_int = (uint32_t)(tds->ppm);

    int len = snprintf(msg, sizeof(msg),
                       "ADC: %lu | Voltage: %lu mV | TDS: %lu ppm\r\n",
                       tds->adc_value,
                       voltage_mv,
                       ppm_int);

    HAL_UART_Transmit(tds->huart, (uint8_t *)msg, len, 100);
}
