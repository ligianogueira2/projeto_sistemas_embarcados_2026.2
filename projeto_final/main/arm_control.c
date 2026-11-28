#include "arm_control.h"
#include "servo.h"
#include "servo_config.h"
#include "servo360.h"
#include "button_control.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ARM_CTRL";

/* Instâncias dos 6 eixos */
static servo360_t s_base;
static Servo s_ombro;
static Servo s_cotovelo;
static Servo s_rot_punho;
static Servo s_flex_punho;
static Servo s_garra;

/* Valores de referência na escala local (0 a 180) de cada ServoConfig:
 * 90 = pulso_neutro_us exato (Ombro 1916us, Cotovelo 939us, Rot 1205us, Flex 1400us, Garra 1000us) */
#define POSE_NEUTRO         90

/* Poses para calibração em bancada (ajuste conforme a cinemática física do braço) */
#define SCAN_OMBRO          70   // Braço levemente abaixado para varredura
#define SCAN_COTOVELO       110
#define SCAN_FLEX_PUNHO     90
#define GARRA_ABERTA        90   // 1000 us (meio aberta)
#define GARRA_FECHADA       150  // Ajuste até segurar o objeto sem forçar o SG90

static void set_servo_logged(const char *nome, int gpio, ledc_channel_t ch, Servo *servo, int angulo_local)
{
    Servo_setAngulo(servo, angulo_local);
    uint32_t duty = ledc_get_duty(LEDC_LOW_SPEED_MODE, ch);
    ESP_LOGD(TAG, "[SERVO] %-13s | GPIO=%2d | CH=%d | Angulo_local=%3d | LEDC_duty=%lu",
             nome, gpio, (int)ch, angulo_local, (unsigned long)duty);
}

esp_err_t arm_control_init(void)
{
    ESP_LOGI(TAG, "Inicializando 5 servos articulados (LEDC_TIMER_0, canais 0..4)...");

    ESP_RETURN_ON_ERROR(Servo_init(&s_ombro, &SERVO_OMBRO), TAG, "Erro Ombro (GPIO 17)");
    ESP_LOGD(TAG, "[INIT] Ombro         -> GPIO 17 (CH 0) OK");

    ESP_RETURN_ON_ERROR(Servo_init(&s_cotovelo, &SERVO_COTOVELO), TAG, "Erro Cotovelo (GPIO 18)");
    ESP_LOGD(TAG, "[INIT] Cotovelo      -> GPIO 18 (CH 1) OK");

    ESP_RETURN_ON_ERROR(Servo_init(&s_rot_punho, &SERVO_ROTACAO_PUNHO), TAG, "Erro Rot Punho (GPIO 8)");
    ESP_LOGD(TAG, "[INIT] Rot Punho     -> GPIO  8 (CH 2) OK");

    ESP_RETURN_ON_ERROR(Servo_init(&s_flex_punho, &SERVO_FLEXAO_PUNHO), TAG, "Erro Flex Punho (GPIO 3)");
    ESP_LOGD(TAG, "[INIT] Flex Punho    -> GPIO  3 (CH 3) OK");

    ESP_RETURN_ON_ERROR(Servo_init(&s_garra, &SERVO_GARRA), TAG, "Erro Garra (GPIO 10)");
    ESP_LOGD(TAG, "[INIT] Garra         -> GPIO 10 (CH 4) OK");

    /* Inicializa a base 360° no canal 5 (mesmo timer 0) */
    s_base.gpio_num = SERVO_GPIO_DEFAULT;
    s_base.channel  = LEDC_CHANNEL_5;
    s_base.timer    = SERVO_LEDC_TIMER;
    ESP_RETURN_ON_ERROR(servo360_init(&s_base), TAG, "Erro Base 360 (GPIO %d)", s_base.gpio_num);
    ESP_LOGD(TAG, "[INIT] Base 360      -> GPIO %2d (CH %d) OK", s_base.gpio_num, (int)s_base.channel);

    ESP_LOGI(TAG, "6 eixos inicializados com sucesso.");
    arm_move_to_idle_pose();
    return ESP_OK;
}

void arm_move_to_idle_pose(void)
{
    ESP_LOGI(TAG, "Movendo para pose IDLE (repouso calibrado)...");
    set_servo_logged("Ombro",      17, LEDC_CHANNEL_0, &s_ombro,      POSE_NEUTRO);  // 1916 us (127°)
    set_servo_logged("Cotovelo",   18, LEDC_CHANNEL_1, &s_cotovelo,   POSE_NEUTRO);  //  939 us (40°)
    set_servo_logged("Rot Punho",   8, LEDC_CHANNEL_2, &s_rot_punho,  POSE_NEUTRO);  // 1205 us (63°)
    set_servo_logged("Flex Punho",  3, LEDC_CHANNEL_3, &s_flex_punho, POSE_NEUTRO);  // 1400 us (81°)
    set_servo_logged("Garra",      10, LEDC_CHANNEL_4, &s_garra,      GARRA_ABERTA); // 1000 us (45°)
    vTaskDelay(pdMS_TO_TICKS(500));

    if (button_control_get_state() != ESTADO_EMERGENCIA) {
        ESP_LOGD(TAG, "[BASE360] Retornando base (GPIO %d) para 0.0°...", s_base.gpio_num);
        servo360_goto_angle(&s_base, 0.0f);
    }
}

void arm_move_to_scan_pose(void)
{
    ESP_LOGI(TAG, "Descendo garra para altura de varredura e indo para %.1f°...", ARM_SCAN_MIN_DEG);
    set_servo_logged("Garra",      10, LEDC_CHANNEL_4, &s_garra,      GARRA_ABERTA);
    set_servo_logged("Ombro",      17, LEDC_CHANNEL_0, &s_ombro,      SCAN_OMBRO);
    set_servo_logged("Cotovelo",   18, LEDC_CHANNEL_1, &s_cotovelo,   SCAN_COTOVELO);
    set_servo_logged("Flex Punho",  3, LEDC_CHANNEL_3, &s_flex_punho, SCAN_FLEX_PUNHO);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (button_control_get_state() != ESTADO_EMERGENCIA) {
        ESP_LOGD(TAG, "[BASE360] Posicionando base (GPIO %d) em %.1f°...", s_base.gpio_num, ARM_SCAN_MIN_DEG);
        servo360_goto_angle(&s_base, ARM_SCAN_MIN_DEG);
    }
}

esp_err_t arm_base_goto_angle(float angle_deg)
{
    ESP_LOGD(TAG, "[BASE360] GPIO %d -> alvo %.1f°", s_base.gpio_num, angle_deg);
    return servo360_goto_angle(&s_base, angle_deg);
}

void arm_pick_at_target(float angle_deg, uint16_t distance_mm)
{
    float target_angle = angle_deg + ARM_TOF_ANGLE_OFFSET_DEG;
    if (target_angle < ARM_SCAN_MIN_DEG) target_angle = ARM_SCAN_MIN_DEG;
    if (target_angle > ARM_SCAN_MAX_DEG) target_angle = ARM_SCAN_MAX_DEG;

    ESP_LOGI(TAG, "Girando base para %.1f° (alvo=%.1f°, offset=%.1f°, dist=%u mm)...",
             target_angle, angle_deg, ARM_TOF_ANGLE_OFFSET_DEG, distance_mm);
    servo360_goto_angle(&s_base, target_angle);

    if (button_control_get_state() == ESTADO_EMERGENCIA) return;

    ESP_LOGI(TAG, "Descendo garra e fechando...");
    set_servo_logged("Ombro",      17, LEDC_CHANNEL_0, &s_ombro,      SCAN_OMBRO);
    set_servo_logged("Cotovelo",   18, LEDC_CHANNEL_1, &s_cotovelo,   SCAN_COTOVELO);
    set_servo_logged("Flex Punho",  3, LEDC_CHANNEL_3, &s_flex_punho, SCAN_FLEX_PUNHO);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (button_control_get_state() == ESTADO_EMERGENCIA) return;

    set_servo_logged("Garra",      10, LEDC_CHANNEL_4, &s_garra,      GARRA_FECHADA);
    vTaskDelay(pdMS_TO_TICKS(600));

    if (button_control_get_state() == ESTADO_EMERGENCIA) return;

    ESP_LOGI(TAG, "Objeto capturado! Subindo braço...");
    set_servo_logged("Ombro",      17, LEDC_CHANNEL_0, &s_ombro,      POSE_NEUTRO);
    set_servo_logged("Cotovelo",   18, LEDC_CHANNEL_1, &s_cotovelo,   POSE_NEUTRO);
    vTaskDelay(pdMS_TO_TICKS(500));
}

void arm_emergency_stop(void)
{
    /* Para imediatamente o giro contínuo da base */
    servo360_stop(&s_base);
    ESP_LOGE(TAG, "EMERGÊNCIA: Base 360° (GPIO %d) parada em pulso neutro (1488 us).", s_base.gpio_num);
}