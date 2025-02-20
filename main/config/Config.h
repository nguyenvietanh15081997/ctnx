#pragma once

#include <stdint.h>
#include <string.h>

#define STRING_VALUE_MAX_SIZE 128
#define CONFIG_ENV "server."
#define CONFIG_ENV_LOCAL "local."

#define HOST_KEY "host"
#define HOST_LC_KEY "host_local"
#define HOST_DEFAULT "mqtt.rangdong.com.vn"
#define PORT_KEY "port"
#define PORT_LC_KEY "port_local"
#define PORT_DEFAULT 1884
#define CLIENT_ID_KEY "client_id"
#define CLIENT_ID_LC_KEY "client_id_local"
#define CLIENT_ID_DEFAULT ""
#define USERNAME_KEY "username"
#define USERNAME_LC_KEY "username_local"
#define USERNAME_DEFAULT ""
#define PASSWORD_KEY "password"
#define PASSWORD_LC_KEY "password_local"
#define PASSWORD_DEFAULT ""
#define KEEP_ALIVE_KEY "keep_alive"
#define KEEP_ALIVE_LC_KEY "keep_alive_local"
#define KEEP_ALIVE_DEFAULT 10

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

	// local
	string localHost;
	int localPort;
	string localClientId;
	string localUsername;
	string localPassword;
	int localKeepAlive;

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

	string GetLocalHost();
	int GetLocalPort();
	string GetLocalClientId();
	string GetLocalUsername();
	string GetLocalPassword();
	int GetLocalKeepAlive();
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

	bool SetLocalHost(string localHost);
	bool SetLocalPort(int localPort);
	bool SetLocalClientId(string localClientId);
	bool SetLocalUsername(string localUsername);
	bool SetLocalPassword(string localPassword);
	bool SetLocalKeepAlive(int keepAlive);
	bool SetSsl(bool ssl);

	bool SetUrlOta(string urlOta);
	bool SetCheckSumOta(string checkSumOta);
	bool SetNameOta(string nameOta);
};
