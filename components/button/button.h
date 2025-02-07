#ifndef BUTTON_H_
#define BUTTON_H_

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "soc/gpio_num.h"

typedef struct {
  bool state;
  int64_t timestamp;
} button_state_info_t;

typedef struct {
  uint8_t debounce_checks;
  uint8_t debounce_interval_ms;
} button_debounce_cfg;

typedef struct {
  gpio_num_t pin;
  bool _active_low;

  bool _raw_state;
  button_state_info_t _state_info;

  uint8_t _debounce_counter;
  esp_timer_handle_t _debounce_timer;
  button_debounce_cfg debounce_cfg;
} button_t;

esp_err_t button_init(button_t *btn, gpio_num_t pin, bool active_low);
esp_err_t button_set_pullmode(button_t *btn, gpio_pull_mode_t pull);
esp_err_t button_set_debounce_conf(button_t *btn, button_debounce_cfg cfg);
esp_err_t button_set_event_handler(button_t *btn, esp_event_handler_t handler,
                                   void *event_handler_arg);
button_state_info_t button_get_state_info(button_t *btn);

#endif