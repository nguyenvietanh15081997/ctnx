
#include "Led.h"

// #define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO_0 (19) // Define the output GPIO
#define LEDC_OUTPUT_IO_1 (18) // Define the output GPIO
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY (4095)                // Set duty to 50%. ((2 ** 13) - 1) * 50% = 4095
#define LEDC_FREQUENCY (1)              // Frequency in Hertz. Set frequency at 5 kHz

static bool statusLedInter = false;
static bool statusLedService = false;
static int modeLedInternet = -1;
static int modeLedService = -1;

static void ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    // ledc_timer_config_t ledc_timer = {
    //     .speed_mode = LEDC_MODE,
    //     .timer_num = LEDC_TIMER_0,
    //     .duty_resolution = LEDC_DUTY_RES,
    //     .freq_hz = LEDC_FREQUENCY // Set output frequency at 5 kHz
    // };
    ledc_timer_config_t ledc_timer;
    ledc_timer.speed_mode = LEDC_MODE;
    ledc_timer.timer_num = LEDC_TIMER_0;
    ledc_timer.duty_resolution = LEDC_DUTY_RES;
    ledc_timer.freq_hz = LEDC_FREQUENCY;
    ledc_timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_timer.timer_num = LEDC_TIMER_1;
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    // ledc_channel_config_t ledc_channel = {
    //     .speed_mode = LEDC_MODE,
    //     .channel = LEDC_CHANNEL_0,
    //     .timer_sel = LEDC_TIMER_0,
    //     .intr_type = LEDC_INTR_DISABLE,
    //     .gpio_num = LEDC_OUTPUT_IO_0,
    //     .duty = 0 // Set duty to 0%
    // };
    ledc_channel_config_t ledc_channel;
    ledc_channel.speed_mode = LEDC_MODE;
    ledc_channel.channel = LEDC_CHANNEL_0;
    ledc_channel.timer_sel = LEDC_TIMER_0;
    ledc_channel.intr_type = LEDC_INTR_DISABLE;
    ledc_channel.gpio_num = LEDC_OUTPUT_IO_0;
    ledc_channel.duty = 0;
    ledc_channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    ledc_channel.channel = LEDC_CHANNEL_1;
    ledc_channel.timer_sel = LEDC_TIMER_1;
    ledc_channel.gpio_num = LEDC_OUTPUT_IO_1;
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void Led_init()
{
    ledc_init();

    // gpio_reset_pin(PIN_RESET_BLE);
    // /* Set the GPIO as a push/pull output */
    // gpio_set_direction(PIN_RESET_BLE, GPIO_MODE_OUTPUT);
    // gpio_set_level(PIN_RESET_BLE, 0);
    // vTaskDelay(1000 / portTICK_PERIOD_MS);
    // gpio_set_level(PIN_RESET_BLE, 1);
}

void SetGpioResetGwBle()
{
    gpio_config_t io_conf;
    // Cấu hình chân GPIO làm output
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << PIN_RESET_BLE);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    gpio_set_level(PIN_RESET_BLE, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(PIN_RESET_BLE, 1);

    gpio_reset_pin(PIN_RESET_BLE);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void SetLedInternet(bool status)
{
    modeLedInternet = LED_ON_OFF;
    statusLedInter = status;
    if (status)
    {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 0));
    }
    else
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, 8192));

    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
    ledc_set_freq(LEDC_MODE, LEDC_TIMER_1, 5);
}

void SetLedService(bool status)
{
    modeLedService = LED_ON_OFF;
    statusLedService = status;
    if (status)
    {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 0));
    }
    else
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, 8192));

    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
    ledc_set_freq(LEDC_MODE, LEDC_TIMER_0, 5);
}

bool GetStatusLedInternet()
{
    return statusLedInter;
}

bool GetStatusLedService()
{
    return statusLedService;
}

void BlinkLedService()
{
    modeLedService = LED_BLINK;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, LEDC_DUTY));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
    ledc_set_freq(LEDC_MODE, LEDC_TIMER_0, 1);
}

void FlashLedService()
{
    modeLedService = LED_FLASH;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, LEDC_DUTY));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0));
    ledc_set_freq(LEDC_MODE, LEDC_TIMER_0, 5);
}

void BlinkLedInternet()
{
    modeLedInternet = LED_BLINK;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, LEDC_DUTY));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
    ledc_set_freq(LEDC_MODE, LEDC_TIMER_1, 1);
}

void FlashLedInternet()
{
    modeLedInternet = LED_FLASH;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, LEDC_DUTY));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1));
    ledc_set_freq(LEDC_MODE, LEDC_TIMER_1, 5);
}

int GetModeLedService()
{
    return modeLedService;
}

int GetModeLedInternet()
{
    return modeLedInternet;
}
