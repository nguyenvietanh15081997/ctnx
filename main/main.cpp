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
#include "ButtonSignal.h"
#include "TimerSchedule.h"
#include "Database.h"
#include "Led.h"

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

	Led_init();
	Wifi::init();
	Sntp::init(Wifi::WaitConnecting());
	Ota::init();

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

	ButtonSignal::getInstance()->init();
	gpio_init();

	TimerSchedule::getInstance()->init();
	// 	fileTransfer = new FileTransfer();
	// 	fileTransfer->init();

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
