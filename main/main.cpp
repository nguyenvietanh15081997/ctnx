/*
                Minihub Rạng Đông
                TTS
*/

#include "Log.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "sqlite3.h"

#include <driver/gpio.h>
#include <driver/uart.h>

#include "Config.h"
#include "Database.h"
#include "Gateway.h"
#include "Led.h"
#include "Ota.h"
#include "Sntp.h"
#include "TimerSchedule.h"
#include "Util.h"
#include "Wifi.h"

extern "C" {
#include "IOT_Core.h"
#include "IOT_CoreEventHandler.h"
#include "IOT_Platform.h"
}
#include "LCD_GC9A01.h"
#include "encoder.h"

#include "button.h"
#include "led_mgmt.h"
#include "relay.h"

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

void *operator new(size_t size) {
  void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  if (!ptr)
    throw std::bad_alloc();
  return ptr;
}

void operator delete(void *ptr) noexcept { heap_caps_free(ptr); }

#include "esp_system.h"

static void onPlatformDeviceEvent(void *arg, const IOT_CoreEvent_t *event) {
  if (event && event->category == IOT_EVENT_DEVICE) {
    if (event->evt.device.type == DEV_EVENT_PROVISION_COMPLETED) {
      LOGI("[APP] WiFi Platform provision completed! Rebooting in 1s to enter "
           "WIFI_MODE...");
      vTaskDelay(pdMS_TO_TICKS(1000));
      esp_restart();
    }
  }
}

static iot_err_t initPlatformSDK() {
  iot_err_t err = IOT_InitPlatform();
  if (err != IOT_OK) {
    LOGE("IOT_InitPlatform failed: %s", iot_err_to_name(err));
    return err;
  }
  const char *modelId = "000001000C04001F"; // CTNX
  // const char *modelId = "000001000C04001B"; // CTCU 3 nut
  err = IOT_CoreInit(modelId);
  if (err != IOT_OK) {
    LOGE("IOT_CoreInit failed: %s", iot_err_to_name(err));
    return err;
  }
  return IOT_OK;
}

static void appButtonEventHandler(void *handler_args, esp_event_base_t base,
                                  int32_t id, void *event_data) {
  if (base == BUTTON_EVENT_BASE) {
    uint8_t btnIdx = *((uint8_t *)event_data);
    LOGI("[Button] Event %d received for Button %d", (int)id, btnIdx);

    if (btnIdx == BUTTON_INDEX_2) {
      if (id == EVENT_BUTTON_CONFIG_WIFI) {
        // Giữ 3s - 10s: Reset riêng hệ HC (xóa dormitory) & Phát AP Mode để
        // thêm HC
        LOGW("[Reset] Button 2 (3s): Resetting HC dormitory & Starting SoftAP "
             "Mode...");
        Gateway::getInstance()->setDormitory("");
        Database::getInstance()->GatewayUpdateDormitory("");
        if (!Wifi::WifiIsAPMode()) {
          Wifi::WifiStartAP();
        }
        LOGW("[Reset] Rebooting to enter DUAL_LISTEN / SoftAP Mode...");
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
      } else if (id == EVENT_BUTTON_KICK_OUT) {
        // Giữ > 10s: Factory Reset Toàn bộ (WiFi Platform SDK + HC DB + NVS)
        LOGW("[Reset] Button 2 (>10s): Factory Resetting entire device!");
        if (IOT_CoreIsProvisioned()) {
          IOT_CoreFactoryReset();
        }
        Gateway::getInstance()->setDormitory("");
        Database::getInstance()->GatewayUpdateDormitory("");
        Database::getInstance()->DelDatabase();
        nvs_flash_erase();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
      }
    }
  }
}

static void initPeripheral() {
  LedButton::getInstance()->ledButtonSetColor(0, LedState::ON, 59, 204, 161,
                                              100);
  LedButton::getInstance()->ledButtonSetColor(0, LedState::OFF, 59, 204, 161,
                                              20);
  LedButton::getInstance()->ledButtonSetColor(1, LedState::ON, 59, 204, 161,
                                              100);
  LedButton::getInstance()->ledButtonSetColor(1, LedState::OFF, 59, 204, 161,
                                              20);
  LedButton::getInstance()->ledButtonSetColor(2, LedState::ON, 59, 204, 161,
                                              100);
  LedButton::getInstance()->ledButtonSetColor(2, LedState::OFF, 59, 204, 161,
                                              20);
  LedButton::getInstance()->ledButtonSetColor(3, LedState::ON, 59, 204, 161,
                                              100);
  LedButton::getInstance()->ledButtonSetColor(3, LedState::OFF, 59, 204, 161,
                                              20);
  LedScreen::getInstance()->ledScreenSetColor(59, 204, 161);
  RelayMgmt::init();
  ButtonManager::getInstance()->init();
  ButtonManager::getInstance()->startListening();
  esp_event_handler_register(BUTTON_EVENT_BASE, ESP_EVENT_ANY_ID,
                             &appButtonEventHandler, NULL);
}

extern "C" void app_main(void) {
  vTaskDelay(pdMS_TO_TICKS(100));
  LOGI("[APP] Startup..");
  LOGI("[APP] Free memory: %d bytes", esp_get_free_heap_size());
  LOGI("[APP] IDF version: %s", esp_get_idf_version());

  esp_log_level_set("*", ESP_LOG_INFO);
  log_set_level_1(XLOG_DEBUG);

  srand(time(NULL));
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
      err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    err = nvs_flash_init();
    if (err != ESP_OK) {
      LOGE("NVS Init error %s", esp_err_to_name(err));
    }
  }

  Wifi::init();
  Sntp::init(Wifi::WaitConnecting());
  Ota::init();
  Led_init();

  Database::getInstance()->init();
  for (int i = 0; i < 5; i++) {
    SetLedInternet(true);
    SetLedService(true);
    usleep(400000);
    SetLedInternet(false);
    SetLedService(false);
    usleep(400000);
  }
  SetLedService(true);
  SetLedInternet(true);

  // config = new Config();
  // config->ReadConfig();
  // if (config->GetUrlOta() != "" && config->GetChecksumOta() != "")
  // {
  // 	config->SetUrlOta("");
  // 	config->SetCheckSumOta("");
  // 	Ota::startOta(config->GetNameOta(), config->GetUrlOta(),
  // config->GetChecksumOta());
  // }

  TimerSchedule::getInstance()->init();

  // --- Early init: đọc DB Gateway → populate dormitoryId vào RAM ---
  Gateway::getInstance()->earlyInit();

  // --- LCD + encoder (cần ở mọi mode) ---
  encoder_signal_init();
  lcd_gc9a01_init();

  // =========================================================
  // Detect app mode và khởi tạo theo mode tương ứng
  // =========================================================
  bool wifiProvisioned = IOT_CoreIsProvisioned();
  bool hcJoined = (Gateway::getInstance()->getDormitory() != "");

  // --- Khởi tạo peripheral dùng chung cho tất cả các mode ---
  initPeripheral();

  if (wifiProvisioned) {
    // ----------------------------------------------------------
    // WIFI_MODE: Thiết bị đã join WiFi platform
    // Chỉ chạy Platform SDK, không khởi động HC/BLE Mesh
    // ----------------------------------------------------------
    LOGI("[APP] Mode: WIFI_MODE (provisioned to WiFi platform)");

    if (initPlatformSDK() == IOT_OK) {
      IOT_DevCoreRegisterEventCb(IOT_EVENT_DEVICE, onPlatformDeviceEvent, NULL);
    }
  } else if (hcJoined) {
    // ----------------------------------------------------------
    // HC_MODE: Thiết bị đã join hệ HC BLE Mesh
    // Chạy đầy đủ: Gateway, BleProtocol, ZigbeeProtocol, ...
    // Không chạy Platform SDK
    // ----------------------------------------------------------
    LOGI("[APP] Mode: HC_MODE (joined HC dormitory: %s)",
         Gateway::getInstance()->getDormitory().c_str());

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
  } else {
    // ----------------------------------------------------------
    // DUAL_LISTEN: Chưa join hệ nào
    // - Platform SDK: BLE provisioning tự start trong IOT_CoreInit()
    // - HC side: chỉ mở UDP để app HC scan tìm thấy gateway
    // - Sau khi join xong → esp_restart() để re-detect mode
    // ----------------------------------------------------------
    LOGI("[APP] Mode: DUAL_LISTEN (waiting to join WiFi platform or HC)");

    if (initPlatformSDK() == IOT_OK) {
      IOT_DevCoreRegisterEventCb(IOT_EVENT_DEVICE, onPlatformDeviceEvent, NULL);
    }

    // HC side: mở UDP scan (không kết nối MQTT, không load toàn bộ DB)
    Gateway::getInstance()->startUdpListen();
  }
}
