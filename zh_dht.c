#include "zh_dht.h"

#define BIT_1_TRANSFER_MAX_DURATION 75			// Signal "1" high time for 1-wire connection.
#define BIT_0_TRANSFER_MAX_DURATION 30			// Signal "0" high time for 1-wire connection.
#define DATA_BIT_START_TRANSFER_MAX_DURATION 55 // Signal "0", "1" low time for 1-wire connection.
#define RESPONSE_MAX_DURATION 85				// Response to low time. Response to high time. For 1-wire connection.
#define MASTER_RELEASE_MAX_DURATION 200			// Bus master has released time for 1-wire connection.
#define ONE_WIRE_DATA_SIZE 40					// Sensor data size for 1-wire connection (in bits).
#define I2C_DATA_SIZE 8							// Sensor data size for I2C connection (in bytes).
#define I2C_ADDRESS 0x5C						// Sensor address for I2C connection.
#define I2C_DATA_READ_COMMAND 0x03, 0x00, 0x04	// Command for read sensor data (temperature and humidity) for I2C connection.

#ifdef CONFIG_IDF_TARGET_ESP8266
#define esp_delay_us(x) os_delay_us(x)
#else
#define esp_delay_us(x) esp_rom_delay_us(x)
#endif

static zh_dht_init_config_t _init_config = {0};
static bool _is_initialized = false;
#ifndef CONFIG_IDF_TARGET_ESP8266
static i2c_master_dev_handle_t _dht_handle = {0};
#endif

static const char *TAG = "zh_dht";

static esp_err_t _read_bit(bool *bit);
static uint16_t _calc_crc(const uint8_t *buf, size_t len);

esp_err_t zh_dht_init(const zh_dht_init_config_t *config)
{
	ESP_LOGI(TAG, "DHT initialization begin.");
	if (config == NULL)
	{
		ESP_LOGE(TAG, "DHT initialization fail. Invalid argument.");
		return ESP_ERR_INVALID_ARG;
	}
	_init_config = *config;
	if (_init_config.sensor_pin != 0xFF)
	{
		gpio_config_t one_wire_config = {0};
		one_wire_config.intr_type = GPIO_INTR_DISABLE;
		one_wire_config.mode = GPIO_MODE_INPUT;
		one_wire_config.pin_bit_mask = (1ULL << _init_config.sensor_pin);
		one_wire_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
		one_wire_config.pull_up_en = GPIO_PULLUP_ENABLE;
		if (gpio_config(&one_wire_config) != ESP_OK)
		{
			ESP_LOGE(TAG, "DHT initialization fail. Incorrect GPIO number.");
			return ESP_ERR_INVALID_ARG;
		}
		ESP_LOGI(TAG, "DHT initialization success. 1-wire connection.");
	}
	else
	{
#ifndef CONFIG_IDF_TARGET_ESP8266
		i2c_device_config_t dht_config = {
			.dev_addr_length = I2C_ADDR_BIT_LEN_7,
			.device_address = I2C_ADDRESS,
			.scl_speed_hz = 100000,
		};
		i2c_master_bus_add_device(_init_config.i2c_handle, &dht_config, &_dht_handle);
		if (i2c_master_probe(_init_config.i2c_handle, I2C_ADDRESS, 1000 / portTICK_PERIOD_MS) != ESP_OK)
		{
			ESP_LOGE(TAG, "DHT initialization fail. Sensor not connected or not responded.");
			return ESP_ERR_NOT_FOUND;
		}
#endif
		ESP_LOGI(TAG, "DHT initialization success. I2C connection.");
	}
	_is_initialized = true;
	return ESP_OK;
}

esp_err_t zh_dht_read(float *humidity, float *temperature)
{
	ESP_LOGI(TAG, "DHT read begin.");
	if (humidity == NULL || temperature == NULL)
	{
		ESP_LOGE(TAG, "DHT read fail. Invalid argument.");
		return ESP_ERR_INVALID_ARG;
	}
	if (_is_initialized == false)
	{
		ESP_LOGE(TAG, "DHT read fail. DHT not initialized.");
		return ESP_ERR_NOT_FOUND;
	}
	if (_init_config.sensor_pin != 0xFF)
	{
		if (gpio_get_level(_init_config.sensor_pin) != 1)
		{
			ESP_LOGE(TAG, "DHT read fail. Bus is busy.");
			return ESP_ERR_INVALID_RESPONSE;
		}
		gpio_set_direction(_init_config.sensor_pin, GPIO_MODE_OUTPUT);
		gpio_set_level(_init_config.sensor_pin, 0);
		vTaskDelay(10 / portTICK_PERIOD_MS);
		gpio_set_level(_init_config.sensor_pin, 1);
		gpio_set_direction(_init_config.sensor_pin, GPIO_MODE_INPUT);
		uint8_t time = 0;
		while (gpio_get_level(_init_config.sensor_pin) == 1)
		{
			if (time > MASTER_RELEASE_MAX_DURATION)
			{
				ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
				return ESP_ERR_TIMEOUT;
			}
			++time;
			esp_delay_us(1);
		}
		time = 0;
		while (gpio_get_level(_init_config.sensor_pin) == 0)
		{
			if (time > RESPONSE_MAX_DURATION)
			{
				ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
				return ESP_ERR_TIMEOUT;
			}
			++time;
			esp_delay_us(1);
		}
		time = 0;
		while (gpio_get_level(_init_config.sensor_pin) == 1)
		{
			if (time > RESPONSE_MAX_DURATION)
			{
				ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
				return ESP_ERR_TIMEOUT;
			}
			++time;
			esp_delay_us(1);
		}
		uint8_t dht_data[ONE_WIRE_DATA_SIZE / 8] = {0};
		uint8_t byte_index = 0;
		uint8_t bit_index = 7;
		for (uint8_t i = 0; i < ONE_WIRE_DATA_SIZE; ++i)
		{
			bool bit = 0;
			if (_read_bit(&bit) != ESP_OK)
			{
				ESP_LOGE(TAG, "DHT read fail. Timeout exceeded.");
				return ESP_ERR_TIMEOUT;
			}
			dht_data[byte_index] |= (bit << bit_index);
			if (bit_index == 0)
			{
				bit_index = 7;
				++byte_index;
			}
			else
			{
				--bit_index;
			}
		}
		if (dht_data[4] != ((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3])))
		{
			ESP_LOGE(TAG, "DHT read fail. Invalid CRC.");
			return ESP_ERR_INVALID_CRC;
		}
		*humidity = (dht_data[0] << 8 | dht_data[1]) / 10.0;
		*temperature = ((dht_data[2] & 0x7F) << 8 | dht_data[3]) / 10.0;
		if ((dht_data[2] & 0x80) != 0)
		{
			*temperature *= -1;
		}
	}
	else
	{
		esp_err_t esp_err = ESP_OK;
		uint8_t dht_data[I2C_DATA_SIZE] = {0};
		uint8_t read_command[] = {I2C_DATA_READ_COMMAND};
#ifdef CONFIG_IDF_TARGET_ESP8266
		i2c_cmd_handle_t i2c_cmd_handle = i2c_cmd_link_create();
		i2c_master_start(i2c_cmd_handle);
		i2c_master_write_byte(i2c_cmd_handle, I2C_ADDRESS << 1 | I2C_MASTER_WRITE, false);
		i2c_master_stop(i2c_cmd_handle);
		esp_err = i2c_master_cmd_begin(_init_config.i2c_port, i2c_cmd_handle, 1000 / portTICK_PERIOD_MS);
		i2c_cmd_link_delete(i2c_cmd_handle);
		if (esp_err != ESP_OK)
		{
			ESP_LOGE(TAG, "DHT read fail. I2C driver error.");
			return esp_err;
		}
		i2c_cmd_handle = i2c_cmd_link_create();
		i2c_master_start(i2c_cmd_handle);
		i2c_master_write_byte(i2c_cmd_handle, I2C_ADDRESS << 1 | I2C_MASTER_WRITE, true);
		for (uint8_t i = 0; i < sizeof(read_command); ++i)
		{
			i2c_master_write_byte(i2c_cmd_handle, read_command[i], true);
		}
		i2c_master_stop(i2c_cmd_handle);
		esp_err = i2c_master_cmd_begin(_init_config.i2c_port, i2c_cmd_handle, 1000 / portTICK_PERIOD_MS);
		i2c_cmd_link_delete(i2c_cmd_handle);
#else
		uint8_t wakeup_command = {0};
		esp_err = i2c_master_transmit(_dht_handle, &wakeup_command, sizeof(wakeup_command), 1000 / portTICK_PERIOD_MS);
		if (esp_err != ESP_OK)
		{
			ESP_LOGE(TAG, "DHT read fail. I2C driver error.");
			return esp_err;
		}
		esp_err = i2c_master_transmit(_dht_handle, read_command, sizeof(read_command), 1000 / portTICK_PERIOD_MS);
#endif
		if (esp_err != ESP_OK)
		{
			ESP_LOGE(TAG, "DHT read fail. I2C driver error.");
			return esp_err;
		}
#ifdef CONFIG_IDF_TARGET_ESP8266
		i2c_cmd_handle = i2c_cmd_link_create();
		i2c_master_start(i2c_cmd_handle);
		i2c_master_write_byte(i2c_cmd_handle, I2C_ADDRESS << 1 | I2C_MASTER_READ, true);
		for (uint8_t i = 0; i < sizeof(dht_data); ++i)
		{
			i2c_master_read_byte(i2c_cmd_handle, &dht_data[i], i == (sizeof(dht_data) - 1) ? I2C_MASTER_NACK : I2C_MASTER_ACK);
		}
		i2c_master_stop(i2c_cmd_handle);
		esp_err = i2c_master_cmd_begin(_init_config.i2c_port, i2c_cmd_handle, 1000 / portTICK_PERIOD_MS);
		i2c_cmd_link_delete(i2c_cmd_handle);
#else
		esp_err = i2c_master_receive(_dht_handle, dht_data, sizeof(dht_data), 1000 / portTICK_PERIOD_MS);
#endif
		if (esp_err != ESP_OK)
		{
			ESP_LOGE(TAG, "DHT read fail. I2C driver error.");
			return esp_err;
		}
		if (_calc_crc(dht_data, I2C_DATA_SIZE - 2) != (dht_data[7] << 8 | dht_data[6]))
		{
			ESP_LOGE(TAG, "DHT read fail. Invalid CRC.");
			return ESP_ERR_INVALID_CRC;
		}
		*humidity = (dht_data[2] << 8 | dht_data[3]) / 10.0;
		*temperature = ((dht_data[4] & 0x7F) << 8 | dht_data[5]) / 10.0;
		if ((dht_data[4] & 0x80) != 0)
		{
			*temperature *= -1;
		}
	}
	ESP_LOGI(TAG, "DHT read success.");
	return ESP_OK;
}

static esp_err_t _read_bit(bool *bit)
{
	uint8_t time = 0;
	while (gpio_get_level(_init_config.sensor_pin) == 0)
	{
		if (time > DATA_BIT_START_TRANSFER_MAX_DURATION)
		{
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	time = 0;
	while (gpio_get_level(_init_config.sensor_pin) == 1)
	{
		if (time > BIT_1_TRANSFER_MAX_DURATION)
		{
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	*bit = (time > BIT_0_TRANSFER_MAX_DURATION) ? 1 : 0;
	return ESP_OK;
}

static uint16_t _calc_crc(const uint8_t *buf, size_t len)
{
	uint16_t crc = 0xFFFF;
	while (len--)
	{
		crc ^= (uint16_t)*buf++;
		for (unsigned i = 0; i < 8; i++)
		{
			if (crc & 0x0001)
			{
				crc >>= 1;
				crc ^= 0xA001;
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}