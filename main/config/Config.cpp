#include <string>
#include <iostream>
#include <endian.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "nvs_handle.hpp"

#include "Config.h"
#include "Log.h"

// #define TAG "Config"
Config *Config::getInstance()
{
	static Config *config = NULL;
	if (!config)
	{
		config = new Config();
	}
	return config;
}

/****************************************
 *                  API                 *
 ***************************************/

static bool set_str_config_entry(const char *name, const char *section_name, const char *value)
{
	esp_err_t err;
	std::shared_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("nvs", NVS_READWRITE, &err);
	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		if (handle->set_string(name, value) == ESP_OK)
			if (handle->commit() == ESP_OK)
				return true;
	}
	return false;
}

static bool set_int_config_entry(const char *section, const char *name, int value)
{
	esp_err_t err;
	std::shared_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("nvs", NVS_READWRITE, &err);
	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		if (handle->set_item(name, value) == ESP_OK)
			if (handle->commit() == ESP_OK)
				return true;
	}
	return false;
}

Config::Config()
{
	host = HOST_DEFAULT;
	port = PORT_DEFAULT;
	clientId = CLIENT_ID_DEFAULT;
	username = USERNAME_DEFAULT;
	password = PASSWORD_DEFAULT;
	keepAlive = KEEP_ALIVE_DEFAULT;
	ssl = true;

	urlOta = OTA_URL_DEFAULT;
	checksumOta = OTA_CHECKSUMS_DEFAULT;
	nameOta = OTA_NAME_DEFAULT;
}

Config::~Config()
{
	LOGW("Delete config object");
}

void Config::ReadConfig()
{
	char str_temp[STRING_VALUE_MAX_SIZE];
	int int_temp = 0;
	printf("Opening Non-Volatile Storage (NVS) handle... ");
	esp_err_t err;
	std::shared_ptr<nvs::NVSHandle> handle = nvs::open_nvs_handle("nvs", NVS_READWRITE, &err);
	if (err != ESP_OK)
	{
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}
	else
	{
		if (handle->get_string(HOST_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			host = string(str_temp);
		if (handle->get_item(PORT_KEY, int_temp) == ESP_OK)
			port = int_temp;
		if (handle->get_string(CLIENT_ID_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			clientId = string(str_temp);
		if (handle->get_string(USERNAME_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			username = string(str_temp);
		if (handle->get_string(PASSWORD_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			password = string(str_temp);
		if (handle->get_item(KEEP_ALIVE_KEY, int_temp) == ESP_OK)
			keepAlive = int_temp;


		if (handle->get_string(OTA_URL_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			urlOta = string(str_temp);
		if (handle->get_string(OTA_CHECKSUMS_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			checksumOta = string(str_temp);
		if (handle->get_string(OTA_NAME_KEY, str_temp, sizeof(str_temp)) == ESP_OK)
			nameOta = string(str_temp);
	}
	Print();
}

void Config::Print()
{
	LOGI("host: %s", host.c_str());
	LOGI("port: %d", port);
	LOGI("clientId: %s", clientId.c_str());
	LOGI("username: %s", username.c_str());
	LOGI("password: %s", password.c_str());
	LOGI("keepAlive: %d", keepAlive);

	LOGI("URL : %s", urlOta.c_str());
	LOGI("Checksum: %s", checksumOta.c_str());
	LOGI("Name: %s", nameOta.c_str());
}

// Get info server
string Config::GetHost()
{
	return host;
}

int Config::GetPort()
{
	return port;
}

string Config::GetClientId()
{
	return clientId;
}

string Config::GetUsername()
{
	return username;
}

string Config::GetPassword()
{
	return password;
}

int Config::GetKeepAlive()
{
	return keepAlive;
}

bool Config::GetSsl()
{
	return false;
}

string Config::GetUrlOta()
{
	return urlOta;
}

string Config::GetChecksumOta()
{
	return checksumOta;
}

string Config::GetNameOta()
{
	return nameOta;
}

bool Config::SetHost(string host)
{
	if (set_str_config_entry(HOST_KEY, HOST_KEY, host.c_str()))
	{
		return true;
	}
	return false;
}

bool Config::SetPort(int port)
{
	if (set_int_config_entry(PORT_KEY, PORT_KEY, port))
	{
		return true;
	}
	return false;
}

bool Config::SetClientId(string clientId)
{
	if (set_str_config_entry(CLIENT_ID_KEY, CLIENT_ID_KEY, clientId.c_str()))
	{
		return true;
	}
	return false;
}

bool Config::SetUsername(string username)
{
	if (set_str_config_entry(USERNAME_KEY, USERNAME_KEY, username.c_str()))
	{
		return true;
	}
	return false;
}

bool Config::SetPassword(string password)
{
	if (set_str_config_entry(PASSWORD_KEY, PASSWORD_KEY, password.c_str()))
	{
		return true;
	}
	return false;
}

bool Config::SetKeepAlive(int keepAlive)
{
	return true;
}

bool Config::SetSsl(bool ssl)
{
	return true;
}

bool Config::SetUrlOta(string urlOta)
{
	if (set_str_config_entry(OTA_URL_KEY, OTA_URL_KEY, urlOta.c_str()))
	{
		return true;
	}
	return false;
}

bool Config::SetCheckSumOta(string checkSumOta)
{
	if (set_str_config_entry(OTA_CHECKSUMS_KEY, OTA_CHECKSUMS_KEY, checkSumOta.c_str()))
	{
		return true;
	}
	return false;
}

bool Config::SetNameOta(string nameOta)
{
	if (set_str_config_entry(OTA_NAME_KEY, OTA_NAME_KEY, nameOta.c_str()))
	{
		return true;
	}
	return false;
}