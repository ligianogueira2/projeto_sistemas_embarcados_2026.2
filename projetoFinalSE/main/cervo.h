#ifndef SERVO_H
#define SERVO_H

#include <stdint.h>
#include "driver/ledc.h"
// Instância do servo motor
typedef struct{
    uint8_t pino;
    uint8_t angulo;
    ledc_channel_t canal; // Cada servo precisa de um canal exclusivo
} Servo;

// ----Preparar o servo para uso----
void Servo_init(Servo *servo, uint8_t pino, ledc_channel_t canal);

// ----Funções de operação do servo motor----
void Servo_setAngulo(Servo *servo, uint8_t angulo);
uint8_t Servo_getAngulo(Servo *servo);

#endif // SERVO_H