#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "button_control.h"
#include "led_control.h"

static const char *TAG = "MAIN_SIM";

static volatile bool s_emergency_logged = false;
static volatile bool s_emergency_pending_print = false;
static TimerHandle_t s_sim_op_timer = NULL;
static volatile magnibot_state_t s_sim_target_state = ESTADO_1_IDLE;

static const char *state_to_str(magnibot_state_t st)
{
    switch (st) {
        case ESTADO_1_IDLE:                 return "ESTADO_1_IDLE";
        case ESTADO_2_VARREDURA:            return "ESTADO_2_VARREDURA";
        case ESTADO_3_MAPEAMENTO_CONCLUIDO: return "ESTADO_3_MAPEAMENTO_CONCLUIDO";
        case ESTADO_4_CAPTURA_DEPOSITO:     return "ESTADO_4_CAPTURA_DEPOSITO";
        case ESTADO_EMERGENCIA:             return "ESTADO_EMERGENCIA";
        default:                            return "ESTADO_DESCONHECIDO";
    }
}

static void sim_op_finish_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    magnibot_state_t finished_st = s_sim_target_state;
    ESP_LOGI(TAG, "[STUB OP] Simulação concluída para %s", state_to_str(finished_st));
    button_control_notify_op_finished(finished_st);
}

/* Callback de mudança de estado: atualiza log e cor do LED WS2812 */
static void on_state_change(magnibot_state_t prev, magnibot_state_t next)
{
    ESP_LOGI(TAG, "[ESTADO/LED] %s -> %s", state_to_str(prev), state_to_str(next));
    ESP_ERROR_CHECK(led_control_apply_state(next));
}

static void stub_on_op_trigger(magnibot_state_t target_state, bool discard_previous)
{
    ESP_LOGI(TAG, "[STUB OP] Disparar %s (descartar_anterior=%s)",
             state_to_str(target_state),
             discard_previous ? "true" : "false");

    s_sim_target_state = target_state;
    if (s_sim_op_timer != NULL) {
        xTimerStop(s_sim_op_timer, 0);
        xTimerChangePeriod(s_sim_op_timer, pdMS_TO_TICKS(1500), 0);
        xTimerStart(s_sim_op_timer, 0);
    }
}

/* Chamado diretamente da ISR do Botão 2: apenas seta flag (sem I/O ou log aqui) */
static void stub_on_emergency(void)
{
    s_emergency_pending_print = true;
}

void app_main(void)
{
    /* 1. Inicializa o LED antes dos botões para já ter cor no estado inicial */
    ESP_ERROR_CHECK(led_control_init());

    s_sim_op_timer = xTimerCreate("sim_op_tmr",
                                  pdMS_TO_TICKS(1500),
                                  pdFALSE,
                                  NULL,
                                  sim_op_finish_timer_cb);

    button_control_callbacks_t cbs = {
        .on_state_change = on_state_change,
        .on_op_trigger   = stub_on_op_trigger,
        .on_emergency    = stub_on_emergency,
    };

    /* 2. Inicializa os botões (já dispara on_state_change para o Estado 1 = verde) */
    ESP_ERROR_CHECK(button_control_init(&cbs));

    while (true) {
        if (s_emergency_pending_print && !s_emergency_logged) {
            s_emergency_pending_print = false;
            s_emergency_logged = true;
            ESP_LOGE(TAG, "[STUB EMERGÊNCIA] Parar PWM e levar servos para posição segura!");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}