#include "zh_dht.h"

#define ZH_DHT_1_BIT_TRANSFER_MAX_DURATION 75		   // Signal "1" high time.
#define ZH_DHT_0_BIT_TRANSFER_MAX_DURATION 30		   // Signal "0" high time.
#define ZH_DHT_DATA_BIT_START_TRANSFER_MAX_DURATION 55 // Signal "0", "1" low time.
#define ZH_DHT_RESPONSE_MAX_DURATION 85				   // Response to low time. Response to high time.
#define ZH_MASTER_RELEASE_MAX_DURATION 200			   // Bus master has released time.
#define ZH_DHT_DATA_SIZE 40

#ifdef CONFIG_IDF_TARGET_ESP8266
#define esp_delay_us(x) os_delay_us(x)
#else
#define esp_delay_us(x) esp_rom_delay_us(x)
static portMUX_TYPE s_spinlock = portMUX_INITIALIZER_UNLOCKED;
#endif

static esp_err_t s_dht_read_bit(zh_dht_handle_t *dht_handle, bool *bit);

zh_dht_handle_t zh_dht_init(zh_dht_sensor_type_t sensor_type, uint8_t sensor_pin)
{
	zh_dht_handle_t zh_dht_handle = {0};
	zh_dht_handle.sensor_type = sensor_type;
	zh_dht_handle.sensor_pin = sensor_pin;
	gpio_config_t config = {0};
	config.intr_type = GPIO_INTR_DISABLE;
	config.mode = GPIO_MODE_INPUT;
	config.pin_bit_mask = (1ULL << sensor_pin);
	config.pull_down_en = GPIO_PULLDOWN_DISABLE;
	config.pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(&config);
	return zh_dht_handle;
}

esp_err_t zh_dht_read(zh_dht_handle_t *dht_handle, float *humidity, float *temperature)
{
	if (dht_handle == NULL || humidity == NULL || temperature == NULL)
	{
		return ESP_ERR_INVALID_ARG;
	}
	if (gpio_get_level(dht_handle->sensor_pin) != 1)
	{
		return ESP_ERR_INVALID_RESPONSE;
	}
	gpio_set_direction(dht_handle->sensor_pin, GPIO_MODE_OUTPUT);
	gpio_set_level(dht_handle->sensor_pin, 0);
	vTaskDelay(10 / portTICK_PERIOD_MS);
#ifdef CONFIG_IDF_TARGET_ESP8266
	taskENTER_CRITICAL();
#else
	taskENTER_CRITICAL(&s_spinlock);
#endif
	gpio_set_level(dht_handle->sensor_pin, 1);
	gpio_set_direction(dht_handle->sensor_pin, GPIO_MODE_INPUT);
	uint8_t time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 1)
	{
		if (time > ZH_MASTER_RELEASE_MAX_DURATION)
		{
#ifdef CONFIG_IDF_TARGET_ESP8266
			taskENTER_CRITICAL();
#else
			taskENTER_CRITICAL(&s_spinlock);
#endif
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 0)
	{
		if (time > ZH_DHT_RESPONSE_MAX_DURATION)
		{
#ifdef CONFIG_IDF_TARGET_ESP8266
			taskEXIT_CRITICAL();
#else
			taskEXIT_CRITICAL(&s_spinlock);
#endif
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 1)
	{
		if (time > ZH_DHT_RESPONSE_MAX_DURATION)
		{
#ifdef CONFIG_IDF_TARGET_ESP8266
			taskEXIT_CRITICAL();
#else
			taskEXIT_CRITICAL(&s_spinlock);
#endif
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	uint8_t dht_data[ZH_DHT_DATA_SIZE / 8] = {0};
	uint8_t byte_index = 0;
	uint8_t bit_index = 7;
	for (uint8_t i = 0; i < 40; ++i)
	{
		bool bit = 0;
		s_dht_read_bit(dht_handle, &bit);
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
#ifdef CONFIG_IDF_TARGET_ESP8266
	taskEXIT_CRITICAL();
#else
	taskEXIT_CRITICAL(&s_spinlock);
#endif
	if (dht_data[4] != ((dht_data[0] + dht_data[1] + dht_data[2] + dht_data[3])))
	{
		return ESP_ERR_INVALID_CRC;
	}
	*humidity = (dht_data[0] << 8 | dht_data[1]) / 10.0;
	if (dht_handle->sensor_type == ZH_DHT22)
	{
		*temperature = ((dht_data[2] & 0b01111111) << 8 | dht_data[3]) / 10.0;
		if ((dht_data[2] & 0b10000000) != 0)
		{
			*temperature *= -1;
		}
	}
	else
	{
		*temperature = (dht_data[2] << 8 | dht_data[3]) / 10.0;
	}
	return ESP_OK;
}

static esp_err_t s_dht_read_bit(zh_dht_handle_t *dht_handle, bool *bit)
{
	uint8_t time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 0)
	{
		if (time > ZH_DHT_DATA_BIT_START_TRANSFER_MAX_DURATION)
		{
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	time = 0;
	while (gpio_get_level(dht_handle->sensor_pin) == 1)
	{
		if (time > ZH_DHT_1_BIT_TRANSFER_MAX_DURATION)
		{
			return ESP_ERR_TIMEOUT;
		}
		++time;
		esp_delay_us(1);
	}
	*bit = (time > ZH_DHT_0_BIT_TRANSFER_MAX_DURATION) ? 1 : 0;
	return ESP_OK;
}