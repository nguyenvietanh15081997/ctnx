
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_app_format.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "errno.h"
#include "Log.h"

#define BUFFSIZE 1024
#define HASH_LEN 32 /* SHA-256 digest length */
#define CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK 1

typedef struct
{
	char name[32];
	char url[256];
	char sum[128];
	uint32_t timeout;
} ota_info_t;

// static ota_info_t *ota_info = NULL;
static ota_info_t *ota_info = NULL;

/*an ota data write buffer ready to write to the flash*/
// static char ota_write_data[BUFFSIZE + 1] = {0};

// extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
// extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

static void http_cleanup(esp_http_client_handle_t client)
{
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

static void __attribute__((noreturn)) task_fatal_error(void)
{
	LOGE("Exiting task due to fatal error...");
	// (void)vTaskDelete(NULL);
	esp_restart();
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

// static void infinite_loop(void)
// {
// 	int i = 0;
// 	LOGI("When a new firmware is available on the server, press the reset button to download it");
// 	while (1)
// 	{
// 		LOGI("Waiting for a new firmware ... %d", ++i);
// 		vTaskDelay(2000 / portTICK_PERIOD_MS);
// 	}
// }

static void ota_example_task()
{
	esp_err_t err;
	/* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
	esp_ota_handle_t update_handle = 0;
	const esp_partition_t *update_partition = NULL;

	LOGI("Starting OTA example: %s", ota_info->url);
	char *ota_write_data = heap_caps_malloc_prefer((BUFFSIZE + 1) * 2, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);

	const esp_partition_t *configured = esp_ota_get_boot_partition();
	const esp_partition_t *running = esp_ota_get_running_partition();

	if (configured != running)
	{
		LOGW("Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
			 configured->address, running->address);
		LOGW("(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
	}
	LOGI("Running partition type %d subtype %d (offset 0x%08x)",
		 running->type, running->subtype, running->address);

	esp_http_client_config_t config = {
		.url = ota_info->url,
		// .cert_pem = (char *)server_cert_pem_start,
		.timeout_ms = ota_info->timeout,
		.keep_alive_enable = true,
	};

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
	config.skip_cert_common_name_check = true;
#endif

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL)
	{
		LOGE("Failed to initialise HTTP connection");
		task_fatal_error();
	}
	err = esp_http_client_open(client, 0);
	if (err != ESP_OK)
	{
		LOGE("Failed to open HTTP connection: %s", esp_err_to_name(err));
		esp_http_client_cleanup(client);
		task_fatal_error();
	}
	esp_http_client_fetch_headers(client);

	update_partition = esp_ota_get_next_update_partition(NULL);
	assert(update_partition != NULL);
	LOGI("Writing to partition subtype %d at offset 0x%x",
		 update_partition->subtype, update_partition->address);

	int binary_file_length = 0;
	/*deal with all receive packet*/
	bool image_header_was_checked = false;
	while (1)
	{
		int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
		if (data_read < 0)
		{
			LOGE("Error: SSL data read error");
			http_cleanup(client);
			task_fatal_error();
		}
		else if (data_read > 0)
		{
			if (image_header_was_checked == false)
			{
				esp_app_desc_t new_app_info;
				if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
				{
					// check current version with downloading
					memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
					LOGI("New firmware version: %s", new_app_info.version);

					esp_app_desc_t running_app_info;
					if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
					{
						LOGI("Running firmware version: %s", running_app_info.version);
					}

					const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
					esp_app_desc_t invalid_app_info;
					if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
					{
						LOGI("Last invalid firmware version: %s", invalid_app_info.version);
					}

					// check current version with last invalid partition
					if (last_invalid_app != NULL)
					{
						if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
						{
							LOGW("New version is the same as invalid version.");
							LOGW("Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
							LOGW("The firmware has been rolled back to the previous version.");
							http_cleanup(client);
							// infinite_loop();
							esp_ota_abort(update_handle);
							task_fatal_error();
						}
					}
#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
					// if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
					// {
					// 	LOGW("Current running version is the same as a new. We will not continue the update.");
					// 	http_cleanup(client);
					// 	// infinite_loop();
					// 	esp_ota_abort(update_handle);
					// 	task_fatal_error();
					// }
#endif

					image_header_was_checked = true;

					err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
					if (err != ESP_OK)
					{
						LOGE("esp_ota_begin failed (%s)", esp_err_to_name(err));
						http_cleanup(client);
						esp_ota_abort(update_handle);
						task_fatal_error();
					}
					LOGI("esp_ota_begin succeeded");
				}
				else
				{
					LOGE("received package is not fit len");
					http_cleanup(client);
					esp_ota_abort(update_handle);
					task_fatal_error();
				}
			}
			err = esp_ota_write(update_handle, (const void *)ota_write_data, data_read);
			if (err != ESP_OK)
			{
				http_cleanup(client);
				esp_ota_abort(update_handle);
				task_fatal_error();
			}
			binary_file_length += data_read;
			LOGD("Written image length %d", binary_file_length);
		}
		else if (data_read == 0)
		{
			/*
			 * As esp_http_client_read never returns negative error code, we rely on
			 * `errno` to check for underlying transport connectivity closure if any
			 */
			if (errno == ECONNRESET || errno == ENOTCONN)
			{
				LOGE("Connection closed, errno = %d", errno);
				break;
			}
			if (esp_http_client_is_complete_data_received(client) == true)
			{
				LOGI("Connection closed");
				break;
			}
		}
	}
	LOGI("Total Write binary data length: %d", binary_file_length);
	if (esp_http_client_is_complete_data_received(client) != true)
	{
		LOGE("Error in receiving complete file");
		http_cleanup(client);
		esp_ota_abort(update_handle);
		task_fatal_error();
	}

	err = esp_ota_end(update_handle);
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
		http_cleanup(client);
		task_fatal_error();
	}

	err = esp_ota_set_boot_partition(update_partition);
	if (err != ESP_OK)
	{
		LOGE("esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
		http_cleanup(client);
		task_fatal_error();
	}
	LOGI("Prepare to restart system!");
	esp_restart();
	return;
}

void ota_init()
{
	uint8_t sha_256[HASH_LEN] = {0};
	esp_partition_t partition;

	// get sha256 digest for the partition table
	partition.address = ESP_PARTITION_TABLE_OFFSET;
	partition.size = ESP_PARTITION_TABLE_MAX_LEN;
	partition.type = ESP_PARTITION_TYPE_DATA;
	esp_partition_get_sha256(&partition, sha_256);
	print_sha256(sha_256, "SHA-256 for the partition table: ");

	// get sha256 digest for bootloader
	partition.address = ESP_BOOTLOADER_OFFSET;
	partition.size = ESP_PARTITION_TABLE_OFFSET;
	partition.type = ESP_PARTITION_TYPE_APP;
	esp_partition_get_sha256(&partition, sha_256);
	print_sha256(sha_256, "SHA-256 for bootloader: ");

	// get sha256 digest for running partition
	esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
	print_sha256(sha_256, "SHA-256 for current firmware: ");

	const esp_partition_t *running = esp_ota_get_running_partition();
	esp_ota_img_states_t ota_state;
	esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
	if (err == ESP_OK)
	{
		// if (ota_state == ESP_OTA_IMG_PENDING_VERIFY)
		// {
		// 	// run diagnostic function ...
		// 	bool diagnostic_is_ok = diagnostic();
		// 	if (diagnostic_is_ok)
		// 	{
		// 		LOGI("Diagnostics completed successfully! Continuing execution ...");
		// 		esp_ota_mark_app_valid_cancel_rollback();
		// 	}
		// 	else
		// 	{
		// 		LOGE("Diagnostics failed! Start rollback to the previous version ...");
		// 		esp_ota_mark_app_invalid_rollback_and_reboot();
		// 	}
		// }
	}
}

void ota_start(char *name, char *url, char *sum, int timeout)
{
	ota_info = (ota_info_t *)heap_caps_malloc_prefer(sizeof(ota_info_t) * 2, 2, MALLOC_CAP_DEFAULT | MALLOC_CAP_SPIRAM, MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
	strcpy(ota_info->name, name);
	strcpy(ota_info->url, url);
	strcpy(ota_info->sum, sum);
	ota_info->timeout = timeout;

	LOGI("Free memory: %d bytes, internal: %d bytes", esp_get_free_heap_size(), esp_get_free_internal_heap_size());
	TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
	UBaseType_t stackSize = uxTaskGetStackHighWaterMark(currentTask);
	LOGW("Stack size free: %u bytes", stackSize * sizeof(StackType_t));
	ota_example_task();
}
