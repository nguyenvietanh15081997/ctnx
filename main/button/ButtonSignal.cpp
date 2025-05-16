#include "ButtonSignal.h"
#include <unistd.h>
#include <stdio.h>
#include "Log.h"
#include "Util.h"
#include "Wifi.h"
#include "Led.h"
#include "Gateway.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"

#ifdef CONFIG_ENABLE_BLE
#include "BleProtocol.h"
#endif

#define GPIO_OUTPUT_IO_0 GPIO_NUM_3
#define GPIO_OUTPUT_PIN_SEL (1ULL << GPIO_OUTPUT_IO_0)
#define GPIO_INPUT_IO_0 GPIO_NUM_0
#define GPIO_INPUT_PIN_SEL (1ULL << GPIO_INPUT_IO_0)
#define ESP_INTR_FLAG_DEFAULT 0

#define DOUBLE_CLICK_TIME 400
#define AP_MODE_WIFI 5
#define MODE_SEND_UDP_BROADCAST 3

ButtonSignal *ButtonSignal::getInstance()
{
	static ButtonSignal *buttonSignal = NULL;
	if (!buttonSignal)
	{
		buttonSignal = new ButtonSignal();
	}
	return buttonSignal;
}

extern int startAPTimeCount;
static bool statusLedService = false;
static bool statusLedInternet = false;
static bool isProvision = false;
static void ButtonSignalHandler(void *arg)
{
	ButtonSignal *buttonSignal = (ButtonSignal *)arg;
	int cnt = 0;
	int led = 0;

	while (1)
	{
		if (buttonSignal->isPressed)
		{
			LOGD("cnt: %d", cnt);
			gpio_set_level(GPIO_OUTPUT_IO_0, (cnt++) % 2);
			if (cnt < 15 || cnt > 20)
			{
				BlinkLedService();
				sleep(1);
			}
			else
				sleep(1);

			if (cnt == 5)
			{
				FlashLedInternet();
			}
			else if (cnt == 9)
			{
				SetLedInternet(statusLedInternet);
			}
			else if (cnt == 15)
			{
				SetLedInternet(false);
				SetLedService(false);
			}
			else if (cnt == 20)
			{
				SetLedInternet(statusLedInternet);
			}
		}
		else
		{
			if (cnt > 0)
			{
				SetLedService(statusLedService);
				if (isProvision)
				{
					isProvision = false;
#ifdef CONFIG_ENABLE_BLE
					BleProtocol::getInstance()->StopScan();
#endif
				}
				if (cnt >= 5 && cnt < 9)
				{
					if (!Wifi::WifiIsAPMode())
					{
						led = 0;
						Wifi::WifiStartAP();
					}
					// gateway->StartUdpBroadcast();
				}
				else if (cnt >= 15 && cnt < 20)
				{
					// if (!Wifi::WifiIsAPMode())
					// {
					led = 0;
					Gateway::getInstance()->ResetFactory();
					Wifi::WifiStartAP();
					esp_restart();
					// }
					// gateway->StartUdpBroadcast();
				}
				else if (cnt >= 25 && cnt <= 30)
				{
					LOGW("Enable pair mode");
					Json::Value pairModeJson;
					pairModeJson["pairMode"] = false;
					Gateway::getInstance()->GatewayAttribute(pairModeJson);
					sleep(1);
					pairModeJson["pairMode"] = true;
					Gateway::getInstance()->GatewayAttribute(pairModeJson);
				}
				cnt = 0;
				gpio_set_level(GPIO_OUTPUT_IO_0, 0);
			}
			else if (Wifi::WifiIsAPMode())
			{
				SetLedService(statusLedService);
				startAPTimeCount++;
				gpio_set_level(GPIO_OUTPUT_IO_0, (led++) % 2);
				sleep(1);
				if (startAPTimeCount >= 300)
				{
					if (Wifi::GetIP() == "10.10.10.1")
					{
						SetLedInternet(false);
					}
					startAPTimeCount = 0;
					// gateway->StopUdpBroadcast();
					Wifi::WifiStartSta();
				}
			}
			else if (!Wifi::WifiIsAPMode() && (startAPTimeCount > 0))
			{
				SetLedService(statusLedService);
				startAPTimeCount = 0;
				if (Wifi::GetIP() == "10.10.10.1")
				{
					SetLedInternet(false);
				}
			}
			else
			{
				sleep(1);
			}
		}
	}
	vTaskDelete(NULL);
}

ButtonSignal::ButtonSignal()
{
	isPressed = false;
}

ButtonSignal::~ButtonSignal()
{
}

void ButtonSignal::init()
{
	LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	if (xTaskCreate(ButtonSignalHandler, "Button", 5120, (void *)this, 10, NULL) != pdPASS)
	{
		LOGE("Failed to create task");
		SetLedService(false);
	}
}

void ButtonSignal::OnPress()
{
	LOGI("OnPress");
	isPressed = true;
	statusLedService = GetStatusLedService();
	statusLedInternet = GetStatusLedInternet();
}

void ButtonSignal::OnRelease()
{
	LOGI("OnRelease");
	isPressed = false;
}

bool ButtonSignal::GetStatus()
{
	return this->isPressed;
}

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
	uint32_t gpio_num = (uint32_t)arg;
	xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
	uint32_t io_num;
	while (1)
	{
		if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
		{
			printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level((gpio_num_t)io_num));
			if (io_num == GPIO_INPUT_IO_0)
			{
				if (gpio_get_level((gpio_num_t)io_num))
				{
					ButtonSignal::getInstance()->OnRelease();
				}
				else
				{
					ButtonSignal::getInstance()->OnPress();
				}
			}
		}
		usleep(200000);
	}
	vTaskDelete(NULL);
}

void gpio_init(void)
{
	// zero-initialize the config structure.
	gpio_config_t io_conf = {};
	// disable interrupt
	io_conf.intr_type = GPIO_INTR_DISABLE;
	// set as output mode
	io_conf.mode = GPIO_MODE_OUTPUT;
	// bit mask of the pins that you want to set,e.g.GPIO18/19
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
	// disable pull-down mode
	io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
	// disable pull-up mode
	io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
	// configure GPIO with the given settings
	gpio_config(&io_conf);

	// interrupt of rising edge
	io_conf.intr_type = GPIO_INTR_POSEDGE;
	// bit mask of the pins, use GPIO4/5 here
	io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
	// set as input mode
	io_conf.mode = GPIO_MODE_INPUT;
	// enable pull-up mode
	io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
	gpio_config(&io_conf);

	// change gpio intrrupt type for one pin
	gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

	// create a queue to handle gpio event from isr
	gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	// start gpio task
	LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	if (xTaskCreate(gpio_task_example, "gpio_task", 2048, NULL, 10, NULL) != pdPASS)
	{
		LOGE("Failed to create task\n");
		SetLedService(false);
	}

	// install gpio isr service
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	// hook isr handler for specific gpio pin
	gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);

	// remove isr handler for gpio number.
	gpio_isr_handler_remove(GPIO_INPUT_IO_0);
	// hook isr handler for specific gpio pin again
	gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
}
