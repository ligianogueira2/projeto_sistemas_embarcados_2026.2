
#ifndef SERVO_CONFIG_H
#define SERVO_CONFIG_H

#include "esp_err.h"
#include "servo.h"

// Configuração compartilhada
#define SERVO_PWM_FREQ       50
#define SERVO_PWM_PERIOD_US  20000

#define SERVO_LEDC_TIMER     LEDC_TIMER_0
#define SERVO_LEDC_MODE      LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_RES       LEDC_TIMER_14_BIT

#define SERVO_LEDC_MAX_DUTY  (1U << 14)


// Inicialização do timer (uma única vez)
esp_err_t Servo_config_init(void);


// Configurações individuais
extern const ServoConfig SERVO_OMBRO;
extern const ServoConfig SERVO_COTOVELO;
extern const ServoConfig SERVO_ROTACAO_PUNHO;
extern const ServoConfig SERVO_FLEXAO_PUNHO;
extern const ServoConfig SERVO_GARRA;

#endif
