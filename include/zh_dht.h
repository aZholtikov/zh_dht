#pragma once

#include "stdint.h"
#include "stdio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#ifdef CONFIG_IDF_TARGET_ESP8266
#include "driver/i2c.h"
#else
#include "driver/i2c_master.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ZH_DHT_INIT_CONFIG_DEFAULT() \
	{                                \
		.sensor_pin = 0xFF,          \
		.i2c_port = 0                \
	}

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct
	{
		uint8_t sensor_pin; // Sensor GPIO connection.
		bool i2c_port;		// I2C port.
#ifndef CONFIG_IDF_TARGET_ESP8266
		i2c_master_bus_handle_t i2c_handle; // Unique I2C bus handle.
#endif
	} zh_dht_init_config_t;

	/**
	 * @brief Initialize DHT sensor.
	 *
	 * @param[in] config Pointer to DHT initialized configuration structure. Can point to a temporary variable.
	 *
	 * @attention I2C driver must be initialized first (for I2C connection only).
	 *
	 * @note Before initialize the sensor recommend initialize zh_dht_init_config_t structure with default values.
	 *
	 * @code zh_dht_init_config_t config = ZH_DHT_INIT_CONFIG_DEFAULT() @endcode
	 *
	 * @return
	 *              - ESP_OK if initialization was success
	 *              - ESP_ERR_INVALID_ARG if parameter error
	 *              - ESP_ERR_NOT_FOUND if sensor not connected or not responded (for I2C connection only)
	 */
	esp_err_t zh_dht_init(const zh_dht_init_config_t *config);

	/**
	 * @brief Read DHT sensor.
	 *
	 * @param[out] humidity Pointer for DHT sensor reading data of humidity.
	 * @param[out] temperature Pointer for DHT sensor reading data of temperature.
	 *
	 * @return
	 *              - ESP_OK if read was success
	 *              - ESP_ERR_INVALID_ARG if parameter error
	 *              - ESP_ERR_NOT_FOUND if DHT is not initialized
	 *              - ESP_ERR_INVALID_RESPONSE if the bus is busy
	 *              - ESP_ERR_INVALID_STATE if I2C driver not installed or not in master mode
	 *              - ESP_ERR_TIMEOUT if operation timeout
	 *              - ESP_ERR_INVALID_CRC if check CRC is fail
	 */
	esp_err_t zh_dht_read(float *humidity, float *temperature);

#ifdef __cplusplus
}
#endif