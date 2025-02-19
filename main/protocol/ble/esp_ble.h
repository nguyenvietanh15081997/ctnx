#ifndef __ESP_BLE_H__
#define __ESP_BLE_H__

#define LED_OFF 0x0
#define LED_ON 0x1

#define CID_RD 0x0211

#define PROV_OWN_ADDR 0x0001

#define MSG_SEND_TTL 3
#define MSG_SEND_REL false
#define MSG_TIMEOUT 0
#define MSG_ROLE ROLE_PROVISIONER

#define COMP_DATA_PAGE_0 0x00

#define APP_KEY_IDX 0x0000
#define APP_KEY_OCTET 0x12

#define COMP_DATA_1_OCTET(msg, offset) (msg[offset])
#define COMP_DATA_2_OCTET(msg, offset) (msg[offset + 1] << 8 | msg[offset])

#define ESP_BLE_MESH_VND_MODEL_ID_CLIENT 0x0000
#define ESP_BLE_MESH_VND_MODEL_ID_SERVER 0x0001

#define ESP_BLE_MESH_VND_MODEL_OP_SEND ESP_BLE_MESH_MODEL_OP_3(0x00, CID_RD)
#define ESP_BLE_MESH_VND_MODEL_OP_STATUS ESP_BLE_MESH_MODEL_OP_3(0x01, CID_RD)

typedef struct
{
	uint16_t net_idx;
	uint16_t app_idx;
	uint8_t app_key[16];
} esp_ble_mesh_key_t;

#endif