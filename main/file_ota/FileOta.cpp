#include "FileOta.h"
#include <fstream>
#include "Log.h"
#include "ErrorCode.h"

#define HASH_LEN 32

FileOta::FileOta(string path, string name) : File(path, name)
{
	update_handle = 0;

	configured = esp_ota_get_boot_partition();
	running = esp_ota_get_running_partition();
	update_partition = esp_ota_get_next_update_partition(NULL);
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
	char hash_print[HASH_LEN * 2 + 1];
	hash_print[HASH_LEN * 2] = 0;
	for (int i = 0; i < HASH_LEN; ++i)
	{
		sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
	}
	LOGI("%s: %s", label, hash_print);
}

void FileOta::Close()
{
	uint8_t sha_256[HASH_LEN] = {0};
	esp_err_t err = esp_ota_end(update_handle);
	if (err != ESP_OK)
	{
		if (err == ESP_ERR_OTA_VALIDATE_FAILED)
		{
			LOGE("Image validation failed, image is corrupted");
		}
		else
		{
			LOGE("esp_ota_end failed (%s)!", esp_err_to_name(err));
		}
	}
	else
	{
		err = esp_ota_set_boot_partition(update_partition);
		if (err != ESP_OK)
		{
			LOGE("esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
		}
		LOGI("Prepare to restart system!");
	}

	esp_partition_t partition;
	partition.address = update_partition->address;
	partition.size = fileSize;
	partition.type = update_partition->type;
	esp_partition_get_sha256(&partition, sha_256);
	print_sha256(sha_256, "SHA-256 for ota: ");
}

void FileOta::OpenToWrite()
{
	LOGD("chunkIndex: %d", chunkIndex);
	esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
	if (err != ESP_OK)
	{
		LOGE("esp_ota_begin failed (%s)", esp_err_to_name(err));
		esp_ota_abort(update_handle);
		// task_fatal_error();
	}
	LOGI("esp_ota_begin succeeded, size: %d", fileSize);
}

bool FileOta::IsOpen()
{
	return true;
}

int FileOta::Write(char *buff, uint32_t size)
{
	esp_err_t err = esp_ota_write(update_handle, (const void *)buff, size);
	if (err != ESP_OK)
	{
		LOGW("Write error");
		esp_ota_abort(update_handle);
		return CODE_ERROR;
	}
	return CODE_OK;
}
