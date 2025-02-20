/* 
		Minihub Rạng Đông
		TTS
*/

#include "nvs_flash.h"

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

// 	buttonSignal = new ButtonSignal();
// 	buttonSignal->init();
// 	gpio_init();

// 	timerSchedule = new TimerSchedule();
// 	timerSchedule->init();
// 	fileTransfer = new FileTransfer();
// 	fileTransfer->init();

// #ifdef CONFIG_BLE_MESH
// 	bleProtocol = new BleProtocol();
// #else
// 	bleProtocol = new BleProtocol(UART_NUM_1, GPIO_NUM_23, GPIO_NUM_22, 115200);
// #endif
// 	bleProtocol->init();

// 	string mac = Wifi::GetMacAddressHasDot();
// 	LOGI("mac: %s", mac.c_str());
// 	gateway = new Gateway(mac, config->GetHost(), config->GetPort(), config->GetClientId(), config->GetUsername(), config->GetPassword(), config->GetKeepAlive());
// 	gateway->init();

// 	bleProtocol->InitKey();
}
