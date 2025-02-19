#pragma once

#include <string>
#include <fstream>
#include "File.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

using namespace std;

class FileOta : public File
{
private:
	const esp_partition_t *configured;
	const esp_partition_t *running;
	const esp_partition_t *update_partition;
	esp_ota_handle_t update_handle;

public:
	FileOta(string path, string name);

	void Close();
	void OpenToWrite();
	bool IsOpen();

	int Write(char *buff, uint32_t size);
};
