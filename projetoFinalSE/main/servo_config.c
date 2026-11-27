
#include "servo_config.h"

#include "esp_check.h"


// =====================================================
// CONFIGURAÇÕES INDIVIDUAIS
// =====================================================

// Ombro - GPIO 16
const ServoConfig SERVO_OMBRO = {
    .nome = "Ombro",
    .gpio = 16,
    .canal = LEDC_CHANNEL_0,

    .pulso_min_us = 1666,
    .pulso_max_us = 2166,
    .pulso_neutro_us = 1916,

    .angulo_min = 0,
    .angulo_max = 180,

    .inverter_sentido = false
};


// Cotovelo - GPIO 17
const ServoConfig SERVO_COTOVELO = {
    .nome = "Cotovelo",
    .gpio = 17,
    .canal = LEDC_CHANNEL_1,

    .pulso_min_us = 500,
    .pulso_max_us = 1378,
    .pulso_neutro_us = 939,

    .angulo_min = 0,
    .angulo_max = 180,

    .inverter_sentido = true
};


// Rotação do punho - GPIO 18
const ServoConfig SERVO_ROTACAO_PUNHO = {
    .nome = "Rotacao do punho",
    .gpio = 18,
    .canal = LEDC_CHANNEL_2,

    .pulso_min_us = 666,
    .pulso_max_us = 1744,
    .pulso_neutro_us = 1205,

    .angulo_min = 0,
    .angulo_max = 180,

    .inverter_sentido = false
};


// Flexão do punho - GPIO 8
const ServoConfig SERVO_FLEXAO_PUNHO = {
    .nome = "Flexao do punho",
    .gpio = 8,
    .canal = LEDC_CHANNEL_3,

    .pulso_min_us = 700,
    .pulso_max_us = 2100,
    .pulso_neutro_us = 1400,

    .angulo_min = 0,
    .angulo_max = 180,

    .inverter_sentido = false
};


// Garra - GPIO 9
const ServoConfig SERVO_GARRA = {
    .nome = "Garra",
    .gpio = 9,
    .canal = LEDC_CHANNEL_4,

    .pulso_min_us = 666,
    .pulso_max_us = 1333,
    .pulso_neutro_us = 1000,

    .angulo_min = 0,
    .angulo_max = 180,

    .inverter_sentido = false
};


// =====================================================
// INICIALIZAÇÃO DO TIMER LEDC
// =====================================================

esp_err_t Servo_config_init(void)
{
    static bool timer_configurado = false;

    if (timer_configurado) {
        return ESP_OK;
    }

    ledc_timer_config_t timer = {
        .speed_mode = SERVO_LEDC_MODE,
        .timer_num = SERVO_LEDC_TIMER,
        .duty_resolution = SERVO_LEDC_RES,
        .freq_hz = SERVO_PWM_FREQ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    esp_err_t erro = ledc_timer_config(&timer);

    if (erro != ESP_OK) {
        return erro;
    }

    timer_configurado = true;

    return ESP_OK;
}
