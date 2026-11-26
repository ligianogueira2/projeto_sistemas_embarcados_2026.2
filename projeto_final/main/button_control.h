#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pinos conforme esquemático ESP32-S3-DEVKITC-1-N8 */
#define BTN1_GPIO     GPIO_NUM_1   // IN_BUTTON1 (3V3 via push-button, pull-down 10k -> ativo em HIGH)[cite: 1]
#define BTN2_GPIO     GPIO_NUM_2   // IN_BUTTON2 (emergência)[cite: 1]

/* Estados operacionais do MagniBot */
typedef enum {
    ESTADO_1_IDLE = 1,
    ESTADO_2_VARREDURA,
    ESTADO_3_MAPEAMENTO_CONCLUIDO,
    ESTADO_4_CAPTURA_DEPOSITO,
    ESTADO_EMERGENCIA
} magnibot_state_t;

/* Tempos de gesto do Botão 1 */
#define BTN1_DEBOUNCE_MS         30U
#define BTN1_SINGLE_MAX_MS     1000U  // Toque simples <= 1s de pressão
#define BTN1_LONG_MIN_MS       2000U  // Toque longo > 2s de pressão
#define BTN1_DOUBLE_TAP_GAP_MS 1000U  // Intervalo entre toques < 1s

/* Callbacks para integração com os outros módulos do grupo */
typedef void (*magnibot_state_change_cb_t)(magnibot_state_t prev_state, magnibot_state_t new_state);
typedef void (*magnibot_op_trigger_cb_t)(magnibot_state_t target_state, bool discard_previous);
typedef void (*magnibot_emergency_cb_t)(void);

typedef struct {
    magnibot_state_change_cb_t on_state_change; // Notifica mudança de estado / LED de status
    magnibot_op_trigger_cb_t   on_op_trigger;   // Dispara varredura (Estado 2) ou captura/depósito (Estado 4)
    magnibot_emergency_cb_t    on_emergency;    // Disparo imediato na emergência (servos / PWM / LED vermelho)
} button_control_callbacks_t;

esp_err_t button_control_init(const button_control_callbacks_t *callbacks);
magnibot_state_t button_control_get_state(void);
void button_control_notify_op_finished(magnibot_state_t completed_state);
void button_control_clear_emergency(void);

#ifdef __cplusplus
}
#endif