#include "button_control.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/portmacro.h"

static const char *TAG = "BTN_CTRL";

static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;
static magnibot_state_t s_state = ESTADO_1_IDLE;
static bool s_emergency_active = false;
static magnibot_state_t s_emerg_prev_state = ESTADO_1_IDLE;

static button_control_callbacks_t s_cbs = {0};

static TaskHandle_t s_btn1_task_handle = NULL;
static TaskHandle_t s_emerg_task_handle = NULL;

static void set_state_internal(magnibot_state_t next_state)
{
    magnibot_state_t prev;
    bool notify = false;

    taskENTER_CRITICAL(&s_state_mux);
    prev = s_state;
    if (prev != next_state && !s_emergency_active) {
        s_state = next_state;
        notify = true;
    }
    taskEXIT_CRITICAL(&s_state_mux);

    if (notify && s_cbs.on_state_change != NULL) {
        s_cbs.on_state_change(prev, next_state);
    }
}

typedef enum {
    CMD_SINGLE_TAP = 1,
    CMD_DOUBLE_TAP,
    CMD_LONG_PRESS
} btn1_gesture_t;

static void handle_btn1_gesture(btn1_gesture_t gesture)
{
    magnibot_state_t current;

    taskENTER_CRITICAL(&s_state_mux);
    current = s_state;
    taskEXIT_CRITICAL(&s_state_mux);

    if (current == ESTADO_EMERGENCIA) {
        ESP_LOGW(TAG, "Botão 1 ignorado em ESTADO_EMERGENCIA.");
        return;
    }

    switch (gesture) {
        case CMD_SINGLE_TAP:
            ESP_LOGI(TAG, "Gesto BTN1: TOQUE SIMPLES (estado=%d)", current);
            if (current == ESTADO_1_IDLE) {
                set_state_internal(ESTADO_2_VARREDURA);
                if (s_cbs.on_op_trigger != NULL) {
                    s_cbs.on_op_trigger(ESTADO_2_VARREDURA, false);
                }
            }
            break;

        case CMD_DOUBLE_TAP:
            ESP_LOGI(TAG, "Gesto BTN1: TOQUE DUPLO (estado=%d)", current);
            if (current == ESTADO_3_MAPEAMENTO_CONCLUIDO) {
                set_state_internal(ESTADO_4_CAPTURA_DEPOSITO);
                if (s_cbs.on_op_trigger != NULL) {
                    s_cbs.on_op_trigger(ESTADO_4_CAPTURA_DEPOSITO, false);
                }
            }
            break;

        case CMD_LONG_PRESS:
            ESP_LOGI(TAG, "Gesto BTN1: TOQUE LONGO (estado=%d)", current);
            if (current == ESTADO_3_MAPEAMENTO_CONCLUIDO) {
                set_state_internal(ESTADO_2_VARREDURA);
                if (s_cbs.on_op_trigger != NULL) {
                    s_cbs.on_op_trigger(ESTADO_2_VARREDURA, true);
                }
            }
            break;
    }
}

static void IRAM_ATTR btn1_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t hp_task_woken = pdFALSE;
    if (s_btn1_task_handle != NULL) {
        vTaskNotifyGiveFromISR(s_btn1_task_handle, &hp_task_woken);
    }
    if (hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void IRAM_ATTR btn2_emergency_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t hp_task_woken = pdFALSE;

    portENTER_CRITICAL_ISR(&s_state_mux);
    if (!s_emergency_active) {
        s_emerg_prev_state = s_state;
        s_emergency_active = true;
        s_state = ESTADO_EMERGENCIA;
    }
    portEXIT_CRITICAL_ISR(&s_state_mux);

    if (s_cbs.on_emergency != NULL) {
        s_cbs.on_emergency();
    }

    if (s_emerg_task_handle != NULL) {
        xTaskNotifyFromISR(s_emerg_task_handle, 1, eNoAction, &hp_task_woken);
    }
    if (hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void emergency_task(void *pvParameters)
{
    (void)pvParameters;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        magnibot_state_t prev_st;
        taskENTER_CRITICAL(&s_state_mux);
        prev_st = s_emerg_prev_state;
        taskEXIT_CRITICAL(&s_state_mux);

        if (s_cbs.on_state_change != NULL) {
            s_cbs.on_state_change(prev_st, ESTADO_EMERGENCIA);
        }
        ESP_LOGE(TAG, "Emergência ativa via BTN2 (GPIO%d).", BTN2_GPIO);
    }
}

static void btn1_gesture_task(void *pvParameters)
{
    (void)pvParameters;

    const int64_t debounce_us   = ((BTN1_DEBOUNCE_MS > 25) ? BTN1_DEBOUNCE_MS : 25) * 1000LL;
    const int64_t single_max_us = (int64_t)BTN1_SINGLE_MAX_MS * 1000LL;
    const int64_t double_gap_us = (int64_t)BTN1_DOUBLE_TAP_GAP_MS * 1000LL;
    const int64_t long_min_us   = (int64_t)BTN1_LONG_MIN_MS * 1000LL;

    int debounced_level = gpio_get_level(BTN1_GPIO);
    int last_raw_level = debounced_level;
    int64_t last_raw_change_us = esp_timer_get_time();

    int64_t press_start_us = 0;
    int64_t first_release_us = 0;
    bool waiting_second_tap = false;
    bool long_press_handled = false;

    while (true) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(10));

        int64_t now_us = esp_timer_get_time();
        int raw_level = gpio_get_level(BTN1_GPIO);

        if (raw_level != last_raw_level) {
            last_raw_level = raw_level;
            last_raw_change_us = now_us;
        }

        /* Só valida a borda após o nível lógico permanecer estável pelo tempo de debounce */
        if (raw_level != debounced_level && (now_us - last_raw_change_us) >= debounce_us) {
            debounced_level = raw_level;

            if (debounced_level == 1) {
                press_start_us = now_us;
                long_press_handled = false;
            } else {
                int64_t press_duration_us = now_us - press_start_us;

                if (long_press_handled) {
                    waiting_second_tap = false;
                } else if (press_duration_us <= single_max_us) {
                    if (waiting_second_tap) {
                        waiting_second_tap = false;
                        handle_btn1_gesture(CMD_DOUBLE_TAP);
                    } else {
                        waiting_second_tap = true;
                        first_release_us = now_us;
                    }
                } else {
                    ESP_LOGW(TAG, "Toque no BTN1 fora da faixa (duracao=%lld ms | simples<=%d ms | longo>=%d ms).",
                             press_duration_us / 1000LL, BTN1_SINGLE_MAX_MS, BTN1_LONG_MIN_MS);
                }
            }
        }

        /* Dispara toque longo automaticamente assim que o tempo mínimo é atingido */
        if (debounced_level == 1 && !long_press_handled) {
            if ((now_us - press_start_us) >= long_min_us) {
                long_press_handled = true;
                waiting_second_tap = false;
                handle_btn1_gesture(CMD_LONG_PRESS);
            }
        }

        /* Só expira a janela do toque simples enquanto o botão estiver solto */
        if (debounced_level == 0 && waiting_second_tap) {
            if ((now_us - first_release_us) >= double_gap_us) {
                waiting_second_tap = false;
                handle_btn1_gesture(CMD_SINGLE_TAP);
            }
        }
    }
}

esp_err_t button_control_init(const button_control_callbacks_t *callbacks)
{
    if (callbacks != NULL) {
        s_cbs = *callbacks;
    }

    xTaskCreate(emergency_task, "emerg_task", 3072, NULL, configMAX_PRIORITIES - 1, &s_emerg_task_handle);
    xTaskCreate(btn1_gesture_task, "btn1_task", 4096, NULL, configMAX_PRIORITIES - 2, &s_btn1_task_handle);

    gpio_config_t btn1_cfg = {
        .pin_bit_mask = (1ULL << BTN1_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_ANYEDGE
    };
    ESP_RETURN_ON_ERROR(gpio_config(&btn1_cfg), TAG, "Falha ao configurar BTN1");

    gpio_config_t btn2_cfg = {
        .pin_bit_mask = (1ULL << BTN2_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_POSEDGE
    };
    ESP_RETURN_ON_ERROR(gpio_config(&btn2_cfg), TAG, "Falha ao configurar BTN2");

    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        return isr_err;
    }

    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(BTN1_GPIO, btn1_isr_handler, NULL), TAG, "Falha ISR BTN1");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(BTN2_GPIO, btn2_emergency_isr_handler, NULL), TAG, "Falha ISR BTN2");

    taskENTER_CRITICAL(&s_state_mux);
    s_state = ESTADO_1_IDLE;
    s_emergency_active = false;
    s_emerg_prev_state = ESTADO_1_IDLE;
    taskEXIT_CRITICAL(&s_state_mux);

    if (s_cbs.on_state_change != NULL) {
        s_cbs.on_state_change(ESTADO_1_IDLE, ESTADO_1_IDLE);
    }

    ESP_LOGI(TAG, "Botoes ativos -> BTN1: GPIO%d (nivel=%d) | BTN2: GPIO%d (nivel=%d)",
             BTN1_GPIO, gpio_get_level(BTN1_GPIO),
             BTN2_GPIO, gpio_get_level(BTN2_GPIO));
    ESP_LOGI(TAG, "Tempos BTN1 -> simples<=%d ms | janela_duplo=%d ms | longo>=%d ms",
             BTN1_SINGLE_MAX_MS, BTN1_DOUBLE_TAP_GAP_MS, BTN1_LONG_MIN_MS);

    return ESP_OK;
}

magnibot_state_t button_control_get_state(void)
{
    magnibot_state_t st;
    taskENTER_CRITICAL(&s_state_mux);
    st = s_state;
    taskEXIT_CRITICAL(&s_state_mux);
    return st;
}

void button_control_notify_op_finished(magnibot_state_t completed_state)
{
    bool emerg;
    magnibot_state_t cur;

    taskENTER_CRITICAL(&s_state_mux);
    emerg = s_emergency_active;
    cur = s_state;
    taskEXIT_CRITICAL(&s_state_mux);

    if (emerg) {
        return;
    }
    if (completed_state == ESTADO_2_VARREDURA && cur == ESTADO_2_VARREDURA) {
        set_state_internal(ESTADO_3_MAPEAMENTO_CONCLUIDO);
    } else if (completed_state == ESTADO_4_CAPTURA_DEPOSITO && cur == ESTADO_4_CAPTURA_DEPOSITO) {
        set_state_internal(ESTADO_1_IDLE);
    }
}

void button_control_clear_emergency(void)
{
    taskENTER_CRITICAL(&s_state_mux);
    s_emergency_active = false;
    s_state = ESTADO_1_IDLE;
    taskEXIT_CRITICAL(&s_state_mux);

    if (s_cbs.on_state_change != NULL) {
        s_cbs.on_state_change(ESTADO_EMERGENCIA, ESTADO_1_IDLE);
    }
}