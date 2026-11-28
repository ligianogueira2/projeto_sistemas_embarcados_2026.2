#include "servo360.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "SERVO360";

#define SERVO_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_DUTY_RES  LEDC_TIMER_14_BIT // 0 a 16383
#define SERVO_LEDC_FREQ_HZ   50                // 50Hz (período de 20.000 us)

static esp_err_t set_pulse_us(servo360_t *servo, uint32_t pulse_us) {
    uint32_t max_duty = (1 << SERVO_LEDC_DUTY_RES);
    uint32_t duty = (pulse_us * max_duty) / 20000;

    esp_err_t err = ledc_set_duty(SERVO_LEDC_MODE, servo->channel, duty);
    if (err != ESP_OK) return err;

    return ledc_update_duty(SERVO_LEDC_MODE, servo->channel);
}

esp_err_t servo360_init(servo360_t *servo) {
    if (servo == NULL) return ESP_ERR_INVALID_ARG;

    servo->current_angle = 0.0f; // Assume que o servo inicia na posição 0°

    ledc_timer_config_t ledc_timer = {
        .speed_mode       = SERVO_LEDC_MODE,
        .duty_resolution  = SERVO_LEDC_DUTY_RES,
        .timer_num        = servo->timer,
        .freq_hz          = SERVO_LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) return err;

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = SERVO_LEDC_MODE,
        .channel        = servo->channel,
        .timer_sel      = servo->timer,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = servo->gpio_num,
        .duty           = 0,
        .hpoint         = 0
    };
    err = ledc_channel_config(&ledc_channel);
    if (err != ESP_OK) return err;

    return servo360_stop(servo);
}

esp_err_t servo360_stop(servo360_t *servo) {
    if (servo == NULL) return ESP_ERR_INVALID_ARG;
    return set_pulse_us(servo, SERVO_STOP_US);
}

void servo360_reset_zero(servo360_t *servo) {
    if (servo != NULL) {
        servo->current_angle = 0.0f;
    }
}

esp_err_t servo360_move_relative(servo360_t *servo, float delta_degrees) {
    if (servo == NULL) return ESP_ERR_INVALID_ARG;
    if (fabsf(delta_degrees) < 0.5f) return ESP_OK; // Ignora movimentos desprezíveis

    // Calcula o ângulo final desejado e trava nos limites físicos (-45° a +90°)
    float target = servo->current_angle + delta_degrees;
    if (target > SERVO_MAX_ANGLE) {
        ESP_LOGW(TAG, "Limite maximo (+90) atingido! Ajustando movimento.");
        delta_degrees = SERVO_MAX_ANGLE - servo->current_angle;
        target = SERVO_MAX_ANGLE;
    } else if (target < SERVO_MIN_ANGLE) {
        ESP_LOGW(TAG, "Limite minimo (-45) atingido! Ajustando movimento.");
        delta_degrees = SERVO_MIN_ANGLE - servo->current_angle;
        target = SERVO_MIN_ANGLE;
    }

    if (fabsf(delta_degrees) < 0.5f) return ESP_OK;

    uint32_t pulse_us;
    uint32_t duration_ms;

    if (delta_degrees > 0) {
        // Sentido + (Direita / Anti-horário) -> 1573 us a 36.8 graus/s
        pulse_us = SERVO_PULSE_PLUS_US;
        duration_ms = (uint32_t)((delta_degrees / SERVO_SPEED_PLUS_DPS) * 1000.0f);
    } else {
        // Sentido - (Esquerda / Horário) -> 1403 us a 36.5 graus/s
        pulse_us = SERVO_PULSE_MINUS_US;
        duration_ms = (uint32_t)((fabsf(delta_degrees) / SERVO_SPEED_MINUS_DPS) * 1000.0f);
    }

    ESP_LOGI(TAG, "Girando %.1f graus (%lu ms) | Pulso: %lu us", delta_degrees, duration_ms, pulse_us);

    esp_err_t err = set_pulse_us(servo, pulse_us);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    err = servo360_stop(servo);
    servo->current_angle = target;

    return err;
}

esp_err_t servo360_goto_angle(servo360_t *servo, float target_angle) {
    if (servo == NULL) return ESP_ERR_INVALID_ARG;

    if (target_angle > SERVO_MAX_ANGLE) target_angle = SERVO_MAX_ANGLE;
    if (target_angle < SERVO_MIN_ANGLE) target_angle = SERVO_MIN_ANGLE;

    float delta = target_angle - servo->current_angle;
    return servo360_move_relative(servo, delta);
}