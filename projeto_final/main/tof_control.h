#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pinos I2C padrão (ajuste conforme sua ligação na ESP32-S3)
#define TOF_I2C_SDA_GPIO        GPIO_NUM_8
#define TOF_I2C_SCL_GPIO        GPIO_NUM_9
#define TOF_I2C_PORT            I2C_NUM_0

// Parâmetros padrão de detecção (alteráveis via função durante os testes)
#define TOF_DEFAULT_MIN_DIST_MM 40   // Ignora ruídos muito próximos da lente
#define TOF_DEFAULT_MAX_DIST_MM 250  // Distância máxima para considerar um objeto
#define TOF_CONFIRM_SAMPLES     3    // Leituras seguidas válidas para confirmar objeto

typedef struct {
    bool     found;
    float    angle_deg;
    uint16_t distance_mm;
} tof_target_t;


esp_err_t tof_scanner_init(void);

void tof_scanner_set_range_mm(uint16_t min_mm, uint16_t max_mm);

void tof_scanner_reset_target(void);

bool tof_scanner_poll_at_angle(float current_angle_deg);

tof_target_t tof_scanner_get_target(void);

#ifdef __cplusplus
}
#endif