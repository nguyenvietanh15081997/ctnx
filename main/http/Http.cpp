#include "Http.h"
#include "Util.h"
#include "Log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "esp_http_client.h"
#include "esp_littlefs.h"

#define WEATHER_API_KEY "ebd13e00acf60358e311499f1701ffc2"

using namespace std;

HTTPRequest::HTTPRequest()
{
}

int HTTPRequest::setMethod(string method)
{
	this->method = method;
	return 0;
}

int HTTPRequest::setUrl(string url)
{
	this->url = url;
	return 0;
}

int HTTPRequest::setToken(string token)
{
	this->token = token;
	return 0;
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
	switch (evt->event_id)
	{
	case HTTP_EVENT_ERROR:
		LOGD("HTTP_EVENT_ERROR");
		break;
	case HTTP_EVENT_ON_CONNECTED:
		LOGD("HTTP_EVENT_ON_CONNECTED");
		break;
	case HTTP_EVENT_HEADER_SENT:
		LOGD("HTTP_EVENT_HEADER_SENT");
		break;
	case HTTP_EVENT_ON_HEADER:
		LOGD("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
		break;
	case HTTP_EVENT_ON_DATA:
		LOGD("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
		if (!esp_http_client_is_chunked_response(evt->client))
		{
			if (evt->user_data)
			{
				string *str = (string *)evt->user_data;
				*str += string((char *)evt->data, evt->data_len);
			}
		}
		else
		{
			if (evt->user_data)
			{
				string *str = (string *)evt->user_data;
				*str += string((char *)evt->data, evt->data_len);
			}
		}
		break;
	case HTTP_EVENT_ON_FINISH:
		LOGD("HTTP_EVENT_ON_FINISH");
		break;
	case HTTP_EVENT_DISCONNECTED:
		LOGI("HTTP_EVENT_DISCONNECTED");
		break;
	case HTTP_EVENT_REDIRECT:
		LOGI("HTTP_EVENT_REDIRECT");
		break;
	}
	return ESP_OK;
}

string HTTPRequest::GetToken(string refreshToken, string dormitory)
{
	string ref = "RefreshToken=" + refreshToken;
	char url_data[200];
	strcpy(url_data, url.c_str());
	LOGD("url: %s", url_data);
	string strRsp = "";
	esp_http_client_config_t config = {
		.url = url_data,
		.method = HTTP_METHOD_POST,
		.event_handler = _http_event_handler,
		.buffer_size_tx = 1024,
		.user_data = &strRsp};
	config.skip_cert_common_name_check = true;
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_header(client, "X-DormitoryId", dormitory.c_str());
	esp_http_client_set_header(client, "Cookie", ref.c_str());
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK)
	{
		LOGI("HTTP POST Status = %d, content_length = %d",
			 esp_http_client_get_status_code(client),
			 esp_http_client_get_content_length(client));

		int status_code = esp_http_client_get_status_code(client);
		if (status_code != 200)
		{
			LOGI("Message sent Failed");
		}
	}
	else
	{
		LOGI("Message sent Failed");
	}
	esp_http_client_cleanup(client);

	Json::Value dataJson;
	if (dataJson.parse(strRsp) && dataJson.isObject())
	{
		if (dataJson.isMember("token") && dataJson["token"].isString())
		{
			return dataJson["token"].asString();
		}
	}
	return strRsp;
}

#define BOUNDARY "----WebKitFormBoundary7MA4YWxkTrZu0gW"
string HTTPRequest::UploadFile(string refreshToken, string dormitory, string pathFile)
{
	string tokenData = "Token=" + token;
	int lenData = pathFile.length();
	char url_data[200];
	strcpy(url_data, url.c_str());
	LOGD("url: %s, token: %s", url_data, tokenData.c_str());
	string strRsp = "";
	esp_http_client_config_t config = {
		.url = url_data,
		.method = HTTP_METHOD_POST,
		.event_handler = _http_event_handler,
		.buffer_size_tx = 1024,
		.user_data = &strRsp};
	config.skip_cert_common_name_check = true;
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_header(client, "X-DormitoryId", dormitory.c_str());
	esp_http_client_set_header(client, "Cookie", tokenData.c_str());

	FILE *file = fopen(pathFile.c_str(), "r");
	if (file == NULL)
	{
		printf("Failed to open file for reading\n");
		esp_http_client_cleanup(client);
		return "";
	}
	fseek(file, 0, SEEK_END);
	size_t file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	printf("Size file: %ld\n", file_size);

	// Set headers
	char content_type[128];
	sprintf(content_type, "multipart/form-data; boundary=%s", BOUNDARY);
	esp_http_client_set_header(client, "Content-Type", content_type);

	// Construct the multipart form data body
	char *part1 = "--" BOUNDARY "\r\n"
				  "Content-Disposition: form-data; name=\"file\"; filename=\"smh2.sqlite\"\r\n"
				  "Content-Type: application/octet-stream\r\n\r\n";

	char *part2 = "\r\n--" BOUNDARY "--\r\n";

	size_t body_len = strlen(part1) + file_size + strlen(part2);

	esp_err_t err = esp_http_client_open(client, body_len);
	if (err != ESP_OK)
	{
		LOGE("Failed to open HTTP connection: %s", esp_err_to_name(err));
		fclose(file);
		esp_http_client_cleanup(client);
		return "";
	}



	int wlen = esp_http_client_write(client, part1, strlen(part1));
	if (wlen < 0)
	{
		LOGE("Write part1 failed");
		esp_http_client_cleanup(client);
		fclose(file);
		return "";
	}

	char buffer[1024];
	size_t chunk_size;
	while ((chunk_size = fread(buffer, 1, sizeof(buffer), file)) > 0)
	{
		wlen = esp_http_client_write(client, buffer, chunk_size);
		if (wlen < 0)
		{
			LOGE("Write file content failed");
			esp_http_client_cleanup(client);
			fclose(file);
			return "";
		}
	}
	
    wlen = esp_http_client_write(client, part2, strlen(part2));
    if (wlen < 0) {
        LOGE("Write part2 failed");
        esp_http_client_cleanup(client);
        fclose(file);
        return "";
    }

	err = esp_http_client_perform(client);
	if (err == ESP_OK)
	{
		int status = esp_http_client_get_status_code(client);
		int content_length = esp_http_client_get_content_length(client);
		LOGI("HTTP POST Status = %d, content_length = %d", status, content_length);
	}
	else
	{
		LOGE("HTTP POST request failed: %s", esp_err_to_name(err));
	}

	fclose(file);
	esp_http_client_cleanup(client);

	return strRsp;
}

bool HTTPRequest::CreateBackup(string refreshToken, string dormitory, string mac, string version, string size, string path, string hcId)
{
	string tokenData = "Token=" + token;
	char url_data[200];
	strcpy(url_data, url.c_str());
	LOGD("url: %s", url_data);
	string strRsp = "";
	esp_http_client_config_t config = {
		.url = url_data,
		.method = HTTP_METHOD_POST,
		.event_handler = _http_event_handler,
		.buffer_size_tx = 1024,
		.user_data = &strRsp};
	config.skip_cert_common_name_check = true;
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_header(client, "X-DormitoryId", dormitory.c_str());
	esp_http_client_set_header(client, "Cookie", tokenData.c_str());
	esp_http_client_set_header(client, "Content-Type", "application/json");

	Json::Value dataJson;
	dataJson["name"] = "minihub-" + mac;
	dataJson["version"] = version;
	dataJson["size"] = size;
	dataJson["url"] = path;
	dataJson["homeControllerId"] = hcId;

	string dataJsonStr = dataJson.toStyledString();
	LOGI("%s", dataJsonStr.c_str());
	int lenData = dataJsonStr.length();

	esp_http_client_set_post_field(client, dataJsonStr.c_str(), lenData);
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK)
	{
		int status_code = esp_http_client_get_status_code(client);
		if (status_code != 200)
		{
			LOGI("Message sent Failed");
		}
		esp_http_client_cleanup(client);
		return true;
	}
	else
	{
		LOGI("Message sent Failed");
	}
	esp_http_client_cleanup(client);
	return false;
}

string HTTPRequest::DownloadFile(string dormitory)
{
	string tokenData = "Token=" + token;
	char url_data[200];
	strcpy(url_data, url.c_str());
	LOGD("url: %s", url_data);
	string strRsp = "";
	esp_http_client_config_t config = {
		.url = url_data,
		.method = HTTP_METHOD_POST,
		.event_handler = _http_event_handler,
		.buffer_size_tx = 1024,
		.user_data = &strRsp};
	config.skip_cert_common_name_check = true;
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_http_client_set_header(client, "Cookie", tokenData.c_str());
	esp_http_client_set_header(client, "X-DormitoryId", dormitory.c_str());
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK)
	{
		int status_code = esp_http_client_get_status_code(client);
		if (status_code != 200)
		{
			LOGI("Message sent Failed");
		}
	}
	else
	{
		LOGI("Message sent Failed");
	}
	esp_http_client_cleanup(client);
	return strRsp;
}

string HTTPRequest::GetWeather(float latitude, float longitude)
{
	return GetWeather(to_string(latitude), to_string(longitude));
}

string HTTPRequest::GetWeather(string latitude, string longitude)
{
	string strRsp = "";
	char url[200];
	snprintf(url,
			 sizeof(url),
			 "http://api.openweathermap.org/data/2.5/weather?lat=%s&lon=%s&appid=" WEATHER_API_KEY "&units=metric",
			 latitude.c_str(),
			 longitude.c_str());
	LOGD("url: %s", url);

	esp_http_client_config_t config = {
		.url = url,
		.method = HTTP_METHOD_GET,
		.event_handler = _http_event_handler,
		.user_data = &strRsp};
	esp_http_client_handle_t client = esp_http_client_init(&config);
	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK)
	{
		int status_code = esp_http_client_get_status_code(client);
		if (status_code != 200)
		{
			LOGI("Message sent Failed");
		}
	}
	else
	{
		LOGI("Message sent Failed");
	}
	esp_http_client_cleanup(client);
	return strRsp;
}
