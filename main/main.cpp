/*
		Minihub Rạng Đông
		TTS
*/

#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sqlite3.h"
#include "Log.h"

#include <driver/uart.h>
#include <driver/gpio.h>

#include "Util.h"
#include "Wifi.h"
#include "Sntp.h"
#include "Ota.h"
#include "Config.h"
#include "Gateway.h"
#include "TimerSchedule.h"
#include "Database.h"
#include "Led.h"

#include "LCD_GC9A01.h"
#include "encoder.h"

#ifdef CONFIG_ENABLE_BLE
#include "BleProtocol.h"
#endif

#ifdef CONFIG_ENABLE_MQTT
#include "MqttProtocol.h"
#endif

#ifdef CONFIG_ENABLE_ZIGBEE
#include "ZigbeeProtocol.h"
#endif

#ifdef CONFIG_ENABLE_BLE_FAST_SCAN
#include "AndroidBleProtocol.h"
#endif

#ifdef CONFIG_ENABLE_MODBUS
#include "ModbusProtocol.h"
#endif

extern void gpio_init(void);


#include "esp_heap_caps.h"

void *operator new(size_t size)
{
	void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
	if (!ptr)
		throw std::bad_alloc();
	return ptr;
}

void operator delete(void *ptr) noexcept
{
	heap_caps_free(ptr);
}

extern "C" void app_main(void)
{
	vTaskDelay(pdMS_TO_TICKS(100));
	LOGI("[APP] Startup..");
	LOGI("[APP] Free memory: %d bytes", esp_get_free_heap_size());
	LOGI("[APP] IDF version: %s", esp_get_idf_version());

	esp_log_level_set("*", ESP_LOG_INFO);
	log_set_level(XLOG_DEBUG);

	srand(time(NULL));
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
		if (err != ESP_OK)
		{
			LOGE("NVS Init error %s", esp_err_to_name(err));
		}
	}
				
	Wifi::init();
	Sntp::init(Wifi::WaitConnecting());
	Ota::init();
	Led_init();

	Database::getInstance()->init();
	for (int i = 0; i < 5; i++)
	{
		SetLedInternet(true);
		SetLedService(true);
		usleep(400000);
		SetLedInternet(false);
		SetLedService(false);
		usleep(400000);
	}
	SetLedService(true);
	SetLedInternet(true);

	// 	config = new Config();
	// 	config->ReadConfig();
	// 	if (config->GetUrlOta() != "" && config->GetChecksumOta() != "")
	// 	{
	// 		config->SetUrlOta("");
	// 		config->SetCheckSumOta("");
	// 		Ota::startOta(config->GetNameOta(), config->GetUrlOta(), config->GetChecksumOta());
	// 	}

	TimerSchedule::getInstance()->init();
    
    // Step 1: Initialize platform (NVS, WiFi, BLE, MQTT, etc.)
    // err = IOT_InitPlatform();
    // if (err != IOT_OK)
    // {
    //     LOGE("IOT_InitPlatform failed: %s", iot_err_to_name(err));
    //     return;
    // }
    // uint8_t modelId[8] = {0x00, 0x00, 0x01, 0x00, 0x0C, 0x04, 0x00, 0x1F}; // CTNX
    // // uint8_t modelId[8] = {0x00, 0x00, 0x01, 0x00, 0x0C, 0x04, 0x00, 0x1B}; // CTCU 3 nut

    // err = IOT_CoreInit(modelId);
    // if (err != IOT_OK)
    // {
    //     LOGE("IOT_CoreInit failed: %s", iot_err_to_name(err));
    //     return;
    // }
    // // Step 3: Register event callbacks
    // err = devRegisterEvent();
    // if (err != IOT_OK)
    // {
    //     LOGE("Failed to register event callback");
    // }   

    encoder_signal_init();
    lcd_gc9a01_init();

	Gateway::getInstance()->init();

#ifdef CONFIG_ENABLE_BLE
	BleProtocol::getInstance()->init();
	BleProtocol::getInstance()->InitKey();
#endif

#ifdef CONFIG_ENABLE_ZIGBEE
	ZigbeeProtocol::getInstance()->init();
#endif

#ifdef CONFIG_ENABLE_MQTT
	mqttProtocol = new MqttProtocol();
	mqttProtocol->init();
#endif

#ifdef CONFIG_ENABLE_BLE_FAST_SCAN
	androidBleProtocol = new AndroidBleProtocol();
	androidBleProtocol->init();
#endif

#ifdef CONFIG_ENABLE_MODBUS
	ModbusProtocol::getInstance()->init();
#endif
}
