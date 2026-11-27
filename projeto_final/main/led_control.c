#include "led_control.h"
#include "esp_check.h"
#include "esp_log.h"
#include "led_strip.h"

static const char *TAG = "LED_CTRL";

static led_strip_handle_t s_led_strip = NULL;

esp_err_t led_control_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_WS2812_GPIO,
        .max_leds = LED_WS2812_NUM_LEDS,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .mem_block_symbols = 0,            // Usa valor padrão do driver
        .flags = {
            .with_dma = false,
        },
    };

    ESP_RETURN_ON_ERROR(
        led_strip_new_rmt_device(&strip_config, &rmt_config, &s_led_strip),
        TAG,
        "Falha ao inicializar LED WS2812 no GPIO38"
    );

    ESP_RETURN_ON_ERROR(led_strip_clear(s_led_strip), TAG, "Falha ao limpar LED");
    ESP_LOGI(TAG, "LED WS2812 inicializado no GPIO%d", LED_WS2812_GPIO);
    return ESP_OK;
}

esp_err_t led_control_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
    ESP_RETURN_ON_FALSE(s_led_strip != NULL, ESP_ERR_INVALID_STATE, TAG, "LED não inicializado");

    ESP_RETURN_ON_ERROR(
        led_strip_set_pixel(s_led_strip, 0, red, green, blue),
        TAG,
        "Falha ao definir cor do pixel"
    );
    return led_strip_refresh(s_led_strip);
}


esp_err_t led_control_set_emergency_red(void)
{
    return led_control_set_color(LED_BRIGHTNESS_EMERG, 0, 0);
}

esp_err_t led_control_apply_state(magnibot_state_t state)
{
    switch (state) 
	{
        case ESTADO_1_IDLE:
            /* Verde */
            return led_control_set_color(0, LED_BRIGHTNESS_NORM, 0);

        case ESTADO_2_VARREDURA:
            /* Azul */
            return led_control_set_color(0, 0, LED_BRIGHTNESS_NORM);

        case ESTADO_3_MAPEAMENTO_CONCLUIDO:
            /* Amarelo (Vermelho + Verde) */
            return led_control_set_color(LED_BRIGHTNESS_NORM, LED_BRIGHTNESS_NORM, 0);

        case ESTADO_4_CAPTURA_DEPOSITO:
            /* Roxo (Vermelho + Azul) */
            return led_control_set_color(LED_BRIGHTNESS_NORM, 0, LED_BRIGHTNESS_NORM);

        case ESTADO_EMERGENCIA:
            /* Vermelho */
            return led_control_set_emergency_red();

        default:
            return ESP_ERR_INVALID_ARG;
    }
}
