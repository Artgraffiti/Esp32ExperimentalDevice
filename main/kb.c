#include "kb.h"

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
    // Delay for watchdog
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // never reach here
  vTaskDelete(NULL);
}