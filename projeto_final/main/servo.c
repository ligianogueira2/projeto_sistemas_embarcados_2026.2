
#include "servo.h"
#include "servo_config.h"

#include <stddef.h>

#include "esp_check.h"


// =====================================================
// CONVERSÃO DE MICROSEGUNDOS PARA DUTY
// =====================================================

static uint32_t Servo_usParaDuty(uint16_t pulso_us)
{
    return (uint32_t)(
        ((uint64_t)pulso_us * SERVO_LEDC_MAX_DUTY) /
        SERVO_PWM_PERIOD_US
    );
}


// =====================================================
// INICIALIZAÇÃO INDIVIDUAL
// =====================================================

esp_err_t Servo_init(Servo *servo, const ServoConfig *config)
{
    ESP_RETURN_ON_FALSE(
        servo != NULL,
        ESP_ERR_INVALID_ARG,
        "SERVO",
        "Ponteiro do servo invalido"
    );

    ESP_RETURN_ON_FALSE(
        config != NULL,
        ESP_ERR_INVALID_ARG,
        "SERVO",
        "Configuracao invalida"
    );

    ESP_RETURN_ON_FALSE(
        config->pulso_min_us < config->pulso_max_us &&
        config->pulso_max_us < SERVO_PWM_PERIOD_US,
        ESP_ERR_INVALID_ARG,
        "SERVO",
        "Limites de pulso invalidos"
    );

    ESP_RETURN_ON_FALSE(
        config->angulo_min < config->angulo_max &&
        config->angulo_max <= 180,
        ESP_ERR_INVALID_ARG,
        "SERVO",
        "Limites angulares invalidos"
    );

    ESP_RETURN_ON_FALSE(
        config->pulso_neutro_us >= config->pulso_min_us &&
        config->pulso_neutro_us <= config->pulso_max_us,
        ESP_ERR_INVALID_ARG,
        "SERVO",
        "Pulso neutro invalido"
    );

    // Inicializa o timer compartilhado
    ESP_RETURN_ON_ERROR(
        Servo_config_init(),
        "SERVO",
        "Falha ao configurar timer LEDC"
    );

    servo->config = *config;

    servo->angulo_atual = 90;
    servo->pulso_atual_us = config->pulso_neutro_us;
    servo->inicializado = false;


    // Configura canal individual
    ledc_channel_config_t canal = {
        .gpio_num = config->gpio,
        .speed_mode = SERVO_LEDC_MODE,
        .channel = config->canal,
        .timer_sel = SERVO_LEDC_TIMER,
        .duty = Servo_usParaDuty(config->pulso_neutro_us),
        .hpoint = 0
    };

    esp_err_t erro = ledc_channel_config(&canal);

    if (erro != ESP_OK) {
        return erro;
    }

    servo->inicializado = true;

    return ESP_OK;
}


// =====================================================
// MOVIMENTAÇÃO ANGULAR
// =====================================================

esp_err_t Servo_setAngulo(Servo *servo, uint8_t angulo)
{
    ESP_RETURN_ON_FALSE(
        servo != NULL && servo->inicializado,
        ESP_ERR_INVALID_STATE,
        "SERVO",
        "Servo nao inicializado"
    );


    // Restringe o ângulo aos limites configurados
    if (angulo < servo->config.angulo_min) {
        angulo = servo->config.angulo_min;
    }

    if (angulo > servo->config.angulo_max) {
        angulo = servo->config.angulo_max;
    }


    // Calcula a posição relativa
    uint32_t deslocamento_angular =
        angulo - servo->config.angulo_min;

    uint32_t faixa_angular =
        servo->config.angulo_max - servo->config.angulo_min;

    uint32_t faixa_pulso =
        servo->config.pulso_max_us -
        servo->config.pulso_min_us;


    // Converte ângulo em pulso
    uint32_t deslocamento_pulso =
        (deslocamento_angular * faixa_pulso) /
        faixa_angular;

    uint32_t pulso;

    if (servo->config.inverter_sentido) {

        pulso =
            servo->config.pulso_max_us -
            deslocamento_pulso;

    } else {

        pulso =
            servo->config.pulso_min_us +
            deslocamento_pulso;
    }


    // Converte pulso para Duty Cycle
    uint32_t duty = Servo_usParaDuty((uint16_t)pulso);


    // Atualiza hardware
    esp_err_t erro = ledc_set_duty(
        SERVO_LEDC_MODE,
        servo->config.canal,
        duty
    );

    if (erro != ESP_OK) {
        return erro;
    }

    erro = ledc_update_duty(
        SERVO_LEDC_MODE,
        servo->config.canal
    );

    if (erro != ESP_OK) {
        return erro;
    }


    // Atualiza referências internas
    servo->angulo_atual = angulo;
    servo->pulso_atual_us = (uint16_t)pulso;

    return ESP_OK;
}


// =====================================================
// CONSULTAS
// =====================================================

uint8_t Servo_getAngulo(const Servo *servo)
{
    if (servo == NULL || !servo->inicializado) {
        return 0;
    }

    return servo->angulo_atual;
}


uint16_t Servo_getPulso(const Servo *servo)
{
    if (servo == NULL || !servo->inicializado) {
        return 0;
    }

    return servo->pulso_atual_us;
}