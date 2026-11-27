#include "tof_control.h"
#include "esp_check.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "vl53l0x.h"

static const char *TAG = "TOF_SCAN";

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static vl53l0x_handle_t s_sensor = NULL;

static portMUX_TYPE s_tof_mux = portMUX_INITIALIZER_UNLOCKED;
static uint16_t s_min_dist_mm = TOF_DEFAULT_MIN_DIST_MM;
static uint16_t s_max_dist_mm = TOF_DEFAULT_MAX_DIST_MM;
static uint8_t  s_hit_counter = 0;

static tof_target_t s_target = {
    .found = false,
    .angle_deg = 0.0f,
    .distance_mm = 0
};

static esp_err_t vl53l0x_read_raw_mm(uint16_t *out_mm)
{
    if (s_sensor == NULL || out_mm == NULL) 
	{
        return ESP_ERR_INVALID_STATE;
    }

    vl53l0x_data_t data = {0};
    esp_err_t err = vl53l0x_single_measure(s_sensor, &data);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Erro na leitura do VL53L0X: %s", esp_err_to_name(err));
        return err;
    }

    if (!data.valid) 
	{
        *out_mm = 8190;
        return ESP_OK;
    }

    *out_mm = data.distance_mm;
    return ESP_OK;
}

esp_err_t tof_scanner_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TOF_I2C_PORT,
        .sda_io_num = TOF_I2C_SDA_GPIO,
        .scl_io_num = TOF_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "Falha ao criar barramento I2C");

    ESP_RETURN_ON_ERROR(vl53l0x_create(&s_sensor, s_i2c_bus), TAG, "Falha ao criar VL53L0X");
    ESP_RETURN_ON_ERROR(vl53l0x_init(s_sensor), TAG, "Falha ao inicializar VL53L0X");

    ESP_LOGI(TAG, "VL53L0X pronto (SDA=%d, SCL=%d | Faixa: %u-%u mm)",
             TOF_I2C_SDA_GPIO, TOF_I2C_SCL_GPIO, s_min_dist_mm, s_max_dist_mm);
    return ESP_OK;
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
    taskEXIT_CRITICAL(&s_tof_mux);
}

bool tof_scanner_poll_at_angle(float current_angle_deg)
{
    uint16_t dist_mm = 0;
    if (vl53l0x_read_raw_mm(&dist_mm) != ESP_OK) 
	{
        s_hit_counter = 0;
        return false;
    }

    uint16_t min_mm, max_mm;
    taskENTER_CRITICAL(&s_tof_mux);
    min_mm = s_min_dist_mm;
    max_mm = s_max_dist_mm;
    taskEXIT_CRITICAL(&s_tof_mux);

    // Log de apoio para calibração durante os testes
    ESP_LOGI(TAG, "[Ângulo %.1f°] Leitura: %u mm", current_angle_deg, dist_mm);

    if (dist_mm >= min_mm && dist_mm <= max_mm) 
	{
        s_hit_counter++;
        if (s_hit_counter >= TOF_CONFIRM_SAMPLES) 
		{
            taskENTER_CRITICAL(&s_tof_mux);
            s_target.found = true;
            s_target.angle_deg = current_angle_deg;
            s_target.distance_mm = dist_mm;
            taskEXIT_CRITICAL(&s_tof_mux);

            ESP_LOGI(TAG, ">>> OBJETO DETECTADO! Ângulo: %.1f° | Distância: %u mm <<<",
                     current_angle_deg, dist_mm);
            return true;
        }
    } else 
	{
        s_hit_counter = 0;
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