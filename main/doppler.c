/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

static const char* TAG = "doppler";

#define LED_GPIO CONFIG_LED_GPIO
#define RADAR_GPIO CONFIG_RADAR_GPIO
#define GPIO_RADAR_PIN_SEL (1ULL<<RADAR_GPIO)
#define QUEUE_SIZE 100

#define GPIO_RADAR_HIGH 1
#define GPIO_RADAR_LOW 0

static xQueueHandle gpio_evt_queue = NULL;

void queue_radar_level() {
  uint32_t state = GPIO_RADAR_LOW;

  if(gpio_get_level(RADAR_GPIO) == 1)
    state = GPIO_RADAR_HIGH;

  xQueueSendFromISR(gpio_evt_queue, &state, NULL);
}

static void IRAM_ATTR radar_isr_handler(void* arg) {
  queue_radar_level();
}

static void radar_led_task(void* arg) {
  uint32_t state;
  queue_radar_level();
  for(;;) {
    if(xQueueReceive(gpio_evt_queue, &state, portMAX_DELAY)) {
      if (state == GPIO_RADAR_HIGH) {
	ESP_LOGI(TAG, "Radar high");
	gpio_set_level(LED_GPIO, 0);
      } else if (state == GPIO_RADAR_LOW) {
	ESP_LOGI(TAG, "Radar low");
	gpio_set_level(LED_GPIO, 1);
      }
    }
  }
}

static void radar_busy_task(void* arg) {
  for(;;) {
    uint32_t state = gpio_get_level(RADAR_GPIO);
    gpio_set_level(LED_GPIO, !state);
    if(state) {
      ESP_LOGI(TAG, "Radar high");
    } else {
      ESP_LOGI(TAG, "Radar low");
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

void app_main()
{
  ESP_LOGI(TAG, "Configuring pin %d for radar interrupt", RADAR_GPIO);
  gpio_set_intr_type(RADAR_GPIO, GPIO_INTR_ANYEDGE);
  gpio_set_direction(RADAR_GPIO, GPIO_MODE_INPUT);
  gpio_pulldown_en(RADAR_GPIO);

  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);

  gpio_evt_queue = xQueueCreate(QUEUE_SIZE, sizeof(uint32_t));

  xTaskCreate(radar_led_task, "radar_led_task", 2048, NULL, 10, NULL);
  //xTaskCreate(radar_busy_task, "radar_busy_task", 2048, NULL, 10, NULL);

  gpio_install_isr_service(0);
  gpio_isr_handler_add(RADAR_GPIO, radar_isr_handler, NULL);

}
