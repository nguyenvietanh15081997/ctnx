#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "encoder.h"

EventGroupHandle_t input_event_group;
/*
    CW : 00 -> 01 -> 11 -> 10 -> 00
    CCW: 00 -> 10 -> 11 -> 01 -> 00

    Quy ước: CW: -1, CCW: +1, No movement: 0, invalid/noise: 0,
             state = AB, (index = (old_state << 1) | current_state)
    Bảng tra như sau:

    - TH1 old_state = 0x00
     newState | transition | result | index
    ---------------------------------------
        00    |   no move  |    0   |   0
        01    |   CW       |   -1   |   1
        10    |   CCW      |   +1   |   2
        11    |   invalid  |    0   |   3

    - TH2 old_state = 0x01
     newState | transition | result | index
    ---------------------------------------
        00    |   CCW      |   +1   |   4
        01    |   no move  |    0   |   5
        10    |   invalid  |    0   |   6
        11    |   CW       |   -1   |   7

    - TH3 old_state = 0x10
     newState | transition | result | index
    ---------------------------------------
        00    |    CW      |   -1   |   8
        01    |   invalid  |    0   |   9
        10    |   no move  |    0   |   10
        11    |   CCW      |    1   |   11

    - TH4 old_state = 0x11
     newState | transition | result | index
    ---------------------------------------
        00    |   invalid  |    0   |   12
        01    |   CCW      |    1   |   13
        10    |   CW       |   -1   |   14
        11    |   no move  |    0   |   15

    Mỗi bước encoder gồm 2 transition
*/

static const int8_t enc_table[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0};

static void IRAM_ATTR encoder_isr(void *arg)
{
    static uint8_t old_state = 0;
    static int8_t step = 0;

    uint8_t a = gpio_get_level(ENCODER_A_GPIO);
    uint8_t b = gpio_get_level(ENCODER_B_GPIO);

    uint8_t current_state = (a << 1) | b;

    uint8_t index = (old_state << 2) | current_state;
    int8_t movement = enc_table[index];

    if (movement != 0)
    {
        step += movement;

        if (step >= 2)
        {
            BaseType_t hp_task_woken = pdFALSE;

            xEventGroupSetBitsFromISR(input_event_group, EVT_ENCODER_LEFT, &hp_task_woken);
            if (hp_task_woken)
                portYIELD_FROM_ISR();

            step = 0;
        }
        else if (step <= -2)
        {
            BaseType_t hp_task_woken = pdFALSE;
            xEventGroupSetBitsFromISR(input_event_group, EVT_ENCODER_RIGHT, &hp_task_woken);
            if (hp_task_woken)
                portYIELD_FROM_ISR();

            step = 0;
        }
    }
    old_state = current_state;
}
extern void OnEncoder(uint8_t direction);
static void input_task(void *arg)
{
    while (1)
    {
        EventBits_t bits = xEventGroupWaitBits(
            input_event_group,
            EVT_ENCODER_LEFT | EVT_ENCODER_RIGHT,
            pdTRUE,  // clear bit
            pdFALSE, // OR
            portMAX_DELAY);
        if (bits & EVT_ENCODER_LEFT)
        {
            printf("-\n");
            OnEncoder(0); // Xoay trái
        }
        if (bits & EVT_ENCODER_RIGHT)
        {
            printf("+\n");
            OnEncoder(1); // Xoay phải
        }

        // vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void encoder_signal_init(void)
{
    input_event_group = xEventGroupCreate();
    if (input_event_group == NULL)
    {
        ESP_LOGE("ENCODER", "Failed to create event group");
        return;
    }
    gpio_install_isr_service(0);

    gpio_config_t enc = {
        .pin_bit_mask = (1ULL << ENCODER_A_GPIO) |
                        (1ULL << ENCODER_B_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE};

    gpio_config(&enc);

    gpio_isr_handler_add(ENCODER_A_GPIO, encoder_isr, NULL);
    gpio_isr_handler_add(ENCODER_B_GPIO, encoder_isr, NULL);

    xTaskCreate(input_task, "input_task", 2048, NULL, 10, NULL);
}