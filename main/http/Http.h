#pragma once

#include <iostream>
#include <string.h>

#define RENEW_TOKEN "/rpc/iot-ebe/account/renew-token"
#define HC_BACKUP_FILE_URL "/rpc/iot-ebe/home-controller/upload-file"
#define HC_CREATE_BACKUP "/rpc/iot-ebe/home-controller/create-backup"

#define POST "POST"

using namespace std;

#define my_sizeof(type) ((char *)(&type + 1) - (char *)(&type))
#define HEADER_SIZE 6

class HTTPRequest
{
private:
	string method;
	string url;
	string body;
	string token;

public:
	HTTPRequest();
	string GetToken(string refreshToken, string dormitory);
	string UploadFile(string refreshToken, string dormitory, string pathFile);
	bool CreateBackup(string refreshToken, string dormitory, string mac, string version, string size, string path, string hcId);
	string DownloadFile(string dormitory);
	int setMethod(string method);
	int setUrl(string url);
	int setToken(string token);
	string GetWeather(float latitude, float longitude);
	string GetWeather(string latitude, string longitude);
};
