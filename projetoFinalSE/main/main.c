
#include <stdio.h>

#include "esp_err.h"
#include "servo.h"
#include "servo_config.h"
// 360 n vai 360 por causa dos fios


static Servo ombro;
static Servo cotovelo;
static Servo rotacao_punho;
static Servo flexao_punho;
static Servo garra;


void app_main(void)
{
    // Inicializa o timer compartilhado
    ESP_ERROR_CHECK(Servo_config_init());

    // Inicializa os cinco servos
    ESP_ERROR_CHECK(Servo_init(&ombro, &SERVO_OMBRO));

    ESP_ERROR_CHECK(Servo_init(&cotovelo, &SERVO_COTOVELO));

    ESP_ERROR_CHECK(
        Servo_init(&rotacao_punho, &SERVO_ROTACAO_PUNHO)
    );

    ESP_ERROR_CHECK(
        Servo_init(&flexao_punho, &SERVO_FLEXAO_PUNHO)
    );

    ESP_ERROR_CHECK(Servo_init(&garra, &SERVO_GARRA));


    // Testes iniciais
    ESP_ERROR_CHECK(Servo_setAngulo(&ombro, 90));

    ESP_ERROR_CHECK(Servo_setAngulo(&cotovelo, 90));

    ESP_ERROR_CHECK(Servo_setAngulo(&rotacao_punho, 90));

    ESP_ERROR_CHECK(Servo_setAngulo(&flexao_punho, 90));

    ESP_ERROR_CHECK(Servo_setAngulo(&garra, 90));
}



/**
 * 
 * 
 * Servo servo1;
    Servo_init(&servo1, 18, LEDC_CHANNEL_0); // pino 18, canal 0
    while(1){
        for(int angulo = 0; angulo <= 180; angulo += 10){
            Servo_setAngulo(&servo1, angulo);
            printf("Servo1 Angulo: %d\n", Servo_getAngulo(&servo1));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        for(int angulo = 180; angulo >= 0; angulo -= 10){
            Servo_setAngulo(&servo1, angulo);
            printf("Servo1 Angulo: %d\n", Servo_getAngulo(&servo1));
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
    }
 */