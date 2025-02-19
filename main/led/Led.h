#pragma once

#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"
#include "unistd.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define LED_BLINK 1
#define LED_FLASH 2
#define LED_ON_OFF 0
#define PIN_RESET_BLE GPIO_NUM_21

void Led_init();

void SetGpioResetGwBle();

void SetLedInternet(bool status);

void SetLedService(bool status);

bool GetStatusLedInternet();

bool GetStatusLedService();

void BlinkLedService();

void FlashLedService();

void BlinkLedInternet();

void FlashLedInternet();

int GetModeLedService();

int GetModeLedInternet();
