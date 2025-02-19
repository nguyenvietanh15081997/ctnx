/* main.c - Application main entry point */

/*
 * Copyright (c) 2018 Espressif Systems (Shanghai) PTE LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>

#include "nvs_flash.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "esp_ble_mesh_sensor_model_api.h"

#include "ble_mesh_example_init.h"

#include "esp_ble.h"

static uint8_t dev_uuid[16];

esp_ble_mesh_key_t prov_key;

esp_ble_mesh_client_t config_client;
esp_ble_mesh_client_t onoff_client;
esp_ble_mesh_client_t sensor_client;

static esp_ble_mesh_cfg_srv_t config_server = {
		.relay = ESP_BLE_MESH_RELAY_DISABLED,
		.beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
		.friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
		.friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
		.gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
		.gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
		.default_ttl = 7,
		/* 3 transmissions with 20ms interval */
		.net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
		.relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

static const esp_ble_mesh_client_op_pair_t vnd_op_pair[] = {
		{ESP_BLE_MESH_VND_MODEL_OP_SEND, ESP_BLE_MESH_VND_MODEL_OP_STATUS},
};

static esp_ble_mesh_client_t vendor_client = {
		.op_pair_size = ARRAY_SIZE(vnd_op_pair),
		.op_pair = vnd_op_pair,
};

static esp_ble_mesh_model_op_t vnd_op[] = {
		ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_VND_MODEL_OP_STATUS, 2),
		ESP_BLE_MESH_MODEL_OP_END,
};

static esp_ble_mesh_model_t root_models[] = {
		// ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
		ESP_BLE_MESH_MODEL_CFG_CLI(&config_client),
		ESP_BLE_MESH_MODEL_GEN_ONOFF_CLI(NULL, &onoff_client),
		ESP_BLE_MESH_MODEL_SENSOR_CLI(NULL, &sensor_client),
};

static esp_ble_mesh_model_t vnd_models[] = {
		ESP_BLE_MESH_VENDOR_MODEL(CID_RD, ESP_BLE_MESH_VND_MODEL_ID_CLIENT,
															vnd_op, NULL, &vendor_client),
};

static esp_ble_mesh_elem_t elements[] = {
		ESP_BLE_MESH_ELEMENT(0, root_models, vnd_models),
};

static esp_ble_mesh_comp_t composition = {
		.cid = CID_RD,
		.elements = elements,
		.element_count = ARRAY_SIZE(elements),
};

static esp_ble_mesh_prov_t provision = {
		.prov_uuid = dev_uuid,
		.prov_unicast_addr = PROV_OWN_ADDR,
		.prov_start_address = 0x0005,
		.prov_attention = 0x00,
		.prov_algorithm = 0x00,
		.prov_pub_key_oob = 0x00,
		.prov_static_oob_val = NULL,
		.prov_static_oob_len = 0x00,
		.flags = 0x00,
		.iv_index = 0x00,
};

esp_err_t ble_mesh_init(void)
{
	esp_err_t err = ESP_OK;

	prov_key.net_idx = ESP_BLE_MESH_KEY_PRIMARY;
	prov_key.app_idx = APP_KEY_IDX;
	memset(prov_key.app_key, APP_KEY_OCTET, sizeof(prov_key.app_key));

	ble_mesh_get_dev_uuid(dev_uuid);

	err = esp_ble_mesh_init(&provision, &composition);
	if (err != ESP_OK)
	{
		LOGE("Failed to initialize mesh stack (err %d)", err);
		return err;
	}

	err = esp_ble_mesh_provisioner_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);
	if (err != ESP_OK)
	{
		LOGE("Failed to enable mesh provisioner (err %d)", err);
		return err;
	}

	err = esp_ble_mesh_provisioner_add_local_app_key(prov_key.app_key, prov_key.net_idx, prov_key.app_idx);
	if (err != ESP_OK)
	{
		LOGE("Failed to add local AppKey (err %d)", err);
		return err;
	}

	LOGI("BLE Mesh Provisioner initialized");

	return err;
}
