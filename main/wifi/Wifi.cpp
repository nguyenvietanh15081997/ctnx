#include "Wifi.h"
#include <string.h>
#include "Log.h"
#include "Util.h"
#include "Led.h"

#include "esp_mac.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/inet.h"

static esp_netif_t *ap = NULL;
static esp_netif_t *sta = NULL;

static wifi_mode_t wifiMode = WIFI_MODE_NULL;
static bool isWaitConnect = false;

int startAPTimeCount = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT)
	{
		if (event_id == WIFI_EVENT_STA_START)
		{
			LOGD("WIFI_EVENT_STA_START");
			esp_wifi_connect();
		}
		else if (event_id == WIFI_EVENT_STA_STOP)
		{
			LOGD("WIFI_EVENT_STA_STOP");
		}
		else if (event_id == WIFI_EVENT_STA_CONNECTED)
		{
			LOGD("WIFI_EVENT_STA_CONNECTED");
		}
		else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
		{
			LOGD("WIFI_EVENT_STA_DISCONNECTED");
			if (wifiMode == WIFI_MODE_STA)
			{
				esp_wifi_connect();
			}
			// xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
		}
		else if (event_id == WIFI_EVENT_AP_START)
		{
			LOGD("WIFI_EVENT_AP_START");
		}
		else if (event_id == WIFI_EVENT_AP_STOP)
		{
			LOGD("WIFI_EVENT_AP_STOP");
		}
		else if (event_id == WIFI_EVENT_AP_STACONNECTED)
		{
			LOGD("WIFI_EVENT_AP_STACONNECTED");
			wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
			LOGI("station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
		}
		else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
		{
			LOGD("WIFI_EVENT_AP_STADISCONNECTED");
			wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
			LOGI("station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
		}
		else
		{
			LOGI("WIFI_EVENT not handle: %d", event_id);
		}
	}
	else if (event_base == IP_EVENT)
	{
		if (event_id == IP_EVENT_STA_GOT_IP)
		{
			LOGD("IP_EVENT_STA_GOT_IP");
			// xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
		}
		else
		{
			LOGI("IP_EVENT not handle: %d", event_id);
		}
	}
	else
	{
		LOGI("event_base not handle: %s", (char *)event_base);
	}
}

void Wifi::init()
{
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	sta = esp_netif_create_default_wifi_sta();
	ap = esp_netif_create_default_wifi_ap();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
#ifndef CONFIG_BLE_MESH
	esp_wifi_set_ps(WIFI_PS_NONE);
#endif

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

	esp_netif_ip_info_t ipInfo;
	memset(&ipInfo, 0, sizeof(esp_netif_ip_info_t));
	inet_aton("10.10.10.1", &ipInfo.ip.addr);
	inet_aton("10.10.10.1", &ipInfo.gw.addr);
	inet_aton("255.255.255.0", &ipInfo.netmask.addr);
	esp_netif_dhcps_stop(ap);
	esp_netif_set_ip_info(ap, &ipInfo);
	esp_netif_dhcps_start(ap);

	wifi_config_t wifi_config;
	esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
	if (ret == ESP_OK)
	{
		LOGI("Wifi configuration already stored in flash partition called NVS");
		LOGI("%s", wifi_config.sta.ssid);
		LOGI("%s", wifi_config.sta.password);
		wifiMode = WIFI_MODE_STA;
		ESP_ERROR_CHECK(esp_wifi_set_mode(wifiMode));
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
		ESP_ERROR_CHECK(esp_wifi_start());
		if (wifi_config.sta.ssid[0])
			isWaitConnect = true;
		else
			isWaitConnect = false;
	}
	else
	{
		isWaitConnect = false;
		LOGI("Wifi configuration not found in flash partition called NVS.");
	}
}

bool Wifi::WaitConnecting()
{
	return isWaitConnect;
}

#define DEFAULT_SCAN_LIST_SIZE 30
void Wifi::ScanWifi(Json::Value &data)
{
	uint16_t number = DEFAULT_SCAN_LIST_SIZE;
	wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
	uint16_t ap_count = 0;
	memset(ap_info, 0, sizeof(ap_info));

	// esp_wifi_disconnect();
	esp_wifi_scan_start(NULL, true);
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
	ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
	LOGI("Total APs scanned = %u", ap_count);
	data = Json::arrayValue;
	for (int i = 0; i < number; i++)
	{
		LOGI("SSID \t\t%s", ap_info[i].ssid);
		LOGI("RSSI \t\t%d", ap_info[i].rssi);

		if (ap_info[i].rssi >= -100)
		{
			Json::Value wifiValue;
			wifiValue["SSID"] = string((char *)ap_info[i].ssid);
			wifiValue["QUALITY"] = ap_info[i].rssi;
			wifiValue["MAC"] = Util::ConvertU32ToHexString(ap_info[i].bssid, 6);
			wifiValue["ENCRYPTION"] = to_string(ap_info[i].authmode);
			data.append(wifiValue);
		}
	}
}

int Wifi::ConnectToWifi(string ssid, string password, string encryption)
{
	LOGD("Connect to Wifi");
	LOGD("SSID: %s, password: %s, encry: %s", ssid.c_str(), password.c_str(), encryption.c_str());
	if (ssid.length() > 32 || password.length() > 64)
	{
		LOGW("Wifi info too long");
		return -1;
	}

	wifi_auth_mode_t wifi_auth_mode = (wifi_auth_mode_t)stoi(encryption);
	startAPTimeCount = 0;
	wifi_config_t wifi_config = {.sta = {}};
	strcpy((char *)wifi_config.sta.ssid, ssid.c_str());

	if (encryption == "0")
	{
		strcpy((char *)wifi_config.sta.password, "");
		wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
	}
	else
	{
		strcpy((char *)wifi_config.sta.password, password.c_str());
		wifi_config.sta.threshold.authmode = wifi_auth_mode;
	}

	if (wifiMode != WIFI_MODE_STA)
	{
		LOGD("esp_wifi_stop");
		ESP_ERROR_CHECK(esp_wifi_stop());
		wifiMode = WIFI_MODE_STA;
	}

	ESP_ERROR_CHECK(esp_wifi_set_mode(wifiMode));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());
	return 0;
}

void Wifi::WifiStartAP(void)
{
	LOGD("WifiStartAP");
	// Led::TaskLedInternet(MODE_FLASH);
	wifi_config_t ap_wifi_config = {
		.ap = {
			// .ssid = "RD_HC_aabb",
			.ssid_len = 10,
			.channel = 1,
			// .password = "",
			.authmode = WIFI_AUTH_WPA2_PSK,
			.max_connection = 2},
	};
	string mac = Wifi::GetMacAddress();
	sprintf((char *)ap_wifi_config.ap.ssid, "RD_MH_%s", mac.substr(mac.size() - 4, 4).c_str());
	for (auto &c : ap_wifi_config.ap.ssid)
		c = toupper(c);
	sprintf((char *)ap_wifi_config.ap.password, "ABC123456");
	if (wifiMode != WIFI_MODE_APSTA)
	{
		LOGD("esp_wifi_stop");
		esp_wifi_disconnect();
		ESP_ERROR_CHECK(esp_wifi_stop());
		wifiMode = WIFI_MODE_APSTA;
	}

	wifi_config_t sta_wifi_config = {.sta = {}};

	ESP_ERROR_CHECK(esp_wifi_set_mode(wifiMode));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_wifi_config));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	LOGI("WifiStartAP finished");
}

void Wifi::WifiStartSta(void)
{
	wifi_config_t wifi_config;
	LOGD("WifiStartSta");
	if (wifiMode != WIFI_MODE_STA)
	{
		LOGD("esp_wifi_stop");
		ESP_ERROR_CHECK(esp_wifi_stop());
		wifiMode = WIFI_MODE_STA;
	}
	esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
	if (ret == ESP_OK)
	{
		LOGI("%s", wifi_config.sta.ssid);
		LOGI("%s", wifi_config.sta.password);
		ESP_ERROR_CHECK(esp_wifi_set_mode(wifiMode));
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
		ESP_ERROR_CHECK(esp_wifi_start());
	}
}

bool Wifi::WifiIsAPMode(void)
{
	return wifiMode == WIFI_MODE_APSTA;
}

bool Wifi::WifiIsStaMode(void)
{
	return wifiMode == WIFI_MODE_AP;
}

string Wifi::GetMacAddress()
{
	LOGD("GetMacAddress");
	uint8_t mac[6] = {0};
	ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
	char uc_Mac[100];
	sprintf((char *)uc_Mac, (const char *)"%.2x%.2x%.2x%.2x%.2x%.2x",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return string(uc_Mac);
}

string Wifi::GetMacAddressHasDot()
{
	LOGD("GetMacAddress");
	uint8_t mac[6] = {0};
	ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));
	char uc_Mac[100];
	sprintf((char *)uc_Mac, (const char *)"%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return string(uc_Mac);
}

string Wifi::GetIP()
{
	// LOGD("GetIP");
	char ip[32] = "10.10.10.1";
	esp_netif_t *netif = NULL;
	esp_netif_ip_info_t ip_info;
	for (int i = 0; i < esp_netif_get_nr_of_ifs(); ++i)
	{
		netif = esp_netif_next(netif);
		ESP_ERROR_CHECK(esp_netif_get_ip_info(netif, &ip_info));
		if (ip_info.ip.addr != 0 && ip_info.ip.addr != 0x010A0A0A) // 10.10.10.1
		{
			sprintf(ip, IPSTR, IP2STR(&ip_info.ip));
			break;
		}
	}
	LOGI("ip: %s", ip);
	return string(ip);
}
