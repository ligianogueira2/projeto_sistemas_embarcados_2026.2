#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "button_control.h"
#include "led_control.h"
#include "tof_control.h"

static const char *TAG = "MAIN_TEST";

/* Parâmetros da varredura e calibração do sensor na ponta da garra */
#define SCAN_ANGLE_MIN_DEG      0.0f
#define SCAN_ANGLE_MAX_DEG      180.0f
#define SCAN_STEP_DEG           2.0f
#define SCAN_STEP_DELAY_MS      50
#define TOF_ANGLE_OFFSET_DEG    0.0f  // Ajuste fino entre a lente do VL53L0X e o centro da garra

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
 * Stubs de movimentação física (substituir pelas chamadas reais dos servos)
 * ========================================================================= */
static void arm_move_to_idle_pose(void)
{
    ESP_LOGI(TAG, "[BRAÇO] Indo para posição IDLE (braço recolhido / garra aberta).");
    // Futuro: servo_claw_open(); servo_arm_up(); servo_base_set_angle(90.0f);
    vTaskDelay(pdMS_TO_TICKS(500));
}

static void arm_move_to_scan_pose(void)
{
    ESP_LOGI(TAG, "[BRAÇO] Descendo garra para altura de varredura e indo para %.1f°.", SCAN_ANGLE_MIN_DEG);
    // Futuro: servo_base_set_angle(SCAN_ANGLE_MIN_DEG); servo_arm_down_scan();
    vTaskDelay(pdMS_TO_TICKS(500));
}

static void arm_base_set_angle(float angle_deg)
{
    (void)angle_deg;
    // Futuro: servo_base_set_angle(angle_deg);
}

static void arm_pick_at_target(float angle_deg, uint16_t distance_mm)
{
    float target_angle = angle_deg + TOF_ANGLE_OFFSET_DEG;
    if (target_angle < SCAN_ANGLE_MIN_DEG) target_angle = SCAN_ANGLE_MIN_DEG;
    if (target_angle > SCAN_ANGLE_MAX_DEG) target_angle = SCAN_ANGLE_MAX_DEG;

    ESP_LOGI(TAG, "[BRAÇO] Girando base para %.1f° (alvo=%.1f°, offset=%.1f°, dist=%u mm)...",
             target_angle, angle_deg, TOF_ANGLE_OFFSET_DEG, distance_mm);
    arm_base_set_angle(target_angle);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (button_control_get_state() == ESTADO_EMERGENCIA) return;

    ESP_LOGI(TAG, "[BRAÇO] Descendo garra e fechando para capturar objeto...");
    // Futuro: servo_arm_reach(distance_mm); servo_claw_close();
    vTaskDelay(pdMS_TO_TICKS(800));

    if (button_control_get_state() == ESTADO_EMERGENCIA) return;

    ESP_LOGI(TAG, "[BRAÇO] Objeto capturado! Subindo braço...");
    // Futuro: servo_arm_up();
    vTaskDelay(pdMS_TO_TICKS(500));
}

/* =========================================================================
 * Callbacks do controle de botões
 * ========================================================================= */
static void on_state_change(magnibot_state_t prev, magnibot_state_t next)
{
    ESP_LOGI(TAG, "[ESTADO/LED] %s -> %s", state_to_str(prev), state_to_str(next));
    ESP_ERROR_CHECK(led_control_apply_state(next));
}

static void on_op_trigger(magnibot_state_t target_state, bool discard_previous)
{
    ESP_LOGI(TAG, "[OP] Disparar %s (descartar_anterior=%s)",
             state_to_str(target_state),
             discard_previous ? "true" : "false");

    if (discard_previous) 
	{
        ESP_LOGI(TAG, "[OP] Descartando alvo anterior conforme solicitado (toque longo).");
        tof_scanner_reset_target();
    }

    if (s_op_task_handle != NULL) 
	{
        xTaskNotify(s_op_task_handle, (uint32_t)target_state, eSetValueWithOverwrite);
    }
}

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

    while (true) 
	{
        if (xTaskNotifyWait(0, UINT32_MAX, &notified_state, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        magnibot_state_t op = (magnibot_state_t)notified_state;

        if (op == ESTADO_2_VARREDURA) 
		{
            tof_scanner_reset_target();
            bool found = false;

            /* 1. Desce a garra para a posição de varredura */
            arm_move_to_scan_pose();

            /* 2. Varre de 0° a 180° lendo o VL53L0X */
            ESP_LOGI(TAG, "[VARREDURA] Iniciando busca angular (%.1f° a %.1f°)...",
                     SCAN_ANGLE_MIN_DEG, SCAN_ANGLE_MAX_DEG);

            for (float ang = SCAN_ANGLE_MIN_DEG; ang <= SCAN_ANGLE_MAX_DEG; ang += SCAN_STEP_DEG) 
			{
                if (button_control_get_state() == ESTADO_EMERGENCIA) 
				{
                    ESP_LOGW(TAG, "[VARREDURA] Abortada por emergência!");
                    break;
                }

                arm_base_set_angle(ang);
                vTaskDelay(pdMS_TO_TICKS(SCAN_STEP_DELAY_MS));

                if (tof_scanner_poll_at_angle(ang)) 
				{
                    found = true;
                    break;
                }
            }

            /* 3. Retorna fisicamente para IDLE e entra no Estado 3 (aguardando confirmação) */
            if (button_control_get_state() != ESTADO_EMERGENCIA) 
			{
                if (!found) 
				{
                    ESP_LOGW(TAG, "[VARREDURA] Fim do curso sem detectar objeto na faixa.");
                }
                arm_move_to_idle_pose();
                button_control_notify_op_finished(ESTADO_2_VARREDURA);
            }
        }
        else if (op == ESTADO_4_CAPTURA_DEPOSITO)
		{
            tof_target_t alvo = tof_scanner_get_target();

            if (!alvo.found) 
			{
                ESP_LOGW(TAG, "[CAPTURA] Nenhum objeto mapeado! Retornando para IDLE.");
            } else 
			{
                arm_pick_at_target(alvo.angle_deg, alvo.distance_mm);
            }

            if (button_control_get_state() != ESTADO_EMERGENCIA) 
			{
                arm_move_to_idle_pose();
                tof_scanner_reset_target();
                button_control_notify_op_finished(ESTADO_4_CAPTURA_DEPOSITO);
            }
        }
    }
}

void app_main(void)
{
    /* 1. Inicializa o LED */
    ESP_ERROR_CHECK(led_control_init());

    /* 2. Inicializa o sensor VL53L0X */
    ESP_ERROR_CHECK(tof_scanner_init());
    // tof_scanner_set_range_mm(40, 250);

    /* 3. Posiciona fisicamente o braço em IDLE no boot */
    arm_move_to_idle_pose();

    /* 4. Cria a task de operação */
    xTaskCreate(magnibot_op_task, "magnibot_op", 4096, NULL, 5, &s_op_task_handle);

    button_control_callbacks_t cbs = {
        .on_state_change = on_state_change,
        .on_op_trigger   = on_op_trigger,
        .on_emergency    = on_emergency,
    };

    /* 5. Inicializa os botões */
    ESP_ERROR_CHECK(button_control_init(&cbs));

    while (true) 
	{
        if (s_emergency_pending_print && !s_emergency_logged) 
		{
            s_emergency_pending_print = false;
            s_emergency_logged = true;
            ESP_LOGE(TAG, "[EMERGÊNCIA] Parar PWM e levar servos para posição segura!");
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}