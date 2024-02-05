#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum zh_dht_sensor_type_t
	{
		ZH_DHT11 = 0,
		ZH_DHT22
	} __attribute__((packed)) zh_dht_sensor_type_t;

	typedef struct zh_dht_handle_t
	{
		uint8_t sensor_pin;
		zh_dht_sensor_type_t sensor_type;
	} __attribute__((packed)) zh_dht_handle_t;

	/**
	 * @brief      Initialize DHT sensor.
	 *
	 * @param[in]  sensor_type  Sensor type (ZH_DHT11 or ZH_DHT22).
	 * @param[in]  sensor_pin   Sensor connection gpio.
	 *
	 * @return
	 *              - Handle of the sensor.
	 */
	zh_dht_handle_t zh_dht_init(zh_dht_sensor_type_t sensor_type, uint8_t sensor_pin);

	/**
	 * @brief      Read DHT sensor.
	 *
	 * @param[in]   dht_handle   Pointer for handle of the sensor.
	 * @param[out]  humidity     Pointer for DHT sensor reading data of humidity.
	 * @param[out]  temperature  Pointer for DHT sensor reading data of temperature.
	 *
	 * @return
	 *              - ESP_OK if read was success
	 *              - ESP_ERR_INVALID_ARG if parameter error
	 * 				- ESP_ERR_INVALID_RESPONSE if the bus is busy
	 * 				- ESP_ERR_TIMEOUT if operation timeout
	 * 				- ESP_ERR_INVALID_CRC if check CRC is fail
	 */
	esp_err_t zh_dht_read(zh_dht_handle_t *dht_handle, float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif