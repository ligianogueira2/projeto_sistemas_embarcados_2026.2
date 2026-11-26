#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "button_control.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LED_WS2812_GPIO GPIO_NUM_38
#define LED_WS2812_NUM_LEDS 1U

#define LED_BRIGHTNESS_NORM 100U
#define LED_BRIGHTNESS_EMERG 150U

esp_err_t led_control_init(void);
esp_err_t led_control_set_color(uint8_t red, uint8_t green, uint8_t blue);
esp_err_t led_control_apply_state(magnibot_state_t state);
esp_err_t led_control_set_emergency_red(void);

#ifdef __cplusplus
}
#endif