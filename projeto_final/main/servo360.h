#ifndef SERVO360_H
#define SERVO360_H

#include <stdint.h>
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Parâmetros calibrados do MG996R 360°
#define SERVO_GPIO_DEFAULT      16
#define SERVO_STOP_US           1488    // Centro da zona morta
#define SERVO_PULSE_PLUS_US     1573    // Sentido + (Direita / Anti-horário)
#define SERVO_PULSE_MINUS_US    1403    // Sentido - (Esquerda / Horário)

#define SERVO_SPEED_PLUS_DPS    36.8f   // Graus/s no sentido +
#define SERVO_SPEED_MINUS_DPS   36.5f   // Graus/s no sentido -

// Faixa de 180° (-90° a +90°). Reduza se houver limite físico de fios.
#define SERVO_MIN_ANGLE         -90.0f
#define SERVO_MAX_ANGLE         90.0f

typedef struct {
    int gpio_num;
    ledc_channel_t channel;
    ledc_timer_t timer;
    float current_angle;
} servo360_t;

esp_err_t servo360_init(servo360_t *servo);
esp_err_t servo360_move_relative(servo360_t *servo, float delta_degrees);
esp_err_t servo360_goto_angle(servo360_t *servo, float target_angle);
esp_err_t servo360_stop(servo360_t *servo);
void servo360_reset_zero(servo360_t *servo);

#ifdef __cplusplus
}
#endif

#endif // SERVO360_H