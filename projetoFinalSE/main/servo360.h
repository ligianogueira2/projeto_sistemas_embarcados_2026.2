#ifndef SERVO360_H
#define SERVO360_H

#include <stdint.h>
#include "driver/ledc.h"
#include "esp_err.h"


// Parâmetros calibrados do seu servo
#define SERVO_GPIO_DEFAULT      15
#define SERVO_STOP_US           1488    // Centro da zona morta
#define SERVO_PULSE_PLUS_US     1573    // 1488 + 85 us (Sentido + / Direita / Anti-horário)
#define SERVO_PULSE_MINUS_US    1403    // 1488 - 85 us (Sentido - / Esquerda / Horário)

#define SERVO_SPEED_PLUS_DPS    36.8f   // Graus por segundo no sentido +
#define SERVO_SPEED_MINUS_DPS   36.5f   // Graus por segundo no sentido -

#define SERVO_MIN_ANGLE         -45.0f  // Limite mínimo de segurança
#define SERVO_MAX_ANGLE         90.0f   // Limite máximo de segurança

typedef struct {
    int gpio_num;               /*!< Pino GPIO (Padrão: 15) */
    ledc_channel_t channel;     /*!< Canal LEDC */
    ledc_timer_t timer;         /*!< Timer LEDC */
    float current_angle;        /*!< Posição angular atual estimada (-45 a +90) */
} servo360_t;

/**
 * @brief Inicializa o PWM e define a posição inicial como 0 graus
 */
esp_err_t servo360_init(servo360_t *servo);

/**
 * @brief Move o servo relativamente por um delta de graus (bloqueante durante o movimento)
 *        Respeita automaticamente os limites de -45° a +90°.
 * 
 * @param servo Ponteiro para a estrutura do servo
 * @param delta_degrees Quantidade de graus a girar (+ para direita, - para esquerda)
 * @return esp_err_t ESP_OK ou ESP_ERR_INVALID_ARG se atingir o limite
 */
esp_err_t servo360_move_relative(servo360_t *servo, float delta_degrees);

/**
 * @brief Move o servo para um ângulo absoluto alvo entre -45° e +90°
 * 
 * @param servo Ponteiro para a estrutura do servo
 * @param target_angle Ângulo desejado (-45.0 a 90.0)
 * @return esp_err_t ESP_OK
 */
esp_err_t servo360_goto_angle(servo360_t *servo, float target_angle);

/**
 * @brief Para imediatamente o servo aplicando o pulso neutro (1488 us)
 */
esp_err_t servo360_stop(servo360_t *servo);

/**
 * @brief Redefine manualmente o zero lógico (caso você ajuste a posição física)
 */
void servo360_reset_zero(servo360_t *servo);

#ifdef __cplusplus
}
#endif

#endif // SERVO360_H