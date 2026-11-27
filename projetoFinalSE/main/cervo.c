#include "servo.h"
#include "esp_err.h"

#define SERVO_FREQ       50
#define SERVO_PERIOD     20000

#define SERVO_MIN_US     500
#define SERVO_MAX_US     2400

#define LEDC_TIMER       LEDC_TIMER_0
#define LEDC_MODE        LEDC_LOW_SPEED_MODE
#define LEDC_RESOLUTION  LEDC_TIMER_14_BIT
#define LEDC_MAX_DUTY    16384 // 2^14

void Servo_init(Servo *servo, uint8_t pino, ledc_channel_t canal) {
    servo->pino = pino;
    servo->angulo = 0;
    servo->canal = canal;

    // Configuração do timer PWM (pode ser chamado mais de uma vez sem problemas)
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz         = SERVO_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    // Configuração do canal PWM individual do servo
    ledc_channel_config_t channel = {
        .gpio_num   = servo->pino,
        .speed_mode = LEDC_MODE,
        .channel    = servo->canal,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void Servo_setAngulo(Servo *servo, uint8_t angulo) {
    if (angulo > 180) angulo = 180;
    servo->angulo = angulo;

    // Calcula a largura de pulso em microssegundos
    int pulso = SERVO_MIN_US + (angulo * (SERVO_MAX_US - SERVO_MIN_US)) / 180;

    // Converte microssegundos para duty cycle de 14 bits
    uint32_t duty = (uint32_t)(((uint64_t)pulso * LEDC_MAX_DUTY) / SERVO_PERIOD);

    // Aplica e ATUALIZA o hardware
    ledc_set_duty(LEDC_MODE, servo->canal, duty);
    ledc_update_duty(LEDC_MODE, servo->canal);
}

uint8_t Servo_getAngulo(Servo *servo) {
    return servo->angulo;
}