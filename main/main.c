#include <stdbool.h>
#include <stdint.h>

#include "button.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "hal/gpio_types.h"
#include "st7789.h"

#define BUTTON_LEFT_GPIO 4
#define BUTTON_DOWN_GPIO 5
#define BUTTON_UP_GPIO 6
#define BUTTON_RIGHT_GPIO 7
#define BUTTON_CONFIRM_GPIO 8
#define BUTTON_CANCEL_GPIO 9

#define DEBOUNCE_CHECKS 5
#define DEBOUNCE_INTERVAL_MS 12

#define CURSOR_COLOR GREEN
#define CURSOR_CLICK_COLOR RED

const static char *TAG = "main";

TFT_t dev;

static uint16_t cursor_color = CURSOR_COLOR;
static int16_t x_dir = 0;
static int16_t y_dir = 0;
static float x_spd = 8.5;
static float y_spd = 8.5;

void btn_left_handler(void *handler_args, esp_event_base_t base, int32_t id,
                      void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    x_dir -= 1;
  } else {
    x_dir += 1;
  }

  ESP_LOGI("LEFT", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_up_handler(void *handler_args, esp_event_base_t base, int32_t id,
                    void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    y_dir -= 1;
  } else {
    y_dir += 1;
  }

  ESP_LOGI("UP", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_down_handler(void *handler_args, esp_event_base_t base, int32_t id,
                      void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    y_dir += 1;
  } else {
    y_dir -= 1;
  }

  ESP_LOGI("DOWN", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_right_handler(void *handler_args, esp_event_base_t base, int32_t id,
                       void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    x_dir += 1;
  } else {
    x_dir -= 1;
  }

  ESP_LOGI("RIGHT", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_confirm_handler(void *handler_args, esp_event_base_t base, int32_t id,
                         void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  if (state_info->state) {
    cursor_color = CURSOR_CLICK_COLOR;
  } else {
    cursor_color = CURSOR_COLOR;
  }

  ESP_LOGI("CONFIRM", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void btn_cancel_handler(void *handler_args, esp_event_base_t base, int32_t id,
                        void *event_data) {
  button_state_info_t *state_info = (button_state_info_t *)event_data;

  ESP_LOGI("CANCEL", "button %d: %s", (int)id,
           state_info->state ? "PRESSED" : "RELEASED");
}

void ST7789(void *pvParameters) {
  // Change SPI Clock Frequency
  spi_clock_speed(40000000);  // 40MHz
  // spi_clock_speed(60000000); // 60MHz

  spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO,
                  CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);
  lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);
  lcdFillScreen(&dev, BLACK);
  lcdDrawFinish(&dev);

  int16_t x_pos = dev._width / 2;
  int16_t y_pos = dev._height / 2;

  int counter = 0;
  int64_t time_start, delta_time;
  float fps;
  while (1) {
    time_start = esp_timer_get_time();
    lcdDrawFillArrow(&dev, x_pos + 14, y_pos + 14, x_pos, y_pos, 6, BLACK);

    x_pos += x_spd * x_dir;
    y_pos += y_spd * y_dir;

    if (x_pos > dev._width) {
      x_pos = 0;
    } else if (x_pos < 0) {
      x_pos = dev._width - 1;
    }

    if (y_pos > dev._height) {
      y_pos = 0;
    } else if (y_pos < 0) {
      y_pos = dev._height - 1;
    }

    lcdDrawFillArrow(&dev, x_pos + 14, y_pos + 14, x_pos, y_pos, 6,
                     cursor_color);
    lcdDrawFinish(&dev);

    // vTaskDelay(pdMS_TO_TICKS(16));

    counter++;
    if (counter == 100) {
      counter = 0;
      delta_time = esp_timer_get_time() - time_start;
      fps = 1000000.0 / delta_time;
      ESP_LOGI("ST7789", "FPS: %.2f", fps);
    }
  }  // end while

  // never reach here
  vTaskDelete(NULL);
}

void keyboard_task(void *pvParameters) {
  button_debounce_cfg debounce_cfg = {
      .debounce_checks = DEBOUNCE_CHECKS,
      .debounce_interval_ms = DEBOUNCE_INTERVAL_MS,
  };

  button_t btn_left, btn_up, btn_down, btn_right, btn_confirm, btn_cancel;
  button_init(&btn_left, BUTTON_LEFT_GPIO, true);
  button_init(&btn_up, BUTTON_UP_GPIO, true);
  button_init(&btn_down, BUTTON_DOWN_GPIO, true);
  button_init(&btn_right, BUTTON_RIGHT_GPIO, true);
  button_init(&btn_confirm, BUTTON_CONFIRM_GPIO, true);
  button_init(&btn_cancel, BUTTON_CANCEL_GPIO, true);

  button_set_pullmode(&btn_left, GPIO_PULLUP_ONLY);
  button_set_pullmode(&btn_up, GPIO_PULLUP_ONLY);
  button_set_pullmode(&btn_down, GPIO_PULLUP_ONLY);
  button_set_pullmode(&btn_right, GPIO_PULLUP_ONLY);
  button_set_pullmode(&btn_confirm, GPIO_PULLUP_ONLY);
  button_set_pullmode(&btn_cancel, GPIO_PULLUP_ONLY);

  button_set_debounce_conf(&btn_left, debounce_cfg);
  button_set_debounce_conf(&btn_up, debounce_cfg);
  button_set_debounce_conf(&btn_down, debounce_cfg);
  button_set_debounce_conf(&btn_right, debounce_cfg);
  button_set_debounce_conf(&btn_confirm, debounce_cfg);
  button_set_debounce_conf(&btn_cancel, debounce_cfg);

  button_set_event_handler(&btn_left, btn_left_handler, NULL);
  button_set_event_handler(&btn_up, btn_up_handler, NULL);
  button_set_event_handler(&btn_down, btn_down_handler, NULL);
  button_set_event_handler(&btn_right, btn_right_handler, NULL);
  button_set_event_handler(&btn_confirm, btn_confirm_handler, NULL);
  button_set_event_handler(&btn_cancel, btn_cancel_handler, NULL);

  while (1) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // never reach here
  vTaskDelete(NULL);
}

void app_main(void) {
  ESP_LOGI(TAG, "Hello, User!!:)");
  esp_event_loop_create_default();

  xTaskCreate(ST7789, "ST7789", 1024 * 6, NULL, 3, NULL);
  xTaskCreate(keyboard_task, "keyboard_task", 1024 * 3, NULL, 2, NULL);
}