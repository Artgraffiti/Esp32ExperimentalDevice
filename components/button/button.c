#include "button.h"

#include <stdbool.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "hal/gpio_types.h"

ESP_EVENT_DEFINE_BASE(BUTTON_EVENTS);

static bool isr_service_installed = false;

bool _button_get_raw_state(button_t *btn) {
  bool ret = gpio_get_level(btn->pin);
  return ret ^ btn->_active_low;
}

void _button_update_state(button_t *btn, bool state) {
  if (btn->_state_info.state == state) return;

  btn->_state_info.state = state;
  btn->_state_info.timestamp = esp_timer_get_time();

  esp_event_post(BUTTON_EVENTS, btn->pin, &(btn->_state_info),
                 sizeof(button_state_info_t), 0);
}

void _debounce_timer_callback(void *arg) {
  button_t *btn = (button_t *)arg;

  bool current_raw_state = _button_get_raw_state(btn);

  if (current_raw_state == btn->_raw_state) {
    btn->_debounce_counter++;

    // if stable state
    if (btn->_debounce_counter >= btn->debounce_cfg.debounce_checks) {
      _button_update_state(btn, btn->_raw_state);
      esp_timer_stop(btn->_debounce_timer);
    }
  } else {
    btn->_debounce_counter = 0;
    btn->_raw_state = current_raw_state;
  }
}

void IRAM_ATTR _button_isr_handler(void *args) {
  button_t *btn = (button_t *)args;

  btn->_raw_state = gpio_get_level(btn->pin);

  esp_timer_start_periodic(btn->_debounce_timer,
                           btn->debounce_cfg.debounce_interval_ms * 1000);
}

void _button_init_debounce_timer(button_t *btn) {
  const esp_timer_create_args_t timer_args = {
      .callback = &_debounce_timer_callback,
      .arg = btn,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "debounce_timer"};

  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &btn->_debounce_timer));
}

esp_err_t button_init(button_t *btn, gpio_num_t pin, bool active_low) {
  esp_err_t ret = ESP_OK;

  gpio_config_t io_conf = {.pin_bit_mask = (1ULL << pin),
                           .mode = GPIO_MODE_INPUT,
                           .pull_up_en = GPIO_PULLUP_DISABLE,
                           .pull_down_en = GPIO_PULLDOWN_DISABLE,
                           .intr_type = GPIO_INTR_ANYEDGE};
  ret = gpio_config(&io_conf);
  if (ret != ESP_OK) {
    return ret;
  }

  btn->pin = pin;
  btn->_active_low = active_low;
  btn->_raw_state = false;
  btn->_state_info.state = false;
  btn->_state_info.timestamp = esp_timer_get_time();
  btn->_debounce_counter = 0;
  btn->debounce_cfg.debounce_checks = 5;
  btn->debounce_cfg.debounce_interval_ms = 10;

  _button_init_debounce_timer(btn);

  if (!isr_service_installed) {
    ret = gpio_install_isr_service(0);
    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE) {
      isr_service_installed = true;
    }
  }
  ESP_ERROR_CHECK(gpio_isr_handler_add(btn->pin, _button_isr_handler, btn));
  return ret;
}

esp_err_t button_set_pullmode(button_t *btn, gpio_pull_mode_t pull) {
  return gpio_set_pull_mode(btn->pin, pull);
}

esp_err_t button_set_debounce_conf(button_t *btn, button_debounce_cfg cfg) {
  esp_err_t ret = ESP_OK;

  btn->debounce_cfg.debounce_checks = cfg.debounce_checks;
  btn->debounce_cfg.debounce_interval_ms = cfg.debounce_interval_ms;

  return ret;
}

esp_err_t button_set_event_handler(button_t *btn, esp_event_handler_t handler,
                                   void *event_handler_arg) {
  esp_err_t ret = ESP_OK;

  ret = esp_event_handler_register(BUTTON_EVENTS, btn->pin, handler,
                                   event_handler_arg);
  return ret;
}

button_state_info_t button_get_state_info(button_t *btn) {
  return btn->_state_info;
}
