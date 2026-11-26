#include "button_control.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/portmacro.h"

static const char *TAG = "BTN_CTRL";

static portMUX_TYPE s_state_mux = portMUX_INITIALIZER_UNLOCKED;
static magnibot_state_t s_state = ESTADO_1_IDLE;
static bool s_emergency_active = false;
static magnibot_state_t s_emerg_prev_state = ESTADO_1_IDLE;

static button_control_callbacks_t s_cbs = {0};

static TaskHandle_t s_btn1_task_handle = NULL;
static TaskHandle_t s_emerg_task_handle = NULL;
static TimerHandle_t s_single_tap_timer = NULL;

static int64_t s_press_start_us = 0;
static int64_t s_last_release_us = 0;
static bool s_waiting_second_tap = false;
static bool s_long_press_handled = false;

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
            if (current == ESTADO_1_IDLE) {
                set_state_internal(ESTADO_2_VARREDURA);
                if (s_cbs.on_op_trigger != NULL) {
                    s_cbs.on_op_trigger(ESTADO_2_VARREDURA, false);
                }
            }
            /* Estados 2 ou 3: ignorado */
            break;

        case CMD_DOUBLE_TAP:
            if (current == ESTADO_3_MAPEAMENTO_CONCLUIDO) {
                set_state_internal(ESTADO_4_CAPTURA_DEPOSITO);
                if (s_cbs.on_op_trigger != NULL) {
                    s_cbs.on_op_trigger(ESTADO_4_CAPTURA_DEPOSITO, false);
                }
            }
            break;

        case CMD_LONG_PRESS:
            if (current == ESTADO_3_MAPEAMENTO_CONCLUIDO) {
                set_state_internal(ESTADO_2_VARREDURA);
                if (s_cbs.on_op_trigger != NULL) {
                    s_cbs.on_op_trigger(ESTADO_2_VARREDURA, true);
                }
            }
            break;
    }
}

static void single_tap_timeout_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    if (s_waiting_second_tap) {
        s_waiting_second_tap = false;
        handle_btn1_gesture(CMD_SINGLE_TAP);
    }
}

static void IRAM_ATTR btn1_isr_handler(void *arg)
{
    (void)arg;
    BaseType_t hp_task_woken = pdFALSE;
    uint32_t level = (uint32_t)gpio_get_level(BTN1_GPIO);
    int64_t now_us = esp_timer_get_time();

    uint32_t packed = ((level & 0x1U) << 31) | ((uint32_t)(now_us & 0x7FFFFFFF));
    xTaskNotifyFromISR(s_btn1_task_handle, packed, eSetValueWithOverwrite, &hp_task_woken);
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

    /* Dispara callback de emergência imediatamente (ex.: acionar flag/interrupção de PWM) */
    if (s_cbs.on_emergency != NULL) {
        s_cbs.on_emergency();
    }

    xTaskNotifyFromISR(s_emerg_task_handle, 1, eNoAction, &hp_task_woken);
    if (hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void emergency_task(void *pvParameters)
{
    (void)pvParameters;
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (s_single_tap_timer != NULL) {
            xTimerStop(s_single_tap_timer, 0);
        }
        s_waiting_second_tap = false;

        magnibot_state_t prev_st;
        taskENTER_CRITICAL(&s_state_mux);
        prev_st = s_emerg_prev_state;
        taskEXIT_CRITICAL(&s_state_mux);

        if (s_cbs.on_state_change != NULL) {
            s_cbs.on_state_change(prev_st, ESTADO_EMERGENCIA);
        }
        ESP_LOGE(TAG, "Emergência ativa via BTN2 (GPIO2).");
    }
}

static void btn1_gesture_task(void *pvParameters)
{
    (void)pvParameters;
    uint32_t notified_val = 0;

    while (true) {
        if (xTaskNotifyWait(0, UINT32_MAX, &notified_val, pdMS_TO_TICKS(50)) == pdTRUE) {
            bool pressed = ((notified_val >> 31) & 0x1U) != 0;
            int64_t now_us = esp_timer_get_time();

            if (pressed) {
                if ((now_us - s_press_start_us) < (BTN1_DEBOUNCE_MS * 1000LL)) {
                    continue;
                }
                s_press_start_us = now_us;
                s_long_press_handled = false;
            } else {
                int64_t press_duration_ms = (now_us - s_press_start_us) / 1000LL;
                if (press_duration_ms < (int64_t)BTN1_DEBOUNCE_MS) {
                    continue;
                }

                if (s_long_press_handled) {
                    s_waiting_second_tap = false;
                    continue;
                }

                if (press_duration_ms <= (int64_t)BTN1_SINGLE_MAX_MS) {
                    int64_t gap_ms = (now_us - s_last_release_us) / 1000LL;
                    s_last_release_us = now_us;

                    if (s_waiting_second_tap && gap_ms < (int64_t)BTN1_DOUBLE_TAP_GAP_MS) {
                        s_waiting_second_tap = false;
                        xTimerStop(s_single_tap_timer, 0);
                        handle_btn1_gesture(CMD_DOUBLE_TAP);
                    } else {
                        s_waiting_second_tap = true;
                        xTimerChangePeriod(s_single_tap_timer,
                                           pdMS_TO_TICKS(BTN1_DOUBLE_TAP_GAP_MS),
                                           0);
                        xTimerStart(s_single_tap_timer, 0);
                    }
                }
            }
        }

        if (!s_long_press_handled && gpio_get_level(BTN1_GPIO) == 1) {
            int64_t held_ms = (esp_timer_get_time() - s_press_start_us) / 1000LL;
            if (held_ms > (int64_t)BTN1_LONG_MIN_MS) {
                s_long_press_handled = true;
                s_waiting_second_tap = false;
                xTimerStop(s_single_tap_timer, 0);
                handle_btn1_gesture(CMD_LONG_PRESS);
            }
        }
    }
}

esp_err_t button_control_init(const button_control_callbacks_t *callbacks)
{
    if (callbacks != NULL) {
        s_cbs = *callbacks;
    }

    s_single_tap_timer = xTimerCreate("btn1_tap_tmr",
                                      pdMS_TO_TICKS(BTN1_DOUBLE_TAP_GAP_MS),
                                      pdFALSE,
                                      NULL,
                                      single_tap_timeout_cb);
    ESP_RETURN_ON_FALSE(s_single_tap_timer != NULL, ESP_ERR_NO_MEM, TAG, "Falha ao criar timer");

    xTaskCreate(emergency_task, "emerg_task", 2560, NULL, configMAX_PRIORITIES - 1, &s_emerg_task_handle);
    xTaskCreate(btn1_gesture_task, "btn1_task", 3072, NULL, configMAX_PRIORITIES - 2, &s_btn1_task_handle);

    gpio_config_t btn1_cfg = {
        .pin_bit_mask = (1ULL << BTN1_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE
    };
    ESP_RETURN_ON_ERROR(gpio_config(&btn1_cfg), TAG, "Falha ao configurar BTN1");

    /* Borda de subida (POSEDGE) pois o botão liga 3V3 com pull-down de 10k para GND[cite: 1] */
    gpio_config_t btn2_cfg = {
        .pin_bit_mask = (1ULL << BTN2_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
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

    if (emerg) 
	{
        return;
    }
    if (completed_state == ESTADO_2_VARREDURA && cur == ESTADO_2_VARREDURA) 
	{
        set_state_internal(ESTADO_3_MAPEAMENTO_CONCLUIDO);
    } else if (completed_state == ESTADO_4_CAPTURA_DEPOSITO && cur == ESTADO_4_CAPTURA_DEPOSITO) 
	{
        set_state_internal(ESTADO_1_IDLE);
    }
}

void button_control_clear_emergency(void)
{
    taskENTER_CRITICAL(&s_state_mux);
    s_emergency_active = false;
    s_state = ESTADO_1_IDLE;
    taskEXIT_CRITICAL(&s_state_mux);

    if (s_cbs.on_state_change != NULL) 
	{
        s_cbs.on_state_change(ESTADO_EMERGENCIA, ESTADO_1_IDLE);
    }
}