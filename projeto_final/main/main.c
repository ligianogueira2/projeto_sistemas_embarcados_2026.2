#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "button_control.h"
#include "led_control.h"
#include "tof_control.h"
#include "arm_control.h"

static const char *TAG = "MAIN_APP";

/* Tempo de estabilização mecânica antes de disparar a leitura do VL53L0X */
#define SCAN_SETTLE_DELAY_MS    40

static volatile bool s_emergency_logged = false;
static volatile bool s_emergency_pending_print = false;
static TaskHandle_t s_op_task_handle = NULL;

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

/* =========================================================================
 * Callbacks do controle de botões
 * ========================================================================= */
static void on_state_change(magnibot_state_t prev, magnibot_state_t next)
{
    ESP_LOGI(TAG, "[ESTADO/LED] %s -> %s", state_to_str(prev), state_to_str(next));
    ESP_ERROR_CHECK(led_control_apply_state(next));

    if (next == ESTADO_1_IDLE) {
        s_emergency_logged = false;
    }
}

static void on_op_trigger(magnibot_state_t target_state, bool discard_previous)
{
    ESP_LOGI(TAG, "[OP] Disparar %s (descartar_anterior=%s)",
             state_to_str(target_state),
             discard_previous ? "true" : "false");

    if (discard_previous) {
        ESP_LOGI(TAG, "[OP] Descartando alvo anterior conforme solicitado (toque longo).");
        tof_scanner_reset_target();
    }

    if (s_op_task_handle != NULL) {
        xTaskNotify(s_op_task_handle, (uint32_t)target_state, eSetValueWithOverwrite);
    }
}

/* Executado dentro da ISR do Botão 2: apenas sinaliza a flag para a task principal */
static void on_emergency(void)
{
    s_emergency_pending_print = true;
}

/* =========================================================================
 * Task principal de operação (Varredura e Captura)
 * ========================================================================= */
static void magnibot_op_task(void *pvParameters)
{
    (void)pvParameters;
    uint32_t notified_state = 0;

    while (true) {
        if (xTaskNotifyWait(0, UINT32_MAX, &notified_state, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        magnibot_state_t op = (magnibot_state_t)notified_state;

        if (op == ESTADO_2_VARREDURA) {
            tof_scanner_reset_target();
            bool found = false;

            /* 1. Desce a garra para a altura de varredura e vai para -90° */
            arm_move_to_scan_pose();

            /* 2. Varre 180° (-90° a +90°) lendo o VL53L0X a cada passo */
            ESP_LOGI(TAG, "[VARREDURA] Iniciando busca angular (%.1f° a %.1f°, passo=%.1f°)...",
                     ARM_SCAN_MIN_DEG, ARM_SCAN_MAX_DEG, ARM_SCAN_STEP_DEG);

            uint16_t step_idx = 0;
            for (float ang = ARM_SCAN_MIN_DEG; ang <= ARM_SCAN_MAX_DEG; ang += ARM_SCAN_STEP_DEG) {
                if (button_control_get_state() == ESTADO_EMERGENCIA) {
                    ESP_LOGW(TAG, "[VARREDURA] Abortada por emergência no passo %u (%.1f°)!", step_idx, ang);
                    break;
                }

                step_idx++;
                ESP_LOGD(TAG, "[VARREDURA] Passo %u -> movendo base para %.1f°", step_idx, ang);
                arm_base_goto_angle(ang);
                vTaskDelay(pdMS_TO_TICKS(SCAN_SETTLE_DELAY_MS));

                if (tof_scanner_poll_at_angle(ang)) {
                    found = true;
                    ESP_LOGI(TAG, "[VARREDURA] Alvo confirmado no passo %u! Encerrando varredura.", step_idx);
                    break;
                }
            }

            /* 3. Retorna fisicamente para IDLE e entra no Estado 3 (aguardando confirmação) */
            if (button_control_get_state() != ESTADO_EMERGENCIA) {
                if (!found) {
                    ESP_LOGW(TAG, "[VARREDURA] Fim do curso (%u passos) sem detectar objeto na faixa.", step_idx);
                }
                arm_move_to_idle_pose();
                button_control_notify_op_finished(ESTADO_2_VARREDURA);
            }
        }
        else if (op == ESTADO_4_CAPTURA_DEPOSITO) {
            tof_target_t alvo = tof_scanner_get_target();

            if (!alvo.found) {
                ESP_LOGW(TAG, "[CAPTURA] Nenhum objeto mapeado! Retornando para IDLE.");
            } else {
                ESP_LOGI(TAG, "[CAPTURA] Executando captura em %.1f° (distancia=%u mm)...",
                         alvo.angle_deg, alvo.distance_mm);
                arm_pick_at_target(alvo.angle_deg, alvo.distance_mm);
            }

            if (button_control_get_state() != ESTADO_EMERGENCIA) {
                arm_move_to_idle_pose();
                tof_scanner_reset_target();
                button_control_notify_op_finished(ESTADO_4_CAPTURA_DEPOSITO);
            }
        }
    }
}

void app_main(void)
{
    /* 0. Ativa logs detalhados de depuração */
    esp_log_level_set("TOF_SCAN", ESP_LOG_DEBUG);
    esp_log_level_set("ARM_CTRL", ESP_LOG_DEBUG);
    esp_log_level_set("SERVO360", ESP_LOG_DEBUG);
    esp_log_level_set("BTN_CTRL", ESP_LOG_DEBUG);
    esp_log_level_set("MAIN_APP", ESP_LOG_DEBUG);

    ESP_LOGI(TAG, "==========================================================");
    ESP_LOGI(TAG, " MagniBot ESP32-S3 | Boot e Diagnostico de Hardware");
    ESP_LOGI(TAG, " Pinos: Base360=16 | Ombro=17 | Cotov=18 | Rot=8 | Flex=3 | Garra=10");
    ESP_LOGI(TAG, "        I2C(VL53L0X): SDA=12, SCL=13 | LED=38 | BTN1=1 | BTN2=2");
    ESP_LOGI(TAG, "==========================================================");

    /* 1. Inicializa o LED RGB onboard (GPIO 38) */
    ESP_ERROR_CHECK(led_control_init());

    /* 2. Inicializa o sensor VL53L0X (SDA=12, SCL=13) sem abortar em caso de mau contato */
    esp_err_t tof_err = tof_scanner_init();
    if (tof_err != ESP_OK) {
        ESP_LOGW(TAG, "VL53L0X indisponivel no boot (%s). Servos e botoes seguem ativos; sensor tentara reconectar na varredura.",
                 esp_err_to_name(tof_err));
    }

    /* Descomente abaixo para restringir a detecção entre 40 mm e 250 mm na bancada: */
    // tof_scanner_set_range_mm(40, 250);

    /* 3. Inicializa os 6 servomotores e posiciona o braço em IDLE */
    ESP_ERROR_CHECK(arm_control_init());

    /* 4. Cria a task de operação dos movimentos */
    xTaskCreate(magnibot_op_task, "magnibot_op", 4096, NULL, 5, &s_op_task_handle);

    button_control_callbacks_t cbs = {
        .on_state_change = on_state_change,
        .on_op_trigger   = on_op_trigger,
        .on_emergency    = on_emergency,
    };

    /* 5. Inicializa os botões (BTN1=GPIO1, BTN2=GPIO2) */
    ESP_ERROR_CHECK(button_control_init(&cbs));

    ESP_LOGI(TAG, "Sistema pronto em ESTADO_1_IDLE (VL53L0X=%s).",
             (tof_err == ESP_OK) ? "OK" : "FALHA/MAU CONTATO");

    while (true) {
        if (s_emergency_pending_print && !s_emergency_logged) {
            s_emergency_pending_print = false;
            s_emergency_logged = true;
            arm_emergency_stop();
            ESP_LOGE(TAG, "[EMERGÊNCIA] Base parada e sistema bloqueado!");
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}