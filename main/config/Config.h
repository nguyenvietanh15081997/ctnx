#pragma once

#include <stdint.h>
#include <string.h>

#define STRING_VALUE_MAX_SIZE 128
#define CONFIG_ENV "server."
#define CONFIG_ENV_LOCAL "local."

#define HOST_KEY "host"
#define HOST_DEFAULT "mqtt.rangdong.com.vn"
#define PORT_KEY "port"
#define PORT_DEFAULT 8883
#define CLIENT_ID_KEY "client_id"
#define CLIENT_ID_DEFAULT ""
#define USERNAME_KEY "username"
#define USERNAME_DEFAULT ""
#define PASSWORD_KEY "password"
#define PASSWORD_DEFAULT ""
#define KEEP_ALIVE_KEY "keep_alive"
#define KEEP_ALIVE_DEFAULT 120

#define OTA_URL_KEY "url"
#define OTA_URL_DEFAULT ""
#define OTA_CHECKSUMS_KEY "checksum"
#define OTA_CHECKSUMS_DEFAULT ""
#define OTA_NAME_KEY "name"
#define OTA_NAME_DEFAULT ""

using namespace std;

class Config
{
private:
	// server
	string host;
	int port;
	string clientId;
	string username;
	string password;
	int keepAlive;
	bool ssl;

	// ota
	string urlOta;
	string checksumOta;
	string nameOta;

	Config();
	~Config();

public:
	static Config *getInstance();

	void ReadConfig();
	void Print();

	string GetHost();
	int GetPort();
	string GetClientId();
	string GetUsername();
	string GetPassword();
	int GetKeepAlive();
	bool GetSsl();

	string GetUrlOta();
	string GetChecksumOta();
	string GetNameOta();

	bool SetHost(string host);
	bool SetPort(int port);
	bool SetClientId(string clientId);
	bool SetUsername(string username);
	bool SetPassword(string password);
	bool SetKeepAlive(int keepAlive);
	bool SetSsl(bool ssl);

	bool SetUrlOta(string urlOta);
	bool SetCheckSumOta(string checkSumOta);
	bool SetNameOta(string nameOta);
};
