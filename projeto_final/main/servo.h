
#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include <stdbool.h>

#include "esp_err.h"
#include "driver/ledc.h"

typedef struct {
    const char *nome;

    uint8_t gpio;
    ledc_channel_t canal;

    uint16_t pulso_min_us;
    uint16_t pulso_max_us;
    uint16_t pulso_neutro_us;

    uint8_t angulo_min;
    uint8_t angulo_max;

    bool inverter_sentido;

} ServoConfig;


typedef struct {
    ServoConfig config;

    uint8_t angulo_atual;
    uint16_t pulso_atual_us;

    bool inicializado;

} Servo;


// Inicialização individual
esp_err_t Servo_init(Servo *servo, const ServoConfig *config);

// Movimentação
esp_err_t Servo_setAngulo(Servo *servo, uint8_t angulo);

// Consulta da referência
uint8_t Servo_getAngulo(const Servo *servo);

uint16_t Servo_getPulso(const Servo *servo);

#endif