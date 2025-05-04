#ifndef KB_H_
#define KB_H_

#include "button.h"
#include "sdkconfig.h"

#define BUTTON_LEFT_GPIO CONFIG_BUTTON_LEFT_GPIO
#define BUTTON_DOWN_GPIO CONFIG_BUTTON_DOWN_GPIO
#define BUTTON_UP_GPIO CONFIG_BUTTON_UP_GPIO
#define BUTTON_RIGHT_GPIO CONFIG_BUTTON_RIGHT_GPIO
#define BUTTON_CONFIRM_GPIO CONFIG_BUTTON_CONFIRM_GPIO
#define BUTTON_CANCEL_GPIO CONFIG_BUTTON_CANCEL_GPIO

#define DEBOUNCE_CHECKS 5
#define DEBOUNCE_INTERVAL_MS 12

void btn_left_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
void btn_up_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
void btn_down_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
void btn_right_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
void btn_confirm_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
void btn_cancel_handler(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);

void keyboard_task(void *pvParameters);

#endif