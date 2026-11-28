#include "tof_control.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

/* 1 = Simulação virtual no Wokwi | 0 = Sensor VL53L0X real na bancada */
#define MODO_SIMULACAO 0

#if !MODO_SIMULACAO
#include "driver/i2c_master.h"
#include "vl53l0x.h"
#endif

static const char *TAG = "TOF_SCAN";

#if !MODO_SIMULACAO
static i2c_master_bus_handle_t s_i2c_bus = NULL;
static vl53l0x_handle_t s_sensor = NULL;
static bool s_sensor_ready = false;
static uint8_t s_consecutive_i2c_errors = 0;
#endif

static portMUX_TYPE s_tof_mux = portMUX_INITIALIZER_UNLOCKED;
static uint16_t s_min_dist_mm = TOF_DEFAULT_MIN_DIST_MM;
static uint16_t s_max_dist_mm = TOF_DEFAULT_MAX_DIST_MM;

static uint8_t  s_hit_counter = 0;
static float    s_first_hit_angle_deg = 0.0f;
static uint32_t s_dist_sum_mm = 0;

static tof_target_t s_target = {
    .found = false,
    .angle_deg = 0.0f,
    .distance_mm = 0
};

#if !MODO_SIMULACAO
static void i2c_diagnostic_scan(void)
{
    if (s_i2c_bus == NULL) {
        return;
    }

    ESP_LOGI(TAG, "[DIAG I2C] Varrendo barramento (SDA=GPIO%d, SCL=GPIO%d)...",
             TOF_I2C_SDA_GPIO, TOF_I2C_SCL_GPIO);

    uint8_t found_count = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++) {
        if (i2c_master_probe(s_i2c_bus, addr, 30) == ESP_OK) {
            ESP_LOGI(TAG, "[DIAG I2C] Dispositivo encontrado no endereco 0x%02X%s",
                     addr, (addr == 0x29) ? " (VL53L0X OK)" : "");
            found_count++;
        }
    }

    if (found_count == 0) {
        ESP_LOGE(TAG, "[DIAG I2C] NENHUM dispositivo respondeu! Verifique mau contato em SDA(GPIO%d), SCL(GPIO%d), 3V3 ou GND.",
                 TOF_I2C_SDA_GPIO, TOF_I2C_SCL_GPIO);
    }
}

static esp_err_t vl53l0x_try_setup_sensor(void)
{
    i2c_diagnostic_scan();

    if (i2c_master_probe(s_i2c_bus, 0x29, 50) != ESP_OK) {
        ESP_LOGE(TAG, "[DIAG I2C] VL53L0X nao respondeu ao ACK em 0x29 (mau contato ou falta de alimentacao).");
        s_sensor_ready = false;
        return ESP_ERR_NOT_FOUND;
    }

    if (s_sensor == NULL) {
        ESP_RETURN_ON_ERROR(vl53l0x_create(&s_sensor, s_i2c_bus), TAG, "Falha ao criar instancia VL53L0X");
    }

    esp_err_t err = vl53l0x_init(s_sensor);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[DIAG I2C] Falha em vl53l0x_init: %s (possivel ruido/mau contato no barramento)",
                 esp_err_to_name(err));
        s_sensor_ready = false;
        return err;
    }

    s_sensor_ready = true;
    s_consecutive_i2c_errors = 0;
    ESP_LOGI(TAG, "VL53L0X inicializado com sucesso (SDA=%d, SCL=%d | Faixa: %u-%u mm)",
             TOF_I2C_SDA_GPIO, TOF_I2C_SCL_GPIO, s_min_dist_mm, s_max_dist_mm);
    return ESP_OK;
}

static esp_err_t vl53l0x_read_raw_mm(uint16_t *out_mm)
{
    if (out_mm == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_sensor_ready) {
        ESP_LOGW(TAG, "[DIAG I2C] Sensor nao estava pronto; tentando reconectar...");
        if (vl53l0x_try_setup_sensor() != ESP_OK) {
            return ESP_ERR_INVALID_STATE;
        }
    }

    vl53l0x_data_t data = {0};
    esp_err_t err = vl53l0x_single_measure(s_sensor, &data);
    if (err != ESP_OK) {
        s_consecutive_i2c_errors++;
        ESP_LOGE(TAG, "[DIAG I2C] Erro de leitura (%u seguidos): %s",
                 s_consecutive_i2c_errors, esp_err_to_name(err));
        if (s_consecutive_i2c_errors >= 3) {
            ESP_LOGW(TAG, "[DIAG I2C] 3 falhas seguidas — marcando sensor para reinicializacao (mau contato).");
            s_sensor_ready = false;
        }
        return err;
    }

    s_consecutive_i2c_errors = 0;

    if (!data.valid) {
        ESP_LOGD(TAG, "[DIAG TOF] Medicao fora de alcance ou sinal fraco (valid=0, raw=%u mm -> 8190 mm)",
                 data.distance_mm);
        *out_mm = 8190;
        return ESP_OK;
    }

    *out_mm = data.distance_mm;
    return ESP_OK;
}
#endif

esp_err_t tof_scanner_init(void)
{
#if MODO_SIMULACAO
    ESP_LOGI(TAG, "[SIMULAÇÃO] VL53L0X virtual ativo (objeto simulado entre 25° e 35° a 120 mm)");
    return ESP_OK;
#else
    if (s_i2c_bus == NULL) {
        i2c_master_bus_config_t bus_cfg = {
            .i2c_port = TOF_I2C_PORT,
            .sda_io_num = TOF_I2C_SDA_GPIO,
            .scl_io_num = TOF_I2C_SCL_GPIO,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .flags.enable_internal_pullup = true,
        };
        ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "Falha ao criar barramento I2C");
    }

    return vl53l0x_try_setup_sensor();
#endif
}

void tof_scanner_set_range_mm(uint16_t min_mm, uint16_t max_mm)
{
    taskENTER_CRITICAL(&s_tof_mux);
    s_min_dist_mm = min_mm;
    s_max_dist_mm = max_mm;
    taskEXIT_CRITICAL(&s_tof_mux);

    ESP_LOGI(TAG, "Nova faixa de detecção: %u mm a %u mm", min_mm, max_mm);
}

void tof_scanner_reset_target(void)
{
    taskENTER_CRITICAL(&s_tof_mux);
    s_target.found = false;
    s_target.angle_deg = 0.0f;
    s_target.distance_mm = 0;
    s_hit_counter = 0;
    s_first_hit_angle_deg = 0.0f;
    s_dist_sum_mm = 0;
    taskEXIT_CRITICAL(&s_tof_mux);
}

bool tof_scanner_poll_at_angle(float current_angle_deg)
{
    uint16_t dist_mm = 0;

#if MODO_SIMULACAO
    if (current_angle_deg >= 25.0f && current_angle_deg <= 35.0f) {
        dist_mm = 120;
    } else {
        dist_mm = 8190;
    }
#else
    if (vl53l0x_read_raw_mm(&dist_mm) != ESP_OK) {
        s_hit_counter = 0;
        s_dist_sum_mm = 0;
        return false;
    }
#endif

    uint16_t min_mm, max_mm;
    taskENTER_CRITICAL(&s_tof_mux);
    min_mm = s_min_dist_mm;
    max_mm = s_max_dist_mm;
    taskEXIT_CRITICAL(&s_tof_mux);

    ESP_LOGI(TAG, "[Ângulo %.1f°] Leitura: %u mm (faixa alvo: %u-%u mm | hits=%u/%u)",
             current_angle_deg, dist_mm, min_mm, max_mm, s_hit_counter, TOF_CONFIRM_SAMPLES);

    if (dist_mm >= min_mm && dist_mm <= max_mm) {
        if (s_hit_counter == 0) {
            s_first_hit_angle_deg = current_angle_deg;
            s_dist_sum_mm = 0;
        }

        s_hit_counter++;
        s_dist_sum_mm += dist_mm;

        if (s_hit_counter >= TOF_CONFIRM_SAMPLES) {
            float center_angle_deg = (s_first_hit_angle_deg + current_angle_deg) * 0.5f;
            uint16_t avg_dist_mm = (uint16_t)(s_dist_sum_mm / s_hit_counter);

            taskENTER_CRITICAL(&s_tof_mux);
            s_target.found = true;
            s_target.angle_deg = center_angle_deg;
            s_target.distance_mm = avg_dist_mm;
            taskEXIT_CRITICAL(&s_tof_mux);

            ESP_LOGI(TAG, ">>> OBJETO DETECTADO! Centro: %.1f° (faixa %.1f° a %.1f°) | Distância média: %u mm <<<",
                     center_angle_deg, s_first_hit_angle_deg, current_angle_deg, avg_dist_mm);
            return true;
        }
    } else {
        s_hit_counter = 0;
        s_dist_sum_mm = 0;
    }

    return false;
}

tof_target_t tof_scanner_get_target(void)
{
    tof_target_t copy;
    taskENTER_CRITICAL(&s_tof_mux);
    copy = s_target;
    taskEXIT_CRITICAL(&s_tof_mux);
    return copy;
}