# ESP32 ESP-IDF and ESP8266 RTOS SDK component for DHT11/DHT22(AM2302) humidity & temperature sensor

## Tested on

1. ESP8266 RTOS_SDK v3.4
2. ESP32 ESP-IDF v5.1.0

## Using

In an existing project, run the following command to install the component:

```text
cd ../your_project/components
git clone http://git.zh.com.ru/alexey.zholtikov/zh_dht.git
```

In the application, add the component:

```c
#include "zh_dht.h"
```

## Example

Reading the sensor:

```c
#include "zh_dht.h"

void app_main()
{
 zh_dht_handle_t zh_dht_handle = zh_dht_init(ZH_DHT22, GPIO_NUM_5);
 float humidity = 0;
 float temperature = 0;
 for (;;)
 {
  zh_dht_read(&zh_dht_handle, &humidity, &temperature);
  printf("Humidity %0.2f\n", humidity);
  printf("Temperature %0.2f\n", temperature);
  vTaskDelay(5000 / portTICK_PERIOD_MS);
 }
}
```

Any [feedback](mailto:github@azholtikov.ru) will be gladly accepted.
